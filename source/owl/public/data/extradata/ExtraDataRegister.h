/**
 * @file ExtraDataRegister.h
 * @author Silmaen
 * @date 19/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/IFactory.h"
#include "data/extradata/ExtraDataRegisterScope.h"

namespace owl::data::extradata {

/**
 * @brief
 *  Return PID of the extra data type.
 * @tparam ExtraDataType Extra data type to get the PID.
 * @return
 *  PID of ExtraDataType.
*/
template<typename ExtraDataType>
auto getMeshExtraDataPid() -> core::FactoryPid {
	return core::getFactoryPid<ExtraDataType>();
}

/**
 * @brief
 *  Register an extra data in factory and mesh extra data policies.
 * @warning
 *  It is strongly advised to store the return scope in a global variable to have destructor call, i.e. call to
 *  unregister function, when the process end.
 * @warning
 *  All call to this function should be done on the same thread. The function is not thread safe.
 * @tparam ExtraDataType Extra data type
 * @return  A scope that will unregister the policy during in its destructor.
 */
template<typename ExtraDataType>
[[nodiscard]] auto registerExtraData() -> ExtraDataRegisterScope {
	if (const std::string type = ExtraDataType::GetStaticType(); type.starts_with("Owl")) {
		OWL_CORE_ASSERT(false, "Static Type of extra data must start with Owl (reserved word ).")
	}
	[[maybe_unused]] core::FactoryPid pid = core::factoryRegisterType<ExtraDataType>();
	return ExtraDataRegisterScope{};
}

}// namespace owl::data::extradata
