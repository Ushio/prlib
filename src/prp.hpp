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

    glm::vec3 GenerateUniformOnSphere(float u0, float u1);
    glm::vec3 GenerateUniformOnHemisphere(float u0, float u1);
    glm::vec3 GenerateUniformOnSphereLimitedAngle(float u0, float u1, float limitedCosTheta);

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
        Real operator()(Real x) const {
            return evaluate(x);
        }
        LinearTransform<Real> inverse() const {
            return LinearTransform(Real(1.0f) / _a, -_b / _a);
        }
    private:
        Real _a;
        Real _b;
    };

    // Building an Orthonormal Basis, Revisited
    template <typename Real>
    inline void GetOrthonormalBasis(const glm::tvec3<Real>& zaxis, glm::tvec3<Real> *xaxis, glm::tvec3<Real> *yaxis) {
        const Real sign = std::copysign(Real(1.0), zaxis.z);
        const Real a = Real(-1.0) / (sign + zaxis.z);
        const Real b = zaxis.x * zaxis.y * a;
        *xaxis = glm::tvec3<Real>(Real(1.0) + sign * zaxis.x * zaxis.x * a, sign * b, -sign * zaxis.x);
        *yaxis = glm::tvec3<Real>(b, sign + zaxis.y * zaxis.y * a, -zaxis.y);
    }

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
}