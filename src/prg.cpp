#include "prg.hpp"

#include <iostream>
#include <memory>

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
		virtual int snprint(uint32_t i, char* dst, uint32_t buffersize) const override {
			return snprintf(dst, buffersize, "%d", _ints[i]);
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
		virtual int snprint(uint32_t i, char* dst, uint32_t buffersize) const override {
			return snprintf(dst, buffersize, "%d", _ints->get()[i]);
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
		int snprint(uint32_t i, char* dst, uint32_t buffersize) const override {
			auto p = _vec3s->get()[i];
			return snprintf(dst, buffersize, "(%f, %f, %f)", p.x, p.y, p.z);
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
		int snprint(uint32_t i, char* dst, uint32_t buffersize) const override {
			auto p = _vec2s->get()[i];
			return snprintf(dst, buffersize, "(%f, %f)", p.x, p.y);
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
		int snprint(uint32_t i, char* dst, uint32_t buffersize) const override {
			V3f p = _vec3s->get()[i];
			return snprintf(dst, buffersize, "(%f, %f, %f)", p.x, p.y, p.z);
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
		int snprint(uint32_t i, char* dst, uint32_t buffersize) const override {
			TVec2 p = _vec2s[i];
			return snprintf( dst, buffersize, "(%f, %f)", p.x, p.y );
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
		int snprint(uint32_t i, char* dst, uint32_t buffersize) const override {
			TVec3 p = _vec3s[i];
			return snprintf(dst, buffersize, "(%f, %f, %f)", p.x, p.y, p.z);
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

		virtual std::string propertyHash() const override {
			return _propertyHash;
		}
		CommonAttribute _common;

		// Standard Mesh
		std::unique_ptr<IVector3Column> _positions;
		std::unique_ptr<IVector3Column> _normals;
		std::unique_ptr<IVector2Column> _uvs;
		std::unique_ptr<IInt32Column> _faceCounts;
		std::unique_ptr<IInt32Column> _faceIndices;

		// Instanced Mesh
		std::string _propertyHash;
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

	class AttributeVector3ColumnImpl : public AttributeVector3Column
	{
	public:
		virtual uint32_t rowCount() const {
			return 0;
		}
		virtual int snprint(uint32_t index, char* buffer, uint32_t buffersize) const
		{
			return 0;
		}
		virtual glm::vec3 get(uint32_t index) const
		{
			const float* p = _vals->get();
			if (_isIndexed)
			{

			}
			float x = p[index * 3];
			float y = p[index * 3 + 1];
			float z = p[index * 3 + 2];
			return glm::vec3(x, y, z);
		}
		static bool matches(const AbcA::PropertyHeader& iHeader)
		{
			DataType srcDataType = iHeader.getDataType();
			if (iHeader.isCompound())
			{

			}
			else if (iHeader.isArray())
			{
				return
					iHeader.getDataType() == DataType(kFloat32POD, 3) ||
					(iHeader.getDataType() == DataType(kFloat32POD, 1) && iHeader.getMetaData().get("arrayExtent") == "3");
			}
			return false;
		}
		static std::shared_ptr<AttributeVector3Column> Get(ICompoundProperty parent, const char* key, ISampleSelector selector )
		{
			const PropertyHeader* header = parent.getPropertyHeader(key);
			if( header->isArray() )
			{
				IFloatArrayProperty vals(parent, key);
				IFloatArrayProperty::sample_ptr_type sample;
				vals.get(sample, selector);
				std::shared_ptr<AttributeVector3ColumnImpl> col(new AttributeVector3ColumnImpl());
				col->_vals = sample;
				return col;
			}
			return std::shared_ptr<AttributeVector3Column>();
		}

		FloatArraySamplePtr _vals;
		bool _isIndexed = false;
	};


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
		if (arbProps)
		{
			for (int i = 0; i < arbProps.getNumProperties(); ++i) {
				const PropertyHeader &propertyHeader = arbProps.getPropertyHeader(i);
				auto key = propertyHeader.getName();

				if (AttributeVector3ColumnImpl::matches(propertyHeader))
				{
					auto data = AttributeVector3ColumnImpl::Get(arbProps, key.c_str(), selector);
				}
				//if (IFloatGeomParam::matches(propertyHeader))
				//{
				//	IFloatGeomParam param = IFloatGeomParam(arbProps, key);
				//	IFloatGeomParam::Sample sample;
				//	param.getIndexed(sample, selector);
				//	int arraysize = sample.getVals()->size();
				//	int indexsize = sample.getIndices()->size();
				//	int arrayExtent = param.getArrayExtent();
				//	printf("");
				//}
				//if (IV2fGeomParam::matches(child_header, kNoMatching /* just checking extent and type */))
				//{
				//	IV2fGeomParam param = IV2fGeomParam(arbProps, key);
				//	IV2fGeomParam::Sample sample;
				//	param.getIndexed(sample, selector);
				//}
				//if (IV3fGeomParam::matches(child_header, kNoMatching /* just checking extent and type */))
				//{
				//	IV3fGeomParam param = IV3fGeomParam(arbProps, key);
				//	IV3fGeomParam::Sample sample;
				//	param.getIndexed(sample, selector);
				//}

				//std::string geoScope = propertyHeader.getMetaData().get("");

				// std::string geoScope = child_header.getMetaData().get("geoScope");
			
				// m_positionsProperty = Abc::IP3fArrayProperty(polyMesh, "P", kNoMatching, ErrorHandler::kQuietNoopPolicy);

				//m_indicesProperty = Abc::IInt32ArrayProperty(polyMesh, ".faceIndices",
				//	iArg0, iArg1);
				//m_countsProperty = Abc::IInt32ArrayProperty(polyMesh, ".faceCounts",
				//	iArg0, iArg1);
				// ALEMBIC_ABC_DECLARE_TYPE_TRAITS( V3f, kFloat32POD, 3, "point", P3fTPTraits );
				//IV3fGeomParam prop = 
			}
		}

		Digest d;
		if (polyMesh.getPropertiesHash(d))
		{
			e->_propertyHash = d.str();
		}
		e->_common = common;
		return e;
	}
	inline void parse_object(IObject o, ISampleSelector selector, std::vector<M44d> xforms, bool visible, std::vector<std::shared_ptr<FSceneEntity>> &entities) 
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
		common.fullname = header.getFullName();
		common.localToWorld = to(combineXform(xforms));

		if (IPolyMesh::matches(header)) {
			IPolyMesh polyMesh(o);
			//parse_common_property(o, object.get(), xforms, selector);

			entities.emplace_back(parse_polymesh(polyMesh, common, selector));
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
				parse_object(child, selector, xforms, visible, entities);
			}
		}
		else {
			for (int i = 0; i < o.getNumChildren(); ++i) {
				IObject child = o.getChild(i);
				parse_object(child, selector, xforms, visible, entities);
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
			parse_object(archive->getTop(), selector, std::vector<M44d>(), true /* visible */, entities );
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

		virtual std::string propertyHash() const override {
			return _propertyHash;
		}

		std::shared_ptr<tinyobj::ObjReader> _sharedObjReader;

		CommonAttribute _common;

		// Standard Mesh
		std::unique_ptr<IVector3Column> _positions;
		std::unique_ptr<IVector3Column> _normals;
		std::unique_ptr<IVector2Column> _uvs;
		std::unique_ptr<IInt32Column> _faceCounts;
		std::unique_ptr<IInt32Column> _faceIndices;

		// Instanced Mesh
		std::string _propertyHash;
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
		int snprint(uint32_t i, char* dst, uint32_t buffersize) const override {
			float x = _ptr[i * 2];
			float y = _ptr[i * 2 + 1];
			return snprintf(dst, buffersize, "(%f, %f)", x, y);
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
		int snprint(uint32_t i, char* dst, uint32_t buffersize) const override {
			float x = _ptr[i * 3];
			float y = _ptr[i * 3 + 1];
			float z = _ptr[i * 3 + 2];
			return snprintf(dst, buffersize, "(%f, %f, %f)", x, y, z);
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