#pragma once

#include <vector>
#include <functional>
#include <string>
#include <cmath>
#include <memory>

#include "pr.hpp"

namespace pr {
	enum IColumnType {
		ColumnType_Int32 = 0,
		ColumnType_Float,
		ColumnType_Vector3,
		ColumnType_String,
	};

	class IColumn
	{
	public:
		virtual ~IColumn() {}
		virtual uint64_t count() const = 0;
		virtual int snprint(uint32_t i, char* dst, uint32_t buffersize) const = 0;
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
	using IInt32ColumnPtr = std::shared_ptr<IInt32Column>;
	using IVector2ColumnPtr = std::shared_ptr<IVector2Column>;
	using IVector3ColumnPtr = std::shared_ptr<IVector3Column>;

	/*
	Flat Scene Entities
	*/
	enum FSceneEntityType {
		FSceneEntityType_PolygonMesh,
		//FSceneEntityType_Point,
		//FSceneEntityType_Curve,
		FSceneEntityType_Camera,
	};
	class FSceneEntity {
	public:
		virtual ~FSceneEntity() {}
		virtual FSceneEntityType type() const = 0;

		virtual std::string fullname() const = 0;
		virtual bool visible() const = 0;
		virtual glm::mat4 localToWorld() const = 0;
	};
	class FPolyMeshEntity : public FSceneEntity {
	public:
		FSceneEntityType type() const override { return FSceneEntityType_PolygonMesh; }

		/*
		  These return the same data if it refer the same kind of instance.
		*/
		virtual std::shared_ptr<IVector3Column> positions() const = 0;
		virtual std::shared_ptr<IVector3Column> normals() const = 0;
		virtual std::shared_ptr<IVector2Column> uvs() const = 0;
		virtual std::shared_ptr<IInt32Column> faceCounts() const = 0;
		virtual std::shared_ptr<IInt32Column> faceIndices() const = 0;

		//AttributeSpreadSheet points;
		//AttributeSpreadSheet vertices;
		//AttributeSpreadSheet primitives;

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
				std::shared_ptr<const T> d = dynamic_pointer_cast<const T>(e);
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
				std::shared_ptr<const T> d = dynamic_pointer_cast<const T>(e);
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
		Result open(const std::string& filePath, std::string& error_message);
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
}