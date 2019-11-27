#pragma once

#include <vector>
#include <functional>
#include "glm/glm.hpp"
#include "glm/ext.hpp"

/*
 Potable Modules
*/
namespace pr {
    enum class Result {
        Sucess,
        FileNotFound,
        AccessDenied,
        Unknown,
    };

    // Random Number Core
    class IRandomNumberGenerator {
    public:
        virtual ~IRandomNumberGenerator() {}

        /* float */
        // 0.0 <= x < 1.0
        virtual float uniformf() = 0;

        // large integer random number for ignore modulo bias.
        virtual uint64_t uniformi() = 0;
    };

    struct Xoshiro128StarStar : public IRandomNumberGenerator {
        Xoshiro128StarStar();
        Xoshiro128StarStar(uint32_t seed);

        float uniformf() override;
        uint64_t uniformi() override;

        // state
        uint32_t s[4];
    };

    glm::vec2 GenerateUniformInCircle(float u0, float u1);
    glm::vec3 GenerateUniformOnSphere(float u0, float u1);
    glm::vec3 GenerateUniformOnHemisphere(float u0, float u1);

    // notes:
    //     this cause numerical error at limitedCosTheta is too small.
    glm::vec3 GenerateUniformOnSphereLimitedAngle(float u0, float u1, float limitedCosTheta);

    // Math 
    struct LinearTransform {
        LinearTransform() :_a(1.0f), _b(0.0f) {}
        LinearTransform(float a, float b) :_a(a), _b(b) {}
        LinearTransform(float inputMin, float inputMax, float outputMin, float outputMax) {
            _a = (outputMax - outputMin) / (inputMax - inputMin);
            _b = outputMin - _a * inputMin;
        }
        float evaluate(float x) const {
            return std::fma(_a, x, _b);
        }
        float operator()(float x) const {
            return evaluate(x);
        }
        LinearTransform inverse() const {
            return LinearTransform(1.0f / _a, -_b / _a);
        }
    private:
        float _a;
        float _b;
    };
    void GetOrthonormalBasis(glm::vec3 zaxis, glm::vec3 *xaxis, glm::vec3 *yaxis);

    class Image2DRGBA8 {
    public:
        void allocate(int w, int h);
        void load(const char *filename);

        glm::u8vec4 *data();
        const glm::u8vec4 *data() const;

        glm::u8vec4 &operator()(int x, int y);
        const glm::u8vec4 &operator()(int x, int y) const;

        glm::u8vec4 &at(int x, int y);
        const glm::vec4 &at(int x, int y) const;

        int width() const;
        int height() const;
    private:
        int _width = 0;
        int _height = 0;
        std::vector<glm::u8vec4> _values;
    };

    glm::vec3 GetCartesian(float theta, float phi);
    void GetSpherical(glm::vec3 direction, float *theta, float *phi);
}