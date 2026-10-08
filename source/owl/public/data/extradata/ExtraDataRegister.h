/**
 * @file ExtraDataRegister.h
 * @author Silmaen
 * @date 20/10/2025
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/extradata/ExtraDataBase.h"
#include "data/extradata/ExtraDataRegisterScope.h"
#include "data/extradata/ExtraDataRegistry.h"

namespace owl::data::extradata {
/**
 * @brief
 *  Return PID of the extra data type.
 * @tparam ExtraDataType Extra data type to get the PID.
 * @return
 *  PID of ExtraDataType.
 */
template<typename ExtraDataType>
auto getMeshExtraDataPid() -> ExtraDataPid {
	return getExtraDataPid<ExtraDataType>();
}

/**
 * @brief
 *  Create a default instance of an extra-data type, as an ExtraDataRegistry creator.
 * @tparam ExtraDataType Extra data type.
 * @return The new instance.
 */
template<typename ExtraDataType>
auto createExtraData() -> uniq<ExtraDataBase> {
	return mkUniq<ExtraDataType>();
}

/**
 * @brief
 *  Register an extra data type in the extra-data registry.
 * @warning
 *  All call to this function should be done on the same thread. The function is not thread safe.
 * @tparam ExtraDataType Extra data type
 * @return A scope reporting whether the registration succeeded.
 */
template<typename ExtraDataType>
[[nodiscard]] auto registerExtraData() -> ExtraDataRegisterScope {
	if (ExtraDataRegistry::registerType(typeid(ExtraDataType), &createExtraData<ExtraDataType>) ==
		g_invalidExtraDataPid)
		return ExtraDataRegisterScope{};
	return ExtraDataRegisterScope{[]() -> void {}};
}

}// namespace owl::data::extradata
