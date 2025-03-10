﻿#include "prp.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "libattopng.h"

#include "ImfHeader.h"
#include "ImfInputFile.h"
#include "ImfChannelList.h"
#include "ImfOutputFile.h"
#include "ImfFrameBuffer.h"

#include "cwalk.h"
#include "xxhash.h"

#include <algorithm>
#include <set>
#include <mutex>

#ifdef _WIN32
    #define NOMINMAX
    #include <ppl.h>
    #include <Windows.h>
#else
    #include <unistd.h>
    #include <linux/limits.h>
    // https://stackoverflow.com/questions/23943239/how-to-get-path-to-current-exe-file-on-linux
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
        // ver 1.1
        // https://prng.di.unimi.it/xoshiro128starstar.c
        uint32_t rotl(const uint32_t x, int k) {
            return (x << k) | (x >> (32 - k));
        }
        uint32_t Xoshiro128StarStarNext(Xoshiro128StarStar &state) {
			const uint32_t result = rotl( state.s[1] * 5, 7 ) * 9;

			const uint32_t t = state.s[1] << 9;

			state.s[2] ^= state.s[0];
			state.s[3] ^= state.s[1];
			state.s[1] ^= state.s[2];
			state.s[0] ^= state.s[3];

			state.s[2] ^= t;

			state.s[3] = rotl( state.s[3], 11 );

			return result;
        }
    }

    float Xoshiro128StarStar::uniformf() {
        uint32_t x = Xoshiro128StarStarNext(*this);
		uint32_t bits = ( x >> 9 ) | 0x3f800000;
		float value;
		memcpy( &value, &bits, sizeof( float ) );
		return value - 1.0f;
    }

    uint64_t Xoshiro128StarStar::uniformi() {
        uint64_t a = Xoshiro128StarStarNext(*this);
        uint64_t b = Xoshiro128StarStarNext(*this);
        return (a << 32) | b;
    }

    MersenneTwister::MersenneTwister() : MersenneTwister(953687342) {
    }
    MersenneTwister::MersenneTwister(uint32_t seed) : s(seed) {
    }
    float MersenneTwister::uniformf() {
        std::uniform_real_distribution<float> d;
        return d(s);
    }

    uint64_t MersenneTwister::uniformi() {
        uint64_t a = s();
        uint64_t b = s();
        return (a << 32) | b;
    }

    PCG::PCG()
    {
		state = 0x853c49e6748fea9bULL;
		inc = 0xda3e39cb94b95bdbULL;
    }
    PCG::PCG(uint64_t seed, uint64_t sequence)
    {
		state = 0U;
		inc = ( sequence << 1u ) | 1u;

		uniform();
		state += seed;
		uniform();
    }
	uint32_t PCG::uniform()
	{
		uint64_t oldstate = state;
		state = oldstate * 6364136223846793005ULL + inc;
		uint32_t xorshifted = ( ( oldstate >> 18u ) ^ oldstate ) >> 27u;
		uint32_t rot = oldstate >> 59u;
		return ( xorshifted >> rot ) | ( xorshifted << ( ( -rot ) & 31 ) );
	}
    float PCG::uniformf()
    {
		uint32_t bits = ( uniform() >> 9 ) | 0x3f800000;
		float value;
		memcpy( &value, &bits, sizeof( float ) );
		return value - 1.0f;
    }
    uint64_t PCG::uniformi()
    {
		uint64_t a = uniform();
		uint64_t b = uniform();
		return ( a << 32 ) | b;
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
        std::string fullPath = GetDataPath(filename);
        if( stbi_is_hdr(fullPath.c_str()) )
        {
            return Result::Failure;
        }
        stbi_uc *pixels = stbi_load(fullPath.c_str(), &_width, &_height, 0, 4);
        if (pixels == nullptr) {
            return Result::Failure;
        }
        _values.resize(_width * _height);
        memcpy(_values.data(), pixels, _width * _height * 4);
        stbi_image_free(pixels);
        return Result::Sucess;
    }
    Result Image2DRGBA8::load(const uint8_t *data, int bytes) {
        if (stbi_is_hdr_from_memory(data, bytes))
        {
            return Result::Failure;
        }
        stbi_uc *pixels = stbi_load_from_memory(data, bytes, &_width, &_height, 0, 4);
        if (pixels == nullptr) {
            return Result::Failure;
        }
        _values.resize(_width * _height);
        memcpy(_values.data(), pixels, _width * _height * 4);
        stbi_image_free(pixels);
        return Result::Sucess;
    }
    Result Image2DRGBA8::saveAsPng( const char* filename ) const {
		if( stbi_write_png( GetDataPath( filename ).c_str(), width(), height(), 4, _values.data(), 0 ) )
		{
			return Result::Sucess;
        }
        return Result::Failure;
    }
	Result Image2DRGBA8::saveAsPngUncompressed( const char* filename ) const
    {
		libattopng_t* png = libattopng_new( width(), height(), PNG_RGBA, 0 );
		png->data = (char*)_values.data();
		bool success = libattopng_save( png, GetDataPath( filename ).c_str() ) == 0;
		png->data = nullptr;
		libattopng_destroy( png );
		return success ? Result::Sucess : Result::Failure;
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
        std::string fullPath = GetDataPath(filename);
        if (stbi_is_hdr(fullPath.c_str()))
        {
            return Result::Failure;
        }
        stbi_uc *pixels = stbi_load(fullPath.c_str(), &_width, &_height, 0, 1);
        if (pixels == nullptr) {
            return Result::Failure;
        }
        _values.resize(_width * _height);
        memcpy(_values.data(), pixels, _width * _height);
        stbi_image_free(pixels);
        return Result::Sucess;
    }
    Result Image2DMono8::load(const uint8_t *data, int bytes) {
        if (stbi_is_hdr_from_memory(data, bytes))
        {
            return Result::Failure;
        }
        stbi_uc *pixels = stbi_load_from_memory(data, bytes, &_width, &_height, 0, 1);
        if (pixels == nullptr) {
            return Result::Failure;
        }
        _values.resize(_width * _height);
        memcpy(_values.data(), pixels, _width * _height);
        stbi_image_free(pixels);
        return Result::Sucess;
    }
	Result Image2DMono8::saveAsPng(const char* filename) const {
		if (stbi_write_png(GetDataPath(filename).c_str(), width(), height(), 1, _values.data(), 0)) {
			return Result::Sucess;
		}
		return Result::Failure;
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
		_values.resize( _width * _height, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) );
	}
	Result Image2DRGBA32::loadFromHDR(const char *filename) {
        std::string fullPath = GetDataPath(filename);
        if (stbi_is_hdr(fullPath.c_str()) == false)
        {
            return Result::Failure;
        }
		float *pixels = stbi_loadf(GetDataPath(filename).c_str(), &_width, &_height, 0, 4);
		if (pixels == nullptr) {
			return Result::Failure;
		}
		_values.resize(_width * _height);
		memcpy(_values.data(), pixels, _width * _height * sizeof(PixelType));
		stbi_image_free(pixels);
		return Result::Sucess;
	}
	Result Image2DRGBA32::loadFromHDR(const uint8_t *data, int bytes) {
        if (stbi_is_hdr_from_memory(data, bytes) == false)
        {
            return Result::Failure;
        }
		float *pixels = stbi_loadf_from_memory(data, bytes, &_width, &_height, 0, 4);
		if (pixels == nullptr) {
			return Result::Failure;
		}
		_values.resize(_width * _height);
		memcpy(_values.data(), pixels, _width * _height * 4 * sizeof(float));
		stbi_image_free(pixels);
		return Result::Sucess;
	}
    Result Image2DRGBA32::loadFromEXR(const char* filename)
    {
        std::vector<std::string> layers;
		pr::LayerListFromEXR( layers, filename );
		if( layers.empty() )
		{
            return Result::Failure;
		}
		return loadFromEXR( filename, layers[0].c_str() );
    }
    Result Image2DRGBA32::loadFromEXR(const char* filename, const char* layer)
    {
        std::string fullPath = GetDataPath(filename);
        using namespace OPENEXR_IMF_NAMESPACE;
        using namespace IMATH_NAMESPACE;
        try
        {
		    InputFile file( fullPath.c_str() );
            const ChannelList& channels = file.header().channels();

            ChannelList::ConstIterator layerBegin, layerEnd;
			
            ChannelList defaultChannelList;
            if( strlen( layer ) == 0 ) // default layer
            {
				if( auto bChannel = channels.findChannel( "B" ) )
				{
					defaultChannelList.insert( "B", *bChannel );
				}
				if( auto gChannel = channels.findChannel( "G" ) )
				{
					defaultChannelList.insert( "G", *gChannel );
				}
				if( auto rChannel = channels.findChannel( "R" ) )
				{
					defaultChannelList.insert( "R", *rChannel );
				}
				if( auto aChannel = channels.findChannel( "A" ) )
				{
					defaultChannelList.insert( "A", *aChannel );
				}
                layerBegin = defaultChannelList.begin();
                layerEnd = defaultChannelList.end();
            }
            else
            {
				channels.channelsInLayer( layer, layerBegin, layerEnd );
            }

			if( layerBegin == layerEnd )
			{
                return Result::Failure;
			}

			Box2i dw = file.header().dataWindow();
			_width = dw.max.x - dw.min.x + 1;
			_height = dw.max.y - dw.min.y + 1;
			int dx = dw.min.x;
			int dy = dw.min.y;

            _values.resize(_width * _height, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

            float* rHead = glm::value_ptr(_values[dy * _width + dx]);
            float* gHead = rHead + 1;
            float* bHead = rHead + 2;
            float* aHead = rHead + 3;
        
            FrameBuffer frameBuffer;

            int channelIndex = 0;
		    for( auto it = layerBegin; it != layerEnd; ++it )
		    {
			    std::string channelName( it.name() );
			    if( channelName.length() == 0 )
			    {
				    continue;
			    }

			    char tail = std::toupper( channelName.back() );
			    switch( tail )
			    {
			    case 'R':
                case 'X':
                case 'U':
				    frameBuffer.insert( channelName,
									    Slice(
										    OPENEXR_IMF_NAMESPACE::FLOAT,
										    (char*)rHead,
										    sizeof( glm::vec4 ) * 1,	   // x stride
										    sizeof( glm::vec4 ) * _width ) // y stride
				    );
				    break;
			    case 'G':
                case 'Y':
                case 'V':
				    frameBuffer.insert( channelName,
									    Slice(
										    OPENEXR_IMF_NAMESPACE::FLOAT,
										    (char*)gHead,
										    sizeof( glm::vec4 ) * 1,	   // x stride
										    sizeof( glm::vec4 ) * _width ) // y stride
				    );
				    break;
                case 'B':
                case 'Z':
				    frameBuffer.insert( channelName,
									    Slice(
										    OPENEXR_IMF_NAMESPACE::FLOAT,
										    (char*)bHead,
										    sizeof( glm::vec4 ) * 1,	   // x stride
										    sizeof( glm::vec4 ) * _width ) // y stride
				    );
				    break;
			    case 'A':
                case 'W':
				    frameBuffer.insert( channelName,
									    Slice(
										    OPENEXR_IMF_NAMESPACE::FLOAT,
										    (char*)aHead,
										    sizeof( glm::vec4 ) * 1,	   // x stride
										    sizeof( glm::vec4 ) * _width ) // y stride
				    );
				    break;
                default:
				    if( channelIndex < 4 )
				    {
					    frameBuffer.insert( channelName,
										    Slice(
											    OPENEXR_IMF_NAMESPACE::FLOAT,
											    (char*)( rHead + 3 - channelIndex ),
											    sizeof( glm::vec4 ) * 1,	   // x stride
											    sizeof( glm::vec4 ) * _width ) // y stride
					    );
				    }
                    break;
			    }

                channelIndex++;
		    }

		    file.setFrameBuffer( frameBuffer );
		    file.readPixels( dw.min.y, dw.max.y );

        }
        catch (std::exception& e)
        {
            // printf( "%s\n", e.what() );
            return Result::Failure;
        }

		return Result::Sucess;
	}
	void SetEnableMultiThreadExrProceccing( bool enabled )
	{
		if( enabled )
		{
			OPENEXR_IMF_NAMESPACE::setGlobalThreadCount( std::thread::hardware_concurrency() );
		}
		else
		{
			OPENEXR_IMF_NAMESPACE::setGlobalThreadCount( 0 );
		}
    }
	Result LayerListFromEXR( std::vector<std::string>& list, const char* filename )
	{
		std::string fullPath = GetDataPath( filename );
		using namespace OPENEXR_IMF_NAMESPACE;
		using namespace IMATH_NAMESPACE;

		try
		{
			InputFile file( fullPath.c_str() );
			const ChannelList& channels = file.header().channels();
			std::set<std::string> layerNames;
			channels.layers( layerNames );

			list = std::vector<std::string>( layerNames.begin(), layerNames.end() );
			if( channels.findChannel( "R" ) )
			{
				list.insert( list.begin(), "" );
			}
		}
		catch( std::exception& e )
		{
			// printf( "%s\n", e.what() );
			return Result::Failure;
		}

		return Result::Sucess;
	}
	Result Image2DRGBA32::saveAsHDR(const char* filename) const {
		if (stbi_write_hdr(GetDataPath(filename).c_str(), width(), height(), 4, glm::value_ptr(*_values.data()))) {
			return Result::Sucess;
		}
		return Result::Failure;
	}
    Result Image2DRGBA32::saveAsEXR(const char* filename) const
    {
        using namespace OPENEXR_IMF_NAMESPACE;
        using namespace IMATH_NAMESPACE;

        try
        {
		    Header header( _width, _height );

		    header.compression() = OPENEXR_IMF_NAMESPACE::ZIPS_COMPRESSION;

		    header.channels().insert( "R", Channel( OPENEXR_IMF_NAMESPACE::FLOAT ) );
		    header.channels().insert( "G", Channel( OPENEXR_IMF_NAMESPACE::FLOAT ) );
		    header.channels().insert( "B", Channel( OPENEXR_IMF_NAMESPACE::FLOAT ) );
		    header.channels().insert( "A", Channel( OPENEXR_IMF_NAMESPACE::FLOAT ) );

		    const float* rHead = glm::value_ptr( _values[0] );
		    const float* gHead = rHead + 1;
		    const float* bHead = rHead + 2;
		    const float* aHead = rHead + 3;

		    FrameBuffer frameBuffer;
		    frameBuffer.insert( "A",
							    Slice(
								    OPENEXR_IMF_NAMESPACE::FLOAT,
								    (char*)aHead,
								    sizeof( glm::vec4 ) * 1,	   // x stride
								    sizeof( glm::vec4 ) * _width ) // y stride
		    );
		    frameBuffer.insert( "B",
							    Slice(
								    OPENEXR_IMF_NAMESPACE::FLOAT,
								    (char*)bHead,
								    sizeof( glm::vec4 ) * 1,	   // x stride
								    sizeof( glm::vec4 ) * _width ) // y stride
		    );
		    frameBuffer.insert( "G",
							    Slice(
								    OPENEXR_IMF_NAMESPACE::FLOAT,
								    (char*)gHead,
								    sizeof( glm::vec4 ) * 1,	   // x stride
								    sizeof( glm::vec4 ) * _width ) // y stride
		    );
		    frameBuffer.insert( "R",
							    Slice(
								    OPENEXR_IMF_NAMESPACE::FLOAT,
								    (char*)rHead,
								    sizeof( glm::vec4 ) * 1,	   // x stride
								    sizeof( glm::vec4 ) * _width ) // y stride
		    );

		    OutputFile file( GetDataPath( filename ).c_str(), header );
		    file.setFrameBuffer( frameBuffer );
		    file.writePixels( _height );
        }
        catch (std::exception& e)
        {
            // printf( "%s\n", e.what() );
            return Result::Failure;
        }

        return Result::Sucess;
    }
    Result MultiLayerExrWriter::saveAs( const char* filename )
    {
        using namespace OPENEXR_IMF_NAMESPACE;
        using namespace IMATH_NAMESPACE;

        std::vector<const Image2DRGBA32*> images = _images;
        std::vector<std::string> layers = _layers;

        if( images.empty() )
		{
			return Result::Failure;
		}
		if( images.size() != layers.size() )
		{
			return Result::Failure;
		}
        int width = images[0]->width();
        int height = images[0]->height();
		if( width == 0 || height == 0 )
        {
            return Result::Failure;
        }
		for( const Image2DRGBA32* image : images )
		{
			if( image->width() != width || image->height() != height )
			{
				return Result::Failure;
			}
		}

		try
        {
            Header header(width, height);

            header.compression() = OPENEXR_IMF_NAMESPACE::ZIPS_COMPRESSION;

            FrameBuffer frameBuffer;

            for (int i = 0; i < layers.size(); ++i)
            {
                auto join = [](std::string a, std::string b)
                {
                    return a == "" ? b : ( a + "." + b );
                };
                std::string R = join(layers[i], "R");
                std::string G = join(layers[i], "G");
                std::string B = join(layers[i], "B");
                std::string A = join(layers[i], "A");

                header.channels().insert(A.c_str(), Channel(OPENEXR_IMF_NAMESPACE::FLOAT));
                header.channels().insert(B.c_str(), Channel(OPENEXR_IMF_NAMESPACE::FLOAT));
                header.channels().insert(G.c_str(), Channel(OPENEXR_IMF_NAMESPACE::FLOAT));
                header.channels().insert(R.c_str(), Channel(OPENEXR_IMF_NAMESPACE::FLOAT));

                const float* rHead = glm::value_ptr(*images[i]->data());
                const float* gHead = rHead + 1;
                const float* bHead = rHead + 2;
                const float* aHead = rHead + 3;

                frameBuffer.insert(A.c_str(),
                    Slice(
                        OPENEXR_IMF_NAMESPACE::FLOAT,
                        (char*)aHead,
                        sizeof(glm::vec4) * 1,	   // x stride
                        sizeof(glm::vec4) * width) // y stride
                );
                frameBuffer.insert(B.c_str(),
                    Slice(
                        OPENEXR_IMF_NAMESPACE::FLOAT,
                        (char*)bHead,
                        sizeof(glm::vec4) * 1,	   // x stride
                        sizeof(glm::vec4) * width) // y stride
                );
                frameBuffer.insert(G.c_str(),
                    Slice(
                        OPENEXR_IMF_NAMESPACE::FLOAT,
                        (char*)gHead,
                        sizeof(glm::vec4) * 1,	   // x stride
                        sizeof(glm::vec4) * width) // y stride
                );
                frameBuffer.insert(R.c_str(),
                    Slice(
                        OPENEXR_IMF_NAMESPACE::FLOAT,
                        (char*)rHead,
                        sizeof(glm::vec4) * 1,	   // x stride
                        sizeof(glm::vec4) * width) // y stride
                );
            }


            OutputFile file(GetDataPath(filename).c_str(), header);
            file.setFrameBuffer(frameBuffer);
            file.writePixels(height);
        }
        catch (std::exception& e)
        {
            // printf( "%s\n", e.what() );
            return Result::Failure;
        }
        return Result::Sucess;
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
    Image2DRGBA32 Image2DRGBA32::map(std::function<PixelType(PixelType)> f) const
    {
        Image2DRGBA32 image;
        image.allocate(width(), height());
        for (int j = 0; j < image.height(); ++j)
        {
            for (int i = 0; i < image.width(); ++i)
            {
                image(i, j) = f((*this)(i, j));
            }
        }
        return image;
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
#ifdef _WIN32
        char *p;
        _get_pgmptr(&p);
#else
        char p[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", p, PATH_MAX);
#endif
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

    std::string GetPathBasename(std::string path)
    {
        const char* basename = nullptr;
        size_t length = 0;
        cwk_path_get_basename(path.c_str(), &basename, &length);
        return std::string(basename);
    }
    std::string GetPathBasenameWithoutExtension(std::string path)
    {
        std::string base = GetPathBasename(path);
        std::string e = GetPathExtension(path);
        return base.substr(0, base.length() - e.length());
    }
    std::string GetPathDirname(std::string path)
    {
        size_t length = 0;
        cwk_path_get_dirname(path.c_str(), &length);
        std::vector<char> dirname(length + 1);
        memcpy(dirname.data(), path.c_str(), length);
        return std::string(dirname.data());
    }
    std::string GetPathExtension(std::string path)
    {
        const char* ext = nullptr;
        size_t length = 0;
        cwk_path_get_extension(path.c_str(), &ext, &length);
        return std::string(ext);
    }
    std::string ChangePathExtension(std::string path, std::string newExtension)
    {
        size_t size = cwk_path_change_extension(path.c_str(), newExtension.c_str(), 0, 0);
        std::vector<char> dirname(size + 1);
        size_t should_be_zero = cwk_path_change_extension(path.c_str(), newExtension.c_str(), dirname.data(), dirname.size());
        return dirname.data();
    }
    bool IsPathAbsolute(std::string path)
    {
        return cwk_path_is_absolute( path.c_str() );
    }
    void SetDataDir(std::string dir) {
        g_dataPath = NormalizePath(dir);
    }
    std::string GetDataPath(std::string filename) {
        if (IsPathAbsolute(filename))
        {
            return filename;
        }
        return NormalizePath(JoinPath(g_dataPath, filename));
    }
    void ParallelFor(int n, std::function<void(int)> f /* f(index) */) {
#ifdef _WIN32
        concurrency::parallel_for(0, n, f);
#else
        // not supported yet...
        SerialFor(n, f);
#endif
    }
    void SerialFor(int n, std::function<void(int)> f /* f(index) */) {
        for (int i = 0; i < n; ++i) {
            f(i);
        }
    }

#ifdef _WIN32
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
#endif

    void SleepForMilliSeconds(int milliseconds)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
    void SleepForSeconds(int seconds)
    {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
    }

    // xxhash
    uint32_t xxhash32( const void* input, size_t length, uint32_t seed )
    {
        return XXH32( input, length, seed );
    }
    uint64_t xxhash64( const void* input, size_t length, uint32_t seed )
    {
        return XXH64(input, length, seed );
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