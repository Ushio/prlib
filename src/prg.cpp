#include "prg.hpp"

#include <iostream>
#include <memory>
#include <unordered_map>

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

using namespace Alembic::Abc;
using namespace Alembic::AbcGeom;

#include <tiny_obj_loader.h>

namespace pr {
	class IInt32ColumnVectorImpl : public IInt32Column {
	public:
		virtual const int32_t* data() const override {
			return _ints.data();
		}
		virtual int64_t count() const override {
			return _ints.size();
		}
		std::vector<int32_t> _ints;
	};

	class IInt32ColumnImpl : public IInt32Column {
	public:
		IInt32ColumnImpl(Int32ArraySamplePtr ptr) :_ints(ptr) {}
		virtual const int32_t* data() const override {
			return _ints->get();
		}
		virtual int64_t count() const override {
			return _ints->size();
		}
		Int32ArraySamplePtr _ints;
	};

	class IVector3ColumnP3fImpl : public IVector3Column {
	public:
		IVector3ColumnP3fImpl(P3fArraySamplePtr ptr) :_vec3s(ptr) {}
		
		virtual const glm::vec3* data() const override {
			static_assert(sizeof(glm::vec3) == sizeof(V3f), "invalid data compatibility");
			static_assert(offsetof(glm::vec3, x) == offsetof(V3f, x), "invalid data compatibility");
			static_assert(offsetof(glm::vec3, y) == offsetof(V3f, y), "invalid data compatibility");
			static_assert(offsetof(glm::vec3, z) == offsetof(V3f, z), "invalid data compatibility");
			return reinterpret_cast<const glm::vec3*>(_vec3s->get());
		}
		virtual int64_t count() const override {
			return _vec3s->size();
		}
		P3fArraySamplePtr _vec3s;
	};
	class IVector2ColumnV2fImpl : public IVector2Column {
	public:
		IVector2ColumnV2fImpl(V2fArraySamplePtr ptr) :_vec2s(ptr) {}

		virtual const glm::vec2* data() const override {
			static_assert(sizeof(glm::vec2) == sizeof(V2f), "invalid data compatibility");
			static_assert(offsetof(glm::vec2, x) == offsetof(V2f, x), "invalid data compatibility");
			static_assert(offsetof(glm::vec2, y) == offsetof(V2f, y), "invalid data compatibility");
			return reinterpret_cast<const glm::vec2*>(_vec2s->get());
		}
		virtual int64_t count() const override {
			return _vec2s->size();
		}
		V2fArraySamplePtr _vec2s;
	};
	class IVector3ColumnN3fImpl : public IVector3Column {
	public:
		IVector3ColumnN3fImpl(N3fArraySamplePtr ptr) :_vec3s(ptr) {}

		virtual const glm::vec3* data() const override {
			static_assert(sizeof(glm::vec3) == sizeof(N3f), "invalid data compatibility");
			static_assert(offsetof(glm::vec3, x) == offsetof(N3f, x), "invalid data compatibility");
			static_assert(offsetof(glm::vec3, y) == offsetof(N3f, y), "invalid data compatibility");
			static_assert(offsetof(glm::vec3, z) == offsetof(N3f, z), "invalid data compatibility");
			return reinterpret_cast<const glm::vec3*>(_vec3s->get());
		}
		virtual int64_t count() const override {
			return _vec3s->size();
		}
		N3fArraySamplePtr _vec3s;
	};
	template <class TVec2>
	class IVector2ColumnImplT : public IVector2Column {
	public:
		virtual const glm::vec2* data() const override {
			static_assert(sizeof(glm::vec2) == sizeof(TVec2), "invalid data compatibility");
			static_assert(offsetof(glm::vec2, x) == offsetof(TVec2, x), "invalid data compatibility");
			static_assert(offsetof(glm::vec2, y) == offsetof(TVec2, y), "invalid data compatibility");
			return reinterpret_cast<const glm::vec2*>(_vec2s.data());
		}
		virtual int64_t count() const override {
			return _vec2s.size();
		}
		std::vector<TVec2> _vec2s;
	};
	template <class TVec3>
	class IVector3ColumnImplT : public IVector3Column {
	public:
		virtual const glm::vec3* data() const override {
			static_assert(sizeof(glm::vec3) == sizeof(TVec3), "invalid data compatibility");
			static_assert(offsetof(glm::vec3, x) == offsetof(TVec3, x), "invalid data compatibility");
			static_assert(offsetof(glm::vec3, y) == offsetof(TVec3, y), "invalid data compatibility");
			static_assert(offsetof(glm::vec3, z) == offsetof(TVec3, z), "invalid data compatibility");
			return reinterpret_cast<const glm::vec3*>(_vec3s.data());
		}
		virtual int64_t count() const override {
			return _vec3s.size();
		}
		std::vector<TVec3> _vec3s;
	};


	struct CommonAttribute
	{
		std::string fullname;
		bool bakedVisibility = true;
		glm::mat4 localToWorld = glm::identity<glm::mat4>();
	};

	// Alembic
	class FPolyMeshEntityAbcImpl : public FPolyMeshEntity
	{
	public:
		virtual WindingOrder winingOrder() const override {
			return WindingOrder::WindingOrder_CW;
		}
		virtual std::string fullname() const override {
			return _common.fullname;
		}
		virtual bool visible() const override {
			return _common.bakedVisibility;
		}
		virtual glm::mat4 localToWorld() const override {
			return _common.localToWorld;
		}

		virtual IVector3Column* positions() const override {
			return _positions.get();
		}
		virtual IVector3Column* normals() const override {
			return _normals.get();
		}
		virtual IVector2Column* uvs() const override {
			return _uvs.get();
		}
		virtual IInt32Column* faceCounts() const override {
			return _faceCounts.get();
		}
		virtual IInt32Column* faceIndices() const override {
			return _faceIndices.get();
		}

		virtual AttributeSpreadsheet* attributeSpreadsheet(AttributeSpreadsheetType type) const override
		{
			return _attributeSpreadsheets[(int)type].get();
		}
		virtual std::string instanceSourceFullname() const override {
			return "";
		}
		CommonAttribute _common;

		// Standard Mesh
		std::unique_ptr<IVector3Column> _positions;
		std::unique_ptr<IVector3Column> _normals;
		std::unique_ptr<IVector2Column> _uvs;
		std::unique_ptr<IInt32Column> _faceCounts;
		std::unique_ptr<IInt32Column> _faceIndices;

		// houdini sheet
		std::unique_ptr<AttributeSpreadsheet> _attributeSpreadsheets[4];
	};

	class FPolyMeshEntityInstance : public FPolyMeshEntity
	{
	public:
		FPolyMeshEntityInstance()
		{
		}
		virtual WindingOrder winingOrder() const override {
			return _ref->winingOrder();
		}

		virtual std::string fullname() const override {
			return _common.fullname;
		}
		virtual bool visible() const override {
			return _common.bakedVisibility;
		}
		virtual glm::mat4 localToWorld() const override {
			return _common.localToWorld;
		}

		virtual IVector3Column* positions() const override {
			return _ref->positions();
		}
		virtual IVector3Column* normals() const override {
			return _ref->normals();
		}
		virtual IVector2Column* uvs() const override {
			return _ref->uvs();
		}
		virtual IInt32Column* faceCounts() const override {
			return _ref->faceCounts();
		}
		virtual IInt32Column* faceIndices() const override {
			return _ref->faceIndices();
		}

		virtual AttributeSpreadsheet* attributeSpreadsheet(AttributeSpreadsheetType type) const override
		{
			return _ref->attributeSpreadsheet(type);
		}
		virtual std::string instanceSourceFullname() const override {
			return _instanceSource;
		}

		CommonAttribute _common;
		std::shared_ptr<FPolyMeshEntity> _ref;
		std::string _instanceSource;
	};

	inline glm::mat4 to(M44d m) {
		glm::mat4 r;
		for (int i = 0; i < 16; ++i) {
			glm::value_ptr(r)[i] = (float)m.getValue()[i];
		}
		return r;
	}
	inline glm::vec3 to(V3d v) {
		return glm::vec3(v.x, v.y, v.z);
	}

	// xforms[0]: Top, xforms[n-1]: Bottom
	// M44d: Transform order L->R
	inline M44d combineXform(const std::vector<M44d>& xforms) {
		M44d m;
		for (int i = 0; i < xforms.size(); ++i) {
			m = xforms[i] * m;
		}
		return m;
	}

	// IN3fGeomParam::value_type : N3f, V3f, etc.
	// sample should be expanded.
	template <class GeomParam>
	inline void varyingStrainghten( std::vector<typename GeomParam::value_type>& straight, const typename GeomParam::Sample& sample, IInt32Column* faceIndices )
	{
		const int64_t nVertices = faceIndices->count();
		auto vals /* TSamplerPtr */ = sample.getVals();
		auto src = vals->get();

		straight.resize( nVertices );
		auto dst = straight.data();

		const int32_t *pIndices = faceIndices->data();
		for (int i = 0; i < nVertices; ++i)
		{
			int pIndex = pIndices[i];
			typename GeomParam::value_type n;
			n = src[pIndex];
			dst[i] = n;
		}
	}

	inline std::string getPodName(const PropertyHeader* header)
	{
		if (header == nullptr)
		{
			return "";
		}
		if( header->isScalar() )
		{
			return PODName(header->getDataType().getPod());
		}
		return header->getMetaData().get("podName");
	}
	inline int getArrayExtent(const PropertyHeader* header)
	{
		if (header == nullptr)
		{
			return 1;
		}
		std::string e = header->getMetaData().get("arrayExtent");
		if (e.empty())
		{
			return 1;
		}
		return std::atoi(e.c_str());
	}
	inline int getPodExtent(const PropertyHeader* header)
	{
		if (header == nullptr)
		{
			return 1;
		}
		if (header->isScalar())
		{
			return header->getDataType().getExtent();
		}
		std::string e = header->getMetaData().get("podExtent");
		if (e.empty())
		{
			return 1;
		}
		return std::atoi(e.c_str());
	}
	inline int getNumCompornent(const PropertyHeader* header)
	{
		return getArrayExtent(header) * getPodExtent(header);
	}

	class IHoudiniFloatGeomParam
	{
	public:
		static bool matches(const PropertyHeader& iHeader)
		{
			std::string podName = getPodName(&iHeader);
			int numCompornent = getNumCompornent(&iHeader);
			return
				podName == Float32PODTraits::name() &&
				numCompornent <= 4;
		}

		class Sample
		{
		public:
			UInt32ArraySamplePtr getIndices() const { return _indices; }
			FloatArraySamplePtr getVals() const { return _vals; }
			bool isIndexed() const { return _isIndexed; }

			void reset()
			{
				_vals.reset();
				_indices.reset();
				_isIndexed = false;
			}

			bool valid() const { return _vals.get() != NULL; }

			FloatArraySamplePtr _vals;
			UInt32ArraySamplePtr _indices;
			bool _isIndexed = false;
		};
		IHoudiniFloatGeomParam(ICompoundProperty prop, std::string key):_prop(prop), _key(key), _scope(kUnknownScope)
		{
			const PropertyHeader* header = _prop.getPropertyHeader(_key);
			_scope = GetGeometryScope(header->getMetaData());
		}
		void getSample(Sample& sample, ISampleSelector selector) const
		{
			sample.reset();

			const PropertyHeader* header = _prop.getPropertyHeader(_key);
			if( header == nullptr )
			{
				return;
			}
			if( header->isScalar() )
			{
				IScalarProperty val(_prop, _key);
				
				// In Scalar case, you need to check "iHeader.getDataType()"
				int nVals = getNumCompornent(header);
				
				float* values = (float*)malloc(sizeof(float) * nVals);
				val.get(values, selector);
				sample._isIndexed = false;

				sample._vals = FloatArraySamplePtr(new FloatArraySample(values, nVals), [values](FloatArraySample* p) {
					const float *ptr = p->get();
					delete p;
					free(values);
				});
			}
			else if( header->isArray() )
			{
				IFloatArrayProperty vals(_prop, _key);
				IFloatArrayProperty::sample_ptr_type floatSample;
				vals.get(floatSample, selector);
				sample._isIndexed = false;
				sample._vals = floatSample;
			}
			else
			{
				ICompoundProperty comp(_prop, _key);
				IFloatArrayProperty vals(comp, ".vals");
				IUInt32ArrayProperty indices(comp, ".indices");

				IFloatArrayProperty::sample_ptr_type floatSample;
				vals.get(floatSample, selector);
				IUInt32ArrayProperty::sample_ptr_type uintSample;
				indices.get(uintSample, selector);

				sample._isIndexed = true;
				sample._vals = floatSample;
				sample._indices = uintSample;
			}
		}
		GeometryScope getScope() const { return _scope; }

		ICompoundProperty _prop;
		std::string _key;
		GeometryScope _scope;
	};

	class IHoudiniIntGeomParam
	{
	public:
		static bool matches(const PropertyHeader& iHeader)
		{
			// just support non indexed.
			// But it's ok because 32bit single interger and 32bit indexed is pointless.
			if( iHeader.isCompound() ) 
			{
				return false;
			}
			std::string podName = getPodName(&iHeader);
			int numCompornent = getNumCompornent(&iHeader);
			return
				podName == Int32PODTraits::name() &&
				numCompornent == 1;
		}

		class Sample
		{
		public:
			Int32ArraySamplePtr getVals() const { return _vals; }
			void reset()
			{
				_vals.reset();
			}
			bool valid() const { return _vals.get() != NULL; }
			Int32ArraySamplePtr _vals;
		};

		IHoudiniIntGeomParam(ICompoundProperty prop, std::string key) :_prop(prop), _key(key), _scope(kUnknownScope)
		{
			const PropertyHeader* header = _prop.getPropertyHeader(_key);
			_scope = GetGeometryScope(header->getMetaData());
		}
		void getSample(Sample& sample, ISampleSelector selector) const
		{
			sample.reset();

			const PropertyHeader* header = _prop.getPropertyHeader(_key);
			if (header->isScalar())
			{
				IInt32Property val(_prop, _key);
				int32_t* value = (int32_t*)malloc(sizeof(int32_t));
				val.get(*value, selector);
				sample._vals = Int32ArraySamplePtr(new Int32ArraySample(value, 1), [value](Int32ArraySample* p) {
					delete p;
					free(value);
				});
			}
			else if (header->isArray())
			{
				IInt32ArrayProperty vals(_prop, _key);
				IInt32ArrayProperty::sample_ptr_type intSample;
				vals.get(intSample, selector);
				sample._vals = intSample;
			}
		}
		GeometryScope getScope() const { return _scope; }

		ICompoundProperty _prop;
		std::string _key;
		GeometryScope _scope;
	};
	class IHoudiniStringGeomParam
	{
	public:
		static bool matches(const PropertyHeader& iHeader)
		{
			std::string podName = getPodName(&iHeader);
			int numCompornent = getNumCompornent(&iHeader);
			return
				podName == StringPODTraits::name() &&
				numCompornent == 1;
		}

		class Sample
		{
		public:
			UInt32ArraySamplePtr getIndices() const { return _indices; }
			StringArraySamplePtr getVals() const { return _vals; }
			bool isIndexed() const { return _isIndexed; }

			void reset()
			{
				_vals.reset();
				_indices.reset();
				_isIndexed = false;
			}

			bool valid() const { return _vals.get() != NULL; }

			StringArraySamplePtr _vals;
			UInt32ArraySamplePtr _indices;
			bool _isIndexed = false;
		};
		IHoudiniStringGeomParam(ICompoundProperty prop, std::string key) :_prop(prop), _key(key), _scope(kUnknownScope)
		{
			const PropertyHeader* header = _prop.getPropertyHeader(_key);
			if (header)
			{
				_scope = GetGeometryScope(header->getMetaData());
			}
		}
		void getSample(Sample& sample, ISampleSelector selector) const
		{
			sample.reset();

			const PropertyHeader* header = _prop.getPropertyHeader(_key);
			if (header == nullptr)
			{
				return;
			}
			if (header->isScalar())
			{
				IScalarProperty val(_prop, _key);
				std::string* value = new std::string;
				val.get(value, selector);
				sample._isIndexed = false;
				sample._vals = StringArraySamplePtr(new StringArraySample(value, 1), [value](StringArraySample* p) {
					delete p;
					delete value;
				});
			}
			else if (header->isArray())
			{
				IStringArrayProperty vals(_prop, _key);
				IStringArrayProperty::sample_ptr_type stringSample;
				vals.get(stringSample, selector);
				sample._isIndexed = false;
				sample._vals = stringSample;
			}
			else
			{
				ICompoundProperty comp(_prop, _key);
				IStringArrayProperty vals(comp, ".vals");
				IUInt32ArrayProperty indices(comp, ".indices");

				IStringArrayProperty::sample_ptr_type stringSample;
				vals.get(stringSample, selector);
				IUInt32ArrayProperty::sample_ptr_type uintSample;
				indices.get(uintSample, selector);

				sample._isIndexed = true;
				sample._vals = stringSample;
				sample._indices = uintSample;
			}
		}
		GeometryScope getScope() const { return _scope; }

		ICompoundProperty _prop;
		std::string _key;
		GeometryScope _scope;
	};
	std::ostream& operator<<(std::ostream& out, const glm::vec2& v)
	{
		return out << v.x << ", " << v.y;
	}
	std::ostream& operator<<(std::ostream& out, const glm::vec3& v)
	{
		return out << v.x << ", " << v.y << ", " << v.z;
	}
	std::ostream& operator<<(std::ostream& out, const glm::vec4& v)
	{
		return out << v.x << ", " << v.y << ", " << v.z << ", " << v.w;
	}

	template <class T, int N, class BaseType>
	class AttributeFloatNColumnImpl : public BaseType
	{
	public:
		AttributeFloatNColumnImpl(IHoudiniFloatGeomParam::Sample sample) :_sample(sample) {
			static_assert(sizeof(T) == sizeof(float) * N, "type mismatch");
			static_assert(std::is_same<T, decltype(get(0))>::value, "type mismatch");
		}
		virtual int64_t count() const {
			if (_sample.isIndexed())
			{
				return _sample.getIndices()->size();
			}
			int64_t nComp = _sample.getVals()->size();

			FloatArraySamplePtr vals = _sample.getVals();
			int64_t bytes = _sample.getVals()->size() * vals->getDataType().getNumBytes();
			int64_t nFloats = bytes / sizeof(float);
			return nFloats / N;
		}
		virtual T get( int64_t index ) const
		{
			if (_sample.valid() == 0)
			{
				return T();
			}
			if( _sample.isIndexed() )
			{
				index = _sample.getIndices()->get()[index];
			}

			const float *ptr = _sample.getVals()->get();

			T v;
			memcpy(&v, ptr + index * N, sizeof(float) * N);
			return v;
		}
		virtual std::string getAsString(int64_t index) const
		{
			std::stringstream ss;
			ss << get(index);
			return ss.str();
		}
		IHoudiniFloatGeomParam::Sample _sample;
	};
	using AttributeFloatColumnImpl = AttributeFloatNColumnImpl<float, 1, AttributeFloatColumn>;
	using AttributeVector2ColumnImpl = AttributeFloatNColumnImpl<glm::vec2, 2, AttributeVector2Column>;
	using AttributeVector3ColumnImpl = AttributeFloatNColumnImpl<glm::vec3, 3, AttributeVector3Column>;
	using AttributeVector4ColumnImpl = AttributeFloatNColumnImpl<glm::vec4, 4, AttributeVector4Column>;

	class AttributeIntColumnImpl : public AttributeIntColumn
	{
	public:
		AttributeIntColumnImpl(Int32ArraySamplePtr values) :_values(values) 
		{
		}
		virtual int64_t count() const 
		{
			return _values->size();
		}
		virtual int32_t get(int64_t index) const
		{
			const int32_t* ptr = _values->get();
			return ptr[index];
		}
		virtual std::string getAsString(int64_t index) const
		{
			return std::to_string(get(index));
		}
		Int32ArraySamplePtr _values;
	};
	class AttributeStringColumnImpl : public AttributeStringColumn
	{
	public:
		AttributeStringColumnImpl(IHoudiniStringGeomParam::Sample sample) :_sample(sample) {

		}
		virtual int64_t count() const {
			if (_sample.isIndexed())
			{
				return _sample.getIndices()->size();
			}
			return _sample.getVals()->size();
		}
		virtual const std::string& get(int64_t index) const
		{
			if (_sample.valid() == 0)
			{
				static std::string e;
				return e;
			}
			if (_sample.isIndexed())
			{
				index = _sample.getIndices()->get()[index];
			}
			const std::string* ptr = _sample.getVals()->get();
			return ptr[index];
		}
		virtual std::string getAsString(int64_t index) const
		{
			return get(index);
		}
		IHoudiniStringGeomParam::Sample _sample;
	};

	class AttributeSpreadsheetImpl : public AttributeSpreadsheet
	{
	public:
		~AttributeSpreadsheetImpl()
		{
            for( auto it = _attributes.begin() ; it != _attributes.end() ; ++it )
			{
				free( (void *)it->first );
			}
		}
		virtual int64_t rowCount() const override
		{
			if (_attributes.empty())
			{
				return 0;
			}
			return _attributes.begin()->second->count();
		}
		virtual int64_t columnCount() const override
		{
			return _attributes.size();
		}
		virtual const std::vector<std::string>& keys() const
		{
			return _keys;
		}
		virtual const AttributeColumn* column(const char* key) const
		{
			auto it = _attributes.find(key);
			if( it == _attributes.end() )
			{
				return nullptr;
			}
			return it->second.get();
		}
		void addAttribute( const char *key, std::unique_ptr<AttributeColumn> attribute ) {
			char* allocatedKey = (char*)malloc(strlen(key) + 1);
			strcpy(allocatedKey, key);
			_attributes[allocatedKey] = std::move(attribute);
			_keys.push_back(key);
		}
	private:
		struct KeyHash {
			using KeyType = const char*;
			std::size_t operator()(const KeyType& key) const
			{
				std::hash<char> h;
				std::size_t v = h( key[0] );
				int i = 0;
				while( key[i] != '\0' )
				{
					i++;
					v = v ^ h( key[i] );
				}
				return v;
			}
		};
		struct KeyEqual {
			bool operator()(const char* a, const char* b) const
			{
				return strcmp(a, b) == 0;
			}
		};
		std::unordered_map<const char*, std::unique_ptr<AttributeColumn>, KeyHash, KeyEqual> _attributes;
		std::vector<std::string> _keys;
	};
	static const std::unique_ptr<AttributeSpreadsheet> kEmptySheet( new AttributeSpreadsheetImpl() );

	inline std::shared_ptr<FPolyMeshEntity> parse_polymesh(IPolyMesh &polyMesh, const CommonAttribute& common, ISampleSelector selector) {
		IPolyMeshSchema& schema = polyMesh.getSchema();
		IPolyMeshSchema::Sample sample;
		schema.get(sample, selector);

		std::shared_ptr<FPolyMeshEntityAbcImpl> e(new FPolyMeshEntityAbcImpl());
		e->_positions = std::unique_ptr<IVector3ColumnP3fImpl>(new IVector3ColumnP3fImpl(sample.getPositions()));
		e->_faceCounts = std::unique_ptr<IInt32ColumnImpl>(new IInt32ColumnImpl(sample.getFaceCounts()));
		e->_faceIndices = std::unique_ptr<IInt32ColumnImpl>(new IInt32ColumnImpl(sample.getFaceIndices()));
		IN3fGeomParam normalParam = schema.getNormalsParam();
		if( normalParam )
		{
			GeometryScope scope = normalParam.getScope();
			IN3fGeomParam::Sample sampleNormal;
			normalParam.getExpanded(sampleNormal, selector);
			switch( scope )
			{
			case kVaryingScope:
			{
				std::unique_ptr<IVector3ColumnImplT<N3f>> normals(new IVector3ColumnImplT<N3f>());
				varyingStrainghten<IN3fGeomParam>(normals->_vec3s, sampleNormal, e->_faceIndices.get());
				e->_normals = std::move(normals);
				break;
			}
			case kFacevaryingScope:
				e->_normals = std::unique_ptr<IVector3ColumnN3fImpl>(new IVector3ColumnN3fImpl(sampleNormal.getVals()));
				break;
			default:
				break;
			}
		}

		IV2fGeomParam uvParam = schema.getUVsParam();
		if (uvParam)
		{
			GeometryScope scope = uvParam.getScope();
			IV2fGeomParam::Sample sampleUV;
			uvParam.getExpanded(sampleUV, selector);
			switch (scope)
			{
			case kVaryingScope:
			{
				std::unique_ptr<IVector2ColumnImplT<V2f>> uvs(new IVector2ColumnImplT<V2f>());
				varyingStrainghten<IV2fGeomParam>(uvs->_vec2s, sampleUV, e->_faceIndices.get());
				e->_uvs = std::move(uvs);
				break;
			}
			case kFacevaryingScope:
				e->_uvs = std::unique_ptr<IVector2ColumnV2fImpl>(new IVector2ColumnV2fImpl(sampleUV.getVals()));
				break;
			default:
				break;
			}
		}

		ICompoundProperty arbProps = schema.getArbGeomParams();
		// ICompoundProperty arbProps = ICompoundProperty(polyMesh.getProperties(), ".geom");
		if (arbProps)
		{
			std::unique_ptr<AttributeSpreadsheetImpl> attributeSpreadsheets[4];
			for (int i = 0; i < 4; ++i)
			{
				attributeSpreadsheets[i] = std::unique_ptr<AttributeSpreadsheetImpl>(new AttributeSpreadsheetImpl());
			}

			for (int i = 0; i < arbProps.getNumProperties(); ++i) {
				const PropertyHeader &propertyHeader = arbProps.getPropertyHeader(i);
				auto key = propertyHeader.getName();

				//if (key[0] == '.')
				//{
				//	continue;
				//}

				std::unique_ptr<AttributeColumn> col;
				GeometryScope scope = kUnknownScope;

				if (IHoudiniFloatGeomParam::matches(propertyHeader))
				{
					IHoudiniFloatGeomParam param(arbProps, key);
					IHoudiniFloatGeomParam::Sample sample;
					param.getSample(sample, selector);
					switch (getNumCompornent(&propertyHeader))
					{
					case 1:
						col = std::unique_ptr<AttributeColumn>(new AttributeFloatColumnImpl(sample));
						break;
					case 2:
						col = std::unique_ptr<AttributeColumn>(new AttributeVector2ColumnImpl(sample));
						break;
					case 3:
						col = std::unique_ptr<AttributeColumn>(new AttributeVector3ColumnImpl(sample));
						break;
					case 4:
						col = std::unique_ptr<AttributeColumn>(new AttributeVector4ColumnImpl(sample));
						break;
					default:
						break;
					}
					scope = param.getScope();
				}
				else if (IHoudiniIntGeomParam::matches(propertyHeader))
				{
					IHoudiniIntGeomParam param(arbProps, key);
					IHoudiniIntGeomParam::Sample sample;
					param.getSample(sample, selector);
					col = std::unique_ptr<AttributeColumn>(new AttributeIntColumnImpl(sample.getVals()));
					scope = param.getScope();
				}
				else if (IHoudiniStringGeomParam::matches(propertyHeader))
				{
					IHoudiniStringGeomParam param(arbProps, key);
					IHoudiniStringGeomParam::Sample sample;
					param.getSample(sample, selector);
					col = std::unique_ptr<AttributeColumn>(new AttributeStringColumnImpl(sample));
					scope = param.getScope();
				}

				if( col )
				{
					switch (scope)
					{
					case kVaryingScope: /* points */
						attributeSpreadsheets[(int)AttributeSpreadsheetType::Points]->addAttribute(key.c_str(), std::move(col));
						break;
					case kFacevaryingScope: /* vertices */
						attributeSpreadsheets[(int)AttributeSpreadsheetType::Vertices]->addAttribute(key.c_str(), std::move(col));
						break;
					case kUniformScope: /* primitives */
						attributeSpreadsheets[(int)AttributeSpreadsheetType::Primitives]->addAttribute(key.c_str(), std::move(col));
						break;
					case kConstantScope: /* details */
						attributeSpreadsheets[(int)AttributeSpreadsheetType::Details]->addAttribute(key.c_str(), std::move(col));
						break;
					default:
						// ?
						break;
					}
				}
			}

			for (int i = 0; i < 4; ++i)
			{
				e->_attributeSpreadsheets[i] = std::move(attributeSpreadsheets[i]);
			}
		}

		e->_common = common;
		return e;
	}

	enum class ParseMode
	{
		Standard,
		Instance
	};

	inline void parse_object(IObject o, ISampleSelector selector, std::vector<M44d> xforms, bool visible, std::vector<std::shared_ptr<FSceneEntity>> &entities, ParseMode parseMode, std::map<std::string, std::shared_ptr<FPolyMeshEntity>> &instances)
	{
		auto header = o.getHeader();

		switch (GetVisibility(o, selector))
		{
		case kVisibilityDeferred:
			break;
		case kVisibilityHidden:
			visible = false;
			break;
		case kVisibilityVisible:
			visible = true;
			break;
		}

		CommonAttribute common;
		common.bakedVisibility = visible;
		common.fullname = o.getFullName();
		common.localToWorld = to(combineXform(xforms));
		
		if (IPolyMesh::matches(header)) {
			IPolyMesh polyMesh(o);
			std::string instanceSourcePath = o.instanceSourcePath();

			if( parseMode == ParseMode::Standard )
			{
				if( instanceSourcePath.empty() )
				{
					std::shared_ptr<FPolyMeshEntity> e = parse_polymesh(polyMesh, common, selector);
					entities.emplace_back(e);
					instances[common.fullname] = e;
				}
			}
			else if( parseMode == ParseMode::Instance )
			{
				if( instanceSourcePath.empty() == false )
				{
					std::shared_ptr<FPolyMeshEntityInstance> e(new FPolyMeshEntityInstance());
					e->_common = common;
					e->_ref = instances[instanceSourcePath];
					e->_instanceSource = instanceSourcePath;
					entities.emplace_back(e);
				}
			}
		}
		else if (IPoints::matches(header)) {
			//IPoints points(o);
			//std::shared_ptr<PointObject> object(new PointObject());

			//parse_common_property(o, object.get(), xforms, selector);
			//parse_points(points, object, selector);

			//objects.emplace_back(object);
		}
		else if (ICurves::matches(header)) {
			//ICurves curves(o);
			//std::shared_ptr<CurveObject> object(new CurveObject());

			//parse_common_property(o, object.get(), xforms, selector);
			//parse_curves(curves, object, selector);

			//objects.emplace_back(object);
		}
		else if (ICamera::matches(header)) {
			// Implementation Notes
			// https://docs.google.com/presentation/d/1f5EVQTul15x4Q30IbeA7hP9_Xc0AgDnWsOacSQmnNT8/edit?usp=sharing

			//ICamera camera(o);
			//auto schema = camera.getSchema();

			//std::shared_ptr<CameraObject> object(new CameraObject());

			//IXform parentXForm(o.getParent());
			//object->name = parentXForm.getFullName();

			//for (int i = 0; i < xforms.size(); ++i) {
			//	object->xforms.push_back(to(xforms[i]));
			//}
			//M44d combined = combine_xform(xforms);
			//object->combinedXforms = to(combined);

			//// http://www.sidefx.com/ja/docs/houdini/io/alembic.html#%E5%8F%AF%E8%A6%96%E6%80%A7
			//auto parentProp = parentXForm.getProperties();
			//if (parentProp.getPropertyHeader("visible")) {
			//	int8_t visible = get_typed_scalar_property<ICharProperty>(parentProp, "visible", selector);
			//	object->visible = visible == -1;
			//}
			//else {
			//	object->visible = true;
			//}

			//M44d inverseTransposed = combined.inverse().transposed();

			//V3d eye;
			//combined.multVecMatrix(V3d(0, 0, 0), eye);
			//V3d forward;
			//V3d up;
			//inverseTransposed.multDirMatrix(V3d(0, 0, -1), forward);
			//inverseTransposed.multDirMatrix(V3d(0, 1, 0), up);

			//V3d right;
			//inverseTransposed.multDirMatrix(V3d(1, 0, 0), right);

			//object->eye = to(eye);
			//object->lookat = to(eye + forward);
			//object->up = to(up);
			//object->down = to(-up);
			//object->forward = to(forward);
			//object->back = to(-forward);
			//object->left = to(-right);
			//object->right = to(right);

			//CameraSample sample;
			//schema.get(sample, selector);

			//// Houdini Parameters [ View ]
			//object->resolution_x = (int)get_typed_scalar_property<IFloatProperty>(schema.getUserProperties(), "resx", selector);
			//object->resolution_y = (int)get_typed_scalar_property<IFloatProperty>(schema.getUserProperties(), "resy", selector);
			//object->focalLength_mm = sample.getFocalLength();
			//object->aperture_horizontal_mm = sample.getHorizontalAperture() * 10.0f;
			//object->aperture_vertical_mm = sample.getVerticalAperture() * 10.0f;
			//object->nearClip = sample.getNearClippingPlane();
			//object->farClip = sample.getFarClippingPlane();

			//// Houdini Parameters [ Sampling ]
			//object->focusDistance = sample.getFocusDistance();
			//object->f_stop = sample.getFStop();

			//// Calculated by Parameters
			//object->fov_horizontal_degree = sample.getFieldOfView();
			//float fov_vertical_radian = std::atan(object->aperture_vertical_mm * 0.5f / object->focalLength_mm) * 2.0f;
			//object->fov_vertical_degree = fov_vertical_radian / (2.0 * M_PI) * 360.0f;

			//float A = object->focalLength_mm / 1000.0f;
			//float B = object->focusDistance;
			//float F = A * B / (A + B);
			//object->lensRadius = F / (2.0f * object->f_stop);

			//object->objectPlaneWidth = 2.0f * object->focusDistance * std::tan(0.5f * object->fov_horizontal_degree / 360.0f * 2.0 * M_PI);
			//object->objectPlaneHeight = 2.0f * object->focusDistance * std::tan(0.5f * object->fov_vertical_degree / 360.0f * 2.0 * M_PI);

			//objects.emplace_back(object);
		}
		else if (IXform::matches(header)) {
			IXform xform(o);
			IXformSchema &schema = xform.getSchema();
			XformSample schemaValue = schema.getValue(selector);
			M44d matrix = schemaValue.getMatrix();
			xforms.push_back(matrix);

			// we ignore schema.getInheritsXforms(selector)

			for (int i = 0; i < o.getNumChildren(); ++i) {
				IObject child = o.getChild(i);
				parse_object(child, selector, xforms, visible, entities, parseMode, instances);
			}
		}
		else {
			for (int i = 0; i < o.getNumChildren(); ++i) {
				IObject child = o.getChild(i);
				parse_object(child, selector, xforms, visible, entities, parseMode, instances);
			}
		}
	}

	AbcArchive::Result AbcArchive::open(const std::string& filePath, std::string& error_message) {
		try {
			_alembicArchive = std::shared_ptr<void>();
			_alembicArchive = std::shared_ptr<void>(new IArchive(Alembic::AbcCoreOgawa::ReadArchive(), filePath), [](void* p) {
				IArchive* archive = static_cast<IArchive*>(p);
				delete archive;
			});

			IArchive* archive = static_cast<IArchive*>(_alembicArchive.get());
			GetArchiveInfo(*archive, _applicationWriter, _alembicVersion, _alembicApiVersion, _dateWritten, _userDescription, _DCCFPS);

			index_t nFrame = 0;
			for (uint32_t i = 0; i < archive->getNumTimeSamplings(); ++i)
			{
				nFrame = std::max(nFrame, archive->getMaxNumSamplesForTimeSamplingIndex(i));
			}
			_frameCount = (uint32_t)nFrame;
		}
		catch (std::exception& e) {
			error_message = e.what();
			return AbcArchive::Result::Failure;
		}
		return AbcArchive::Result::Sucess;
	}
	bool AbcArchive::isOpened() const {
		return (bool)_alembicArchive;
	}
	void AbcArchive::close() {
		_alembicArchive = std::shared_ptr<void>();
	}
	std::shared_ptr<FScene> AbcArchive::readFlat(uint32_t index, std::string& error_message) const {
		if (!_alembicArchive) {
			return std::shared_ptr<FScene>();
		}
		try {
			ISampleSelector selector((index_t)index);
			std::vector<std::shared_ptr<FSceneEntity>> entities;
			IArchive* archive = static_cast<IArchive*>(_alembicArchive.get());
			std::map<std::string, std::shared_ptr<FPolyMeshEntity>> instances;
			parse_object( archive->getTop(), selector, std::vector<M44d>(), true /* visible */, entities, ParseMode::Standard, instances );
			parse_object( archive->getTop(), selector, std::vector<M44d>(), true /* visible */, entities, ParseMode::Instance, instances );
			return std::shared_ptr<FScene>(new FScene(std::move(entities)));
		}
		catch (std::exception& e) {
			error_message = e.what();
		}
		return std::shared_ptr<FScene>();
	}

	class FPolyMeshEntityTinyObjLoaderImpl : public FPolyMeshEntity
	{
	public:
		FPolyMeshEntityTinyObjLoaderImpl()
		{

		}
		virtual std::string fullname() const override {
			return _common.fullname;
		}
		virtual bool visible() const override {
			return _common.bakedVisibility;
		}
		virtual glm::mat4 localToWorld() const override {
			return _common.localToWorld;
		}
		virtual WindingOrder winingOrder() const override {
			return WindingOrder::WindingOrder_CCW;
		}
		virtual IVector3Column* positions() const override {
			return _positions.get();
		}
		virtual IVector3Column * normals() const override {
			return _normals.get();
		}
		virtual IVector2Column* uvs() const override {
			return _uvs.get();
		}
		virtual IInt32Column* faceCounts() const override {
			return _faceCounts.get();
		}
		virtual IInt32Column* faceIndices() const override {
			return _faceIndices.get();
		}
		virtual AttributeSpreadsheet* attributeSpreadsheet(AttributeSpreadsheetType type) const override
		{
			return kEmptySheet.get();
		}
		virtual std::string instanceSourceFullname() const override {
			return "";
		}

		std::shared_ptr<tinyobj::ObjReader> _sharedObjReader;

		CommonAttribute _common;

		// Standard Mesh
		std::unique_ptr<IVector3Column> _positions;
		std::unique_ptr<IVector3Column> _normals;
		std::unique_ptr<IVector2Column> _uvs;
		std::unique_ptr<IInt32Column> _faceCounts;
		std::unique_ptr<IInt32Column> _faceIndices;
	};

	class IVector2ColumnFloatRefImpl : public IVector2Column {
	public:
		IVector2ColumnFloatRefImpl(const float *ptr, int64_t count) :_ptr(ptr), _count(count) {}

		virtual const glm::vec2* data() const override {
			return reinterpret_cast<const glm::vec2*>(_ptr);
		}
		virtual int64_t count() const override {
			return _count;
		}
		const float* _ptr = nullptr;
		int64_t _count = 0;
	};
	class IVector3ColumnFloatRefImpl : public IVector3Column {
	public:
		IVector3ColumnFloatRefImpl(const float* ptr, int64_t count) :_ptr(ptr), _count(count) {}

		virtual const glm::vec3* data() const override {
			return reinterpret_cast<const glm::vec3*>(_ptr);
		}
		virtual int64_t count() const override {
			return _count;
		}
		const float* _ptr = nullptr;
		int64_t _count = 0;
	};

	std::shared_ptr<FScene> ReadWavefrontObj(const std::string& filePath, std::string& error_message)
	{
		std::shared_ptr<tinyobj::ObjReader> reader(new tinyobj::ObjReader());
		if (reader->ParseFromFile(filePath) == false)
		{
			error_message = reader->Error();
			return std::shared_ptr<FScene>();
		}

		const std::vector<tinyobj::shape_t>& shapes = reader->GetShapes();
		const tinyobj::attrib_t& attrib = reader->GetAttrib();

		std::shared_ptr<FPolyMeshEntityTinyObjLoaderImpl> e(new FPolyMeshEntityTinyObjLoaderImpl());
		e->_sharedObjReader = reader;

		e->_positions = std::unique_ptr<IVector3ColumnFloatRefImpl>(new IVector3ColumnFloatRefImpl(
			attrib.vertices.data(),
			attrib.vertices.size() / 3
		));

		int64_t indexCount = 0;
		int64_t faceCount = 0;
		for (int i = 0; i < shapes.size(); ++i)
		{
			const tinyobj::shape_t& shape = shapes[i];
			indexCount += shape.mesh.indices.size();
			faceCount += shape.mesh.num_face_vertices.size();
		}

		std::unique_ptr<IInt32ColumnVectorImpl> faceCounts(new IInt32ColumnVectorImpl());
		faceCounts->_ints.resize(faceCount);
		std::unique_ptr<IInt32ColumnVectorImpl> faceIndices(new IInt32ColumnVectorImpl());
		faceIndices->_ints.resize(indexCount);

		int32_t* faceCountsPtr = faceCounts->_ints.data();
		int32_t* faceIndicesPtr = faceIndices->_ints.data();

		std::unique_ptr<IVector3ColumnImplT<glm::vec3>> normals;
		glm::vec3* normalsPtr = nullptr;
		std::unique_ptr<IVector2ColumnImplT<glm::vec2>> uvs;
		glm::vec2* uvsPtr = nullptr;

		int64_t index_i = 0;
		int64_t face_i = 0;
		for (int i = 0; i < shapes.size(); ++i)
		{
			const tinyobj::shape_t& shape = shapes[i];
			
			for (int j = 0; j < shape.mesh.num_face_vertices.size(); ++j)
			{
				faceCountsPtr[face_i++] = shape.mesh.num_face_vertices[j];
			}
			for (int j = 0; j < shape.mesh.indices.size(); ++j)
			{
				auto index = shape.mesh.indices[j];
				faceIndicesPtr[index_i] = index.vertex_index;

				// setup normals
				if(0 <= index.normal_index)
				{
					if( normalsPtr == nullptr )
					{
						normals = std::unique_ptr<IVector3ColumnImplT<glm::vec3>>(new IVector3ColumnImplT<glm::vec3>());
						normals->_vec3s.resize(indexCount);
						normalsPtr = normals->_vec3s.data();
					}
					
					normalsPtr[index_i] = {
						attrib.normals[index.normal_index * 3],
						attrib.normals[index.normal_index * 3 + 1],
						attrib.normals[index.normal_index * 3 + 2]
					};
				}

				// setup UV
				if (0 <= index.texcoord_index)
				{
					if (uvsPtr == nullptr)
					{
						uvs = std::unique_ptr<IVector2ColumnImplT<glm::vec2>>(new IVector2ColumnImplT<glm::vec2>());
						uvs->_vec2s.resize(indexCount);
						uvsPtr = uvs->_vec2s.data();
					}
					uvsPtr[index_i] = {
						attrib.texcoords[index.texcoord_index * 2],
						attrib.texcoords[index.texcoord_index * 2 + 1],
					};
				}

				index_i++;
			}
		}
		e->_faceCounts = std::move(faceCounts);
		e->_faceIndices = std::move(faceIndices);

		e->_normals = std::move(normals);
		e->_uvs = std::move(uvs);

		return std::shared_ptr<FScene>(new FScene({ e }));
	}
}