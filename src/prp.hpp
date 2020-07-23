#pragma once

#include <vector>
#include <functional>
#include <string>
#include <stdexcept>
#include <functional>
#include <random>
#include <cmath>
#include "glm/glm.hpp"
#include "glm/ext.hpp"

/*
 Potable Modules
*/
namespace pr {
    enum class Result {
        Sucess,
        Failure
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
    struct MersenneTwister : public IRandomNumberGenerator {
        MersenneTwister();
        MersenneTwister(uint32_t seed);

        float uniformf() override;
        uint64_t uniformi() override;

        // state
        std::mt19937 s;
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

    // http://iquilezles.org/www/articles/sincos/sincos.htm
    class CircleGenerator {
    public:
        CircleGenerator(float stepThetaT)
            :_sinDeltaT(std::sin(stepThetaT)), _cosDeltaT(std::cos(stepThetaT)) {
        }
        CircleGenerator(float stepThetaT, float theta)
            :_sinDeltaT(std::sin(stepThetaT)), _cosDeltaT(std::cos(stepThetaT))
            , _sinT(std::sin(theta)), _cosT(std::cos(theta)) {
        }

        // sin(theta)
        float sin() const {
            return _sinT;
        }

        // cos(theta)
        float cos() const {
            return _cosT;
        }

        // theta += stepThetaT
        void step() {
            float newSin = _sinT * _cosDeltaT + _cosT * _sinDeltaT;
            float newCos = _cosT * _cosDeltaT - _sinT * _sinDeltaT;
            _sinT = newSin;
            _cosT = newCos;
        }

        void setDeltaT(float stepThetaT) {
            _sinDeltaT = std::sin(stepThetaT);
            _cosDeltaT = std::cos(stepThetaT);
        }
    private:
        float _sinDeltaT = 0.0f;
        float _cosDeltaT = 0.0f;
        float _sinT = 0.0f;
        float _cosT = 1.0f;
    };

    class Image2DRGBA8 {
    public:
        void allocate(int w, int h);
        Result load(const char *filename);
        Result load(const uint8_t *data, int bytes);
        Result save(const char* filename) const;

        glm::u8vec4 *data();
        const glm::u8vec4 *data() const;
        int bytes() const;

        glm::u8vec4 &operator()(int x, int y);
        const glm::u8vec4 &operator()(int x, int y) const;

        glm::u8vec4 &at(int x, int y);
        const glm::u8vec4 &at(int x, int y) const;

        int width() const;
        int height() const;
    private:
        int _width = 0;
        int _height = 0;
        std::vector<glm::u8vec4> _values;
    };
    class Image2DMono8 {
    public:
        void allocate(int w, int h);
        Result load(const char *filename);
        Result load(const uint8_t *data, int bytes);

		Result save(const char* filename) const;

        uint8_t *data();
        const uint8_t *data() const;
        int bytes() const;

        uint8_t &operator()(int x, int y);
        const uint8_t &operator()(int x, int y) const;

        uint8_t &at(int x, int y);
        const uint8_t &at(int x, int y) const;

        int width() const;
        int height() const;
    private:
        int _width = 0;
        int _height = 0;
        std::vector<uint8_t> _values;
    };

	class Image2DRGBA32 {
	public:
		using PixelType = glm::vec4;

		void allocate(int w, int h);

		// warning: image will be degamma when you load 8bit image.
		Result load(const char *filename);
		Result load(const uint8_t *data, int bytes);

		Result save(const char* filename) const;

		PixelType *data();
		const PixelType *data() const;
		int bytes() const;

		PixelType &operator()(int x, int y);
		const PixelType &operator()(int x, int y) const;

		PixelType &at(int x, int y);
		const PixelType &at(int x, int y) const;

		int width() const;
		int height() const;
	private:
		int _width = 0;
		int _height = 0;
		std::vector<PixelType> _values;
	};

	// Simple Linear Translate
	Image2DRGBA32 Image2DRGBA8_to_Image2DRGBA32(const Image2DRGBA8 &src);

    glm::vec3 GetCartesian(float theta, float phi);
    void GetSpherical(glm::vec3 direction, float *theta, float *phi);

    void        SetDataDir(std::string dir);
    std::string NormalizePath(std::string path);
    std::string ExecutableDir();
    std::string JoinPath(std::string a, std::string b);
    std::string JoinPath(std::string a, std::string b, std::string c);
    std::string JoinPath(std::string a, std::string b, std::string c, std::string d);
    std::string JoinPath(std::string a, std::string b, std::string c, std::string d, std::string e);

    std::string GetPathBasename(std::string path); // note: includes extension 
    std::string GetPathDirname(std::string path);
    std::string GetPathExtension(std::string path);
    std::string ChangePathExtension(std::string path, std::string newExtension);

    std::string GetDataPath(std::string filename);

    void ParallelFor(int n, std::function<void(int)> f /* f(index) */);
    void SerialFor(int n, std::function<void(int)> f /* f(index) */);

    // warning: we assume string use system encoding
    std::wstring string_to_wstring(const std::string& s);
    std::string wstring_to_string(const std::wstring& s);

    class BinaryLoader
    {
    public:
        Result load(const char* file);

        uint8_t* data()
        {
            return _data.data();
        }
        const uint8_t* data() const
        {
            return _data.data();
        }
        std::size_t size() const
        {
            return _data.size();
        }
        void push_back(uint8_t c)
        {
            _data.push_back(c);
        }
    private:
        std::vector<uint8_t> _data;
    };

	// Simple Generator
	class CameraRayGenerator
	{
	public:
		CameraRayGenerator(glm::mat4 viewMatrix, glm::mat4 projMatrix, int width, int height)
		{
			glm::mat4 vp = projMatrix * viewMatrix;
			_inverseVP = glm::inverse(vp);

			_i2x = LinearTransform(0, (float)width, -1, 1);
			_j2y = LinearTransform(0, (float)height, 1, -1);
		}

		void shoot(glm::vec3 *ro, glm::vec3 *rd, int x, int y, float xoffsetInPixel = 0.0f, float yoffsetInPixel = 0.0f) const {
			auto h = [](glm::vec4 v) {
				return glm::vec3(v / v.w);
			};
			*ro = h(_inverseVP * glm::vec4(_i2x((float)x + xoffsetInPixel), _j2y((float)y + yoffsetInPixel), -1 /*near*/, 1));
			*rd = h(_inverseVP * glm::vec4(_i2x((float)x + xoffsetInPixel), _j2y((float)y + yoffsetInPixel), +1 /*far */, 1)) - *ro;
			*rd = glm::normalize(*rd);
		}
	private:
		glm::mat4 _inverseVP;
		LinearTransform _i2x;
		LinearTransform _j2y;
	};
}