#include "prp.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "cwalk.h"

#include <algorithm>
#include <ppl.h>

#include <Windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace pr {
    namespace {
        std::string g_dataPath;
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
    glm::vec2 GenerateUniformInCircle(float u0, float u1) {
        float r = std::sqrt(u0);
        float theta = u1 * glm::pi<float>() * 2.0f;
        float x = r * cos(theta);
        float y = r * sin(theta);
        return { x, y };
    }
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

    // Building an Orthonormal Basis, Revisited
    void GetOrthonormalBasis(glm::vec3 zaxis, glm::vec3 *xaxis, glm::vec3 *yaxis) {
        const float sign = std::copysign(1.0f, zaxis.z);
        const float a = -1.0f / (sign + zaxis.z);
        const float b = zaxis.x * zaxis.y * a;
        *xaxis = glm::vec3(1.0f + sign * zaxis.x * zaxis.x * a, sign * b, -sign * zaxis.x);
        *yaxis = glm::vec3(b, sign + zaxis.y * zaxis.y * a, -zaxis.y);
    }
    glm::vec3 GetCartesian(float theta, float phi) {
        float sinTheta = std::sin(theta);
        return glm::vec3(
            sinTheta * std::cos(phi),
            sinTheta * std::sin(phi),
            std::cos(theta)
        );
    }
    void GetSpherical(glm::vec3 direction, float *theta, float *phi) {
        if (direction.x == 0.0f && direction.y == 0.0f) {
            *theta = 0.0f;
            *phi = 0.0f;
            return;
        }

        float r_xy = std::hypot(direction.x, direction.y);
        *theta = std::atan2(r_xy, direction.z);
        *phi = std::atan2(direction.y, direction.x);
    }

    void Image2DRGBA8::allocate(int w, int h) {
        _width = w;
        _height = h;
        _values.clear();
        _values.resize(_width * _height);
    }
    Result Image2DRGBA8::load(const char *filename) {
        stbi_uc *pixels = stbi_load(GetDataPath(filename).c_str(), &_width, &_height, 0, 4);
        if (pixels == nullptr) {
            return Result::Failure;
        }
        _values.resize(_width * _height);
        memcpy(_values.data(), pixels, _width * _height * 4);
        stbi_image_free(pixels);
        return Result::Sucess;
    }
    Result Image2DRGBA8::load(const uint8_t *data, int bytes) {
        stbi_uc *pixels = stbi_load_from_memory(data, bytes, &_width, &_height, 0, 4);
        if (pixels == nullptr) {
            return Result::Failure;
        }
        _values.resize(_width * _height);
        memcpy(_values.data(), pixels, _width * _height * 4);
        stbi_image_free(pixels);
        return Result::Sucess;
    }
    Result Image2DRGBA8::save(const char* filename) const {
        if (stbi_write_png(GetDataPath(filename).c_str(), width(), height(), 4, _values.data(), 0)) {
            return Result::Sucess;
        }
        return Result::Failure;
    }
    glm::u8vec4 *Image2DRGBA8::data() {
        return _values.data();
    }
    const glm::u8vec4 *Image2DRGBA8::data() const {
        return _values.data();
    }
    int Image2DRGBA8::bytes() const {
        return _width * _height * sizeof(glm::u8vec4);
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
    const glm::u8vec4 &Image2DRGBA8::at(int x, int y) const {
        if (x < 0 || _width < x) {
            throw std::out_of_range("x is out of range");
        }
        if (y < 0 || _height < y) {
            throw std::out_of_range("y is out of range");
        }
        return this->operator()(x, y);
    }
    int Image2DRGBA8::width() const {
        return _width;
    }
    int Image2DRGBA8::height() const {
        return _height;
    }

    // --
    void Image2DMono8::allocate(int w, int h) {
        _width = w;
        _height = h;
        _values.clear();
        _values.resize(_width * _height);
    }
    Result Image2DMono8::load(const char *filename) {
        stbi_uc *pixels = stbi_load(GetDataPath(filename).c_str(), &_width, &_height, 0, 1);
        if (pixels == nullptr) {
            return Result::Failure;
        }
        _values.resize(_width * _height);
        memcpy(_values.data(), pixels, _width * _height);
        stbi_image_free(pixels);
        return Result::Sucess;
    }
    Result Image2DMono8::load(const uint8_t *data, int bytes) {
        stbi_uc *pixels = stbi_load_from_memory(data, bytes, &_width, &_height, 0, 1);
        if (pixels == nullptr) {
            return Result::Failure;
        }
        _values.resize(_width * _height);
        memcpy(_values.data(), pixels, _width * _height);
        stbi_image_free(pixels);
        return Result::Sucess;
    }
    uint8_t *Image2DMono8::data() {
        return _values.data();
    }
    const uint8_t *Image2DMono8::data() const {
        return _values.data();
    }
    int Image2DMono8::bytes() const {
        return _width * _height * sizeof(uint8_t);
    }
    uint8_t &Image2DMono8::operator()(int x, int y) {
        return _values[y * _width + x];
    }
    const uint8_t &Image2DMono8::operator()(int x, int y) const {
        return _values[y * _width + x];
    }
    uint8_t &Image2DMono8::at(int x, int y) {
        if (x < 0 || _width < x) {
            throw std::out_of_range("x is out of range");
        }
        if (y < 0 || _height < y) {
            throw std::out_of_range("y is out of range");
        }
        return (*this)(x, y);
    }
    const uint8_t &Image2DMono8::at(int x, int y) const {
        if (x < 0 || _width < x) {
            throw std::out_of_range("x is out of range");
        }
        if (y < 0 || _height < y) {
            throw std::out_of_range("y is out of range");
        }
        return (*this)(x, y);
    }
    int Image2DMono8::width() const {
        return _width;
    }
    int Image2DMono8::height() const {
        return _height;
    }

	// 32F
	void Image2DRGBA32::allocate(int w, int h) {
		_width = w;
		_height = h;
		_values.clear();
		_values.resize(_width * _height);
	}
	Result Image2DRGBA32::load(const char *filename) {
		float *pixels = stbi_loadf(GetDataPath(filename).c_str(), &_width, &_height, 0, 4);
		if (pixels == nullptr) {
			return Result::Failure;
		}
		_values.resize(_width * _height);
		memcpy(_values.data(), pixels, _width * _height * sizeof(PixelType));
		stbi_image_free(pixels);
		return Result::Sucess;
	}
	Result Image2DRGBA32::load(const uint8_t *data, int bytes) {
		float *pixels = stbi_loadf_from_memory(data, bytes, &_width, &_height, 0, 4);
		if (pixels == nullptr) {
			return Result::Failure;
		}
		_values.resize(_width * _height);
		memcpy(_values.data(), pixels, _width * _height * 4);
		stbi_image_free(pixels);
		return Result::Sucess;
	}
	Result Image2DRGBA32::save(const char* filename) const {
		if (stbi_write_hdr(GetDataPath(filename).c_str(), width(), height(), 4, glm::value_ptr(*_values.data()))) {
			return Result::Sucess;
		}
		return Result::Failure;
	}
	Image2DRGBA32::PixelType *Image2DRGBA32::data() {
		return _values.data();
	}
	const Image2DRGBA32::PixelType *Image2DRGBA32::data() const {
		return _values.data();
	}
	int Image2DRGBA32::bytes() const {
		return _width * _height * sizeof(PixelType);
	}
	Image2DRGBA32::PixelType &Image2DRGBA32::operator()(int x, int y) {
		return _values[y * _width + x];
	}
	const Image2DRGBA32::PixelType &Image2DRGBA32::operator()(int x, int y) const {
		return _values[y * _width + x];
	}
	Image2DRGBA32::PixelType &Image2DRGBA32::at(int x, int y) {
		if (x < 0 || _width < x) {
			throw std::out_of_range("x is out of range");
		}
		if (y < 0 || _height < y) {
			throw std::out_of_range("y is out of range");
		}
		return (*this)(x, y);
	}
	const Image2DRGBA32::PixelType &Image2DRGBA32::at(int x, int y) const {
		if (x < 0 || _width < x) {
			throw std::out_of_range("x is out of range");
		}
		if (y < 0 || _height < y) {
			throw std::out_of_range("y is out of range");
		}
		return this->operator()(x, y);
	}
	int Image2DRGBA32::width() const {
		return _width;
	}
	int Image2DRGBA32::height() const {
		return _height;
	}

	// Simple Linear Translate
	Image2DRGBA32 Image2DRGBA8_to_Image2DRGBA32(const Image2DRGBA8 &src) {
		Image2DRGBA32 image;
		image.allocate(src.width(), src.height());
		for (int j = 0; j < image.height(); ++j)
		{
			for (int i = 0; i < image.width(); ++i)
			{
				image(i, j) = glm::vec4(src(i, j)) / glm::vec4(255.0f);
			}
		}
		return image;
	}

    std::string NormalizePath(std::string path) {
        std::size_t normLength = cwk_path_normalize(path.data(), 0, 0);
        std::vector<char> normname(normLength + 1);
        cwk_path_normalize(path.data(), normname.data(), normname.size());
        return normname.data();
    }
    std::string ExecutableDir() {
        char *p;
        _get_pgmptr(&p);

        std::size_t dirLength;
        cwk_path_get_dirname(p, &dirLength);

        std::vector<char> dirname(dirLength + 1);
        memcpy(dirname.data(), p, dirLength);

        return NormalizePath(dirname.data());
    }
    std::string JoinPath(std::string a, std::string b) {
        std::size_t joinLength = cwk_path_join(a.c_str(), b.c_str(), 0, 0);
        std::vector<char> joined(joinLength + 1);
        cwk_path_join(a.c_str(), b.c_str(), joined.data(), joined.size());
        return NormalizePath(joined.data());
    }
    std::string JoinPath(std::string a, std::string b, std::string c) 
    {
        return JoinPath(JoinPath(a, b), c);
    }
    std::string JoinPath(std::string a, std::string b, std::string c, std::string d)
    {
        return JoinPath(JoinPath(a, b, c), d);
    }
    std::string JoinPath(std::string a, std::string b, std::string c, std::string d, std::string e)
    {
        return JoinPath(JoinPath(a, b, c, d), e);
    }

    void SetDataDir(std::string dir) {
        g_dataPath = NormalizePath(dir);
    }
    std::string GetDataPath(std::string filename) {
        return NormalizePath(JoinPath(g_dataPath, filename));
    }
    void ParallelFor(int n, std::function<void(int)> f /* f(index) */) {
        concurrency::parallel_for(0, n, f);
    }
    void SerialFor(int n, std::function<void(int)> f /* f(index) */) {
        for (int i = 0; i < n; ++i) {
            f(i);
        }
    }

    std::wstring string_to_wstring(const std::string& s)
    {
        int in_length = (int)s.length();
        int out_length = MultiByteToWideChar(CP_ACP, 0, s.c_str(), in_length, 0, 0);
        std::vector<wchar_t> buffer(out_length);
        if (out_length) {
            MultiByteToWideChar(CP_ACP, 0, s.c_str(), in_length, &buffer[0], out_length);
        }
        return std::wstring(buffer.begin(), buffer.end());
    }
    std::string wstring_to_string(const std::wstring& s)
    {
        int in_length = (int)s.length();
        int out_length = WideCharToMultiByte(CP_ACP, 0, s.c_str(), in_length, 0, 0, 0, 0);
        std::vector<char> buffer(out_length);
        if (out_length) {
            WideCharToMultiByte(CP_ACP, 0, s.c_str(), in_length, &buffer[0], out_length, 0, 0);
        }
        return std::string(buffer.begin(), buffer.end());
    }

    Result BinaryLoader::load(const char* file)
    {
        FILE* fp = fopen(GetDataPath(file).c_str(), "rb");
        if (fp == nullptr)
        {
            return Result::Failure;
        }

        fseek(fp, 0, SEEK_END);

        _data.resize(ftell(fp));

        fseek(fp, 0, SEEK_SET);

        size_t s = fread(_data.data(), 1, _data.size(), fp);
        if (s != _data.size())
        {
            _data.clear();
            return Result::Failure;
        }

        fclose(fp);
        fp = nullptr;
        return Result::Sucess;
    }
}