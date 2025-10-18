/**
 * @file ExtraDataTable.h
 * @author Silmaen
 * @date 22/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once


#include "data/extradata/ExtraDataContainer.h"

namespace owl::data::extradata {

/**
 * @brief
 *  This class implements a table of extra data.
 */
class OWL_API ExtraDataTable {
public:
	/**
	 * @brief
	 *  Default constructor
	 */
	ExtraDataTable();
	/**
	 * @brief
	 *  Default destructor
	 */
	~ExtraDataTable();
	ExtraDataTable(const ExtraDataTable&) = default;
	ExtraDataTable(ExtraDataTable&&) noexcept = default;
	auto operator=(const ExtraDataTable&) -> ExtraDataTable& = default;
	auto operator=(ExtraDataTable&&) noexcept -> ExtraDataTable& = default;

	/**
	 * @brief
	 *  Clone the extra data table.
	 * @return A copy of the extra data table.
	 */
	[[nodiscard]] auto clone() const -> ExtraDataTable;

	/**
	 * @brief
	 *  Clear all extra data from the table.
	 */
	void clear();

	/**
	 * @brief
	 *  Check if extra data of given type is defined.
	 * @tparam ExtraDataType Type of extra data to check.
	 * @return True if extra data is defined, false otherwise.
	 */
	template<class ExtraDataType>
	[[nodiscard]] auto isExtraDataDefined() const -> bool {
		return isExtraDataDefined(core::getFactoryPid<ExtraDataType>());
	}

	/**
	 * @brief
	 *  Add extra data to the container.
	 * @note
	 *  Each type of extra data should be unique.
	 * @note
	 *  The function takes the ownership on iExtraData.
	 * @tparam ExtraDataType Type of extra data to add.
	 * @param iExtraData Extra data to put in the list.
	 * @return True if extra data has been added, false if extra data of the same type is already defined.
	 */
	template<typename ExtraDataType>
	auto addExtraData(ExtraDataType& iExtraData) -> bool {
		return addExtraData(core::getFactoryPid<ExtraDataType>(), iExtraData);
	}

	/**
	 * @brief
	 *  Delete extra data of given type.
	 * @tparam ExtraDataType Type of extra data to delete.
	 * @return True if extra data has been found and deleted, false otherwise.
	 */
	template<typename ExtraDataType>
	auto deleteExtraData() -> bool {
		return deleteExtraData(core::getFactoryPid<ExtraDataType>());
	}
	/**
	 * @brief
	 *  Check if extra data of given type is defined.
	 * @param iPid Product ID of extra data to check.
	 * @return True if extra data is defined, false otherwise.
	 */
	[[nodiscard]] auto isExtraDataDefined(core::FactoryPid iPid) const -> bool;
	/**
	 * @brief
	 *  Add extra data to the container.
	 * @note
	 *  Each type of extra data should be unique.
	 * @note
	 *  The function takes the ownership on iExtraData.
	 * @param iPid Product ID of extra data to add.
	 * @return True if extra data has been added, false if extra data of the same type is already defined.
	 */
	auto addExtraData(core::FactoryPid iPid) -> bool;
	/**
	 * @brief
	 *  Delete extra data of given type.
	 * @param iPid Product ID of extra data to delete.
	 * @return True if extra data has been found and deleted, false otherwise.
	 */
	auto deleteExtraData(core::FactoryPid iPid) -> bool;

	/**
	 * @brief
	 *  Get extra data container of given type.
	 * @param iPid Product ID of extra data to get.
	 * @return The extra data container.
	 */
	[[nodiscard]] auto getExtraData(core::FactoryPid iPid) const -> const ExtraDataContainer*;
	/**
	 * @brief
	 *  Remove element at given index from all extra data containers.
	 * @param iIndex Index of the element to remove.
	 */
	void removeElement(size_t iIndex);

	/**
	 * @brief
	 *  Resize the extra data containers.
	 * @param iNewSize The new size of the extra data containers.
	 */
	void resize(size_t iNewSize);

private:
	/**
	 * @brief
	 *  Size of the extra data containers, they should all have the same.
	 */
	size_t m_size{0};
	/** @brief
	 *  List containing ExtraDataContainer.
	 *  Each ExtraDataContainer contains only one ExtraData type but as many as the object where ExtraData
	 *  is related (eg. Vertex, Triangle, etc.).
	 */
	std::list<ExtraDataContainer> m_extraDataList;
};

}// namespace owl::data::extradata
