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
    using namespace linalg::aliases;

    struct Config {
        int ScreenWidth = 1280;
        int ScreenHeight = 768;
        int SwapInterval = 1;
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
    void ClearBackground(float r, float g, float b, float a, bool clearDepth = true);

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
        void draw(PrimitiveMode mode);
        void width(float w);
    private:
        float _width = 1.0f;
        std::vector<float3> _positions;
        std::vector<byte3> _colors;
    };

    // Simple Functions
    void DrawLine(float3 p0, float3 p1, byte3 c, float width = 1.0f);

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
}