/**
 * @file ExtraDataBase.h
 * @author Silmaen
 * @date 18/10/2025
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Macros.h"
#include "data/extradata/ExtraDataRegistry.h"

/**
 * @brief
 *  Namespace for extra data management.
 */
namespace owl::data::extradata {
/**
 * @brief
 *  Base class for all extra data.
 */

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class OWL_API ExtraDataBase {
public:
	ExtraDataBase() = default;

	virtual ~ExtraDataBase() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	ExtraDataBase(const ExtraDataBase&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	ExtraDataBase(ExtraDataBase&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const ExtraDataBase&) -> ExtraDataBase& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(ExtraDataBase&&) -> ExtraDataBase& = default;

	/**
	 * @brief
	 *  Get the identifier of the extra-data type.
	 * @return The identifier given by ExtraDataRegistry.
	 */
	[[nodiscard]] virtual auto getPid() const -> ExtraDataPid = 0;

	/**
	 * @brief
	 *  Clone the extra data.
	 * @return A unique pointer to the cloned extra data.
	 */
	[[nodiscard]] virtual auto clone() const -> uniq<ExtraDataBase> = 0;
};
OWL_DIAG_POP

/**
 * @brief
 *  Template base class for all mesh extra data.
 * @tparam ExtraData Type of the extra data to implement.
 * @tparam DataType  Underlying data type.
 */
template<typename ExtraData, typename DataType>
class OWL_API MeshExtraData : public ExtraDataBase {
	friend ExtraData;

	MeshExtraData() = default;

public:
	/// Type of the stored data.
	using Type = DataType;

	/**
	 * @brief
	 *  Return the product identifier of the extra data.
	 * @note
	 *  This id should be unique.
	 * @return The product identifier of the extra data.
	 */
	[[nodiscard]] auto getPid() const -> ExtraDataPid override { return getExtraDataPid<ExtraData>(); }

	/**
	 * @brief
	 *  Clone the extra data.
	 * @return A unique pointer to the cloned extra data.
	 */
	[[nodiscard]] auto clone() const -> uniq<ExtraDataBase> override {
		return mkUniq<ExtraData>(static_cast<const ExtraData&>(*this));
	}

	/**
	 * @brief
	 *  Get the underlying value.
	 * @return The stored value.
	 */
	[[nodiscard]] virtual auto getValue() const -> const Type& = 0;
};

}// namespace owl::data::extradata
