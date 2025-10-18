/**
 * @file ExtraDataContainer.h
 * @author Silmaen
 * @date 20/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/extradata/ExtraDataBase.h"
#include <vector>

namespace owl::data::extradata {

/**
 * @brief
 *  This class implements a container for extra data.
 */
class OWL_API ExtraDataContainer {
public:
	using ExtraDataPtr = shared<ExtraDataBase>;
	using ExtraDataList = std::vector<ExtraDataPtr>;
	using ExtraDataIterator = ExtraDataList::iterator;
	using ConstExtraDataIterator = ExtraDataList::const_iterator;

	ExtraDataContainer() = delete;
	~ExtraDataContainer();
	ExtraDataContainer(const ExtraDataContainer&);
	ExtraDataContainer(ExtraDataContainer&&) noexcept;
	auto operator=(const ExtraDataContainer&) -> ExtraDataContainer&;
	auto operator=(ExtraDataContainer&&) noexcept -> ExtraDataContainer&;

	/**
	 * @brief
	 *  Constructor with extra data type and initial size.
	 * @param iEdPid       Product ID of the extra data.
	 * @param iInitialSize Initial size of the extra data container.
	 */
	ExtraDataContainer(core::FactoryPid iEdPid, size_t iInitialSize);

	/**
	 * @brief
	 *  Get extra data of given type.
	 * @tparam ExtraDataType Type of extra data to get.
	 * @param iIndex Index of extra data to get.
	 * @return Pointer on the extra data if found, nullptr otherwise.
	 */
	template<class ExtraDataType>
	auto getExtraDataAs(const size_t iIndex) const -> shared<ExtraDataType> {
		if (const ExtraDataPtr ed = getExtraData(iIndex)) {
			return std::static_pointer_cast<ExtraDataType>(ed);
		}
		return nullptr;
	}

	/**
	 * @brief
	 *  Get extra data of given type.
	 * @param iIndex Index of extra data to get.
	 * @return Pointer on the extra data if found, nullptr otherwise.
	 */
	[[nodiscard]] auto getExtraData(size_t iIndex) const -> ExtraDataPtr;

	/**
	 * @brief
	 *  Erase extra data at given index.
	 * @param iIndex Index of extra data to erase.
	 */
	void removeExtraData(size_t iIndex);

	/**
	 * @brief Get the Product ID of the extra data.
	 * @return The Product ID of the extra data.
	 */
	[[nodiscard]] auto getEdPid() const -> core::FactoryPid { return m_edPid; }

	/**
	 * @brief Resize the extra data container.
	 * @param iNewSize The new size of the extra data container.
	 */
	void resize(size_t iNewSize);

private:
	/// Product ID of the extra data.
	core::FactoryPid m_edPid{core::INVALID_FACTORY_PID};
	/**
	 * @brief
	 *  Clone the current extra data container.
	 * @return A copy of the current extra data container.
	 */
	[[nodiscard]] auto clone() const -> ExtraDataContainer;
	/**
	 * @brief Initialize or reinitialize the extra data container.
	 */
	void init();
	/// List of extra data.
	ExtraDataList m_extraDataList;
};

}// namespace owl::data::extradata
