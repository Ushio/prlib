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
    class MSFrameBufferObject;

    // Global Variable (Internal)
    Config g_config;
    GLFWwindow *g_window = nullptr;

    // MainFrameBuffer
    MSFrameBufferObject *g_frameBuffer = nullptr;

    void *g_current_pipeline = nullptr;
    PrimitivePipeline *g_primitive_pipeline = nullptr;

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
    std::queue<InputEvent> g_events;

    std::map<int, ButtonCondition> g_mouseButtonConditions;
    std::map<int, ButtonCondition> g_mouseButtonConditionsPrevious;
    glm::vec2 g_mousePosition    = glm::vec2(0, 0);
    glm::vec2 g_mouseDelta       = glm::vec2(0, 0);
    glm::vec2 g_mouseScrollDelta = glm::vec2(0, 0);

    std::map<int, ButtonCondition> g_keyConditions;
    std::map<int, ButtonCondition> g_keyConditionsPrevious;

    class ICamera;
    // Global Variable (User)
    std::stack<std::unique_ptr<const ICamera>> g_cameraStack;

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
        void bindAttributeLocation(int location, const char *variable) {
            glBindAttribLocation(_program, location, variable);
        }
        void bindFragLocation(int location, const char *variable) {
            glBindFragDataLocation(_program, location, variable);
        }
        void setUniformMatrix(glm::mat4 m, const char *variable) {
            GLint cur_program;
            glGetIntegerv(GL_CURRENT_PROGRAM, &cur_program);

            glUseProgram(_program);
            auto location = glGetUniformLocation(_program, variable);
            glUniformMatrix4fv(location, 1, GL_FALSE, (float *)&m);

            glUseProgram(cur_program);
        }
    private:
        GLuint _vs = 0;
        GLuint _fs = 0;
        GLuint _program = 0;
    };
    class ArrayBuffer {
    public:
        ArrayBuffer():_bytes(0) {
            glGenBuffers(1, &_buffer);
        }
        ~ArrayBuffer() {
            glDeleteBuffers(1, &_buffer);
        }
        ArrayBuffer(const ArrayBuffer&) = delete;
        ArrayBuffer& operator=(const ArrayBuffer&) = delete;

        void upload(void *p, int bytes) {
            glBindBuffer(GL_ARRAY_BUFFER, _buffer);

            if (bytes > _bytes) {
                glBufferData(GL_ARRAY_BUFFER, bytes, p, GL_DYNAMIC_DRAW);
                _bytes = bytes;
            } else {
                glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, p);
            }
            
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        void bind() {
            glBindBuffer(GL_ARRAY_BUFFER, _buffer);
        }
        void unbind() {
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        int bytes() const {
            return _bytes;
        }
    private:
        int _bytes;
        GLuint _buffer;
    };
    class VertexArrayObject {
    public:
        VertexArrayObject() {
            glGenVertexArrays(1, &_vao);
        }
        ~VertexArrayObject() {
            glDeleteVertexArrays(1, &_vao);
        }
        VertexArrayObject(const VertexArrayObject&) = delete;
        VertexArrayObject& operator=(const VertexArrayObject&) = delete;

        void bind() {
            glBindVertexArray(_vao);
        }
        void unbind() {
            glBindVertexArray(0);
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
                
                in vec3 in_position;
                in vec3 in_color;

                out vec3 tofs_color;

                void main() {
                    gl_Position = u_vp * vec4(in_position, 1.0);
                    tofs_color = in_color;
                }
            )";
            const char *fs = R"(
                #version 410

                in vec3 tofs_color;
                out vec4 out_fragColor;

                void main() {
                  out_fragColor = vec4(tofs_color, 1.0);
                }
            )";
            _shader = std::unique_ptr<Shader>(new Shader(vs, fs));
            _shader->bindAttributeLocation(0, "in_position");
            _shader->bindAttributeLocation(1, "in_color");
            _shader->bindFragLocation(0, "out_fragColor");

            _positions = std::unique_ptr<ArrayBuffer>(new ArrayBuffer());
            _colors    = std::unique_ptr<ArrayBuffer>(new ArrayBuffer());

            _vao->bind();

            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            _positions->bind();
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
            _colors->bind();
            glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0);

            _vao->unbind();
        }
        void bind() {
            _shader->bind();
            _vao->bind();
        }
        void unbind() {
            _shader->unbind();
            _vao->unbind();
        }
        
        // float x 3
        ArrayBuffer *positions() {
            return _positions.get();
        }
        // byte x 4
        ArrayBuffer *colors() {
            return _colors.get();
        }

        void setVP(glm::mat4 viewMatrix, glm::mat4 projMatrix) {
            _shader->setUniformMatrix(projMatrix * viewMatrix, "u_vp");
        }
    private:
        std::unique_ptr<VertexArrayObject> _vao;
        std::unique_ptr<Shader> _shader;
        std::unique_ptr<ArrayBuffer> _positions;
        std::unique_ptr<ArrayBuffer> _colors;
    };

    static void KeepFrameBuffer(std::function<void(void)> f) {
        GLint curDrawFBO = 0;
        GLint curReadFBO = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &curDrawFBO);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &curReadFBO);

        f();

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, curDrawFBO);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, curReadFBO);
    }

    class MSFrameBufferObject {
    public:
        MSFrameBufferObject() {
            glGenFramebuffers(1, &_fb);
            glGenRenderbuffers(1, &_color);
            glGenRenderbuffers(1, &_depth);
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

            glBindRenderbuffer(GL_RENDERBUFFER, _color);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, _samples, GL_RGBA8, _width, _height);
            glBindRenderbuffer(GL_RENDERBUFFER, _depth);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, _samples, GL_DEPTH_COMPONENT24, _width, _height);

            glBindRenderbuffer(GL_RENDERBUFFER, 0);

            KeepFrameBuffer([&]() {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fb);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _color);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depth);

                auto status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
                PR_ASSERT(status == GL_FRAMEBUFFER_COMPLETE);

                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            });
        }
        void bind() {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fb);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        }
        void copyToScreen() {
            KeepFrameBuffer([&]() {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, _fb);

                glBlitFramebuffer(
                    0, 0, _width, _height,
                    0, 0, _width, _height,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST
                );
            });
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
            return glm::lookAt(_camera.origin, _camera.lookat, _camera.up);
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
        g_primitive_pipeline->setVP(camera->getViewMatrix(), camera->getProjectionMatrx());
    }

    /*
     Graphics Core
    */
    void ClearBackground(float r, float g, float b, float a) {
        glClearColor(r, g, b, a);
        GLbitfield flag = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
        glClear(flag);
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

    void Primitive::clear() {
        _positions.clear();
        _colors.clear();
    }
    void Primitive::add(glm::vec3 p, glm::u8vec3 c) {
        _positions.emplace_back(p);
        _colors.emplace_back(c);
    }
    void Primitive::draw(PrimitiveMode mode, float width) {
        if (g_current_pipeline != g_primitive_pipeline) {
            g_current_pipeline = g_primitive_pipeline;
            g_primitive_pipeline->bind();
        }
        PR_ASSERT(_positions.size() == _colors.size());

        auto p = g_primitive_pipeline->positions();
        auto c = g_primitive_pipeline->colors();
        p->upload(_positions.data(), (int)(_positions.size() * sizeof(glm::vec3)));
        c->upload(_colors.data(), (int)(_colors.size() * sizeof(glm::u8vec3)));

        switch (mode) {
        case PrimitiveMode::Points:
            glPointSize(width);
            glDrawArrays(GL_POINTS, 0, (int)_positions.size());
            break;
        case PrimitiveMode::Lines:
            glLineWidth(width);
            glDrawArrays(GL_LINES, 0, (int)_positions.size());
            break;
        case PrimitiveMode::LineStrip:
            glLineWidth(width);
            glDrawArrays(GL_LINE_STRIP, 0, (int)_positions.size());
            break;
        }
    }

    void DrawLine(glm::vec3 p0, glm::vec3 p1, glm::u8vec3 c, float lineWidth) {
        static Primitive prim;
        prim.add(p0, c);
        prim.add(p1, c);
        prim.draw(PrimitiveMode::Lines, lineWidth);
        prim.clear();
    }
    void DrawPoint(glm::vec3 p, glm::u8vec3 c, float pointSize) {
        static Primitive prim;
        prim.add(p, c);
        prim.draw(PrimitiveMode::Points, pointSize);
        prim.clear();
    }
    void DrawCircle(glm::vec3 o, glm::u8vec3 c, float radius, int vertexCount, float lineWidth) {
        LinearTransform<float> i2rad(0.0f, (float)(vertexCount - 1), 0.0f, glm::pi<float>() * 2.0f);

        static Primitive prim;
        for (int i = 0; i < vertexCount; ++i) {
            float radian = i2rad.evaluate((float)i);
            glm::vec3 p = {
                std::cos(radian),
                std::sin(radian),
                0.0f
            };
            prim.add(o + radius * p, c);
        }
        prim.draw(PrimitiveMode::LineStrip, lineWidth);
        prim.clear();
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

        static Primitive prim;
        {
            // u line
            auto L = uaxis * (-hWide);
            auto R = uaxis * (+hWide);
            for (int ui = -hBlockCount; ui <= hBlockCount; ++ui) {
                prim.add(L + vaxis * (float)ui * step, c);
                prim.add(R + vaxis * (float)ui * step, c);
            }
        }
        {
            // v line
            auto L = vaxis * (-hWide);
            auto R = vaxis * (+hWide);
            for (int vi = -hBlockCount; vi <= hBlockCount; ++vi) {
                prim.add(uaxis * (float)vi * step + L, c);
                prim.add(uaxis * (float)vi * step + R, c);
            }
        }
        prim.draw(PrimitiveMode::Lines, lineWidth);
        prim.clear();
    }
    void DrawTube(glm::vec3 p0, glm::vec3 p1, float radius0, float radius1, glm::u8vec3 c, int vertexCount, float lineWidth) {
        if (radius0 == 0.0f && radius1 == 0.0f) {
            DrawLine(p0, p1, c, lineWidth);
        }

        glm::vec3 T = p1 - p0;
        glm::vec3 z = glm::normalize(T);

        glm::vec3 x;
        glm::vec3 y;
        getOrthonormalBasis<float>(z, &x, &y);

        std::vector<glm::vec3> circleVertices(vertexCount);
        LinearTransform<float> i2rad(0.0f, (float)(vertexCount - 1), 0.0f, glm::pi<float>() * 2.0f);
        for (int i = 0; i < vertexCount; ++i) {
            float radian = i2rad.evaluate((float)i);
            circleVertices[i] = x * std::sin(radian) + y * std::cos(radian);
        }

        static Primitive prim;

        // Body
        for (int i = 0; i < vertexCount; ++i) {
            glm::vec3 a = p0 + radius0 * circleVertices[i];
            glm::vec3 b = p1 + radius1 * circleVertices[i];
            prim.add(a, c);
            prim.add(b, c);
        }

        // Two Circles
        if (0.0f < radius0) {
            for (int i = 0; i < vertexCount - 1; ++i) {
                glm::vec3 a = p0 + radius0 * circleVertices[i];
                glm::vec3 b = p0 + radius0 * circleVertices[i + 1];
                prim.add(a, c);
                prim.add(b, c);
            }
        }
        if (0.0f < radius1) {
            for (int i = 0; i < vertexCount - 1; ++i) {
                glm::vec3 a = p1 + radius1 * circleVertices[i];
                glm::vec3 b = p1 + radius1 * circleVertices[i + 1];
                prim.add(a, c);
                prim.add(b, c);
            }
        }

        prim.draw(PrimitiveMode::Lines, lineWidth);
        prim.clear();
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
    static void SetupGraphics() {
        glViewport(0, 0, g_config.ScreenWidth, g_config.ScreenHeight);

        g_primitive_pipeline = new PrimitivePipeline();
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
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
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
        delete g_primitive_pipeline;
        g_primitive_pipeline = nullptr;

        delete g_frameBuffer;
        g_frameBuffer = nullptr;

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

    // Random Number
    float IRandom::uniform() {
        return this->uniform_float();
    }
    float IRandom::uniform(float a, float b) {
        return glm::mix(a, b, uniform_float());
    }
    int IRandom::uniform(int a, int b) {
        int64_t length = (int64_t)b - (int64_t)a;
        return a + (int)(this->uniform_integer() % length);
    }

    // http://xoshiro.di.unimi.it/splitmix64.c
    // for generate seed
    struct splitmix64 {
        uint64_t x = 0; /* The state can be seeded with any value. */
        uint64_t next() {
            uint64_t z = (x += 0x9e3779b97f4a7c15);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
            z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
            return z ^ (z >> 31);
        }
    };

    /*
    http://xoshiro.di.unimi.it/xoshiro128starstar.c
    */
    struct Xoshiro128StarStar : public IRandom {
        Xoshiro128StarStar(uint32_t seed) {
            splitmix64 sp;
            sp.x = seed;
            uint64_t r0 = sp.next();
            uint64_t r1 = sp.next();
            s[0] = r0 & 0xFFFFFFFF;
            s[1] = (r0 >> 32) & 0xFFFFFFFF;
            s[2] = r1 & 0xFFFFFFFF;
            s[3] = (r1 >> 32) & 0xFFFFFFFF;

            if (state() == glm::uvec4(0, 0, 0, 0)) {
                s[0] = 1;
            }
        }
        float uniform_float() {
            uint32_t x = next();
            uint32_t bits = (x >> 9) | 0x3f800000;
            float value = *reinterpret_cast<float *>(&bits) - 1.0f;
            return value;
        }
        uint64_t uniform_integer() {
            // [0, 2^62-1]
            uint64_t a = next() >> 1;
            uint64_t b = next() >> 1;
            return (a << 31) | b;
        }
        glm::uvec4 state() const {
            return glm::uvec4(s[0], s[1], s[2], s[3]);
        }
    private:
        uint32_t rotl(const uint32_t x, int k) {
            return (x << k) | (x >> (32 - k));
        }
        uint32_t next() {
            const uint32_t result_starstar = rotl(s[0] * 5, 7) * 9;

            const uint32_t t = s[1] << 9;

            s[2] ^= s[0];
            s[3] ^= s[1];
            s[1] ^= s[2];
            s[0] ^= s[3];

            s[2] ^= t;

            s[3] = rotl(s[3], 11);

            return result_starstar;
        }
    private:
        uint32_t s[4];
    };

    IRandom *CreateRandomNumberGenerator(uint32_t seed) {
        return new Xoshiro128StarStar(seed);
    }

    float Radians(float degrees) {
        return glm::radians(degrees);
    }
    float Degrees(float radians) {
        return glm::degrees(radians);
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