#pragma once

#include <string>
#include <vector>

#ifdef GLM_VERSION
    #ifndef GLM_FORCE_CTOR_INIT
        #error "please #define GLM_FORCE_CTOR_INIT or include pr.hpp first"
    #endif
    #ifndef GLM_FORCE_PURE
        #error "please #define GLM_FORCE_PURE or include pr.hpp first"
    #endif
#else
    #ifndef GLM_FORCE_CTOR_INIT
        #define GLM_FORCE_CTOR_INIT
    #endif
    #ifndef GLM_FORCE_PURE
        #define GLM_FORCE_PURE
    #endif
#endif


#include "glm/glm.hpp"
#include "glm/ext.hpp"

/*
 Macros
*/
#include <intrin.h>
#define PR_ASSERT(ExpectTrue) if((ExpectTrue) == 0) { __debugbreak(); }

namespace pr {
    struct Config {
        int ScreenWidth = 1280;
        int ScreenHeight = 768;
        int SwapInterval = 1;
        int NumSamples = 4;
        std::string Title;
    };
    void Initialize(Config config);
    void CleanUp();

    int GetScreenWidth();
    int GetScreenHeight();

    /*
      - Swap BackBuffer
      - Process Window Event
      - return true if the window should close
    */
    bool ProcessSystem();

    // Random Number
    struct IRandom {
        virtual ~IRandom() {}

        // 0.0 <= x < 1.0
        float uniform();

        // a <= x < b
        float uniform(float a, float b);

        // a <= x < b
        int uniform(int a, int b);
    protected:
        /* float */
        // 0.0 <= x < 1.0
        virtual float uniform_float() = 0;

        /* A large integer enough to ignore modulo bias */
        virtual uint64_t uniform_integer() = 0;
    };

    IRandom *CreateRandomNumberGenerator(uint32_t seed);

    // Math

    template <class Real>
    struct LinearTransform {
        LinearTransform() :_a(Real(1.0)), _b(Real(0.0)) {}
        LinearTransform(Real a, Real b) :_a(a), _b(b) {}
        LinearTransform(Real inputMin, Real inputMax, Real outputMin, Real outputMax) {
            _a = (outputMax - outputMin) / (inputMax - inputMin);
            _b = outputMin - _a * inputMin;
        }
        Real evaluate(Real x) const {
            return std::fma(_a, x, _b);
        }
        LinearTransform<Real> inverse() const {
            return LinearTransform(Real(1.0f) / _a, -_b / _a);
        }
    private:
        Real _a;
        Real _b;
    };
    float Radians(float degrees);
    float Degrees(float radians);

    // Drawing
    void ClearBackground(float r, float g, float b, float a);
    void SetDepthTest(bool enabled);

    // Camera
    struct Camera3D {
        glm::vec3 origin;
        glm::vec3 lookat;
        glm::vec3 up = { 0.0f, 1.0f, 0.0f };
        float fovy     = Radians(45.0f); // vertical field of view
        float zNear = 0.01f;
        float zFar  = 1000.0f;
    };
    void BeginCamera(Camera3D camera);
    void EndCamera();

    // Batch Module
    enum class PrimitiveMode {
        Points,
        Lines,
        LineStrip,
    };
    class Primitive {
    public:
        void clear();
        void add(glm::vec3 p, glm::u8vec3 c);
        void draw(PrimitiveMode mode, float width = 1.0f);
    private:
        std::vector<glm::vec3> _positions;
        std::vector<glm::u8vec3> _colors;
    };

    // Simple Functions
    void DrawLine(glm::vec3 p0, glm::vec3 p1, glm::u8vec3 c, float lineWidth = 1.0f);
    void DrawPoint(glm::vec3 p, glm::u8vec3 c, float pointSize = 1.0f);
    void DrawCircle(glm::vec3 o, glm::u8vec3 c, float radius, int vertexCount = 32, float lineWidth = 1.0f);

}