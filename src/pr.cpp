#include "pr.hpp"

#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <functional>

namespace pr {
    // foward
    class Shader;
    class PrimitivePipeline;
    class MSFrameBufferObject;

    // Global 
    Config g_config;
    GLFWwindow *g_window = nullptr;

    // MainFrameBuffer
    MSFrameBufferObject *g_frameBuffer = nullptr;

    void *g_current_pipeline = nullptr;
    PrimitivePipeline *g_primitive_pipeline = nullptr;

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
                #version 420

                in vec3 in_position;
                in vec3 in_color;

                out vec3 tofs_color;

                void main() {
                    gl_Position = vec4(in_position, 1.0);
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
    void Primitive::clear() {
        _positions.clear();
        _colors.clear();
    }
    void Primitive::add(float3 p, byte3 c) {
        _positions.emplace_back(p);
        _colors.emplace_back(c);
    }
    void Primitive::draw(PrimitiveMode mode, float width) {
        if (g_current_pipeline != g_primitive_pipeline) {
            g_current_pipeline = g_primitive_pipeline;
            g_primitive_pipeline->bind();
        }

        auto p = g_primitive_pipeline->positions();
        auto c = g_primitive_pipeline->colors();
        p->upload(_positions.data(), _positions.size() * sizeof(float3));
        c->upload(_colors.data(), _colors.size() * sizeof(byte3));

        switch (mode) {
        case PrimitiveMode::Points:
            glPointSize(width);
            glDrawArrays(GL_POINTS, 0, _positions.size());
            break;
        case PrimitiveMode::Lines:
            glLineWidth(width);
            glDrawArrays(GL_LINES, 0, _positions.size());
            break;
        case PrimitiveMode::LineStrip:
            glLineWidth(width);
            glDrawArrays(GL_LINE_STRIP, 0, _positions.size());
            break;
        }
    }

    void DrawLine(float3 p0, float3 p1, byte3 c, float lineWidth) {
        static Primitive prim;
        prim.add(p0, c);
        prim.add(p1, c);
        prim.draw(PrimitiveMode::Lines, lineWidth);
        prim.clear();
    }
    void DrawPoint(float3 p, byte3 c, float pointSize) {
        static Primitive prim;
        prim.add(p, c);
        prim.draw(PrimitiveMode::Points, pointSize);
        prim.clear();
    }
    void DrawCircle(float3 o, byte3 c, float radius, int vertexCount, float lineWidth) {
        LinearTransform<float> i2rad(0, vertexCount - 1, 0, pi * 2.0f);

        static Primitive prim;
        for (int i = 0; i < vertexCount; ++i) {
            float radian = i2rad.evaluate(i);
            float3 p = {
                std::cos(radian),
                std::sin(radian),
                0.0f
            };
            prim.add(o + radius * p, c);
        }
        prim.draw(PrimitiveMode::LineStrip, lineWidth);
        prim.clear();
    }

    static void SetupGraphics() {
        glViewport(0, 0, g_config.ScreenWidth, g_config.ScreenHeight);

        g_primitive_pipeline = new PrimitivePipeline();
        g_frameBuffer = new MSFrameBufferObject();
        g_frameBuffer->resize(g_config.ScreenWidth, g_config.ScreenHeight, g_config.NumSamples);
        g_frameBuffer->bind();

        glDisable(GL_BLEND);
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
    bool ProcessSystem() {
        g_frameBuffer->copyToScreen();
        glfwSwapBuffers(g_window);

        glfwPollEvents();
        int width, height;
        glfwGetFramebufferSize(g_window, &width, &height);
        g_frameBuffer->resize(width, height, g_config.NumSamples);

        glViewport(0, 0, width, height);

        return glfwWindowShouldClose(g_window);
    }

    // Random Number
    float IRandom::uniform() {
        return this->uniform_float();
    }
    float IRandom::uniform(float a, float b) {
        return linalg::lerp(a, b, uniform_float());
    }
    int IRandom::uniform(int a, int b) {
        int64_t length = (int64_t)b - (int64_t)a;
        return a + this->uniform_integer() % length;
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

            if (state() == uint4(0, 0, 0, 0)) {
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
        uint4 state() const {
            return uint4(s[0], s[1], s[2], s[3]);
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
}