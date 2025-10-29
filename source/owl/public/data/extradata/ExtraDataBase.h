/**
 * @file ExtraDataBase.h
 * @author Silmaen
 * @date 18/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/IFactory.h"

/**
 * @brief Namespace for extra data management.
 */
namespace owl::data::extradata {
/**
 * @brief Base class for all extra data.
 */
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class OWL_API ExtraDataBase : public core::FactoryProduct {
public:
	/**
	 * @brief Clone the extra data.
	 * @return A unique pointer to the cloned extra data.
	 */
	[[nodiscard]] virtual auto clone() const -> std::unique_ptr<ExtraDataBase> = 0;
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
public:
	using Type = DataType;
	/**
	 * @brief
	 *  Return the product identifier of the extra data.
	 * @note
	 *  This id should be unique.
	 * @return The product identifier of the extra data.
	 */
	[[nodiscard]] auto getPid() const -> core::FactoryPid override { return core::getFactoryPid<ExtraData>(); }

	/**
	 * @brief Clone the extra data.
	 * @return A unique pointer to the cloned extra data.
	 */
	[[nodiscard]] auto clone() const -> std::unique_ptr<ExtraDataBase> override {
		return std::make_unique<ExtraData>(static_cast<const ExtraData&>(*this));
	}

	/**
	 * @brief Get the underlying value.
	 * @return The stored value.
	 */
	[[nodiscard]] virtual auto getValue() const -> const Type& = 0;

	/**
	 * @brief Set the underlying value.
	 * @param iValue The value to set.
	 */
	virtual void setValue(const Type& iValue) = 0;
	/**
	 * @brief Set the underlying value.
	 * @param iValue The value to set.
	 */
	virtual void setValue(Type&& iValue) = 0;
};

}// namespace owl::data::extradata
