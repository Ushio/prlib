#pragma once

#include <string>
#include <vector>

// https://github.com/sgorsten/linalg
#include "linalg.h"

/*
 Macros
*/
#include <intrin.h>
#define PR_ASSERT(ExpectTrue) if((ExpectTrue) == 0) { __debugbreak(); }

namespace pr {
    // Constant
    static const float pi = 3.14159265358979323846f;
    static const float e  = 2.71828182845904523536f;

    using namespace linalg::aliases;

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

    // Drawing
    void ClearBackground(float r, float g, float b, float a);
    void SetDepthTest(bool enabled);

    // Camera
    struct Camera3D {
        float3 origin;
        float3 lookat;
        float3 up;                // up direction. If it is zero, used (0, 1, 0) vector
        float fovy     = 0.0f;    // vertical field of view
        float nearclip = 0.01f;
        float farclip  = 1000.0f;
    };

    // Batch Module
    enum class PrimitiveMode {
        Points,
        Lines,
        LineStrip,
    };
    class Primitive {
    public:
        void clear();
        void add(float3 p, byte3 c);
        void draw(PrimitiveMode mode, float width = 1.0f);
    private:
        std::vector<float3> _positions;
        std::vector<byte3> _colors;
    };

    // Simple Functions
    void DrawLine(float3 p0, float3 p1, byte3 c, float lineWidth = 1.0f);
    void DrawPoint(float3 p, byte3 c, float pointSize = 1.0f);
    void DrawCircle(float3 o, byte3 c, float radius, int vertexCount, float lineWidth = 1.0f);

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
}