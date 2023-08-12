#pragma once

/*
 Potable Modules
 You can use this as also itself only. but usually #include "pr.hpp" is an easy way.
*/

#include <vector>
#include <functional>
#include <string>
#include <stdexcept>
#include <functional>
#include <random>
#include <cmath>
#include <algorithm>
#include <chrono>

#include "glm/glm.hpp"
#include "glm/ext.hpp"

// #define TRIVIAL_VECTOR_ENABLE_OUT_OF_BOUNDS_DETECTION

#if defined(TRIVIAL_VECTOR_ENABLE_OUT_OF_BOUNDS_DETECTION)
#include <stdexcept>
#include <inttypes.h>
#endif

namespace pr {
    template <class T>
    class trivial_vector
    {
    public:
        trivial_vector()
        {

        }
        trivial_vector(int64_t n):_size(n), _capacity(n), _data((T *)malloc(sizeof(T) * n))
        {
        }
        trivial_vector(const trivial_vector<T>& rhs)
        {
            resize(rhs.size());
            std::copy(rhs.begin(), rhs.end(), begin());
        }
        trivial_vector(trivial_vector<T>&& rhs) noexcept
        {
            _data = rhs._data;
            _capacity = rhs._capacity;
            _size = rhs._size;
            rhs._data = nullptr;
            rhs._capacity = 0;
            rhs._size = 0;
        }
        ~trivial_vector()
        {
            if (_data)
            {
                free(_data);
            }
        }
        trivial_vector<T>& operator=(const trivial_vector<T>& rhs)
        {
            resize(rhs.size());
            std::copy(rhs.begin(), rhs.end(), begin());
            return *this;
        }
        trivial_vector<T>& operator=(trivial_vector<T>&& rhs) noexcept
        {
            _data = rhs._data;
            _capacity = rhs._capacity;
            _size = rhs._size;
            rhs._data = nullptr;
            rhs._capacity = 0;
            rhs._size = 0;
            return *this;
        }

        const T &operator[](int64_t i) const {
#if defined(TRIVIAL_VECTOR_ENABLE_OUT_OF_BOUNDS_DETECTION)
            if (i < 0 || _size <= i)
            {
                printf("out of bounds! trivial_vector[%" PRId64 "] (size=%" PRId64 ")", i, _size);
                abort();
            }
#endif
            return _data[i];
        }
        T& operator[](int64_t i) {
#if defined(TRIVIAL_VECTOR_ENABLE_OUT_OF_BOUNDS_DETECTION)
            if (i < 0 || _size <= i)
            {
                printf("out of bounds! trivial_vector[%" PRId64 "] (size=%" PRId64 ")", i, _size);
                abort();
            }
#endif
            return _data[i];
        }

        T* begin() { return _data; }
        T* end() { return _data + _size; }
        const T* begin() const { return _data; }
        const T* end() const  { return _data + _size; }
        T* data() { return _data; }
        const T* data() const { return _data; }

        void clear()
        {
            _size = 0;
        }
        void reserve(int64_t n)
        {
            if( n <= _capacity )
            {
                return;
            }

            T* newPtr = (T *)realloc(_data, sizeof(T) * n);
            if( newPtr == nullptr )
            {
                return;
            }

            _data = newPtr;
            _capacity = n;
        }
        void resize(int64_t n)
        {
            if( n <= _capacity)
            {
                _size = n;
                return;
            }

            T* newPtr = (T*)realloc(_data, sizeof(T) * n);
            if (newPtr == nullptr)
            {
                return;
            }
            _data = newPtr;
            _capacity = n;
            _size = n;
        }
        void shrink_to_fit()
        {
            if (_size == _capacity)
            {
                return;
            }
            if (_size == 0)
            {
                free(_data);
                _data = 0;
                _capacity = 0;
                return;
            }

            T* newPtr = (T*)realloc(_data, sizeof(T) * _size);
            if (newPtr == nullptr)
            {
                return;
            }
            _data = newPtr;
            _capacity = _size;
        }
        void push_back( const T &value )
        {
            if( _capacity == _size )
            {
                int64_t n = std::max( _capacity * 2, (int64_t)1 );
                T* newPtr = (T*)realloc(_data, sizeof(T) * n);
                if (newPtr == nullptr)
                {
                    return;
                }
                _data = newPtr;
                _capacity = n;
            }
            _data[_size] = value;
            _size++;
        }
        int64_t size() const { return _size; }
        int64_t bytes() const { return _size * sizeof(T); }
        int64_t capacity() const { return _capacity; }
        bool empty() const { return _size == 0; }
    private:
        int64_t _size = 0;
        int64_t _capacity = 0;
        T* _data = nullptr;
    };

    class Stopwatch {
    public:
        using clock = std::chrono::steady_clock;
        Stopwatch() :_started(clock::now()) {}

        // seconds
        double elapsed() const {
            auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - _started).count();
            return (double)microseconds * 0.001 * 0.001;
        }
    private:
        clock::time_point _started;
    };

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
		Result saveAsPng( const char* filename ) const;
		Result saveAsPngUncompressed( const char* filename ) const;

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

		Result saveAsPng(const char* filename) const;

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

		Result loadFromHDR(const char *filename);
		Result loadFromHDR(const uint8_t *data, int bytes);

        // load default layer or first layer
        Result loadFromEXR(const char* filename);

        // load specific layer. layer = "" means default layer
        Result loadFromEXR(const char* filename, const char* layer);

        Result saveAsHDR(const char* filename) const;
        Result saveAsEXR(const char* filename) const;

		PixelType *data();
		const PixelType *data() const;
		int bytes() const;

		PixelType &operator()(int x, int y);
		const PixelType &operator()(int x, int y) const;

		PixelType &at(int x, int y);
		const PixelType &at(int x, int y) const;

		int width() const;
		int height() const;

        Image2DRGBA32 map(std::function<PixelType(PixelType)> f) const;
	private:
		int _width = 0;
		int _height = 0;
		std::vector<PixelType> _values;
	};

    void SetEnableMultiThreadExrProceccing( bool enabled );

    // enum layers. "" means default layer.
    Result LayerListFromEXR( std::vector<std::string>& list, const char* filename );

    class MultiLayerExrWriter
    {
    public:
        void add(const Image2DRGBA32* image, std::string layer) { _images.push_back(image); _layers.push_back(layer); }
        Result saveAs( const char* filename );
    private:
        std::vector<const Image2DRGBA32*> _images;
        std::vector<std::string> _layers;
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
    std::string GetPathBasenameWithoutExtension(std::string path);
    std::string GetPathDirname(std::string path);
    std::string GetPathExtension(std::string path);
    std::string ChangePathExtension(std::string path, std::string newExtension);
    bool IsPathAbsolute(std::string path);

    std::string GetDataPath(std::string filename);

    void ParallelFor(int n, std::function<void(int)> f /* f(index) */);
    void SerialFor(int n, std::function<void(int)> f /* f(index) */);

#ifdef _WIN32
    // warning: we assume "std::string s" use the current code page (CP_ACP)
    std::wstring string_to_wstring(const std::string& s);
    std::string wstring_to_string(const std::wstring& s);
#endif

    void SleepForMilliSeconds(int milliseconds);
    void SleepForSeconds(int seconds);

    // xxhash
    uint32_t xxhash32( const void* input, size_t length, uint32_t seed );
    uint64_t xxhash64( const void* input, size_t length, uint32_t seed );

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
		CameraRayGenerator(glm::mat4 viewMatrix, glm::mat4 projMatrix, int width, int height):_width(width), _height(height)
		{
			glm::mat4 vp = projMatrix * viewMatrix;

            auto h = [](glm::dvec4 v) {
                return glm::dvec3(v / v.w);
            };

            glm::dmat4 inverseVP = glm::inverse(glm::dmat4(vp));
            glm::dvec3 nearO = h(inverseVP * glm::vec4(-1, +1, -1 /*near*/, 1));
            _nearO = nearO;
            _nearRight = h(inverseVP * glm::vec4(+1, +1, -1 /*near*/, 1)) - nearO;
            _nearDown = h(inverseVP * glm::vec4(-1, -1, -1 /*near*/, 1)) - nearO;

            glm::dvec3 farO = h(inverseVP * glm::vec4(-1, +1, +1 /*far*/, 1));
            _farO = farO;
            _farRight = h(inverseVP * glm::vec4(+1, +1, +1 /*far*/, 1)) - farO;
            _farDown = h(inverseVP * glm::vec4(-1, -1, +1 /*far*/, 1)) - farO;
		}
        /*
        a----b----+
        |    |    |
        |    |    |
        c----d----+
        |    |    |
        |    |    |
        +----+----+

        x: 0, y: 0 => shoot from a
        x: 1, y: 0 => shoot from b
        x: 0, y: 1 => shoot from c
        x: 1, y: 1 => shoot from d

        a------------------
        |    |
        |    |
        |    v
        |--->(xoffsetInPixel, yoffsetInPixel)
        |
        |
        */
		void shoot(glm::vec3 *ro, glm::vec3 *rd, int x, int y, float xoffsetInPixel = 0.0f, float yoffsetInPixel = 0.0f) const {

            float xf = (x + xoffsetInPixel) / _width;
            float yf = (y + yoffsetInPixel) / _height;
            glm::vec3 near = _nearO + _nearRight * xf + _nearDown * yf;
            glm::vec3 far = _farO + _farRight * xf + _farDown * yf;
            *ro = near;
            *rd = far - near;
		}
	public:
        int _width, _height;
        glm::vec3 _nearO;
        glm::vec3 _nearRight;
        glm::vec3 _nearDown;
        glm::vec3 _farO;
        glm::vec3 _farRight;
        glm::vec3 _farDown;
	};

    template <class T>
    class OnlineMean {
    public:
        void addSample(T newValue) {
            _count++;
            auto delta = newValue - _mean;
            _mean += delta / _count;
        }
        T mean() const {
            return _mean;
        }
        int sampleCount() const {
            return _count;
        }
    private:
        int _count = 0;
        T _mean = T(0.0);
    };

    template <class T>
    class OnlineVariance {
    public:
        void addSample(T newValue) {
            _count++;
            auto delta = newValue - _mean;
            _mean += delta / _count;
            auto delta2 = newValue - _mean;
            _M2 += delta * delta2;
        }
        T mean() const {
            return _mean;
        }
        T sampleVariance() const {
            if ( _count <= 0 )
            {
                return T(0.0);
            }
            return _M2 / _count;
        }
        T unbiasedVariance() const {
            if (_count <= 1)
            {
                return T(0.0);
            }
            return _M2 / (_count - 1);
        }
        int sampleCount() const {
            return _count;
        }
    private:
        int _count = 0;
        T _mean = T(0.0);
        T _M2 = T(0.0);
    };
}