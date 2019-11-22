#include "pr.hpp"

#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <functional>
#include <stack>
#include <queue>
#include <map>

#define MAX_CAMERA_STACK_SIZE 1000

namespace pr {
    // foward
    class Shader;
    class PrimitivePipeline;
    class TexturedTrianglePipeline;
    class MSFrameBufferObject;

    namespace {
        // Global Variable (Internal)
        Config g_config;
        GLFWwindow *g_window = nullptr;

        // MainFrameBuffer
        MSFrameBufferObject *g_frameBuffer = nullptr;
        PrimitivePipeline *g_primitivePipeline = nullptr;
        TexturedTrianglePipeline *g_texturedTrianglePipeline = nullptr;
    }

    // Input System
    enum class InputEventType {
        MouseButtons,
        MouseMove,
        MouseScroll,
        Keys
    };
    struct MouseButtonEvent {
        int button; // GLFW_MOUSE_BUTTON_LEFT or GLFW_MOUSE_BUTTON_RIGHT  or GLFW_MOUSE_BUTTON_MIDDLE or other 
        int action; // One of GLFW_PRESS or GLFW_RELEASE.
    };
    struct MouseMoveEvent {
        float x;
        float y;
    };
    struct MouseScrollEvent {
        float dx;
        float dy;
    };
    struct KeyEvent {
        int key;
        int action; // GLFW_PRESS, GLFW_RELEASE or GLFW_REPEAT.
    };
    struct InputEvent {
        InputEventType type;
        union {
            MouseButtonEvent mouseButtonEvent;
            MouseMoveEvent   mouseMoveEvent;
            MouseScrollEvent mouseScrollEvent;
            KeyEvent         keyEvent;
        } data;
    };
    enum class ButtonCondition {
        Released = 0,
        Pressed  = 1
    };

    class ICamera;

    namespace {
        std::queue<InputEvent> g_events;

        std::map<int, ButtonCondition> g_mouseButtonConditions;
        std::map<int, ButtonCondition> g_mouseButtonConditionsPrevious;
        glm::vec2 g_mousePosition = glm::vec2(0, 0);
        glm::vec2 g_mouseDelta = glm::vec2(0, 0);
        glm::vec2 g_mouseScrollDelta = glm::vec2(0, 0);

        std::map<int, ButtonCondition> g_keyConditions;
        std::map<int, ButtonCondition> g_keyConditionsPrevious;

        // Global Variable (User)
        std::stack<std::unique_ptr<const ICamera>> g_cameraStack;
    }

    // Notes:
    //     VAO, Shader, FrameBuffer states are changed anywhere

    static bool HasCompileError(GLuint shader) {
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        return success == GL_FALSE;
    }
    static std::string GetShaderInfoLog(GLuint shader) {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> logBuffer(maxLength + 1);
        glGetShaderInfoLog(shader, maxLength, &maxLength, logBuffer.data());
        return logBuffer.data();
    }
    static std::string GetProgramInfoLog(GLuint shader) {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> logBuffer(maxLength + 1);
        glGetProgramInfoLog(shader, maxLength, &maxLength, logBuffer.data());
        return logBuffer.data();
    }
    static bool HasLinkError(GLuint program) {
        GLint success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        return success == GL_FALSE;
    }
    class Shader {
    public:
        Shader(const char *vs, const char *fs) {
            _vs = glCreateShader(GL_VERTEX_SHADER);
            _fs = glCreateShader(GL_FRAGMENT_SHADER);
            PR_ASSERT(_vs);
            PR_ASSERT(_fs);
            glShaderSource(_vs, 1, &vs, nullptr);
            glShaderSource(_fs, 1, &fs, nullptr);
            glCompileShader(_vs);
            glCompileShader(_fs);

            if (HasCompileError(_vs)) {
                std::cout << GetShaderInfoLog(_vs) << std::endl;
                PR_ASSERT(0);
            }
            if (HasCompileError(_fs)) {
                std::cout << GetShaderInfoLog(_fs) << std::endl;
                PR_ASSERT(0);
            }

            _program = glCreateProgram();
            glAttachShader(_program, _vs);
            glAttachShader(_program, _fs);
            glLinkProgram(_program);

            if (HasLinkError(_program)) {
                std::cout << GetProgramInfoLog(_program) << std::endl;
                PR_ASSERT(0);
            }
        }
        ~Shader() {
            glDetachShader(_program, _vs);
            glDetachShader(_program, _fs);
            glDeleteProgram(_program);
        }
        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        void bind() {
            glUseProgram(_program);
        }
        void unbind() {
            glUseProgram(0);
        }
        void setUniformMatrix(glm::mat4 m, const char *variable) {
            auto location = glGetUniformLocation(_program, variable);
            glProgramUniformMatrix4fv(_program, location, 1, GL_FALSE, (float *)&m);
        }
    private:
        GLuint _vs = 0;
        GLuint _fs = 0;
        GLuint _program = 0;
    };

    class PersistentBuffer {
    public:
        PersistentBuffer() {
            std::fill(_buffers, _buffers + BUFFER_COUNT, 0);
            std::fill(_pointers, _pointers + BUFFER_COUNT, nullptr);
            std::fill(_fences, _fences + BUFFER_COUNT, nullptr);
        }
        ~PersistentBuffer() {
            for (int i = 0; i < BUFFER_COUNT; ++i) {
                if (_pointers[i]) {
                    glUnmapNamedBuffer(_buffers[i]);
                    glDeleteBuffers(1, _buffers + i);
                }
            }
        }

        // return 
        //     uploaded head byte offset
        int upload(void *p, int bytes) {
            if (_capacityBytes - _head < bytes) {
                // Have to Expand the buffers
                glFinish();

                int requiredBytes = _head + bytes;
                // printf("%d -> %d\n", _capacityBytes, std::max(_capacityBytes * 2, requiredBytes));
                _capacityBytes = std::max(_capacityBytes * 2, requiredBytes);

                GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

                for (int i = 0; i < BUFFER_COUNT; ++i) {
                    if (_pointers[i]) {
                        glUnmapNamedBuffer(_buffers[i]);
                        glDeleteBuffers(1, _buffers + i);
                    }
                    glCreateBuffers(1, _buffers + i);
                    glNamedBufferStorage(_buffers[i], _capacityBytes, nullptr, flags);
                    _pointers[i] = glMapNamedBufferRange(_buffers[i], 0, _capacityBytes, flags);
                    PR_ASSERT(_pointers[i]);
                }
            }
            
            int head = _head;
            void *d = (uint8_t *)_pointers[_index] + _head;
            memcpy(d, p, bytes);
            _head += bytes;
            return head;
        }

        void finishFrame() {
            PR_ASSERT(_fences[_index] == 0);
            _fences[_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

            _index = (_index + 1) % BUFFER_COUNT;

            if (_fences[_index]) {
                GLenum e = glClientWaitSync(_fences[_index], 0, 0xFFFFFFFFFFFFFFFF);
                if (e == GL_ALREADY_SIGNALED) {
                    ; // It's ok
                }
                else if (e == GL_CONDITION_SATISFIED) {
                    ; // It's ok
                }
                else {
                    PR_ASSERT(0);
                }
                glDeleteSync(_fences[_index]);
                _fences[_index] = 0;
            }

            _head = 0;
        }

        GLuint buffer() {
            return _buffers[_index];
        }
        int capacityBytes() const {
            return _capacityBytes;
        }
    private:
        int _capacityBytes = 0;
        int _head = 0;

        enum {
            BUFFER_COUNT = 2
        };
        GLuint _buffers [BUFFER_COUNT];
        void * _pointers[BUFFER_COUNT];
        GLsync _fences  [BUFFER_COUNT];
        int    _index = 0;
    };

    class VertexArrayObject {
    public:
        VertexArrayObject() {
            glCreateVertexArrays(1, &_vao);
        }
        ~VertexArrayObject() {
            glDeleteVertexArrays(1, &_vao);
        }
        VertexArrayObject(const VertexArrayObject&) = delete;
        VertexArrayObject& operator=(const VertexArrayObject&) = delete;

        void bind() {
            glBindVertexArray(_vao);
        }
        void enable(int index) {
            glEnableVertexArrayAttrib(_vao, index);
        }
    private:
        GLuint _vao = 0;
    };


    class PrimitivePipeline {
    public:
        PrimitivePipeline() {
            _vao = std::unique_ptr<VertexArrayObject>(new VertexArrayObject());

            const char *vs = R"(
                #version 410

                uniform mat4 u_vp;
                
                layout(location = 0) in vec3 in_position;
                layout(location = 1) in vec3 in_color;
                out vec3 tofs_color;

                void main() {
                    gl_Position = u_vp * vec4(in_position, 1.0);
                    tofs_color = in_color;
                }
            )";
            const char *fs = R"(
                #version 410

                in vec3 tofs_color;
                layout(location = 0) out vec4 out_fragColor;

                void main() {
                  out_fragColor = vec4(tofs_color, 1.0);
                }
            )";
            _shader = std::unique_ptr<Shader>(new Shader(vs, fs));

            _vertices = std::unique_ptr<PersistentBuffer>(new PersistentBuffer());
            _indices  = std::unique_ptr<PersistentBuffer>(new PersistentBuffer());

            _vao->enable(0);
            _vao->enable(1);
        }
        void bind() {
            _shader->bind();

            _vao->bind();

            glBindBuffer(GL_ARRAY_BUFFER, _vertices->buffer());
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, p));
            glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void *)offsetof(Vertex, c));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indices->buffer());
        }

        PersistentBuffer *vertices() {
            return _vertices.get();
        }
        PersistentBuffer *indices() {
            return _indices.get();
        }
        void setVP(glm::mat4 viewMatrix, glm::mat4 projMatrix) {
            _shader->setUniformMatrix(projMatrix * viewMatrix, "u_vp");
        }
        void finishFrame() {
            _vertices->finishFrame();
            _indices->finishFrame();
        }
        struct Vertex {
            glm::vec3 p;
            glm::u8vec3 c;
        };
    private:
        std::unique_ptr<VertexArrayObject> _vao;
        std::unique_ptr<Shader> _shader;
        std::unique_ptr<PersistentBuffer> _vertices;
        std::unique_ptr<PersistentBuffer> _indices;
    };

    class Primitive {
    public:
        void clear() {
            _vertices.clear();
            _indices.clear();
            _maxIndex = 0;
        }
        uint32_t addVertex(glm::vec3 p, glm::u8vec3 c) {
            uint32_t index = (uint32_t)_vertices.size();
            PrimitivePipeline::Vertex v = { p, c };
            _vertices.emplace_back(v);
            return index;
        }
        void addIndex(uint32_t index) {
            _maxIndex = std::max(_maxIndex, index);
            _indices.emplace_back(index);
        }

    private:
        template <class T>
        void indexStore(std::vector<T> &storage, const std::vector<uint32_t> &indices) const {
            storage.clear();
            storage.reserve(indices.size());
            for (auto index : indices) {
                storage.emplace_back((T)index);
            }
        }
    public:
        void draw(PrimitiveMode mode, float width) {
            auto vertexBuffer = g_primitivePipeline->vertices();
            auto vertexOffsetBytes = vertexBuffer->upload(_vertices.data(), (int)(_vertices.size() * sizeof(PrimitivePipeline::Vertex)));
            auto vertexOffset = vertexOffsetBytes / sizeof(PrimitivePipeline::Vertex);

            auto indexBuffer = g_primitivePipeline->indices();
            int indexOffsetBytes = 0;

            GLuint indexBufferType = 0;
            if (_indices.empty() == false) {
                auto indices = g_primitivePipeline->indices();
                if (_maxIndex < std::numeric_limits<uint8_t>::max()) {
                    indexStore(_indices8, _indices);
                    indexOffsetBytes = indexBuffer->upload(_indices8.data(), (int)(_indices8.size() * sizeof(uint8_t)));
                    indexBufferType = GL_UNSIGNED_BYTE;
                } else if (_maxIndex < std::numeric_limits<uint16_t>::max()) {
                    indexStore(_indices16, _indices);
                    indexOffsetBytes = indices->upload(_indices16.data(), (int)_indices16.size() * sizeof(uint16_t)) / sizeof(uint16_t);
                    indexBufferType = GL_UNSIGNED_SHORT;
                }
                else {
                    indexOffsetBytes = indices->upload(_indices.data(), (int)_indices.size() * sizeof(uint32_t));
                    indexBufferType = GL_UNSIGNED_INT;
                }
            }

            g_primitivePipeline->bind();

            switch (mode) {
            case PrimitiveMode::Points:
                glPointSize(width);
                glDrawArrays(GL_POINTS, (GLint)vertexOffset, (GLsizei)_vertices.size());
                break;
            case PrimitiveMode::Lines:
                glLineWidth(width);
                if (_indices.empty()) {
                    glDrawArrays(GL_LINES, (GLint)vertexOffset, (GLsizei)_vertices.size());
                }
                else {
                    glDrawElementsBaseVertex(GL_LINES, (GLsizei)_indices.size(), indexBufferType, (uint8_t *)0 + indexOffsetBytes, (GLint)vertexOffset);
                }
                break;
            case PrimitiveMode::LineStrip:
                glLineWidth(width);
                glDrawArrays(GL_LINE_STRIP, (GLint)vertexOffset, (GLsizei)_vertices.size());
                break;
            }
        }
    private:
        std::vector<PrimitivePipeline::Vertex> _vertices;
        std::vector<uint32_t> _indices;

        // for upload
        std::vector<uint8_t> _indices8;
        std::vector<uint16_t> _indices16;

        uint32_t _maxIndex = 0;
    };

    class TexturedTrianglePipeline {
    public:
        TexturedTrianglePipeline() {
            _vao = std::unique_ptr<VertexArrayObject>(new VertexArrayObject());

            const char *vs = R"(
                #version 450

                uniform mat4 u_vp;
                
                layout(location = 0) in vec3 in_position;
                layout(location = 1) in vec2 in_texcoord;
                layout(location = 2) in vec3 in_color;

                out vec2 tofs_texcoord;
                out vec3 tofs_color;

                void main() {
                    gl_Position = u_vp * vec4(in_position, 1.0);
                    tofs_texcoord = in_texcoord;
                    tofs_color = in_color;
                }
            )";
            const char *fs = R"(
                #version 450

                uniform sampler2D u_image;

                in vec2 tofs_texcoord;
                in vec3 tofs_color;

                layout(location = 0) out vec4 out_fragColor;

                void main() {
                  out_fragColor = vec4(tofs_color, 1.0) * texture(u_image, tofs_texcoord);
                }
            )";
            _shader = std::unique_ptr<Shader>(new Shader(vs, fs));

            _vertices = std::unique_ptr<PersistentBuffer>(new PersistentBuffer());
            _indices = std::unique_ptr<PersistentBuffer>(new PersistentBuffer());

            _vao->enable(0);
            _vao->enable(1);
        }
        void bind() {
            _shader->bind();

            _vao->bind();

            glBindBuffer(GL_ARRAY_BUFFER, _vertices->buffer());
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, p));
            glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void *)offsetof(Vertex, c));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indices->buffer());
        }

        PersistentBuffer *vertices() {
            return _vertices.get();
        }
        PersistentBuffer *indices() {
            return _indices.get();
        }
        void setVP(glm::mat4 viewMatrix, glm::mat4 projMatrix) {
            _shader->setUniformMatrix(projMatrix * viewMatrix, "u_vp");
        }
        void finishFrame() {
            _vertices->finishFrame();
            _indices->finishFrame();
        }
        struct Vertex {
            glm::vec3 p;
            glm::vec2 uv;
            glm::u8vec3 c;
        };
    private:
        std::unique_ptr<VertexArrayObject> _vao;
        std::unique_ptr<Shader> _shader;
        std::unique_ptr<PersistentBuffer> _vertices;
        std::unique_ptr<PersistentBuffer> _indices;
    };

    class MSFrameBufferObject {
    public:
        MSFrameBufferObject() {
            glCreateFramebuffers(1, &_fb);
            glCreateRenderbuffers(1, &_color);
            glCreateRenderbuffers(1, &_depth);
        }
        ~MSFrameBufferObject() {
            glDeleteFramebuffers(1, &_fb);
        }
        void resize(int width, int height, int samples) {
            if (width == _width && height == _height && _samples == samples) {
                return;
            }
            _width = width;
            _height = height;
            _samples = samples;

            glNamedRenderbufferStorageMultisample(_color, _samples, GL_RGBA8, _width, _height);
            glNamedRenderbufferStorageMultisample(_depth, _samples, GL_DEPTH_COMPONENT24, _width, _height);

            glNamedFramebufferRenderbuffer(_fb, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _color);
            glNamedFramebufferRenderbuffer(_fb, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depth);

            auto status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            PR_ASSERT(status == GL_FRAMEBUFFER_COMPLETE);

            float clearColor[4] = { 0.0f, 0, 0, 0 };
            float clearDepth = 1.0f;
            glClearNamedFramebufferfv(_fb, GL_COLOR, 0, clearColor);
            glClearNamedFramebufferfv(_fb, GL_DEPTH, 0, &clearDepth);
        }
        void clear(float r, float g, float b, float a, bool depthClear) {
            float clearColor[4] = { r, g, b, a};
            glClearNamedFramebufferfv(_fb, GL_COLOR, 0, clearColor);
            if (depthClear) {
                float d = 1.0f;
                glClearNamedFramebufferfv(_fb, GL_DEPTH, 0, &d);
            }
        }
        void bind() {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fb);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        }
        void copyToScreen() {
            glBlitNamedFramebuffer(
                _fb, /* read  */
                0,   /* write */
                0, 0, _width, _height,
                0, 0, _width, _height,
                GL_COLOR_BUFFER_BIT, GL_NEAREST
            );
        }
    private:
        GLuint _width = 0;
        GLuint _height = 0;
        GLuint _samples = 0;
        GLuint _fb = 0;
        GLuint _color = 0;
        GLuint _depth = 0;
    };

    class ICamera {
    public:
        virtual ~ICamera() {};
        virtual glm::mat4 getProjectionMatrx() const = 0;
        virtual glm::mat4 getViewMatrix() const = 0;
    };
    class CameraDefault : public ICamera {
    public:
        virtual glm::mat4 getProjectionMatrx() const override {
            return glm::identity<glm::mat4>();
        }
        virtual glm::mat4 getViewMatrix() const override {
            return glm::identity<glm::mat4>();
        }
    };

    class Camera3DObject : public ICamera {
    public:
        Camera3DObject(Camera3D camera):_camera(camera) {

        }
        virtual glm::mat4 getProjectionMatrx() const override {
            return glm::perspectiveFov(_camera.fovy, (float)GetScreenWidth(), (float)GetScreenHeight(), _camera.zNear, _camera.zFar);
        }
        virtual glm::mat4 getViewMatrix() const override {
            glm::mat4 m = glm::lookAt(_camera.origin, _camera.lookat, _camera.up);

            if (_camera.zUp) {
                m = glm::rotate(
                    m,
                    glm::pi<float>() * 0.5f,
                    glm::vec3(-1, 0, 0)
                );
            }
            return m;
        }
    private:
        Camera3D _camera;
    };

    static const ICamera *GetCurrentCamera() {
        if (g_cameraStack.empty()) {
            static CameraDefault d;
            return &d;
        }
        return g_cameraStack.top().get();
    }
    static void UpdateCurrentMatrix() {
        auto camera = GetCurrentCamera();
        auto v = camera->getViewMatrix();
        auto p = camera->getProjectionMatrx();
        auto vp = v * p;
        g_primitivePipeline->setVP(camera->getViewMatrix(), camera->getProjectionMatrx());
    }

    /*
     Graphics Core
    */
    void ClearBackground(float r, float g, float b, float a) {
        g_frameBuffer->clear(r, g, b, a, true);
    }
    void SetDepthTest(bool enabled) {
        if (enabled) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }
    void BeginCamera(Camera3D camera) {
        PR_ASSERT(g_cameraStack.size() <= MAX_CAMERA_STACK_SIZE);

        g_cameraStack.push(std::unique_ptr<const ICamera>(new Camera3DObject(camera)));
        UpdateCurrentMatrix();
    }
    void EndCamera() {
        PR_ASSERT(g_cameraStack.empty() == false);

        g_cameraStack.pop();
        UpdateCurrentMatrix();
    }
    namespace {
        bool g_primitiveEnabled = false;
        Primitive g_primitive;
        PrimitiveMode g_primitiveMode;
        float g_primitiveWidth = 0.0f;
    }

    void PrimBegin(PrimitiveMode mode, float width) {
        PR_ASSERT(g_primitiveEnabled == false);
        g_primitiveEnabled = true;
        g_primitiveMode = mode;
        g_primitiveWidth = width;
    }
    uint32_t PrimVertex(glm::vec3 p, glm::u8vec3 c) {
        PR_ASSERT(g_primitiveEnabled);
        return g_primitive.addVertex(p, c);
    }
    void PrimIndex(uint32_t index) {
        PR_ASSERT(g_primitiveEnabled);
        return g_primitive.addIndex(index);
    }
    void PrimEnd() {
        PR_ASSERT(g_primitiveEnabled);
        g_primitive.draw(g_primitiveMode, g_primitiveWidth);
        g_primitive.clear();
        g_primitiveEnabled = false;
    }

    void DrawLine(glm::vec3 p0, glm::vec3 p1, glm::u8vec3 c, float lineWidth) {
        PrimBegin(PrimitiveMode::Lines, lineWidth);
        PrimVertex(p0, c);
        PrimVertex(p1, c);
        PrimEnd();
    }
    void DrawPoint(glm::vec3 p, glm::u8vec3 c, float pointSize) {
        PrimBegin(PrimitiveMode::Points, pointSize);
        PrimVertex(p, c);
        PrimEnd();
    }
    void DrawCircle(glm::vec3 o, glm::u8vec3 c, float radius, int vertexCount, float lineWidth) {
        LinearTransform<float> i2rad(0.0f, (float)(vertexCount - 1), 0.0f, glm::pi<float>() * 2.0f);

        PrimBegin(PrimitiveMode::LineStrip, lineWidth);
        for (int i = 0; i < vertexCount; ++i) {
            float radian = i2rad.evaluate((float)i);
            glm::vec3 p = {
                std::cos(radian),
                std::sin(radian),
                0.0f
            };
            PrimVertex(o + radius * p, c);
        }
        PrimEnd();
    }
    void DrawGrid(GridAxis axis, float step, int blockCount, glm::u8vec3 c, float lineWidth) {
        int hBlockCount = blockCount / 2;
        float hWide = blockCount * step * 0.5f;

        glm::vec3 uaxis;
        glm::vec3 vaxis;
        switch (axis) {
        case GridAxis::XY:
            uaxis = glm::vec3(1, 0, 0);
            vaxis = glm::vec3(0, 1, 0);
            break;
        case GridAxis::XZ:
            uaxis = glm::vec3(1, 0, 0);
            vaxis = glm::vec3(0, 0, 1);
            break;
        case GridAxis::YZ:
            uaxis = glm::vec3(0, 1, 0);
            vaxis = glm::vec3(0, 0, 1);
            break;
        default:
            PR_ASSERT(0);
            break;
        }

        PrimBegin(PrimitiveMode::Lines, lineWidth);
        {
            // u line
            auto L = uaxis * (-hWide);
            auto R = uaxis * (+hWide);
            for (int ui = -hBlockCount; ui <= hBlockCount; ++ui) {
                PrimVertex(L + vaxis * (float)ui * step, c);
                PrimVertex(R + vaxis * (float)ui * step, c);
            }
        }
        {
            // v line
            auto L = vaxis * (-hWide);
            auto R = vaxis * (+hWide);
            for (int vi = -hBlockCount; vi <= hBlockCount; ++vi) {
                PrimVertex(uaxis * (float)vi * step + L, c);
                PrimVertex(uaxis * (float)vi * step + R, c);
            }
        }
        PrimEnd();
    }
    void DrawTube(glm::vec3 p0, glm::vec3 p1, float radius0, float radius1, glm::u8vec3 c, int vertexCount, float lineWidth) {
        if (radius0 == 0.0f && radius1 == 0.0f) {
            DrawLine(p0, p1, c, lineWidth);
        }

        glm::vec3 T = p1 - p0;
        glm::vec3 z = glm::normalize(T);

        glm::vec3 x;
        glm::vec3 y;
        GetOrthonormalBasis<float>(z, &x, &y);

        PrimBegin(PrimitiveMode::Lines, lineWidth);

        std::vector<uint32_t> circleVerticesB(vertexCount);
        std::vector<uint32_t> circleVerticesT(vertexCount);
        LinearTransform<float> i2rad(0.0f, (float)(vertexCount), 0.0f, glm::pi<float>() * 2.0f);
        for (int i = 0; i < vertexCount; ++i) {
            float radian = i2rad.evaluate((float)i);
            glm::vec3 ring = x * std::sin(radian) + y * std::cos(radian);
            circleVerticesB[i] = PrimVertex(p0 + ring * radius0, c);
            circleVerticesT[i] = PrimVertex(p1 + ring * radius1, c);
        }

        // Body
        for (int i = 0; i < vertexCount; ++i) {
            PrimIndex(circleVerticesB[i]);
            PrimIndex(circleVerticesT[i]);
        }

        // Two Circles
        if (0.0f < radius0) {
            for (int i = 0; i < vertexCount; ++i) {
                PrimIndex(circleVerticesB[i]);
                PrimIndex(circleVerticesB[(i + 1) % vertexCount]);
            }
        }
        if (0.0f < radius1) {
            for (int i = 0; i < vertexCount; ++i) {
                PrimIndex(circleVerticesT[i]);
                PrimIndex(circleVerticesT[(i + 1) % vertexCount]);
            }
        }

        PrimEnd();
    }
    void DrawArrow(glm::vec3 p0, glm::vec3 p1, float bodyRadius, glm::u8vec3 c, int vertexCount, float lineWidth) {
        float triR = bodyRadius * 2.5f;
        float triH = bodyRadius * 7.0f;
        glm::vec3 T = p1 - p0;
        float length = glm::length(T);
        T /= length;

        glm::vec3 triO = p0 + T * (length - triH);
        DrawTube(p0, triO, bodyRadius, bodyRadius, c, vertexCount, lineWidth);
        DrawTube(triO, p1, triR, 0.0f, c, vertexCount, lineWidth);
    }
    void DrawXYZAxis(float length, float bodyRadius, int vertexCount, float lineWidth) {
        DrawArrow({}, glm::vec3((float)length, 0, 0), bodyRadius, { 255, 0, 0 }, vertexCount, lineWidth);
        DrawArrow({}, glm::vec3(0, (float)length, 0), bodyRadius, { 0, 255, 0 }, vertexCount, lineWidth);
        DrawArrow({}, glm::vec3(0, 0, (float)length), bodyRadius, { 0, 0, 255 }, vertexCount, lineWidth);
    }

    static void MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* message, void const* user_param)
    {
        std::string src_str = [source]() {
            switch (source)
            {
            case GL_DEBUG_SOURCE_API: return "API";
            case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "WINDOW SYSTEM";
            case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER COMPILER";
            case GL_DEBUG_SOURCE_THIRD_PARTY: return "THIRD PARTY";
            case GL_DEBUG_SOURCE_APPLICATION: return "APPLICATION";
            case GL_DEBUG_SOURCE_OTHER: return "OTHER";
            }
            return "";
        }();

        std::string type_str = [type]() {
            switch (type)
            {
            case GL_DEBUG_TYPE_ERROR: return "ERROR";
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED_BEHAVIOR";
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "UNDEFINED_BEHAVIOR";
            case GL_DEBUG_TYPE_PORTABILITY: return "PORTABILITY";
            case GL_DEBUG_TYPE_PERFORMANCE: return "PERFORMANCE";
            case GL_DEBUG_TYPE_MARKER: return "MARKER";
            case GL_DEBUG_TYPE_OTHER: return "OTHER";
            }
            return "";
        }();

        std::string severity_str = [severity]() {
            switch (severity) {
            case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
            case GL_DEBUG_SEVERITY_LOW: return "LOW";
            case GL_DEBUG_SEVERITY_MEDIUM: return "MEDIUM";
            case GL_DEBUG_SEVERITY_HIGH: return "HIGH";
            }
            return "";
        }();

        std::cout << src_str << ", " << type_str << ", " << severity_str << ", " << id << ": " << message << '\n';
    }
    static void SetupGraphics() {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(MessageCallback, nullptr);

        glViewport(0, 0, g_config.ScreenWidth, g_config.ScreenHeight);

        g_primitivePipeline = new PrimitivePipeline();
        g_texturedTrianglePipeline = new TexturedTrianglePipeline();

        g_frameBuffer = new MSFrameBufferObject();
        g_frameBuffer->resize(g_config.ScreenWidth, g_config.ScreenHeight, g_config.NumSamples);
        g_frameBuffer->bind();

        glDisable(GL_BLEND);

        UpdateCurrentMatrix();
    }

    // Event Handling
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        InputEvent e;
        e.type = InputEventType::MouseButtons;
        e.data.mouseButtonEvent.action = action;
        e.data.mouseButtonEvent.button = button;
        g_events.push(e);
    }
    static void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
    {
        InputEvent e;
        e.type = InputEventType::MouseMove;
        e.data.mouseMoveEvent.x = (float)xpos;
        e.data.mouseMoveEvent.y = (float)ypos;
        g_events.push(e);
    }
    void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        InputEvent e;
        e.type = InputEventType::MouseScroll;
        e.data.mouseScrollEvent.dx = (float)xoffset;
        e.data.mouseScrollEvent.dy = (float)yoffset;
        g_events.push(e);
    }

    void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        if (action == GLFW_REPEAT) {
            return;
        }

        InputEvent e;
        e.type = InputEventType::Keys;
        e.data.keyEvent.key = key;
        e.data.keyEvent.action = action;
        g_events.push(e);
    }

    void Initialize(Config config) {
        g_config = config;

        glfwInit();

        glfwWindowHint(GLFW_SAMPLES, config.NumSamples);

        glfwWindowHint(GLFW_RED_BITS,   8);
        glfwWindowHint(GLFW_GREEN_BITS, 8);
        glfwWindowHint(GLFW_BLUE_BITS,  8);
        glfwWindowHint(GLFW_ALPHA_BITS, 8);

        glfwWindowHint(GLFW_DEPTH_BITS, 0);
        glfwWindowHint(GLFW_STENCIL_BITS, 0);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        g_window = glfwCreateWindow(config.ScreenWidth, config.ScreenHeight, config.Title.c_str(), NULL, NULL);
        
        glfwMakeContextCurrent(g_window);

        GLenum r = glewInit();
        if (r != GLEW_OK) {
            std::cout << glewGetErrorString(r) << std::endl;
            PR_ASSERT(0);
        }

        glfwSwapInterval(config.SwapInterval);

        glfwSetMouseButtonCallback(g_window, MouseButtonCallback);
        glfwSetCursorPosCallback(g_window, CursorPositionCallback);
        glfwSetKeyCallback(g_window, KeyCallback);
        glfwSetScrollCallback(g_window, ScrollCallback);

        double cx, cy;
        glfwGetCursorPos(g_window, &cx, &cy);
        g_mousePosition.x = (float)cx;
        g_mousePosition.y = (float)cy;

        SetupGraphics();
    }
    void CleanUp() {
        delete g_primitivePipeline;
        g_primitivePipeline = nullptr;

        delete g_frameBuffer;
        g_frameBuffer = nullptr;
        delete g_texturedTrianglePipeline;
        g_texturedTrianglePipeline = nullptr;

        glfwDestroyWindow(g_window);
        glfwTerminate();
    }
    int GetScreenWidth() {
        if (g_window) {
            int w, h;
            glfwGetWindowSize(g_window, &w, &h);
            return w;
        }
        return 0;
    }
    int GetScreenHeight() {
        if (g_window) {
            int w, h;
            glfwGetWindowSize(g_window, &w, &h);
            return h;
        }
        return 0;
    }

    double GetElapsedTime() {
        return glfwGetTime();
    }

    static void HandleInputEvents() {
        auto previousMousePosition = g_mousePosition;
        g_mouseButtonConditionsPrevious = g_mouseButtonConditions;
        g_keyConditionsPrevious = g_keyConditions;
        g_mouseScrollDelta = glm::vec2(0, 0);

        std::map<int, bool> mousebuttonChanged;
        std::map<int, bool> keyChanged;

        while (g_events.empty() == false) {
            InputEvent e = g_events.front();

            switch (e.type)
            {
            case InputEventType::MouseButtons: {
                MouseButtonEvent data = e.data.mouseButtonEvent;
                auto condition = g_mouseButtonConditions[data.button];
                if (data.action != GLFW_PRESS && data.action != GLFW_RELEASE) {
                    break;
                }
                auto newCondition = data.action == GLFW_PRESS ? ButtonCondition::Pressed : ButtonCondition::Released;
                if (condition == newCondition) {
                    break;
                }

                // suspend process event. Because we can't handle on-off per 1 frame by polling senario.
                if (mousebuttonChanged[data.button]) {
                    goto suspend_event_handle;
                }
                mousebuttonChanged[data.button] = true;

                // update mouse button state
                g_mouseButtonConditions[data.button] = newCondition;
                break;
            }
            case InputEventType::MouseMove: {
                MouseMoveEvent data = e.data.mouseMoveEvent;
                g_mousePosition.x = data.x;
                g_mousePosition.y = data.y;
                break;
            }
            case InputEventType::MouseScroll: {
                MouseScrollEvent data = e.data.mouseScrollEvent;
                g_mouseScrollDelta.x += data.dx;
                g_mouseScrollDelta.y += data.dy;
                break;
            }
            case InputEventType::Keys: {
                KeyEvent data = e.data.keyEvent;
                auto condition = g_keyConditions[data.key];
                if (data.action != GLFW_PRESS && data.action != GLFW_RELEASE) {
                    break;
                }
                auto newCondition = data.action == GLFW_PRESS ? ButtonCondition::Pressed : ButtonCondition::Released;
                if (condition == newCondition) {
                    break;
                }

                // suspend process event. Because we can't handle on-off per 1 frame by polling senario.
                if (keyChanged[data.key]) {
                    goto suspend_event_handle;
                }
                keyChanged[data.key] = true;

                // update key state
                g_keyConditions[data.key] = newCondition;
                break;
            }
            default:
                PR_ASSERT(0);
                break;
            }

            g_events.pop();
        }
suspend_event_handle:
        ;

        g_mouseDelta = g_mousePosition - previousMousePosition;
    }
    bool ProcessSystem() {
        g_frameBuffer->copyToScreen();
        glfwSwapBuffers(g_window);

        g_primitivePipeline->finishFrame();

        glfwPollEvents();

        HandleInputEvents();

        int width, height;
        glfwGetFramebufferSize(g_window, &width, &height);
        g_frameBuffer->resize(width, height, g_config.NumSamples);

        glViewport(0, 0, width, height);

        return glfwWindowShouldClose(g_window);
    }

    glm::vec2 GetMousePosition() {
        return g_mousePosition;
    }
    glm::vec2 GetMouseDelta() {
        return g_mouseDelta;
    }
    glm::vec2 GetMouseScrollDelta() {
        return g_mouseScrollDelta;
    }
    int MOUSE_BUTTON_LEFT   = GLFW_MOUSE_BUTTON_LEFT;
    int MOUSE_BUTTON_MIDDLE = GLFW_MOUSE_BUTTON_MIDDLE;
    int MOUSE_BUTTON_RIGHT  = GLFW_MOUSE_BUTTON_RIGHT;

    bool IsMouseButtonPressed(int button) {
        return g_mouseButtonConditions[button] == ButtonCondition::Pressed;
    }
    bool IsMouseButtonDown(int button) {
        return
            g_mouseButtonConditions[button] == ButtonCondition::Pressed &&
            g_mouseButtonConditionsPrevious[button] == ButtonCondition::Released;
    }
    bool IsMouseButtonUp(int button) {
        return
            g_mouseButtonConditions[button] == ButtonCondition::Released &&
            g_mouseButtonConditionsPrevious[button] == ButtonCondition::Pressed;
    }

    extern int KEY_LEFT_SHIFT = GLFW_KEY_LEFT_SHIFT;
    extern int KEY_RIGHT_SHIFT = GLFW_KEY_RIGHT_SHIFT;

    bool IsKeyPressed(int button) {
        return g_keyConditions[button] == ButtonCondition::Pressed;
    }

    bool IsKeyDown(int button) {
        return
            g_keyConditions[button] == ButtonCondition::Pressed &&
            g_keyConditionsPrevious[button] == ButtonCondition::Released;
    }
    bool IsKeyUp(int button) {
        return
            g_keyConditions[button] == ButtonCondition::Released &&
            g_keyConditionsPrevious[button] == ButtonCondition::Pressed;
    }

    // Interactions
    void UpdateCameraBlenderLike(
        Camera3D *camera,
        float wheel_sensitivity,
        float zoom_mouse_sensitivity,
        float rotate_sensitivity,
        float shift_sensitivity
    ) {
        glm::vec2 mousePositionDelta = GetMouseDelta();

        bool shift = IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT);

        bool middle_pressed = IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && !IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
        bool right_pressed  = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)  && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

        float deltaWheel = GetMouseScrollDelta().y;
        if (deltaWheel != 0.0f) {
            auto lookat = camera->lookat;
            auto origin = camera->origin;

            float d = glm::distance(lookat, origin);
            d -= d * deltaWheel * wheel_sensitivity;
            d = std::max(0.01f, d);

            auto dir = glm::normalize(origin - lookat);
            origin = lookat + dir * d;
            camera->origin = origin;
        } else if (right_pressed) {
            if (mousePositionDelta.y != 0.0f) {
                auto lookat = camera->lookat;
                auto origin = camera->origin;

                float d = glm::distance(lookat, origin);
                d -= d * mousePositionDelta.y * zoom_mouse_sensitivity;
                d = std::max(0.1f, d);

                auto dir = glm::normalize(origin - lookat);
                origin = lookat + dir * d;
                camera->origin = origin;
            }
        }

        // Handle Rotation
        if (middle_pressed && shift == false) {
            if (mousePositionDelta.x != 0.0f) {
                auto lookat = camera->lookat;
                auto origin = camera->origin;
                auto up = camera->up;
                auto S = origin - lookat;

                auto q = glm::angleAxis(-rotate_sensitivity * mousePositionDelta.x, glm::vec3(0, 1, 0));

                S  = q * S;
                up = q * up;

                camera->origin = lookat + S;
                camera->up = up;
            }

            if (mousePositionDelta.y != 0.0f) {
                auto lookat = camera->lookat;
                auto origin = camera->origin;

                auto foward = origin - lookat;

                auto up = camera->up;
                auto right = glm::normalize(glm::cross(foward, up));
                up = glm::cross(right, foward);
                up = glm::normalize(up);

                auto S = origin - lookat;
                auto q = glm::angleAxis(rotate_sensitivity * mousePositionDelta.y, right);

                S  = q * S;
                up = q * up;

                camera->origin = lookat + S;
                camera->up = up;
            }
        }

        if (middle_pressed && shift) {
            auto lookat = camera->lookat;
            auto origin = camera->origin;

            auto foward = origin - lookat;

            auto up = camera->up;
            if (glm::length(up) <= std::numeric_limits<float>::epsilon()) {
                up = glm::vec3 { 0, 1, 0 };
            }
            auto right = glm::normalize(glm::cross(foward, up));
            up = glm::cross(right, foward);
            up = glm::normalize(up);

            float d = glm::distance(lookat, origin);

            auto move =
                d * up * shift_sensitivity * mousePositionDelta.y +
                d * right * shift_sensitivity * mousePositionDelta.x;

            lookat = lookat + move;
            origin = origin + move;

            camera->lookat = lookat;
            camera->origin = origin;
        }
    }
}