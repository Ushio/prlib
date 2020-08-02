#pragma once

/*
 Geometry Modules
 You can use this as also itself only. but usually #include "pr.hpp" is an easy way.
*/

#include <vector>
#include <functional>
#include <string>
#include <cmath>
#include <memory>

#include "glm/glm.hpp"
#include "glm/ext.hpp"

namespace pr {
	enum class WindingOrder
	{
		WindingOrder_CW,
		WindingOrder_CCW
	};

	class IColumn
	{
	public:
		virtual ~IColumn() {}
		virtual int64_t count() const = 0;
	};
	class IInt32Column : public IColumn {
	public:
		virtual const int32_t* data() const = 0;
	};
	class IVector2Column : public IColumn {
	public:
		virtual const glm::vec2* data() const = 0;
	};
	class IVector3Column : public IColumn {
	public:
		virtual const glm::vec3* data() const = 0;
	};

	template <class T>
	class ColumnView
	{
	public:
		template <class U> 
		ColumnView(const U* col):_ptr(col ? col->data() : nullptr ), _count(col ? col->count() : 0) {

		}
		ColumnView(const T* ptr, int64_t count):_ptr(ptr), _count(count) {

		}
		const T& operator[](int64_t i) const {
			return _ptr[i];
		}
		int64_t count() const {
			return _count;
		}
		const T* begin() const {
			return _ptr;
		}
		const T* end() const {
			return _ptr + _count;
		}
		bool empty() const {
			return _count == 0;
		}
	private:
		const T* _ptr;
		int64_t _count;
	};

	/*
	Houdini Specific
	*/
	enum class AttributeType {
		Int = 0,
		Float,
		Vector2,
		Vector3,
		Vector4,
		String,
	};
	inline const char* attributeTypeToVexTypeString(AttributeType type) {
		static const char* types[] = {
			"int",
			"float",
			"vector2",
			"vector",
			"vector4",
			"string",
		};
		return types[(int)type];
	}

	class AttributeColumn {
	public:
		AttributeColumn() {}
		AttributeColumn(const AttributeColumn&) = delete;
		void operator=(const AttributeColumn&) = delete;

		virtual ~AttributeColumn() {}
		virtual AttributeType attributeType() const = 0;
		virtual int64_t count() const = 0;
		virtual std::string getAsString(int64_t index) const = 0; /* It's slow. just for testing */
	};
	class AttributeIntColumn : public AttributeColumn {
		AttributeType attributeType() const override {
			return AttributeType::Int;
		}
		virtual int32_t get(int64_t index) const = 0;
	};
	class AttributeFloatColumn : public AttributeColumn {
	public:
		AttributeType attributeType() const override {
			return AttributeType::Float;
		}
		virtual float get(int64_t index) const = 0;
	};
	class AttributeVector2Column : public AttributeColumn {
	public:
		AttributeType attributeType() const override {
			return AttributeType::Vector2;
		}
		virtual glm::vec2 get(int64_t index) const = 0;
	};
	class AttributeVector3Column : public AttributeColumn {
	public:
		AttributeType attributeType() const override {
			return AttributeType::Vector3;
		}
		virtual glm::vec3 get(int64_t index) const = 0;
	};
	class AttributeVector4Column : public AttributeColumn {
	public:
		AttributeType attributeType() const override {
			return AttributeType::Vector4;
		}
		virtual glm::vec4 get(int64_t index) const = 0;
	};
	class AttributeStringColumn : public AttributeColumn {
	public:
		AttributeType attributeType() const override {
			return AttributeType::String;
		}
		virtual const std::string& get(int64_t index) const = 0;
	};

	class AttributeSpreadsheet {
	public:
		virtual ~AttributeSpreadsheet() {}

		virtual int64_t rowCount() const = 0;
		virtual int64_t columnCount() const = 0;
		virtual const std::vector<std::string>& keys() const = 0;
		virtual const AttributeColumn* column(const char* key) const = 0;

		const AttributeFloatColumn* columnAsFloat(const char* key) const
		{
			return dynamic_cast<const AttributeFloatColumn*>(column(key));
		}
		const AttributeIntColumn* columnAsInt(const char* key) const
		{
			return dynamic_cast<const AttributeIntColumn*>(column(key));
		}
		const AttributeStringColumn* columnAsString(const char* key) const
		{
			return dynamic_cast<const AttributeStringColumn*>(column(key));
		}
		const AttributeVector2Column* columnAsVector2(const char* key) const
		{
			return dynamic_cast<const AttributeVector2Column*>(column(key));
		}
		const AttributeVector3Column* columnAsVector3(const char* key) const
		{
			return dynamic_cast<const AttributeVector3Column*>(column(key));
		}
		const AttributeVector4Column* columnAsVector4(const char* key) const
		{
			return dynamic_cast<const AttributeVector4Column*>(column(key));
		}
	};
	
	/*
	Flat Scene Entities
	*/
	enum class FSceneEntityType {
		PolygonMesh,
	};
	class FSceneEntity {
	public:
		virtual ~FSceneEntity() {}
		virtual FSceneEntityType type() const = 0;

		virtual std::string fullname() const = 0;
		virtual bool visible() const = 0;
		virtual glm::mat4 localToWorld() const = 0;
	};

	enum class AttributeSpreadsheetType : int
	{
		Points = 0,
		Vertices,
		Primitives,
		Details,
	};

	class FPolyMeshEntity : public FSceneEntity {
	public:
		FSceneEntityType type() const override { return FSceneEntityType::PolygonMesh; }

		/*
		  These return the same data if it refer the same kind of instance.
		  The data's live cycles belong to this entity.
		*/
		virtual WindingOrder winingOrder() const = 0;
		virtual IVector3Column* positions() const = 0;
		virtual IVector3Column* normals() const = 0; // optional
		virtual IVector2Column* uvs() const = 0; // optional
		virtual IInt32Column* faceCounts() const = 0;
		virtual IInt32Column* faceIndices() const = 0;

		/*
			These are houdini specific.
		*/
		virtual AttributeSpreadsheet* attributeSpreadsheet(AttributeSpreadsheetType type) const = 0;

		// return HashCode It's ok to use as key for identifying instance source.
		virtual std::string propertyHash() const = 0;
	};
	class FScene {
	public:
		FScene(const std::vector<std::shared_ptr<FSceneEntity>>& entities) :_entities(entities) 
		{
		}
		enum class VisitorAction {
			Continue,
			Break,
		};
		template <class T>
		void visit(std::function<VisitorAction(std::shared_ptr<const T>)> visitor) const
		{
			for (std::shared_ptr<const FSceneEntity> e : _entities)
			{
				std::shared_ptr<const T> d = std::dynamic_pointer_cast<const T>(e);
				if (d)
				{
					if (visitor(d) == VisitorAction::Break)
					{
						break;
					}
				}
			}
		}
		template <class T>
		void visit(std::function<void(std::shared_ptr<const T>)> visitor) const
		{
			for (std::shared_ptr<const FSceneEntity> e : _entities)
			{
				std::shared_ptr<const T> d = std::dynamic_pointer_cast<const T>(e);
				if (d)
				{
					visitor(d);
				}
			}
		}
		void visitPolyMesh(std::function<void(std::shared_ptr<const FPolyMeshEntity>)> visitor) const { visit(visitor); }
		void visitPolyMesh(std::function<VisitorAction(std::shared_ptr<const FPolyMeshEntity>)> visitor) const { visit(visitor); }
	private:
		std::vector<std::shared_ptr<FSceneEntity>> _entities;
	};

	class AbcArchive
	{
	public:
		enum class Result {
			Sucess,
			Failure
		};
		Result open(const std::string& filePath /* it doesn't refer GetDataPath */, std::string& error_message);
		bool isOpened() const;
		void close();

		std::shared_ptr<FScene> readFlat(uint32_t index, std::string& error_message) const;

		/*
			archive informations
		*/
		uint32_t frameCount() const {
			return _frameCount;
		}
		std::string applicationWriter() const {
			return _applicationWriter;
		}
		std::string alembicVersion() const {
			return _alembicVersion;
		}
		uint32_t alembicApiVersion() const {
			return _alembicApiVersion;
		}
		std::string dateWritten() const {
			return _dateWritten;
		}
		std::string userDescription() const {
			return _userDescription;
		}

		// Be careful. This is an optional. It could be zero.
		// It highly depends on its exporter.
		double DDCFPS() const {
			return _DCCFPS;
		}
	private:
		std::shared_ptr<void> _alembicArchive;

		uint32_t _frameCount = 0;
		std::string _applicationWriter;
		std::string _alembicVersion;
		uint32_t _alembicApiVersion;
		std::string _dateWritten;
		std::string _userDescription;
		double _DCCFPS = 0.0;
	};


	std::shared_ptr<FScene> ReadWavefrontObj(const std::string& filePath /* it doesn't refer GetDataPath */, std::string& error_message);
}