/**
 * @file ExtraDataRegistry.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstdint>
#include <typeindex>
#include <typeinfo>

namespace owl::data::extradata {

class ExtraDataBase;

/// Identifier of a registered extra-data type, valid for the life of the process.
using ExtraDataPid = int32_t;

/// Identifier of an extra-data type that is not registered.
constexpr ExtraDataPid g_invalidExtraDataPid = -1;

/**
 * @brief
 *  Registry of the mesh extra-data types: gives each type an identifier and creates instances from it.
 *
 * Registration happens at static-initialisation time and from the main thread only (not thread safe).
 */
class OWL_API ExtraDataRegistry final {
public:
	/// Function creating a default instance of one extra-data type.
	using Creator = auto (*)() -> uniq<ExtraDataBase>;

	ExtraDataRegistry() = delete;

	~ExtraDataRegistry() = delete;

	ExtraDataRegistry(const ExtraDataRegistry&) = delete;

	ExtraDataRegistry(ExtraDataRegistry&&) = delete;

	auto operator=(const ExtraDataRegistry&) -> ExtraDataRegistry& = delete;

	auto operator=(ExtraDataRegistry&&) -> ExtraDataRegistry& = delete;

	/**
	 * @brief
	 *  Register a type, or return its identifier when it is already registered.
	 * @param[in] iType The extra-data type.
	 * @param[in] iCreator The function creating a default instance of it.
	 * @return The identifier of the type, or g_invalidExtraDataPid when iCreator is null.
	 */
	static auto registerType(std::type_index iType, Creator iCreator) -> ExtraDataPid;

	/**
	 * @brief
	 *  Get the identifier of a type.
	 * @param[in] iType The extra-data type.
	 * @return The identifier, or g_invalidExtraDataPid when the type is not registered.
	 */
	[[nodiscard]] static auto getPid(std::type_index iType) -> ExtraDataPid;

	/**
	 * @brief
	 *  Check if an identifier belongs to a registered type.
	 * @param[in] iPid The identifier.
	 * @return True when a type has this identifier.
	 */
	[[nodiscard]] static auto isRegistered(ExtraDataPid iPid) -> bool;

	/**
	 * @brief
	 *  Create a default instance of a registered type.
	 * @param[in] iPid The identifier of the type.
	 * @return The new instance, or nullptr when the identifier is unknown.
	 */
	[[nodiscard]] static auto create(ExtraDataPid iPid) -> uniq<ExtraDataBase>;
};

/**
 * @brief
 *  Get the identifier of an extra-data type.
 * @tparam ExtraDataType The extra-data type.
 * @return The identifier, or g_invalidExtraDataPid when the type is not registered.
 */
template<typename ExtraDataType>
[[nodiscard]] auto getExtraDataPid() -> ExtraDataPid {
	return ExtraDataRegistry::getPid(typeid(ExtraDataType));
}

}// namespace owl::data::extradata
