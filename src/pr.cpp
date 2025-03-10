﻿#include "pr.hpp"

#ifdef _WIN32
    #define NOMINMAX
    #include <Windows.h>
    #if defined(DrawText)
    #undef DrawText
    #endif
#endif

#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include "imgui.h"

#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <functional>
#include <stack>
#include <queue>
#include <map>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <clocale>
#include <fstream>

#include "pr_sdf.h"
#include "pr_verdana.h"

#define MAX_CAMERA_STACK_SIZE 1000

static const float MANIP_HIGHLIGHT_LINE_WIDTH = 3.0f;

namespace pr {
    // foward
    class Shader;
    class PrimitivePipeline;
    class TexturedTrianglePipeline;
    class FontPipeline;
    class MSFrameBufferObject;
    class TextDam;

    namespace {
        // Global Variable (Internal)
        Config g_config;
        GLFWwindow *g_window = nullptr;
        GLFWcursor *g_mouseCursors[ImGuiMouseCursor_COUNT];

        // MainFrameBuffer
        MSFrameBufferObject      *g_frameBuffer = nullptr;
        PrimitivePipeline        *g_primitivePipeline = nullptr;
        TexturedTrianglePipeline *g_texturedTrianglePipeline = nullptr;
        FontPipeline             *g_fontPipeline = nullptr;
        
        // Font
        TextDam *g_textDam = nullptr;

        // Time
        double g_frameTime = 0.0;
        double g_frameDeltaTime = 0.0;
        double g_framePerSeconds = 0.0;

        // extension 
        bool g_hasExternalObjectsExt = false;
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

        std::function<void(std::vector<std::string>)> g_onFileDrop;

        glm::mat4 g_objectTransform = glm::identity<glm::mat4>();
        std::stack<std::unique_ptr<const ICamera>> g_cameraStack;
    }

    struct GraphicState {
        bool depthTest = false;
        BlendMode blendMode = BlendMode::None;
        bool scissorTest = false;
        int scissorX = 0;
        int scissorY = 0;
        int scissorWidth = 0;
        int scissorHeight = 0;

        void apply() {
            if (depthTest) {
                glEnable(GL_DEPTH_TEST);
            } else {
                glDisable(GL_DEPTH_TEST);
            }
            switch (blendMode) {
            case BlendMode::None:
                glDisable(GL_BLEND);
                break;
            case BlendMode::Alpha:
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BlendMode::Additive:
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;
            case BlendMode::Multiply:
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA /* GL_ZERO or GL_ONE_MINUS_SRC_ALPHA */);
                break;
            case BlendMode::Screen:
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);
                break;
            }
            if (scissorTest) {
                glEnable(GL_SCISSOR_TEST);
                glScissor(scissorX, scissorY, scissorWidth, scissorHeight);
            }
            else {
                glDisable(GL_SCISSOR_TEST);
            }
        }
    };
    namespace {
        std::stack<GraphicState> g_states;
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
        void setUniformTextureIndex(GLuint index, const char *variable) {
            auto location = glGetUniformLocation(_program, variable);
            glProgramUniform1i(_program, location, index);
        }
        void setUniformVec3(glm::u8vec3 value, const char *variable) {
            auto location = glGetUniformLocation(_program, variable);
            glProgramUniform3f(_program, location, value.x / 255.0f, value.y / 255.0f, value.z / 255.0f);
        }
        void setUniformFloat(float value, const char *variable) {
            auto location = glGetUniformLocation(_program, variable);
            glProgramUniform1f(_program, location, value);
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

                // If requiredBytes is bigger than limit and the bytes is small enough then skip allocation.
                if( g_config.PersistentBufferLimitHint < requiredBytes && bytes <= _capacityBytes )
                {
                    // Just reset head 
                    _head = 0;
                }
                else
                {
                    // expand buffer
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
        void bindShaderAndBuffer() {
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

    class PrimitiveDam {
    public:
        using Vertex = PrimitivePipeline::Vertex;

        void clear() {
            _vertices.clear();
            _indices.clear();
            _maxIndex = 0;
        }
        uint32_t addVertex(glm::vec3 p, glm::u8vec3 c) {
            uint32_t index = (uint32_t)_vertices.size();
            Vertex v = { p, c };
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
            width = glm::max(width, 1.0f);

            auto vertexBuffer = g_primitivePipeline->vertices();
            auto vertexOffsetBytes = vertexBuffer->upload(_vertices.data(), (int)(_vertices.size() * sizeof(Vertex)));
            auto vertexOffset = vertexOffsetBytes / sizeof(Vertex);

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
                    indexOffsetBytes = indices->upload(_indices16.data(), (int)_indices16.size() * sizeof(uint16_t));
                    indexBufferType = GL_UNSIGNED_SHORT;
                }
                else {
                    indexOffsetBytes = indices->upload(_indices.data(), (int)_indices.size() * sizeof(uint32_t));
                    indexBufferType = GL_UNSIGNED_INT;
                }
            }

            g_primitivePipeline->bindShaderAndBuffer();

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
        std::vector<Vertex> _vertices;
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
                layout(location = 2) in vec4 in_color;

                out vec2 tofs_texcoord;
                out vec4 tofs_color;

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
                in vec4 tofs_color;

                layout(location = 0) out vec4 out_fragColor;

                void main() {
                  out_fragColor = tofs_color * texture(u_image, tofs_texcoord);
                }
            )";
            _shader = std::unique_ptr<Shader>(new Shader(vs, fs));
            _shader->setUniformTextureIndex(0, "u_image");

            _vertices = std::unique_ptr<PersistentBuffer>(new PersistentBuffer());
            _indices = std::unique_ptr<PersistentBuffer>(new PersistentBuffer());

            _vao->enable(0);
            _vao->enable(1);
            _vao->enable(2);
        }
        void bindShaderAndBuffer() {
            _shader->bind();

            _vao->bind();

            glBindBuffer(GL_ARRAY_BUFFER, _vertices->buffer());
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, p));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, uv));
            glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void *)offsetof(Vertex, c));
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
            glm::u8vec4 c;
        };
    private:
        std::unique_ptr<VertexArrayObject> _vao;
        std::unique_ptr<Shader> _shader;
        std::unique_ptr<PersistentBuffer> _vertices;
        std::unique_ptr<PersistentBuffer> _indices;
    };

    class TexturedTriangleDam {
    public:
        using Vertex = TexturedTrianglePipeline::Vertex;
        void clear() {
            _vertices.clear();
            _indices.clear();
            _maxIndex = 0;
        }
        uint32_t addVertex(glm::vec3 p, glm::vec2 uv, glm::u8vec4 c) {
            uint32_t index = (uint32_t)_vertices.size();
            Vertex v = { p, uv, c };
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
        void draw(const ITexture *texture) {
            if (!_white) {
                _white = std::unique_ptr<ITexture>(CreateTexture());
                uint8_t white[4] = { 255, 255, 255, 255 };
                _white->uploadAsRGBA8(white, 1, 1);
            }

            auto vertexBuffer = g_texturedTrianglePipeline->vertices();
            auto vertexOffsetBytes = vertexBuffer->upload(_vertices.data(), (int)(_vertices.size() * sizeof(Vertex)));
            auto vertexOffset = vertexOffsetBytes / sizeof(Vertex);

            auto indexBuffer = g_texturedTrianglePipeline->indices();
            int indexOffsetBytes = 0;

            GLuint indexBufferType = 0;
            if (_indices.empty() == false) {
                auto indices = g_texturedTrianglePipeline->indices();
                if (_maxIndex < std::numeric_limits<uint8_t>::max()) {
                    indexStore(_indices8, _indices);
                    indexOffsetBytes = indexBuffer->upload(_indices8.data(), (int)(_indices8.size() * sizeof(uint8_t)));
                    indexBufferType = GL_UNSIGNED_BYTE;
                }
                else if (_maxIndex < std::numeric_limits<uint16_t>::max()) {
                    indexStore(_indices16, _indices);
                    indexOffsetBytes = indices->upload(_indices16.data(), (int)_indices16.size() * sizeof(uint16_t)) / sizeof(uint16_t);
                    indexBufferType = GL_UNSIGNED_SHORT;
                }
                else {
                    indexOffsetBytes = indices->upload(_indices.data(), (int)_indices.size() * sizeof(uint32_t));
                    indexBufferType = GL_UNSIGNED_INT;
                }
            }

            g_texturedTrianglePipeline->bindShaderAndBuffer();

            if( texture )
            {
                texture->bind();
            }
            else
            {
                _white->bind();
            }

            if (_indices.empty()) {
                glDrawArrays(GL_TRIANGLES, (GLint)vertexOffset, (GLsizei)_vertices.size());
            }
            else {
                glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)_indices.size(), indexBufferType, (uint8_t *)0 + indexOffsetBytes, (GLint)vertexOffset);
            }
        }
    private:
        std::vector<Vertex> _vertices;
        std::vector<uint32_t> _indices;

        // for upload
        std::vector<uint8_t> _indices8;
        std::vector<uint16_t> _indices16;

        uint32_t _maxIndex = 0;

        // White Texture
        std::unique_ptr<ITexture> _white;
    };

    class FontPipeline {
    public:
        FontPipeline() {
            _vao = std::unique_ptr<VertexArrayObject>(new VertexArrayObject());

            const char *vs = R"(
                #version 450

                uniform mat4 u_vp;
                
                layout(location = 0) in vec2 in_position;
                layout(location = 1) in vec2 in_texcoord;

                out vec2 tofs_texcoord;

                void main() {
                    gl_Position = u_vp * vec4(in_position, 0.0, 1.0);
                    tofs_texcoord = in_texcoord;
                }
            )";
            const char *fs = R"(
                #version 450

                // Text Properties
                uniform vec3  u_mainColor;
                uniform vec3  u_outlineColor;
                uniform float u_outlineWidth;
                uniform float u_fontscaled;

                uniform sampler2D u_image;

                in vec2 tofs_texcoord;

                layout(location = 0) out vec4 out_fragColor;

                void main() {
                    float fallunit = 13.0 / 255.0; /* fallof per px */
                    float center = 0.48;          // center hand tuning
                    vec3  mainColor = u_mainColor;
                    vec3  outlineColor = u_outlineColor;
                    float outlineWidth = u_outlineWidth;
        
                    float sdf_raw = texture(u_image, tofs_texcoord).r;
                    float sd = (sdf_raw - center) / fallunit;
                    float sd_fw = 0.7 * fwidth(sd);

                    // when sdf_raw == 0 is the farest distance by cur texture.
                    // So we have to limit the outline offset inside of the farest distance. max_outline is the value.
                    float max_outline = 0.45 / fallunit;
                    float sd_ol = (sdf_raw - center) / fallunit + min(outlineWidth / u_fontscaled, max_outline);

                    // float c = step(0.0, sd);
                    float main    = smoothstep(-sd_fw, sd_fw, sd);
                    float outline = smoothstep(-sd_fw, sd_fw, sd_ol);
                    if(outline <= 0.0) {
                        discard;
                    }
                    vec3 color = mix(outlineColor, mainColor, main);
                    
                    out_fragColor = vec4(color, outline);
                }
            )";
            _shader = std::unique_ptr<Shader>(new Shader(vs, fs));
            _shader->setUniformTextureIndex(0, "u_image");

            _vertices = std::unique_ptr<PersistentBuffer>(new PersistentBuffer());

            _vao->enable(0);
            _vao->enable(1);
        }
        void bindShaderAndBuffer() {
            _shader->bind();

            _vao->bind();

            glBindBuffer(GL_ARRAY_BUFFER, _vertices->buffer());
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, p));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, uv));
        }

        PersistentBuffer *vertices() {
            return _vertices.get();
        }
        void setVP(glm::mat4 viewMatrix, glm::mat4 projMatrix) {
            _shader->setUniformMatrix(projMatrix * viewMatrix, "u_vp");
        }
        void finishFrame() {
            _vertices->finishFrame();
        }
        struct Vertex {
            glm::vec2 p;
            glm::vec2 uv;
        };

        void setFontColor(glm::i8vec3 color) {
            _shader->setUniformVec3(color, "u_mainColor");
        }
        void setOutlineColor(glm::i8vec3 color) {
            _shader->setUniformVec3(color, "u_outlineColor");
        }
        void setOutlineWidth(float outlineWidth) {
            _shader->setUniformFloat(outlineWidth, "u_outlineWidth");
        }
        void setFontscaled(float scale) {
            _shader->setUniformFloat(scale, "u_fontscaled");
        }
    private:
        std::unique_ptr<VertexArrayObject> _vao;
        std::unique_ptr<Shader> _shader;
        std::unique_ptr<PersistentBuffer> _vertices;
    };

    class TextDam {
    public:
        using Vertex = FontPipeline::Vertex;

        TextDam() {
            for (auto cdef : characters_Verdana) {
                _charmap[cdef.codePoint] = cdef;
            }

            Image2DMono8 image;
            image.load(sdf_image, sizeof(sdf_image));

            _texture = std::unique_ptr<ITexture>(CreateTexture());
            _texture->upload(image);
            _texture->setFilter(TextureFilter::Linear);
        }
        void add(glm::vec3 location, std::string text, float fontSize, glm::ivec3 fontColor, float outlineWidth, glm::ivec3 outlineColor) {
            _commands.emplace_back(
                Command {
                    location,
                    text, 
                    fontSize,
                    fontColor,
                    outlineWidth,
                    outlineColor
                }
            );
        }
        void draw() {
            _texture->bind();

            BeginCamera2DCanvas();
            PushGraphicState();
            SetDepthTest(false);
            SetBlendMode(BlendMode::Alpha);

            for (const Command &command : _commands) {
                const float fontscaling = command.fontSize / FONT_SIZE;

                float dstx = command.location.x;
                float dsty = command.location.y;
                _buffer.clear();

                for (char c : command.text) {
                    if (c == ' ') {
                        dstx += SPACE_WIDTH * fontscaling;
                        continue;
                    }

                    auto it = _charmap.find(c);
                    CharacterDef chardef;
                    if (it == _charmap.end()) {
                        chardef = _charmap['?'];
                    }
                    else {
                        chardef = it->second;
                    }

                    /*
                    CharacterDef notes

                          (x, y)
                          +----------------------------+
                          |                            |
                          |                            |
                          |                            |
                          |    +------------------+    |
                          |    |                  |    |
                          |    |                  |    |
                          |    |   +         +    |    |
                          |    |   |         |    |    |
                    height|    |   |         |    |    |
                          |    |   +---------+    |    |
                          |    |   |         |    |    |
                          |    |   |         |    |    |
                          |    |   +         +    |    |
                          |    |                  |    |
                          |    +------------------+    |
                          |    ^-origin x,y(based x,y) |
                          |                            |
                          +----------------------------+
                                      width
                    */
                    float x = dstx - chardef.originX * fontscaling;
                    float y = dsty - chardef.originY * fontscaling;
                    float w = chardef.width * fontscaling;
                    float h = chardef.height * fontscaling;

                    float sux = 1.0f / _texture->width();
                    float suy = 1.0f / _texture->height();
                    float sx = chardef.x * sux;
                    float sy = chardef.y * suy;
                    float sw = chardef.width * sux;
                    float sh = chardef.height * suy;

                    dstx += (chardef.width - chardef.originX * 2) * fontscaling;

                    Vertex ps[] = {
                        { { x, y }, { sx, sy } },
                        { { x + w, y }, { sx + sw, sy } },
                        { { x, y + h }, { sx, sy + sh } },
                        { { x + w, y + h }, { sx + sw, sy + sh } },
                    };

                    _buffer.emplace_back(ps[0]);
                    _buffer.emplace_back(ps[1]);
                    _buffer.emplace_back(ps[2]);

                    _buffer.emplace_back(ps[1]);
                    _buffer.emplace_back(ps[2]);
                    _buffer.emplace_back(ps[3]);
                }

                auto vertices = g_fontPipeline->vertices();
                auto offsetBytes = vertices->upload(_buffer.data(), (int)_buffer.size() * (int)sizeof(Vertex));

                g_fontPipeline->setFontColor(command.fontColor);
                g_fontPipeline->setOutlineColor(command.outlineColor);
                g_fontPipeline->setOutlineWidth(command.outlineWidth);
                g_fontPipeline->setFontscaled(fontscaling);
                g_fontPipeline->bindShaderAndBuffer();

                glDrawArrays(GL_TRIANGLES, offsetBytes / sizeof(Vertex), (GLsizei)_buffer.size());
            }

            PopGraphicState();
            EndCamera();
        }
        void clear() {
            _commands.clear();
        }
    private:
        struct Command {
            glm::vec3 location;
            std::string text;
            float fontSize = 0.0f;
            glm::ivec3 fontColor;
            float outlineWidth = 0.0f;
            glm::ivec3 outlineColor;
        };
        std::vector<Command> _commands;
        std::map<char, CharacterDef> _charmap;
        std::vector<Vertex> _buffer;
        std::unique_ptr<ITexture> _texture;
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

            if (_width * _height == 0) {
                return;
            }

            glNamedRenderbufferStorageMultisample(_color, _samples, GL_RGBA8, _width, _height);
            glNamedRenderbufferStorageMultisample(_depth, _samples, GL_DEPTH_COMPONENT24, _width, _height);

            glNamedFramebufferRenderbuffer(_fb, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _color);
            glNamedFramebufferRenderbuffer(_fb, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depth);

            auto status = glCheckNamedFramebufferStatus(_fb, GL_DRAW_FRAMEBUFFER);
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
        int width() const {
            return _width;
        }
        int height() const {
            return _height;
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
		virtual glm::mat4 getProjectionMatrix( int width, int height ) const = 0;
        virtual glm::mat4 getViewMatrix() const = 0;
    };
    class CameraNone : public ICamera {
    public:
		virtual glm::mat4 getProjectionMatrix( int width, int height ) const override
		{
            return glm::identity<glm::mat4>();
        }
        virtual glm::mat4 getViewMatrix() const override {
            return glm::identity<glm::mat4>();
        }
    };
    class Camera2DCanvas : public ICamera {
    public:
		virtual glm::mat4 getProjectionMatrix( int width, int height ) const override
		{
			return glm::orthoLH<float>( 0.0f, width, height, 0.0f, 1.0f, -1.0f );
        }
        virtual glm::mat4 getViewMatrix() const override {
            return glm::identity<glm::mat4>();
        }
    };

    class Camera3DObject : public ICamera {
    public:
        Camera3DObject(Camera3D camera):_camera(camera) {

        }
		virtual glm::mat4 getProjectionMatrix( int width, int height ) const override
		{
			float zn = std::max( _camera.zNear, FLT_EPSILON );
			float zf = std::max( _camera.zFar, FLT_EPSILON );
			float fovy = std::max( _camera.fovy, FLT_EPSILON );
			float aspect = (float)width / (float)height;

            if( _camera.perspective == 1.0f )
				return glm::perspectiveFov( fovy, (float)width, (float)height, zn, zf );
            
            //glm::mat4 ortho = glm::ortho(
            //    -_camera.orthoy * aspect * 0.5f, _camera.orthoy * aspect * 0.5f,
            //    -_camera.orthoy * 0.5f, _camera.orthoy * 0.5f, zn, zf);

            // https://github.com/Ushio/GenerizedPerspective2
            float tanFovy = tan(fovy * 0.5f);

            float Rn_presp = zn * tanFovy;
            float Rf_presp = zf * tanFovy;

            float zOrtho = glm::distance(_camera.lookat, _camera.origin);
            float R_ortho = zOrtho * tanFovy;
            float Rn = glm::mix(R_ortho, Rn_presp, _camera.perspective);
            float Rf = glm::mix(R_ortho, Rf_presp, _camera.perspective);

            //// avoid singular
            //const float eps_Rf = 0.1f;
            //if (Rf < eps_Rf) {
            //    // Rn + a * z == eps
            //    // z == (eps - Rn) / a
            //    float a = (Rf - Rn) / (zf - zn);
            //    float z = (eps_Rf - Rn) / a;
            //    zf = z;
            //    Rf = Rn + a * z;
            //}

            float b = zf - zn;
            float a = b / aspect;
            float c = -Rn - Rf;
            float d = -Rn * zf - Rf * zn;
            float f =  Rn * zf - Rf * zn;
            float e = Rn - Rf;
            glm::mat4 persp = glm::mat4(
                a, 0, 0, 0,
                0, b, 0, 0,
                0, 0, c, e,
                0, 0, d, f
            );
            return persp;
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
            static CameraNone d;
            return &d;
        }
        return g_cameraStack.top().get();
    }
    static void UpdateCurrentMatrix() {
        auto camera = GetCurrentCamera();
        auto v = camera->getViewMatrix() * g_objectTransform;
        auto p = camera->getProjectionMatrix( GetScreenWidth(), GetScreenHeight() );
        g_primitivePipeline->setVP(v, p);
        g_texturedTrianglePipeline->setVP(v, p);
        g_fontPipeline->setVP(v, p);
    }

    void GetCameraMatrix( Camera3D camera3d, glm::mat4* proj, glm::mat4* view, int width, int height )
	{
        Camera3DObject camera(camera3d);
		*proj = camera.getProjectionMatrix( width, height );
        *view = camera.getViewMatrix();
    }

    /*
     Graphics Core
    */
    void ClearBackground(float r, float g, float b, float a) {
        g_frameBuffer->clear(r, g, b, a, true);
    }
    void ClearBackground(ITexture *texture) {
        ClearBackground(0, 0, 0, 0);

        BeginCameraNone();
        PushGraphicState();
        SetDepthTest(false);

        TriBegin(texture);

        uint32_t vs[] = {
            TriVertex({ -1, +1, 1 }, { 0, 0 }, { 255, 255, 255, 255 }),
            TriVertex({ +1, +1, 1 }, { 1, 0 }, { 255, 255, 255, 255 }),
            TriVertex({ -1, -1, 1 }, { 0, 1 }, { 255, 255, 255, 255 }),
            TriVertex({ +1, -1, 1 }, { 1, 1 }, { 255, 255, 255, 255 }),
        };

        TriIndex(vs[0]);
        TriIndex(vs[1]);
        TriIndex(vs[2]);
        TriIndex(vs[1]);
        TriIndex(vs[2]);
        TriIndex(vs[3]);
        TriEnd();

        PopGraphicState();
        EndCamera();
    }
    void CaptureScreen(Image2DRGBA8* toImage, bool noAlpha) {
        toImage->allocate(GetScreenWidth(), GetScreenHeight());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadPixels(
            0, 0, GetScreenWidth(), GetScreenHeight(),
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            toImage->data()
        );

        if (noAlpha) {
            int n = toImage->width() * toImage->height();
            for (int i = 0; i < n; ++i) {
                toImage->data()[i].w = 255;
            }
        }

        // flip vertically
        int h = toImage->height();
        int n = h / 2;
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < toImage->width(); ++x) {
                std::swap((*toImage)(x, y), (*toImage)(x, h - y - 1));
            }
        }
    }
    void PushGraphicState() {
        PR_ASSERT(g_states.empty() == false);
        g_states.push(g_states.top());
    }
    void PopGraphicState() {
        g_states.pop();
        PR_ASSERT(g_states.empty() == false);
        g_states.top().apply();
    }
    void SetDepthTest(bool enabled) {
        PR_ASSERT(g_states.empty() == false);
        g_states.top().depthTest = enabled;
        g_states.top().apply();
    }
    void SetBlendMode(BlendMode blendMode) {
        PR_ASSERT(g_states.empty() == false);
        g_states.top().blendMode = blendMode;
        g_states.top().apply();
    }
    void SetScissor(bool enabled) {
        PR_ASSERT(g_states.empty() == false);
        g_states.top().scissorTest = enabled;
        g_states.top().apply();
    }
    void SetScissorRect(int x, int y, int width, int height) {
        PR_ASSERT(g_states.empty() == false);
        GraphicState &s = g_states.top();
        s.scissorX = x;
        s.scissorY = y;
        s.scissorWidth  = width;
        s.scissorHeight = height;
        g_states.top().apply();
    }
    void SetObjectTransform(glm::mat4 transform)
    {
        g_objectTransform = transform;
        UpdateCurrentMatrix();
    }
    glm::mat4 GetObjectTransform()
    {
        return g_objectTransform;
    }
    void SetObjectIdentify()
    {
        g_objectTransform = glm::identity<glm::mat4>();
        UpdateCurrentMatrix();
    }
    void BeginCamera(Camera3D camera) {
        PR_ASSERT(g_cameraStack.size() <= MAX_CAMERA_STACK_SIZE);

        g_cameraStack.push(std::unique_ptr<const ICamera>(new Camera3DObject(camera)));
        UpdateCurrentMatrix();
    }
    void BeginCamera2DCanvas() {
        PR_ASSERT(g_cameraStack.size() <= MAX_CAMERA_STACK_SIZE);

        g_cameraStack.push(std::unique_ptr<const ICamera>(new Camera2DCanvas()));
        UpdateCurrentMatrix();
    }
    void BeginCameraNone() {
        PR_ASSERT(g_cameraStack.size() <= MAX_CAMERA_STACK_SIZE);

        g_cameraStack.push(std::unique_ptr<const ICamera>(new CameraNone()));
        UpdateCurrentMatrix();
    }
    void EndCamera() {
        PR_ASSERT(g_cameraStack.empty() == false);

        g_cameraStack.pop();
        UpdateCurrentMatrix();
    }
    glm::mat4 GetCurrentProjMatrix() {
		return GetCurrentCamera()->getProjectionMatrix( GetScreenWidth(), GetScreenHeight() );
    }
    glm::mat4 GetCurrentViewMatrix() {
        return GetCurrentCamera()->getViewMatrix();
    }
    namespace {
        bool g_primitiveEnabled = false;
        PrimitiveDam g_primitiveDam;
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
        return g_primitiveDam.addVertex(p, c);
    }
    void PrimIndex(uint32_t index) {
        PR_ASSERT(g_primitiveEnabled);
        return g_primitiveDam.addIndex(index);
    }
    void PrimEnd() {
        PR_ASSERT(g_primitiveEnabled);
        g_primitiveEnabled = false;
        g_primitiveDam.draw(g_primitiveMode, g_primitiveWidth);
        g_primitiveDam.clear();
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
    void DrawCircle(glm::vec3 o, glm::vec3 dir, glm::u8vec3 c, float radius, int vertexCount, float lineWidth) {
        if (vertexCount <= 0) {
            return;
        }

        glm::vec3 x;
        glm::vec3 y;
        GetOrthonormalBasis(dir, &x, &y);

        PrimBegin(PrimitiveMode::LineStrip, lineWidth);
        CircleGenerator circle(glm::pi<float>() * 2.0f / (vertexCount - 1));
        for (int i = 0; i < vertexCount; ++i) {
            glm::vec3 p = x * circle.cos() + y * circle.sin();
            PrimVertex(o + radius * p, c);
            circle.step();
        }
        PrimEnd();
    }
    void DrawEllipse(glm::vec3 o, glm::vec3 axis0, glm::vec3 axis1, glm::u8vec3 c, int vertexCount, float lineWidth )
    {
		if( vertexCount <= 0 )
		{
			return;
		}

		PrimBegin( PrimitiveMode::LineStrip, lineWidth );
		CircleGenerator circular( glm::pi<float>() * 2.0f / vertexCount );
		for( int i = 0; i <= vertexCount; i++ )
		{
			PrimVertex( o + axis0 * circular.sin() + axis1 * circular.cos(), c );
			circular.step();
		}
		PrimEnd();
    }
    void DrawCube(glm::vec3 o, glm::vec3 size, glm::u8vec3 c, float lineWidth) {
        glm::vec3 h = size * 0.5f;
        PrimBegin(PrimitiveMode::Lines, lineWidth);
        uint32_t ps[8] = {
            PrimVertex(o + glm::vec3(-h.x, +h.y, -h.z), c),
            PrimVertex(o + glm::vec3(+h.x, +h.y, -h.z), c),
            PrimVertex(o + glm::vec3(+h.x, +h.y, +h.z), c),
            PrimVertex(o + glm::vec3(-h.x, +h.y, +h.z), c),

            PrimVertex(o + glm::vec3(-h.x, -h.y, -h.z), c),
            PrimVertex(o + glm::vec3(+h.x, -h.y, -h.z), c),
            PrimVertex(o + glm::vec3(+h.x, -h.y, +h.z), c),
            PrimVertex(o + glm::vec3(-h.x, -h.y, +h.z), c),
        };
        for (int i = 0; i < 4; ++i) {
            PrimIndex(ps[i]);
            PrimIndex(ps[(i + 1) % 4]);

            PrimIndex(ps[4 + i]);
            PrimIndex(ps[4 + (i + 1) % 4]);

            PrimIndex(ps[i]);
            PrimIndex(ps[4 + i]);
        }
        PrimEnd();
    }
    void DrawAABB(glm::vec3 a, glm::vec3 b, glm::u8vec3 c, float lineWidth) {
        DrawCube((a + b) * 0.5f, glm::abs(a - b), c, lineWidth);
    }
    void DrawGrid(GridAxis axis, float step, int blockCount, glm::u8vec3 c, float lineWidth) {
        if (blockCount <= 0) {
            return;
        }

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
	void DrawFreeGrid( glm::vec3 o, glm::vec3 dir0, glm::vec3 dir1, int blockCountHalf, glm::u8vec3 c, float lineWidth )
	{
        PrimBegin( PrimitiveMode::Lines, lineWidth );

		for( int i = -blockCountHalf; i <= blockCountHalf; ++i )
		{
			PrimVertex( o + dir0 * (float)i - (float)blockCountHalf * dir1, c );
			PrimVertex( o + dir0 * (float)i + (float)blockCountHalf * dir1, c );
		}
		for( int i = -blockCountHalf; i <= blockCountHalf; ++i )
		{
			PrimVertex( o + dir1 * (float)i - (float)blockCountHalf * dir0, c );
			PrimVertex( o + dir1 * (float)i + (float)blockCountHalf * dir0, c );
		}

		PrimEnd();
	}
    void DrawTube(glm::vec3 p0, glm::vec3 p1, float radius0, float radius1, glm::u8vec3 c, int vertexCount, float lineWidth) {
        if (vertexCount <= 0) {
            return;
        }

        if (radius0 == 0.0f && radius1 == 0.0f) {
            DrawLine(p0, p1, c, lineWidth);
        }

        glm::vec3 T = p1 - p0;
        glm::vec3 z = glm::normalize(T);

        glm::vec3 x;
        glm::vec3 y;
        GetOrthonormalBasis(z, &x, &y);

        PrimBegin(PrimitiveMode::Lines, lineWidth);

        std::vector<uint32_t> circleVerticesB(vertexCount);
        std::vector<uint32_t> circleVerticesT(vertexCount);
        CircleGenerator circle(glm::pi<float>() * 2.0f / vertexCount);
        for (int i = 0; i < vertexCount; ++i) {
            glm::vec3 ring = x * circle.sin() + y * circle.cos();
            circle.step();
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
        if (vertexCount <= 0) {
            return;
        }

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
    void DrawSphere(glm::vec3 origin, float radius, glm::u8vec3 c, int rowCount, int colCount, glm::vec3 orientation, float lineWidth) {
        if (rowCount < 0 || colCount < 0) {
            return;
        }
        glm::vec3 u, v;
        GetOrthonormalBasis(orientation, &u, &v);
        glm::mat3 m = glm::mat3(
            u,
            orientation,
            v
        );
        
        CircleGenerator circle(glm::pi<float>() * 2.0f / colCount);

        std::vector<glm::vec3> cp(colCount);
        for (int i = 0; i < colCount; ++i) {
            cp[i] = glm::vec3(circle.sin(), 0.0f, circle.cos());
            circle.step();
        }

        LinearTransform r2rad(0.0f, (float)(rowCount - 1), 0.0f, glm::pi<float>());
        std::vector<int> ps(rowCount * colCount);

        PrimBegin(PrimitiveMode::Lines);

        for (int row = 0; row < rowCount; ++row) {
            float y = std::cos(r2rad((float)row));
            float xz = std::sqrt(std::max(1.0f - y * y, 0.0f));

            for (int col = 0; col < colCount; ++col) {
                glm::vec3 p = { xz * cp[col].x, y, xz * cp[col].z };
                ps[row * colCount + col] = PrimVertex(origin + m * (radius * p), c);
            }
        }

        for (int row = 0; row < rowCount - 1; ++row) {
            for (int col = 0; col < colCount; ++col) {
                // V line
                PrimIndex(ps[row * colCount + col]);
                PrimIndex(ps[(row + 1) * colCount + col]);

                // H line
                if (0 < row && row < rowCount - 1) {
                    PrimIndex(ps[row * colCount + col]);
                    PrimIndex(ps[row * colCount + (col + 1) % colCount]);
                }
            }
        }

        PrimEnd();
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
        g_fontPipeline = new FontPipeline();

        g_textDam = new TextDam();

        g_frameBuffer = new MSFrameBufferObject();
        g_frameBuffer->resize(g_config.ScreenWidth, g_config.ScreenHeight, g_config.NumSamples);
        g_frameBuffer->bind();

        glDisable(GL_BLEND);

        UpdateCurrentMatrix();

        g_states.push(GraphicState());
        g_states.top().apply();
    }
    static void CleanUpGraphics() {
        delete g_primitivePipeline;
        g_primitivePipeline = nullptr;

        delete g_texturedTrianglePipeline;
        g_texturedTrianglePipeline = nullptr;

        delete g_fontPipeline;
        g_fontPipeline = nullptr;

        delete g_textDam;
        g_textDam = nullptr;

        delete g_frameBuffer;
        g_frameBuffer = nullptr;
    }

    // ImGui Data
    namespace {
        ITexture *g_fontTexture = nullptr;
    }
    static void SetupImGui() {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = 1;
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;

        ImFontConfig fontCfg;
        fontCfg.FontData = (void *)verdana;
        fontCfg.FontDataSize = sizeof(verdana);
        fontCfg.FontDataOwnedByAtlas = false;
        fontCfg.SizePixels = (float)g_config.imguiFontSize;
        io.Fonts->AddFont(&fontCfg);

        io.BackendRendererName = "prlib";

        io.KeyMap[ImGuiKey_Tab] = KEY_TAB;
        io.KeyMap[ImGuiKey_LeftArrow] = KEY_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = KEY_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow] = KEY_UP;
        io.KeyMap[ImGuiKey_DownArrow] = KEY_DOWN;
        io.KeyMap[ImGuiKey_PageUp] = KEY_PAGE_UP;
        io.KeyMap[ImGuiKey_PageDown] = KEY_PAGE_DOWN;
        io.KeyMap[ImGuiKey_Home] = KEY_HOME;
        io.KeyMap[ImGuiKey_End] = KEY_END;
        io.KeyMap[ImGuiKey_Insert] = KEY_INSERT;
        io.KeyMap[ImGuiKey_Delete] = KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Space] = KEY_SPACE;
        io.KeyMap[ImGuiKey_Enter] = KEY_ENTER;
        io.KeyMap[ImGuiKey_Escape] = KEY_ESCAPE;
        io.KeyMap[ImGuiKey_KeyPadEnter] = KEY_KP_ENTER;
        io.KeyMap[ImGuiKey_A] = KEY_A;
        io.KeyMap[ImGuiKey_C] = KEY_C;
        io.KeyMap[ImGuiKey_V] = KEY_V;
        io.KeyMap[ImGuiKey_X] = KEY_X;
        io.KeyMap[ImGuiKey_Y] = KEY_Y;
        io.KeyMap[ImGuiKey_Z] = KEY_Z;

        
        io.SetClipboardTextFn = [](void* user_data, const char* text) { glfwSetClipboardString(g_window, text); };
        io.GetClipboardTextFn = [](void* user_data) { return glfwGetClipboardString(g_window); };

        static GLFWcharfun charcallback_previous = nullptr;
        charcallback_previous = glfwSetCharCallback(g_window, [](GLFWwindow* window, unsigned int c) {
            if (charcallback_previous)
            {
                charcallback_previous(window, c);
            }

            ImGuiIO& io = ImGui::GetIO();
            io.AddInputCharacter(c);
        });

        ImGui::StyleColorsDark();

        // Build texture atlas
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

        g_fontTexture = CreateTexture();
        g_fontTexture->uploadAsRGBA8(pixels, width, height);
        io.Fonts->TexID = (ImTextureID)g_fontTexture;
    }
    static void CleanUpImGui() {
        delete g_fontTexture;
        g_fontTexture = nullptr;
        ImGui::DestroyContext();
    }
    void BeginImGui() {
        FlashTextDrawing();

        ImGuiIO& io = ImGui::GetIO();

        // Setup display size (every frame to accommodate for window resizing)
        io.DisplaySize = ImVec2((float)GetScreenWidth(), (float)GetScreenHeight());
        io.DisplayFramebufferScale = ImVec2(1, 1);

        // Setup time step
        io.DeltaTime = (float)GetFrameDeltaTime();
        io.MousePos = ImVec2(GetMousePosition().x, GetMousePosition().y);

        io.MouseDown[0] = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        io.MouseDown[1] = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        io.MouseDown[2] = IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);

        io.MouseWheelH += GetMouseScrollDelta().x;
        io.MouseWheel  += GetMouseScrollDelta().y;

        static const int imkeys[] = {
            KEY_TAB,
            KEY_LEFT,
            KEY_RIGHT,
            KEY_UP,
            KEY_DOWN,
            KEY_PAGE_UP,
            KEY_PAGE_DOWN,
            KEY_HOME,
            KEY_END,
            KEY_INSERT,
            KEY_DELETE,
            KEY_BACKSPACE,
            KEY_SPACE,
            KEY_ENTER,
            KEY_ESCAPE,
            KEY_KP_ENTER,
            KEY_A,
            KEY_C,
            KEY_V,
            KEY_X,
            KEY_Y,
            KEY_Z,
            KEY_LEFT_CONTROL,
            KEY_RIGHT_CONTROL,
            KEY_LEFT_SHIFT,
            KEY_RIGHT_SHIFT,
            KEY_LEFT_ALT,
            KEY_RIGHT_ALT,
            KEY_LEFT_SUPER,
            KEY_RIGHT_SUPER,
        };
        for (int key : imkeys) {
            if (IsKeyPressed(key)) {
                io.KeysDown[key] = true;
            }
            if (IsKeyUp(key)) {
                io.KeysDown[key] = false;
            }
        }

        io.KeyCtrl = IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL);
        io.KeyShift = IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT);
        io.KeyAlt = IsKeyPressed(KEY_LEFT_ALT) || IsKeyPressed(KEY_RIGHT_ALT);
        io.KeySuper = IsKeyPressed(KEY_LEFT_SUPER) || IsKeyPressed(KEY_RIGHT_SUPER);

        // Mouse
        if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == false) {
            ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
            if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
            {
                // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
                glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            }
            else
            {
                // Show OS mouse cursor
                // FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with GLFW 3.2, but 3.3 works here.
                glfwSetCursor(g_window, g_mouseCursors[imgui_cursor] ? g_mouseCursors[imgui_cursor] : g_mouseCursors[ImGuiMouseCursor_Arrow]);
                glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }

        ImGui::NewFrame();
    }

    static glm::u8vec4 color_cvt(ImU32 in) {
        return {
            ((in >> IM_COL32_R_SHIFT) & 0xFF),
            ((in >> IM_COL32_G_SHIFT) & 0xFF),
            ((in >> IM_COL32_B_SHIFT) & 0xFF),
            ((in >> IM_COL32_A_SHIFT) & 0xFF)
        };
    }
    void EndImGui() {
        ImGui::Render();

        ImDrawData *draw_data = ImGui::GetDrawData();
        int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
        int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
        if (fb_width == 0 || fb_height == 0) {
            return;
        }

        // Will project scissor/clipping rectangles into framebuffer space
        ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
        ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

        BeginCamera2DCanvas();
        PushGraphicState();
        SetBlendMode(BlendMode::Alpha);
        SetDepthTest(false);
        SetScissor(true);
        SetScissorRect(0, 0, fb_width, fb_height);

        // Render command lists
        for (int n = 0; n < draw_data->CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
            const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;

            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
            {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

                // Not Supported
                PR_ASSERT(pcmd->UserCallback == 0);
                PR_ASSERT(pcmd->VtxOffset == 0); 

                // Project scissor/clipping rectangles into framebuffer space
                ImVec4 clip_rect;
                clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

                if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
                {
                    // Apply scissor/clipping rectangle
                    SetScissorRect(
                        (int)clip_rect.x, (int)(fb_height - clip_rect.w), 
                        (int)(clip_rect.z - clip_rect.x), (int)(clip_rect.w - clip_rect.y)
                    );

                    TriBegin((ITexture *)pcmd->TextureId);
                    for (int i = 0; i < (int)pcmd->ElemCount; i++) {
                        ImDrawVert v = vtx_buffer[idx_buffer[i]];
                        glm::vec3 p(v.pos.x, v.pos.y, 0.0f);
                        glm::vec2 uv(v.uv.x, v.uv.y);
                        glm::u8vec4 c = color_cvt(v.col);
                        TriVertex(p, uv, c);
                    }
                    TriEnd();
                }
                
                idx_buffer += pcmd->ElemCount;
            }
        }

        PopGraphicState();
        EndCamera();
    }
    bool IsImGuiUsingMouse() {
        ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse;
    }
    void ImGuiSliderDirection(const char *label, glm::vec3 *dir, float minTheta, float maxTheta) {
		ImGui::SetNextItemOpen( true, ImGuiCond_Once );
        if (ImGui::TreeNode(label)) {
            char thetaLabel[128];
            sprintf(thetaLabel, "theta##%s", label);
            char phiLabel[128];
            sprintf(phiLabel, "phi##%s", label);

            float theta, phi;
            GetSpherical(*dir, &theta, &phi);
            bool editted = false;
            if (ImGui::SliderFloat(thetaLabel, &theta, minTheta, maxTheta)) {
                editted = true;
            }
            if (ImGui::SliderFloat(phiLabel, &phi, -glm::pi<float>(), glm::pi<float>()))
            {
                editted = true;
            }
            if (editted) {
                *dir = GetCartesian(theta, phi);
            }
            ImGui::TreePop();
        }
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

    void SetUTF8CodePage()
    {
        std::locale::global(std::locale("en_US.UTF-8"));
    }
    void Initialize(Config config) {
        g_config = config;

        glfwInit();

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

        // Check Extensions
        // PR_ASSERT(GLEW_ARB_bindless_texture);

        bool has_GL_EXT_memory_object = false;
		bool has_GL_EXT_memory_object_win32 = false;
        GLint n = 0;
		glGetIntegerv( GL_NUM_EXTENSIONS, &n );
		for( GLint i = 0; i < n; i++ )
		{
			const char* extension = (const char*)glGetStringi( GL_EXTENSIONS, i );
			if( strstr( extension, "GL_EXT_memory_object" ) )
			{
				has_GL_EXT_memory_object = true;
            }
			if( strstr( extension, "GL_EXT_memory_object_win32" ) )
			{
				has_GL_EXT_memory_object_win32 = true;
			}
		}
		g_hasExternalObjectsExt = has_GL_EXT_memory_object && has_GL_EXT_memory_object_win32;

        glfwSwapInterval(config.SwapInterval);

        glfwSetMouseButtonCallback(g_window, MouseButtonCallback);
        glfwSetCursorPosCallback(g_window, CursorPositionCallback);
        glfwSetKeyCallback(g_window, KeyCallback);
        glfwSetScrollCallback(g_window, ScrollCallback);
        glfwSetDropCallback(g_window, [](GLFWwindow* window, int count, const char** paths) {
            std::vector<std::string> files;
            for(int i = 0; i < count; i++)
            {
                files.emplace_back(paths[i]);
            }

            if(g_onFileDrop)
            {
                g_onFileDrop(files);
            }
        });

        double cx, cy;
        glfwGetCursorPos(g_window, &cx, &cy);
        g_mousePosition.x = (float)cx;
        g_mousePosition.y = (float)cy;

        // cursor
        g_mouseCursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        g_mouseCursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
        g_mouseCursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);   // FIXME: GLFW doesn't have this.
        g_mouseCursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
        g_mouseCursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
        g_mouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);  // FIXME: GLFW doesn't have this.
        g_mouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);  // FIXME: GLFW doesn't have this.
        g_mouseCursors[ImGuiMouseCursor_Hand] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);

        SetupGraphics();
        SetupImGui();
    }

    void CleanUp() {
        CleanUpImGui();
        CleanUpGraphics();

        for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
        {
            glfwDestroyCursor(g_mouseCursors[cursor_n]);
            g_mouseCursors[cursor_n] = NULL;
        }
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
    void SetScreenSize(int width, int height)
    {
		glfwSetWindowSize( g_window, width, height );
    }

    double GetElapsedTime() {
        // work around non initializeed
        if( g_window == nullptr )
        {
            static std::chrono::high_resolution_clock::time_point s_start = std::chrono::high_resolution_clock::now();
            auto now = std::chrono::high_resolution_clock::now();
            return (double)std::chrono::duration_cast<std::chrono::microseconds>(now - s_start).count() * 0.001 * 0.001;
        }
        return glfwGetTime();
    }
    double GetFrameTime() {
        return g_frameTime;
    }
    double GetFrameDeltaTime() {
        return g_frameDeltaTime;
    }
    double GetFrameRate() {
        return g_framePerSeconds;
    }

    bool hasExternalObjectExtension() 
    {
		return g_hasExternalObjectsExt;
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

    void FlashTextDrawing() {
        g_textDam->draw();
        g_textDam->clear();
    }

    bool NextFrame() {
        FlashTextDrawing();

        g_frameBuffer->copyToScreen();
        glfwSwapBuffers(g_window);

        g_primitivePipeline->finishFrame();
        g_texturedTrianglePipeline->finishFrame();
        g_fontPipeline->finishFrame();

        glfwPollEvents();

        HandleInputEvents();

        int width, height;
        glfwGetFramebufferSize(g_window, &width, &height);
        g_frameBuffer->resize(width, height, g_config.NumSamples);

        glViewport(0, 0, width, height);

        // update frame time
        double previousFrameTime = g_frameTime;
        g_frameTime = GetElapsedTime();
        g_frameDeltaTime = g_frameTime - previousFrameTime;

        // calculate framerate
        static std::vector<double> frameDeltas;
        frameDeltas.push_back(g_frameDeltaTime);
        if (30 < frameDeltas.size()) {
            double avg_delta = std::accumulate(frameDeltas.begin(), frameDeltas.end(), 0.0, std::plus<double>()) / (double)frameDeltas.size();
            g_framePerSeconds = 1.0 / avg_delta;
            frameDeltas.clear();
        }

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

    const int KEY_TAB = GLFW_KEY_TAB;
    const int KEY_LEFT = GLFW_KEY_LEFT;
    const int KEY_RIGHT = GLFW_KEY_RIGHT;
    const int KEY_UP = GLFW_KEY_UP;
    const int KEY_DOWN = GLFW_KEY_DOWN;
    const int KEY_PAGE_UP = GLFW_KEY_PAGE_UP;
    const int KEY_PAGE_DOWN = GLFW_KEY_PAGE_DOWN;
    const int KEY_HOME = GLFW_KEY_HOME;
    const int KEY_END = GLFW_KEY_END;
    const int KEY_INSERT = GLFW_KEY_INSERT;
    const int KEY_DELETE = GLFW_KEY_DELETE;
    const int KEY_BACKSPACE = GLFW_KEY_BACKSPACE;
    const int KEY_SPACE = GLFW_KEY_SPACE;
    const int KEY_ENTER = GLFW_KEY_ENTER;
    const int KEY_ESCAPE = GLFW_KEY_ESCAPE;
    const int KEY_KP_ENTER = GLFW_KEY_KP_ENTER;
    const int KEY_0 = GLFW_KEY_0;
    const int KEY_1 = GLFW_KEY_1;
    const int KEY_2 = GLFW_KEY_2;
    const int KEY_3 = GLFW_KEY_3;
    const int KEY_4 = GLFW_KEY_4;
    const int KEY_5 = GLFW_KEY_5;
    const int KEY_6 = GLFW_KEY_6;
    const int KEY_7 = GLFW_KEY_7;
    const int KEY_8 = GLFW_KEY_8;
    const int KEY_9 = GLFW_KEY_9;
    const int KEY_SEMICOLON = GLFW_KEY_SEMICOLON;  /* ; */
    const int KEY_EQUAL = GLFW_KEY_EQUAL;  /* = */
    const int KEY_A = GLFW_KEY_A;
    const int KEY_B = GLFW_KEY_B;
    const int KEY_C = GLFW_KEY_C;
    const int KEY_D = GLFW_KEY_D;
    const int KEY_E = GLFW_KEY_E;
    const int KEY_F = GLFW_KEY_F;
    const int KEY_G = GLFW_KEY_G;
    const int KEY_H = GLFW_KEY_H;
    const int KEY_I = GLFW_KEY_I;
    const int KEY_J = GLFW_KEY_J;
    const int KEY_K = GLFW_KEY_K;
    const int KEY_L = GLFW_KEY_L;
    const int KEY_M = GLFW_KEY_M;
    const int KEY_N = GLFW_KEY_N;
    const int KEY_O = GLFW_KEY_O;
    const int KEY_P = GLFW_KEY_P;
    const int KEY_Q = GLFW_KEY_Q;
    const int KEY_R = GLFW_KEY_R;
    const int KEY_S = GLFW_KEY_S;
    const int KEY_T = GLFW_KEY_T;
    const int KEY_U = GLFW_KEY_U;
    const int KEY_V = GLFW_KEY_V;
    const int KEY_W = GLFW_KEY_W;
    const int KEY_X = GLFW_KEY_X;
    const int KEY_Y = GLFW_KEY_Y;
    const int KEY_Z = GLFW_KEY_Z;

    const int KEY_LEFT_CONTROL = GLFW_KEY_LEFT_CONTROL;
    const int KEY_RIGHT_CONTROL = GLFW_KEY_RIGHT_CONTROL;
    const int KEY_LEFT_SHIFT = GLFW_KEY_LEFT_SHIFT;
    const int KEY_RIGHT_SHIFT = GLFW_KEY_RIGHT_SHIFT;
    const int KEY_LEFT_ALT = GLFW_KEY_LEFT_ALT;
    const int KEY_RIGHT_ALT = GLFW_KEY_RIGHT_ALT;
    const int KEY_LEFT_SUPER = GLFW_KEY_LEFT_SUPER;
    const int KEY_RIGHT_SUPER = GLFW_KEY_RIGHT_SUPER;

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
    void SetFileDropCallback(std::function<void(std::vector<std::string>)> onFileDrop)
    {
        g_onFileDrop = onFileDrop;
    }

    // Interactions
	bool UpdateCameraBlenderLike(
        Camera3D *camera,
        float wheel_sensitivity,
        float zoom_mouse_sensitivity,
        float rotate_sensitivity,
        float shift_sensitivity
    ) {
        const float camera_near_limit = 0.000001f;
		bool isChange = false;

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
            d = std::max(camera_near_limit, d);

            auto dir = glm::normalize(origin - lookat);
            origin = lookat + dir * d;
            camera->origin = origin;

			isChange = true;
        } else if (right_pressed) {
            if (mousePositionDelta.y != 0.0f) {
                auto lookat = camera->lookat;
                auto origin = camera->origin;

                float d = glm::distance(lookat, origin);
                d -= d * mousePositionDelta.y * zoom_mouse_sensitivity;
                d = std::max(camera_near_limit, d);

                auto dir = glm::normalize(origin - lookat);
                origin = lookat + dir * d;
                camera->origin = origin;

				isChange = true;
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

				isChange = true;
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

				isChange = true;
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

			if (mousePositionDelta.x != 0.0 || mousePositionDelta.y != 0.0f) {
				isChange = true;
			}
        }

		return isChange;
    }

    Camera3D cameraFromEntity( const pr::FCameraEntity* e )
	{
		glm::mat4 m = e->localToWorld();
		glm::mat3 IT = glm::inverseTranspose( m );

		Camera3D c;
		c.origin = m * glm::vec4( 0.0f, 0.0f, 0.0f, 1.0f );
		c.lookat = m * glm::vec4( 0.0f, 0.0f, -1.0f, 1.0f );
		c.up = IT * glm::vec3( 0.0f, 1.0f, 0.0f );
		c.fovy = e->fovV();
		c.zNear = e->nearClip();
		c.zFar = e->farClip();
		return c;
	}

    class Texture : public ITexture {
    public:
        Texture() {
            
        }
        ~Texture() {
            if (_texture) {
                glDeleteTextures(1, &_texture);
                _texture = 0;
            }
			if( _interopMem )
			{
				glDeleteMemoryObjectsEXT( 1, &_interopMem );
				_interopMem = 0;
            }
        }
        void upload(const Image2DRGBA8 &image) override {
            uploadAsRGBA8((const uint8_t *)image.data(), image.width(), image.height());
        }
        void upload(const Image2DMono8 &image) override {
            uploadAsMono8((const uint8_t *)image.data(), image.width(), image.height());
        }
		virtual void upload(const Image2DRGBA32 &image) override {
			uploadAsRGBAF32(glm::value_ptr(*image.data()), image.width(), image.height());
		}
        void uploadAsRGBA8(const uint8_t *source, int width, int height) override {
			applyToStorage( width, height, GL_RGBA8, _filter, _isInterlop );

            glTextureSubImage2D(_texture, 0, 0, 0, _width, _height, GL_RGBA, GL_UNSIGNED_BYTE, source);
        }
        void uploadAsMono8(const uint8_t *source, int width, int height) override {
			applyToStorage( width, height, GL_RGB8, _filter, _isInterlop );

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTextureSubImage2D(_texture, 0, 0, 0, _width, _height, GL_LUMINANCE, GL_UNSIGNED_BYTE, source);
        }
		void uploadAsRGBAF32(const float *source, int width, int height) override
		{
			applyToStorage( width, height, GL_RGBA32F, _filter, _isInterlop );

			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTextureSubImage2D(_texture, 0, 0, 0, _width, _height, GL_RGBA, GL_FLOAT, source);
		}
		void fromInteropRGBA8( void* handle, int width, int height ) override
		{
			if( hasExternalObjectExtension() )
			{
				applyToStorage( width, height, GL_RGBA8, _filter, true );

                if( _interopMem == 0 )
				{
					glCreateMemoryObjectsEXT( 1, &_interopMem );
                }

                glImportMemoryWin32HandleEXT( _interopMem, 4 * _width * _height, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, handle );
				glTextureStorageMem2DEXT( _texture, 1, GL_RGBA8, _width, _height, _interopMem, 0 );
            }
        }
        int width() const override {
            return _width;
        }
        int height() const override {
            return _height;
        }
        void setFilter(TextureFilter filter) override {
			applyToStorage( _width, _height, _internalFormat, filter, _isInterlop );
        }
        void bind() const override {
            glBindTextureUnit(0, _texture);
        }
    private:
        void applyToStorage(int w, int h, GLenum internalFormat, TextureFilter filter, bool interlop ) {
            bool reallocate = false;
            reallocate = reallocate || _width != w;
            reallocate = reallocate || _height != h;
            reallocate = reallocate || _internalFormat != internalFormat;
			reallocate = reallocate || _isInterlop != interlop;
            if (reallocate) {
                if (_texture) {
                    glDeleteTextures(1, &_texture);
                }

                _width  = w;
                _height = h;
                _internalFormat = internalFormat;
				_isInterlop = interlop;

                glCreateTextures(GL_TEXTURE_2D, 1, &_texture);
				if( interlop == false )
				{
                    glTextureStorage2D( _texture, 1, _internalFormat, _width, _height );
                }
            }

            if (_filter != filter || reallocate) {
                _filter = filter;

                switch (_filter) {
                case TextureFilter::None:
                    glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    break;
                case TextureFilter::Linear:
                    glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    break;
                default:
                    PR_ASSERT(0);
                    break;
                }
            }
        }
        GLuint _texture = 0;
        GLenum _internalFormat = 0;
        int _width = 0;
        int _height = 0;
        TextureFilter _filter = TextureFilter::None;
		bool _isInterlop = false;
		GLuint _interopMem = 0;
    };
   
    ITexture *CreateTexture() {
        return new Texture();
    }

    namespace {
        bool g_triangleEnabled = false;
        TexturedTriangleDam g_texturedTriangleDam;
        const ITexture *g_texture = nullptr;
    }
    void TriBegin(const ITexture *texture) {
        PR_ASSERT(g_triangleEnabled == false);
        g_triangleEnabled = true;
        g_texture = texture;
    }
    uint32_t TriVertex(glm::vec3 p, glm::vec2 uv, glm::u8vec4 c) {
        PR_ASSERT(g_triangleEnabled);
        return g_texturedTriangleDam.addVertex(p, uv, c);
    }
    void TriIndex(uint32_t index) {
        PR_ASSERT(g_triangleEnabled);
        g_texturedTriangleDam.addIndex(index);
    }
    void TriEnd() {
        PR_ASSERT(g_triangleEnabled);
        g_triangleEnabled = false;

        g_texturedTriangleDam.draw(g_texture);
        g_texturedTriangleDam.clear();
        g_texture = nullptr;
    }
    void DrawTextScreen(float screen_x, float screen_y, std::string text, float fontSize, glm::u8vec3 fontColor, float outlineWidth, glm::u8vec3 outlineColor) {
        g_textDam->add(glm::vec3(screen_x, screen_y, 0.0f), text, fontSize, fontColor, outlineWidth, outlineColor);
    }
    void DrawText(glm::vec3 p_world, std::string text, float fontSize, glm::u8vec3 fontColor, float outlineWidth, glm::u8vec3 outlineColor) {
        auto camera = GetCurrentCamera();
		auto screen = glm::project( p_world, camera->getViewMatrix() * g_objectTransform, camera->getProjectionMatrix( GetScreenWidth(), GetScreenHeight() ), glm::vec4( 0, 0, GetScreenWidth(), GetScreenHeight() ) );
        
        // back cliping
        if (screen.z > 1.0f) {
            return;
        }

        screen.y = GetScreenHeight() - screen.y;
        DrawTextScreen(screen.x, screen.y, text, fontSize, fontColor, outlineWidth, outlineColor);
    }


    // return closest t
    float closest_cylinder(glm::vec3 ro, glm::vec3 rd, glm::vec3 P0, glm::vec3 P1, glm::vec3* p_closest_online, bool* touch_body) {
        glm::vec3 T = P1 - P0;
        float H = glm::length(T);
        float one_over_H = 1.0f / H;
        T *= one_over_H;

        glm::vec3 S = ro - P0;

        // Shift the origin
        ro = S;

        // Get z
        float ro_z = glm::dot(T, ro);
        float rd_z = glm::dot(T, rd);

        // Cut z component
        ro = ro - ro_z * T;
        rd = rd - rd_z * T;

        float A = glm::dot(rd, rd);
        float B = glm::dot(rd, ro);
        float t = (-B) / A;

        if (t < 0) {
            return -1;
        }

        float hit_z = ro_z + rd_z * t;
        *touch_body = 0.0f <= hit_z && hit_z < H;
        *p_closest_online = glm::mix(P0, P1, hit_z * one_over_H);
        return t;
    }

    void ManipulateLineConstraint(glm::vec3* v,
        glm::vec3 ro_cur, glm::vec3 rd_cur,
        glm::vec3 ro_pre, glm::vec3 rd_pre,
        glm::vec3 direction, glm::u8vec3 color) {
        using namespace pr;

        static void* s_grab = nullptr;
        static glm::vec3 s_direction;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) == false) {
            s_grab = nullptr;
            s_direction = glm::vec3(0.0f);
        }

        bool manipulatable = v == s_grab && s_direction == direction;

        glm::vec3 po = *v + direction * 0.2f;
        glm::vec3 px = *v + direction;
        float arrow_radius = glm::length(direction) * 0.04f;
        float arrow_radius_cond = arrow_radius * 3 /*bias*/;

        glm::vec3 closest_online_pre;
        glm::vec3 closest_online_cur;
        bool touch_body_pre;
        bool touch_body_cur;
        float t_pre = closest_cylinder(ro_pre, rd_pre, po, px, &closest_online_pre, &touch_body_pre);
        float t_cur = closest_cylinder(ro_cur, rd_cur, po, px, &closest_online_cur, &touch_body_cur);

        bool grabable = 0 < t_cur && glm::distance(closest_online_cur, ro_cur + rd_cur * t_cur) < arrow_radius_cond && touch_body_cur;
        bool bright = manipulatable || grabable;
        glm::u8vec3 dark = color / (uint8_t)2;
        DrawArrow(po, px, arrow_radius, bright ? color : dark, 8, bright ? MANIP_HIGHLIGHT_LINE_WIDTH : 1.0f);

        if (s_grab == nullptr) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                if (0 < t_cur && grabable) {
                    s_grab = v;
                    s_direction = direction;
                }
            }
        }
        else {
            if (manipulatable) {
                if (0 < t_pre && 0 < t_cur) {
                    glm::vec3 move = closest_online_cur - closest_online_pre;
                    *v += move;
                }
            }
        }
    }

    namespace {
        float compMin(glm::vec3 v) {
            return glm::min(glm::min(v.x, v.y), v.z);
        }
        float compMax(glm::vec3 v) {
            return glm::max(glm::max(v.x, v.y), v.z);
        }
        glm::vec3 select(glm::vec3 a, glm::vec3 b, glm::bvec3 s) {
            return {
                s.x ? b.x : a.x,
                s.y ? b.y : a.y,
                s.z ? b.z : a.z
            };
        }
        bool slabs(glm::vec3 p0, glm::vec3 p1, glm::vec3 ro, glm::vec3 one_over_rd) {
            glm::vec3 t0 = (p0 - ro) * one_over_rd;
            glm::vec3 t1 = (p1 - ro) * one_over_rd;

            t0 = select(t0, -t1, glm::isnan(t0));
            t1 = select(t1, -t0, glm::isnan(t1));

            glm::vec3 tmin = min(t0, t1), tmax = max(t0, t1);
            float region_min = compMax(tmin);
            float region_max = compMin(tmax);
            return region_min <= region_max && 0.0f <= region_max;
        }

        float project_on_plane(glm::vec3 ro, glm::vec3 rd, glm::vec3 o, glm::vec3 T) {
            return -glm::dot(ro - o, T) / glm::dot(T, rd);
        }
    }
    void ManipulatePlaneConstraint(glm::vec3* v,
        glm::vec3 ro_cur, glm::vec3 rd_cur,
        glm::vec3 ro_pre, glm::vec3 rd_pre,
        glm::vec3 axis_a, glm::vec3 axis_b) {
        static void* s_grab = nullptr;
        static glm::vec3 s_T;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) == false) {
            s_grab = nullptr;
            s_T = glm::vec3(0.0f);
        }

        glm::vec3 T = glm::cross(axis_a, axis_b);
        bool manipulatable = v == s_grab && s_T == T;

        float lower = 0.25f;
        float upper = 0.75f;
        glm::vec3 xy_a = *v + (axis_a + axis_b) * lower;
        glm::vec3 xy_b = *v + (axis_a + axis_b) * upper;

        bool grabable = slabs(xy_a, xy_b, ro_cur, glm::vec3(1.0f) / rd_cur);
        bool bright = manipulatable || grabable;
        glm::u8vec3 color = bright ? glm::u8vec3{ 176, 255, 255 } : glm::u8vec3{ 128, 128, 128 };
        DrawAABB(xy_a, xy_b, color, bright ? MANIP_HIGHLIGHT_LINE_WIDTH : 1.0f);
        DrawLine(xy_a, xy_b, color, bright ? MANIP_HIGHLIGHT_LINE_WIDTH : 1.0f);
        DrawLine(
            *v + axis_a * lower + axis_b * upper,
            *v + axis_a * upper + axis_b * lower,
            color, bright ? MANIP_HIGHLIGHT_LINE_WIDTH : 1.0f);

        if (s_grab == nullptr) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                if (grabable) {
                    s_grab = v;
                    s_T = T;
                }
            }
        }
        else {
            if (manipulatable) {
                float t_cur = project_on_plane(ro_cur, rd_cur, *v, T);
                float t_pre = project_on_plane(ro_pre, rd_pre, *v, T);
                if (0 < t_pre && 0 < t_cur) {
                    glm::vec3 closest_online_pre = ro_pre + rd_pre * t_pre;
                    glm::vec3 closest_online_cur = ro_cur + rd_cur * t_cur;
                    glm::vec3 move = closest_online_cur - closest_online_pre;
                    *v += move;
                }
            }
        }
    }
    /*
        x  : intersected t. -1 is no-intersected
        yzw: un-normalized normal
    */
    float intersect_sphere(glm::vec3 ro, glm::vec3 rd, glm::vec3 o, float r) {
        float A = glm::dot(rd, rd);
        glm::vec3 S = ro - o;
        glm::vec3 SxRD = cross(S, rd);
        float D = A * r * r - glm::dot(SxRD, SxRD);

        if (D < 0.0f) {
            return -1;
        }

        float B = glm::dot(S, rd);
        float sqrt_d = sqrt(D);
        float t0 = (-B - sqrt_d) / A;
        if (0.0f < t0) {
            return t0;
        }
        return -1;
    }

    void ManipulatePlaneConstraintNonAxisAligned(glm::vec3* v,
        glm::vec3 ro_cur, glm::vec3 rd_cur,
        glm::vec3 ro_pre, glm::vec3 rd_pre,
        glm::vec3 cameradir, float size) {
        static void* s_grab = nullptr;
        static glm::vec3 s_T;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) == false) {
            s_grab = nullptr;
            s_T = glm::vec3(0.0f);
        }

        glm::vec3 T = cameradir;
        bool manipulatable = v == s_grab && s_T == T;

        float radius = size * 0.1f;
        bool grabable = 0.0f < intersect_sphere(ro_cur, rd_cur, *v, radius);

        bool bright = manipulatable || grabable;
        glm::u8vec3 color = bright ? glm::u8vec3{ 255, 0, 255 } : glm::u8vec3{ 128, 128, 128 };
        DrawSphere(*v, radius, color, 8, 8, {0, 1, 0}, bright ? MANIP_HIGHLIGHT_LINE_WIDTH : 1.0f);

        if (s_grab == nullptr) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                if (grabable) {
                    s_grab = v;
                    s_T = T;
                }
            }
        }
        else {
            if (manipulatable) {
                float t_cur = project_on_plane(ro_cur, rd_cur, *v, T);
                float t_pre = project_on_plane(ro_pre, rd_pre, *v, T);
                if (0 < t_pre && 0 < t_cur) {
                    glm::vec3 closest_online_pre = ro_pre + rd_pre * t_pre;
                    glm::vec3 closest_online_cur = ro_cur + rd_cur * t_cur;
                    glm::vec3 move = closest_online_cur - closest_online_pre;
                    *v += move;
                }
            }
        }
    }
    void ManipulatePosition(const pr::Camera3D& camera, glm::vec3* v, float manipulatorSize) {
        using namespace pr;
        glm::mat4 view;
        glm::mat4 proj;
		GetCameraMatrix( camera, &proj, &view, GetScreenWidth(), GetScreenHeight() );
        glm::mat4 inverse_vp = glm::inverse(proj * view);

        auto curMouse = GetMousePosition();
        auto preMouse = GetMousePosition() - GetMouseDelta();

        auto h = [](glm::vec4 v) {
            return glm::vec3(v / v.w);
        };
        LinearTransform i2x(0, (float)GetScreenWidth(), -1, 1);
        LinearTransform j2y(0, (float)GetScreenHeight(), 1, -1);
        auto ro_cur = h(inverse_vp * glm::vec4(i2x(curMouse.x), j2y(curMouse.y), -1 /*near*/, 1));
        auto rd_cur = h(inverse_vp * glm::vec4(i2x(curMouse.x), j2y(curMouse.y), +1 /*far */, 1)) - ro_cur;
        rd_cur = glm::normalize(rd_cur);

        auto ro_pre = h(inverse_vp * glm::vec4(i2x(preMouse.x), j2y(preMouse.y), -1 /*near*/, 1));
        auto rd_pre = h(inverse_vp * glm::vec4(i2x(preMouse.x), j2y(preMouse.y), +1 /*far */, 1)) - ro_pre;
        rd_pre = glm::normalize(rd_pre);

        glm::vec3 xaxis = { manipulatorSize, 0.0f, 0.0f };
        glm::vec3 yaxis = { 0.0f, manipulatorSize, 0.0f };
        glm::vec3 zaxis = { 0.0f, 0.0f, manipulatorSize };
        ManipulateLineConstraint(v,
            ro_cur, rd_cur, ro_pre, rd_pre,
            xaxis, glm::u8vec3{ 255, 0, 0 });
        ManipulateLineConstraint(v,
            ro_cur, rd_cur, ro_pre, rd_pre,
            yaxis, glm::u8vec3{ 0, 255, 0 });
        ManipulateLineConstraint(v,
            ro_cur, rd_cur, ro_pre, rd_pre,
            zaxis, glm::u8vec3{ 0, 0, 255 });
        
        ManipulatePlaneConstraint(v,
            ro_cur, rd_cur, ro_pre, rd_pre,
            xaxis, yaxis
        );
        ManipulatePlaneConstraint(v,
            ro_cur, rd_cur, ro_pre, rd_pre,
            yaxis, zaxis
        );
        ManipulatePlaneConstraint(v,
            ro_cur, rd_cur, ro_pre, rd_pre,
            zaxis, xaxis
        );

		glm::vec3 dir = glm::transpose(glm::mat3(view)) * glm::vec3(0.0f, 0.0f, -1.0f);
        ManipulatePlaneConstraintNonAxisAligned(v,
            ro_cur, rd_cur, ro_pre, rd_pre,
			dir, manipulatorSize
        );
    }
}