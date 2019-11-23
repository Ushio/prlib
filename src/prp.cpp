#include "prp.hpp"
#include <algorithm>

namespace pr {
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
    Xoshiro128StarStar::Xoshiro128StarStar():Xoshiro128StarStar(953687342) {

    }
    Xoshiro128StarStar::Xoshiro128StarStar(uint32_t seed) {
        splitmix64 sp;
        sp.x = seed;
        uint64_t r0 = sp.next();
        uint64_t r1 = sp.next();
        s[0] = r0 & 0xFFFFFFFF;
        s[1] = (r0 >> 32) & 0xFFFFFFFF;
        s[2] = r1 & 0xFFFFFFFF;
        s[3] = (r1 >> 32) & 0xFFFFFFFF;
        
        if (std::all_of(s, s + 4, [](uint32_t x) { return x == 0; })) {
            s[0] = 1;
        }
    }

    namespace {
        uint32_t rotl(const uint32_t x, int k) {
            return (x << k) | (x >> (32 - k));
        }
        uint32_t Xoshiro128StarStarNext(Xoshiro128StarStar &state) {
            const uint32_t result_starstar = rotl(state.s[0] * 5, 7) * 9;
            const uint32_t t = state.s[1] << 9;

            state.s[2] ^= state.s[0];
            state.s[3] ^= state.s[1];
            state.s[1] ^= state.s[2];
            state.s[0] ^= state.s[3];

            state.s[2] ^= t;

            state.s[3] = rotl(state.s[3], 11);

            return result_starstar;
        }
    }

    float Xoshiro128StarStar::uniformf() {
        uint32_t x = Xoshiro128StarStarNext(*this);
        uint32_t bits = (x >> 9) | 0x3f800000;
        float value = *reinterpret_cast<float *>(&bits) - 1.0f;
        return value;
    }

    uint64_t Xoshiro128StarStar::uniformi() {
        uint64_t a = Xoshiro128StarStarNext(*this) >> 1;
        uint64_t b = Xoshiro128StarStarNext(*this) >> 1;
        return (a << 31) | b;
    }

    // Random Number Helper
    glm::vec3 GenerateUniformOnSphere(float u0, float u1) {
        return GenerateUniformOnSphereLimitedAngle(u0, u1, -1.0f);
    }
    glm::vec3 GenerateUniformOnHemisphere(float u0, float u1) {
        return GenerateUniformOnSphereLimitedAngle(u0, u1, 0.0f);
    }
    glm::vec3 GenerateUniformOnSphereLimitedAngle(float u0, float u1, float limitedCosTheta) {
        float phi = u0 * glm::pi<float>() * 2.0f;
        float z = glm::mix(1.0f, limitedCosTheta, u1);
        float r_xy = std::sqrt(std::max(1.0f - z * z, 0.0f));
        float x = r_xy * std::cos(phi);
        float y = r_xy * std::sin(phi);
        return glm::vec3(x, y, z);
    }

    void Image2DRGBA8::allocate(int w, int h) {
        _width = w;
        _height = h;
        _values.clear();
        _values.resize(_width * _height);
    }
    glm::u8vec4 *Image2DRGBA8::data() {
        return _values.data();
    }
    const glm::u8vec4 *Image2DRGBA8::data() const {
        return _values.data();
    }
    glm::u8vec4 &Image2DRGBA8::operator()(int x, int y) {
        return _values[y * _width + x];
    }
    const glm::u8vec4 &Image2DRGBA8::operator()(int x, int y) const {
        return _values[y * _width + x];
    }
    glm::u8vec4 &Image2DRGBA8::at(int x, int y) {
        if (x < 0 || _width < x) {
            throw std::out_of_range("x is out of range");
        }
        if (y < 0 || _height < y) {
            throw std::out_of_range("y is out of range");
        }
        return (*this)(x, y);
    }
    const glm::vec4 &Image2DRGBA8::at(int x, int y) const {
        if (x < 0 || _width < x) {
            throw std::out_of_range("x is out of range");
        }
        if (y < 0 || _height < y) {
            throw std::out_of_range("y is out of range");
        }
        return (*this)(x, y);
    }
    int Image2DRGBA8::width() const {
        return _width;
    }
    int Image2DRGBA8::height() const {
        return _height;
    }
}