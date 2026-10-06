/**
 * @file AtomicFileFault.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"
#include "platform/AtomicFile.h"

#include <optional>

namespace owl::platform {

/**
 * @brief
 *  Test hook: make the next atomic writes fail at the given step, as a crash or a full disk would.
 *
 * `WriteError::WriteFailed` writes half of the content before failing, to mimic an interrupted write.
 * Pass `std::nullopt` to restore normal behaviour. Only meant for tests.
 * @param[in] iFault The step to fail, or nothing.
 */
OWL_API void setAtomicWriteFault(std::optional<WriteError> iFault);

}// namespace owl::platform
