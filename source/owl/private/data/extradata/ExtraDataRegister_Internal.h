/**
 * @file ExtraDataRegister_Internal.h
 * @author Silmaen
 * @date 19/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/extradata/ExtraDataRegister.h"

namespace owl::data::extradata {

template<typename ExtraDataType>
[[nodiscard]] auto registerExtraDataInternal() -> ExtraDataRegisterScope {
	[[maybe_unused]] core::FactoryPid pid = core::factoryRegisterType<ExtraDataType>();
	return ExtraDataRegisterScope{};
}

}// namespace owl::data::extradata
