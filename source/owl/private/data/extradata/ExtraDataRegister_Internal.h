/**
 * @file ExtraDataRegister_Internal.h
 * @author Silmaen
 * @date 19/10/2025
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/extradata/ExtraDataRegister.h"

namespace owl::data::extradata {

/**
 * @brief
 *  Internal helper used by `registerExtraData` to bind an engine-provided extra-data type.
 *
 * Skips the `Owl`-prefix assertion that the public version enforces, since engine types are allowed to
 * use the reserved prefix.
 * @tparam ExtraDataType Extra-data type to register.
 * @return A scope object that unregisters the type when destroyed.
 */
template<typename ExtraDataType>
[[nodiscard]] auto registerExtraDataInternal() -> ExtraDataRegisterScope {
	[[maybe_unused]] const core::FactoryPid pid = core::factoryRegisterType<ExtraDataType>();
	return ExtraDataRegisterScope{};
}

}// namespace owl::data::extradata
