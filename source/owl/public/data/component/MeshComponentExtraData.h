/**
 * @file MeshComponentExtraData.h
 * @author Silmaen
 * @date 20/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/IFactory.h"
#include "data/component/MeshComponent.h"
#include "data/geometry/MeshCursorBase.h"

namespace owl::data::geometry {
class StaticMesh;
}// namespace owl::data::geometry

namespace owl::data::component {


template<typename TExtraData, geometry::MeshElementType ElementType>
class MeshExtraDataReader;
template<typename TExtraData, geometry::MeshElementType ElementType>
class MeshExtraDataWriter;
template<typename ExtraDataConversionType, geometry::MeshElementType ElementType>
class MeshExtraDataReaderPid;
template<typename ExtraDataConversionType, geometry::MeshElementType ElementType>
class MeshExtraDataWriterPid;

/**
 * @brief
 *  Reader for static mesh extra data.
 * @tparam TExtraData Type of the extra data to read.
 * @tparam ElementType The type of element
 */
template<typename TExtraData, geometry::MeshElementType ElementType>
struct OWL_API ReadMeshExtraData {
	using DataType = TExtraData;
	using ComponentType = MeshExtraDataReader<TExtraData, ElementType>;

	/**
	 * @brief
	 *  Constructor.
	 */
	ReadMeshExtraData() = default;

	/**
	 * @brief
	 *  Get the extra data product id.
	 * @return The extra data product id.
	 */
	[[nodiscard]] static auto getPid() -> core::FactoryPid { return core::getFactoryPid<TExtraData>(); }
};

/**
 * @brief
 *  Reader for vertex extra data.
 * @tparam TExtraData Type of the extra data to read.
 */
template<typename TExtraData>
struct OWL_API ReadMeshVertexExtraData : public ReadMeshExtraData<TExtraData, geometry::MeshElementType::Vertex> {
	using ComponentType = MeshExtraDataReader<TExtraData, geometry::MeshElementType::Vertex>;
	using ReadMeshExtraData<TExtraData, geometry::MeshElementType::Vertex>::ReadMeshExtraData;
};
/**
 * @brief
 *  Reader for triangle extra data.
 * @tparam TExtraData Type of the extra data to read.
 */
template<typename TExtraData>
struct OWL_API ReadMeshTriangleExtraData : public ReadMeshExtraData<TExtraData, geometry::MeshElementType::Triangle> {
	using ComponentType = MeshExtraDataReader<TExtraData, geometry::MeshElementType::Triangle>;
	using ReadMeshExtraData<TExtraData, geometry::MeshElementType::Triangle>::ReadMeshExtraData;
};

/**
 * @brief
 *  Read/Write structure for an extra data.
 * @tparam TExtraData Type of extra data to write.
 * @tparam ElementType The type of element
 */
template<typename TExtraData, geometry::MeshElementType ElementType>
struct OWL_API WriteMeshExtraData : public ReadMeshExtraData<TExtraData, ElementType> {
	using ComponentType = MeshExtraDataWriter<TExtraData, ElementType>;
	using ReadMeshExtraData<TExtraData, ElementType>::ReadMeshExtraData;
};
/**
 * @brief
 *  Read structure for a vertex extra data.
 * @tparam TExtraData Type of extra data to read.
 */
template<typename TExtraData>
struct OWL_API WriteMeshVertexExtraData : public WriteMeshExtraData<TExtraData, geometry::MeshElementType::Vertex> {
	using ComponentType = MeshExtraDataWriter<TExtraData, geometry::MeshElementType::Vertex>;
	using WriteMeshExtraData<TExtraData, geometry::MeshElementType::Vertex>::WriteMeshExtraData;
};
/**
 * @brief
 *  Read structure for a triangle extra data.
 * @tparam TExtraData Type of extra data to read.
 */
template<typename TExtraData>
struct OWL_API WriteMeshTriangleExtraData : public WriteMeshExtraData<TExtraData, geometry::MeshElementType::Triangle> {
	using ComponentType = MeshExtraDataWriter<TExtraData, geometry::MeshElementType::Triangle>;
	using WriteMeshExtraData<TExtraData, geometry::MeshElementType::Triangle>::WriteMeshExtraData;
};

/**
 * @brief
 *  Read structure for an extra data from a PID.
 * @tparam ExtraDataConversionType Type of extra data to write.
 * @tparam ElementType The type of element
 */
template<typename ExtraDataConversionType, geometry::MeshElementType ElementType>
struct OWL_API ReadMeshExtraDataPid : public ReadMeshExtraData<ExtraDataConversionType, ElementType> {
	using ComponentType = MeshExtraDataReaderPid<ExtraDataConversionType, ElementType>;
	using ReadMeshExtraData<ExtraDataConversionType, ElementType>::ReadMeshExtraData;
};

/**
 * @brief
 *  Read/Write structure for an extra data from a PID.
 * @tparam ExtraDataConversionType Type of extra data to write.
 * @tparam ElementType The type of element
 */
template<typename ExtraDataConversionType, geometry::MeshElementType ElementType>
struct OWL_API WriteMeshExtraDataPid : public ReadMeshExtraDataPid<ExtraDataConversionType, ElementType> {
	using ComponentType = MeshExtraDataWriterPid<ExtraDataConversionType, ElementType>;
	using ReadMeshExtraDataPid<ExtraDataConversionType, ElementType>::ReadMeshExtraDataPID;
};

/**
 * @brief
 *   Read iterate component for an extra data.
 * @tparam TExtraData Extra data type to iterate.
 * @tparam ElementType The type of element
 */
template<typename TExtraData, geometry::MeshElementType ElementType>
class OWL_API MeshExtraDataReader : public MeshComponentBase<true, ElementType> {
	using CursorType = MeshComponentBase<true, ElementType>::CursorType;

public:
	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iExtraData Extra data to iterate.
	 * @param[in] iCursor Cursor used to iterate.
	 * @param[in] iIndex Starting index.
	 * @param[in] iReset True if the component's iterator must be reset.
	 */
	MeshExtraDataReader(const ReadMeshExtraData<TExtraData, ElementType>& iExtraData, CursorType& iCursor,
						const size_t iIndex = 0, const bool iReset = true)
		: MeshComponentBase<true, ElementType>(iCursor), m_extraData(&iExtraData),
		  m_extraDataContainer(const_cast<::owl::data::extradata::ExtraDataContainer*>(
				  this->m_cursor->getExtraDataContainer(m_extraData->getPid()))) {
		if (iReset)
			reset(iIndex);
	}

	/**
	 * @brief
	 *  Copy constructor.
	 */
	MeshExtraDataReader(const MeshExtraDataReader&) = default;

	/**
	 * @brief
	 *  Assignment operator.
	 */
	auto operator=(const MeshExtraDataReader&) -> MeshExtraDataReader& = default;

	/**
	 * @brief
	 *  Deleted move constructor.
	 */
	MeshExtraDataReader(MeshExtraDataReader&&) noexcept = delete;

	/**
	 * @brief
	 *  Deleted assignment operator.
	 */
	auto operator=(MeshExtraDataReader&&) noexcept -> MeshExtraDataReader& = delete;

	/**
	 * @brief Destructor
	 */
	~MeshExtraDataReader() = default;
	/**
	 * @brief
	 *  Reset the component's iterator.
	 * @param[in] iIndex Starting index.
	 */
	void reset(const size_t iIndex = 0) { m_currentIndex = iIndex; }

	/**
	 * @brief
	 *  Move to the next element.
	 * @param[in] iIncrement Increment value.
	 */
	void moveNext(const size_t iIncrement = 1) { m_currentIndex += iIncrement; }

	/**
	 * @brief
	 *  Check if the extra data is defined for the current element.
	 * @retval True if the extra data is defined.
	 * @retval False otherwise.
	 */
	[[nodiscard]] auto hasValue() const -> bool {
		if (m_extraDataContainer == nullptr)
			return false;
		auto ptr = m_extraDataContainer->getExtraDataAs<TExtraData>(m_currentIndex);
		return ptr != nullptr;
	}

	/**
	 * @brief
	 *  Get the extra data of the current element.
	 * @return An extra data.
	 */
	auto value() const -> shared<TExtraData> {
		OWL_CORE_ASSERT(this->hasValue(), "MeshExtraDataReader::value")
		return m_extraDataContainer==nullptr ? nullptr : m_extraDataContainer->getExtraDataAs<TExtraData>(m_currentIndex);
	}
	/**
	 * @brief
	 *  Get the extra data of the current element.
	 * @return An extra data.
	 */
	auto value() -> shared<TExtraData> {
		OWL_CORE_ASSERT(this->hasValue(), "MeshExtraDataReader::value")
		return m_extraDataContainer==nullptr ? nullptr : m_extraDataContainer->getExtraDataAs<TExtraData>(m_currentIndex);
	}

protected:
	using ExtraDataIterator = typename std::vector<typename TExtraData::Type>::iterator;

	/// Extra data to iterate.
	const ReadMeshExtraData<TExtraData, ElementType>* m_extraData;
	/// Iterator on extra data.
	extradata::ExtraDataContainer* m_extraDataContainer = nullptr;
	size_t m_currentIndex = 0;
};

/**
 * @brief
 *   Read iterate component for a vertex extra data.
 * @tparam TExtraData Extra data type to iterate.
 */
template<typename TExtraData>
class OWL_API MeshVertexExtraDataReader : public MeshExtraDataReader<TExtraData, geometry::MeshElementType::Vertex> {
	using CursorType = ReadMeshExtraData<TExtraData, geometry::MeshElementType::Vertex>::CursorType;
	MeshVertexExtraDataReader(const ReadMeshExtraData<TExtraData, geometry::MeshElementType::Vertex>& iExtraData,
							  CursorType& iCursor, const size_t iIndex = 0, const bool iReset = true)
		: MeshExtraDataReader<TExtraData, geometry::MeshElementType::Vertex>(iExtraData, iCursor, iIndex, iReset) {}
};

/**
 * @brief
 *   Read iterate component for a triangle extra data.
 * @tparam TExtraData Extra data type to iterate.
 */
template<typename TExtraData>
class OWL_API MeshTriangleExtraDataReader : public MeshExtraDataReader<TExtraData, geometry::MeshElementType::Vertex> {
	using CursorType = ReadMeshExtraData<TExtraData, geometry::MeshElementType::Vertex>::CursorType;
	MeshTriangleExtraDataReader(const ReadMeshExtraData<TExtraData, geometry::MeshElementType::Vertex>& iExtraData,
								CursorType& iCursor, const size_t iIndex = 0, const bool iReset = true)
		: MeshExtraDataReader<TExtraData, geometry::MeshElementType::Vertex>(iExtraData, iCursor, iIndex, iReset) {}
};

/**
 * @brief
 *   Read/Write iterate component for an extra data.
 * @note
 *  The extra data is created if it is not defined in cloud or unshared if already defined.
 *  So the component can be thread safe if the range is created before the multithreaded call.
 * @tparam TExtraData Extra data type to iterate.
 */
template<typename TExtraData, geometry::MeshElementType ElementType>
class OWL_API MeshExtraDataWriter : public MeshExtraDataReader<TExtraData, ElementType> {
public:
	using BaseExtraDataReader = MeshExtraDataReader<TExtraData, ElementType>;

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iExtraData Extra data to iterate.
	 * @param[in] iCursor Cursor used to iterate.
	 * @param[in] iIndex Starting index.
	 */
	MeshExtraDataWriter(const WriteMeshExtraData<TExtraData, ElementType>& iExtraData,
						geometry::MeshCursorBase<false, ElementType>& iCursor, size_t iIndex = 0)
		: BaseExtraDataReader(iExtraData, iCursor, iIndex, false) {
		this->reset(iIndex);
	}

	/**
	 * @brief
	 *  Set the extra data of the current vertex.
	 * @param[in] iExtraData New value of the extra data.
	 */
	template<typename T = TExtraData>
	void setValue(const TExtraData& iExtraData) {
		*this->m_ExtraDataIte = iExtraData.GetValue();
	}


	/**
	 * @brief
	 *  Create and set the extra data of the current vertex.
	 * @param[in] iArgs args for create the extra data to set.
	 */
	template<typename... Args>
	void setValue(Args&&... iArgs) {
		TExtraData ed(std::forward<Args>(iArgs)...);
		*this->m_ExtraDataIte = std::move(ed.GetValue());
	}
};

/**
 * @brief
 *   Read/Write iterate component for a vertex extra data.
 * @tparam TExtraData Extra data type to iterate.
 */
template<typename TExtraData>
class OWL_API MeshVertexExtraDataWriter : public MeshExtraDataWriter<TExtraData, geometry::MeshElementType::Vertex> {
	using CursorType = WriteMeshExtraData<TExtraData, geometry::MeshElementType::Vertex>::CursorType;
	MeshVertexExtraDataWriter(const WriteMeshExtraData<TExtraData, geometry::MeshElementType::Vertex>& iExtraData,
							  CursorType& iCursor, const size_t iIndex = 0, const bool iReset = true)
		: MeshExtraDataWriter<TExtraData, geometry::MeshElementType::Vertex>(iExtraData, iCursor, iIndex, iReset) {}
};

/**
 * @brief
 *   Read/Write iterate component for a triangle extra data.
 * @tparam TExtraData Extra data type to iterate.
 */
template<typename TExtraData>
class OWL_API MeshTriangleExtraDataWriter : public MeshExtraDataWriter<TExtraData, geometry::MeshElementType::Vertex> {
	using CursorType = WriteMeshExtraData<TExtraData, geometry::MeshElementType::Vertex>::CursorType;
	MeshTriangleExtraDataWriter(const WriteMeshExtraData<TExtraData, geometry::MeshElementType::Vertex>& iExtraData,
								CursorType& iCursor, const size_t iIndex = 0, const bool iReset = true)
		: MeshExtraDataWriter<TExtraData, geometry::MeshElementType::Vertex>(iExtraData, iCursor, iIndex, iReset) {}
};

/**
 * @brief
 *   Read iterate component for an extra data from a PID.
 * @tparam ExtraDataConversionType Type used to interpret fetched extra data.
 * @tparam ElementType The type of element
 */
template<typename ExtraDataConversionType, geometry::MeshElementType ElementType>
class OWL_API MeshExtraDataReaderPid : public MeshComponentBase<true, ElementType> {
	using CursorType = MeshComponentBase<true, ElementType>::CursorType;

public:
	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iExtraDataPid Extra data PID to iterate.
	 * @param[in] iCursor Cursor used to iterate.
	 * @param[in] iIndex Starting index.
	 * @param[in] iReset True if the component's iterator must be reset.
	 */
	MeshExtraDataReaderPid(const ReadMeshExtraDataPid<ExtraDataConversionType, ElementType>& iExtraDataPid,
						   CursorType& iCursor, const size_t iIndex = 0, const bool iReset = true)
		: MeshComponentBase<true, ElementType>(iCursor), m_extraDataPid(iExtraDataPid.extraDataId) {
		if (iReset)
			reset(iIndex);
	}
	MeshExtraDataReaderPid(const MeshExtraDataReaderPid&) = default;
	auto operator=(const MeshExtraDataReaderPid&) -> MeshExtraDataReaderPid& = default;
	MeshExtraDataReaderPid(MeshExtraDataReaderPid&&) noexcept = delete;
	auto operator=(MeshExtraDataReaderPid&&) noexcept -> MeshExtraDataReaderPid& = delete;

	/**
	 * @brief Destructor
	 */
	~MeshExtraDataReaderPid() = default;

	/**
	 * @brief
	 *  Reset the component's iterator.
	 * @param[in] iIndex Starting index.
	 */
	void reset(size_t iIndex = 0) {
		m_extraDataIte = ExtraDataIterator(this->m_cursor->GetExtraDataIterator(m_extraDataPid, iIndex));
	}

	/**
	 * @brief
	 *  Move to the next element.
	 * @param[in] iIncrement Increment value.
	 */
	void moveNext(size_t iIncrement = 1) {
		if (m_extraDataIte.IsValid()) {
			if (iIncrement == 1)
				++m_extraDataIte;
			else
				m_extraDataIte += iIncrement;
		}
	}

	/**
	 * @brief
	 *  Check if the extra data is defined for the current element.
	 * @retval True if the extra data is defined.
	 * @retval False otherwise.
	 */
	[[nodiscard]] auto hasValue() const -> bool { return m_extraDataIte.isValid(); }

	/**
	 * @brief
	 *  Get the extra data of the current element.
	 * @return An extra data.
	 */
	auto value() const -> ExtraDataConversionType {
		OWL_CORE_ASSERT(this->hasValue(), "MeshExtraDataReaderPiD::value")
		return *static_cast<ExtraDataConversionType*>(*m_extraDataIte);
	}

protected:
	using ExtraDataIterator = typename std::vector<typename ExtraDataConversionType::Type>::iterator;

	/// Iterator on extra data.
	ExtraDataIterator m_extraDataIte;

	/// ExtraData PID.
	core::FactoryPid m_extraDataPid;
};

/**
 * @brief
 *   Read iterate component for a vertex extra data from a PID.
 * @tparam TExtraData Extra data type to iterate.
 */
template<typename TExtraData>
class OWL_API MeshVertexExtraDataReaderPid
	: public MeshExtraDataReaderPid<TExtraData, geometry::MeshElementType::Vertex> {
	using CursorType = ReadMeshExtraDataPid<TExtraData, geometry::MeshElementType::Vertex>::CursorType;
	MeshVertexExtraDataReaderPid(const ReadMeshExtraDataPid<TExtraData, geometry::MeshElementType::Vertex>& iExtraData,
								 CursorType& iCursor, const size_t iIndex = 0, const bool iReset = true)
		: MeshExtraDataReaderPid<TExtraData, geometry::MeshElementType::Vertex>(iExtraData, iCursor, iIndex, iReset) {}
};

/**
 * @brief
 *   Read iterate component for a triangle extra data from a PID.
 * @tparam TExtraData Extra data type to iterate.
 */
template<typename TExtraData>
class OWL_API MeshTriangleExtraDataReaderPid
	: public MeshExtraDataReaderPid<TExtraData, geometry::MeshElementType::Vertex> {
	using CursorType = ReadMeshExtraDataPid<TExtraData, geometry::MeshElementType::Vertex>::CursorType;
	MeshTriangleExtraDataReaderPid(
			const ReadMeshExtraDataPid<TExtraData, geometry::MeshElementType::Vertex>& iExtraData, CursorType& iCursor,
			const size_t iIndex = 0, const bool iReset = true)
		: MeshExtraDataReaderPid<TExtraData, geometry::MeshElementType::Vertex>(iExtraData, iCursor, iIndex, iReset) {}
};

/**
 * @brief
 *   Read/Write iterate component for an extra data from a PID.
 * @note
 *  The extra data is created if it is not defined in cloud or unshared if already defined.
 *  So the component can be thread safe if the range is created before the multithreaded call.
 * @tparam ExtraDataConversionType Type used to interpret fetched extra data.
 * @tparam ElementType The type of element
 */
template<typename ExtraDataConversionType, geometry::MeshElementType ElementType>
class OWL_API MeshExtraDataWriterPid : public MeshExtraDataReaderPid<ExtraDataConversionType, ElementType> {
public:
	using BaseExtraDataReader = MeshExtraDataReaderPid<ExtraDataConversionType, ElementType>;

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iExtraDataPID Extra data PID to iterate.
	 * @param[in] iCursor Cursor used to iterate.
	 * @param[in] iIndex Starting index.
	 */
	MeshExtraDataWriterPid(const WriteMeshExtraDataPid<ExtraDataConversionType, ElementType>& iExtraDataPID,
						   geometry::MeshCursorBase<false, ElementType>& iCursor, size_t iIndex = 0)
		: BaseExtraDataReader(iExtraDataPID, iCursor, iIndex, false) {
		this->reset(iIndex);
	}

	/**
	 * @brief
	 *  Set the extra data of the current point.
	 * @param[in] iExtraData New value of the extra data.
	 */
	template<typename T = ExtraDataConversionType>
	void setValue(const ExtraDataConversionType& iExtraData) {
		auto* ptr = this->m_extraDataContainer->template getExtraDataAs<ExtraDataConversionType>(this->m_currentIndex);
		OWL_CORE_ASSERT(ptr != nullptr, "MeshExtraDataWriter::setValue")
		*ptr = iExtraData;
	}

	template<typename... Args>
	void setValue(Args&&... iArgs) {
		ExtraDataConversionType ed(std::forward<Args>(iArgs)...);
		setValue(ed);
	}
};

/**
 * @brief
 *   Read/Write iterate component for a vertex extra data from a PID.
 * @tparam TExtraData Extra data type to iterate.
 */
template<typename TExtraData>
class OWL_API MeshVertexExtraDataWriterPid
	: public MeshExtraDataWriterPid<TExtraData, geometry::MeshElementType::Vertex> {
	using CursorType = WriteMeshExtraDataPid<TExtraData, geometry::MeshElementType::Vertex>::CursorType;
	MeshVertexExtraDataWriterPid(const WriteMeshExtraDataPid<TExtraData, geometry::MeshElementType::Vertex>& iExtraData,
								 CursorType& iCursor, const size_t iIndex = 0, const bool iReset = true)
		: MeshExtraDataWriterPid<TExtraData, geometry::MeshElementType::Vertex>(iExtraData, iCursor, iIndex, iReset) {}
};

/**
 * @brief
 *   Read/Write iterate component for a triangle extra data from a PID.
 * @tparam TExtraData Extra data type to iterate.
 */
template<typename TExtraData>
class OWL_API MeshTriangleExtraDataWriterPid
	: public MeshExtraDataWriterPid<TExtraData, geometry::MeshElementType::Vertex> {
	using CursorType = WriteMeshExtraDataPid<TExtraData, geometry::MeshElementType::Vertex>::CursorType;
	MeshTriangleExtraDataWriterPid(
			const WriteMeshExtraDataPid<TExtraData, geometry::MeshElementType::Vertex>& iExtraData, CursorType& iCursor,
			const size_t iIndex = 0, const bool iReset = true)
		: MeshExtraDataWriterPid<TExtraData, geometry::MeshElementType::Vertex>(iExtraData, iCursor, iIndex, iReset) {}
};

}// namespace owl::data::component
