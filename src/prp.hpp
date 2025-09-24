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

    struct PCG : public IRandomNumberGenerator
	{
		PCG();
		PCG( uint64_t seed, uint64_t sequence /*0 to 0x7FFFFFFFFFFFFFFF */ );

		float uniformf() override;
		uint64_t uniformi() override;

        uint32_t uniform();

        uint64_t state; // RNG state.  All values are possible.
		uint64_t inc;	// Controls which RNG sequence(stream) is selected. Must
						// *always* be odd.
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

    template <class F>
	void drawLineDDA( glm::vec2 a, glm::vec2 b, F putPixel /* ( int x, int y ) */ )
	{
		auto abs_of = []( int x )
		{ return x < 0 ? -x : x; };
		auto floor_of = []( float x )
		{
			float d;
			_mm_store_ss( &d, _mm_floor_ss( _mm_setzero_ps(), _mm_set_ss( x ) ) );
			return d;
		};
		int x1 = floor_of( a.x );
		int y1 = floor_of( a.y );
		int x2 = floor_of( b.x );
		int y2 = floor_of( b.y );

		float dx = ( x2 - x1 );
		float dy = ( y2 - y1 );

		int abs_dx = abs_of( dx );
		int abs_dy = abs_of( dy );
		int step = abs_dy <= abs_dx ? abs_dx : abs_dy;

		dx = (float)dx / step;
		dy = (float)dy / step;

		for( int i = 0; i <= step; i++ )
		{
			int x = floor_of( x1 + dx * i );
			int y = floor_of( y1 + dy * i );
			putPixel( x, y );
		}
	}
    template <class IMAGE, class C>
	void drawLineDDA( IMAGE* image, glm::vec2 a, glm::vec2 b, C color )
    {
		drawLineDDA(a, b, [image, color](int x, int y) {
            if( 0 <= x && x < image->width() && 0 <= y && y < image->height() )
            {
                ( *image )( x, y ) = color;
            }
        });
    }

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

    // https://www.shadertoy.com/view/WlfXRN
    inline glm::vec3 viridis( float t )
	{
		t = glm::clamp( t, 0.0f, 1.0f );
		const glm::vec3 c0 = glm::vec3( 0.2777273272234177f, 0.005407344544966578f, 0.3340998053353061f );
		const glm::vec3 c1 = glm::vec3( 0.1050930431085774f, 1.404613529898575f, 1.384590162594685f );
		const glm::vec3 c2 = glm::vec3( -0.3308618287255563f, 0.214847559468213f, 0.09509516302823659f );
		const glm::vec3 c3 = glm::vec3( -4.634230498983486f, -5.799100973351585f, -19.33244095627987f );
		const glm::vec3 c4 = glm::vec3( 6.228269936347081f, 14.17993336680509f, 56.69055260068105f );
		const glm::vec3 c5 = glm::vec3( 4.776384997670288f, -13.74514537774601f, -65.35303263337234f );
		const glm::vec3 c6 = glm::vec3( -5.435455855934631f, 4.645852612178535f, 26.3124352495832f );

		return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
	}

	inline glm::vec3 plasma( float t )
	{
		t = glm::clamp( t, 0.0f, 1.0f );
		const glm::vec3 c0 = glm::vec3( 0.05873234392399702f, 0.02333670892565664f, 0.5433401826748754f );
		const glm::vec3 c1 = glm::vec3( 2.176514634195958f, 0.2383834171260182f, 0.7539604599784036f );
		const glm::vec3 c2 = glm::vec3( -2.689460476458034f, -7.455851135738909f, 3.110799939717086f );
		const glm::vec3 c3 = glm::vec3( 6.130348345893603f, 42.3461881477227f, -28.51885465332158f );
		const glm::vec3 c4 = glm::vec3( -11.10743619062271f, -82.66631109428045f, 60.13984767418263f );
		const glm::vec3 c5 = glm::vec3( 10.02306557647065f, 71.41361770095349f, -54.07218655560067f );
		const glm::vec3 c6 = glm::vec3( -3.658713842777788f, -22.93153465461149f, 18.19190778539828f );

		return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
	}
}