/**
 * @file openExternalUrl.h
 * @author Silmaen
 * @date 29/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include <core/Core.h>

#include <string_view>

namespace owl::platform {
/**
 * @brief
 *  Open an URL in the user's default external handler (typically a web browser).
 *
 * Supported schemes: `http://`, `https://`, `mailto:`. Any other scheme is rejected
 * and the call becomes a no-op (logged at warn level). On Linux the URL is dispatched
 * to `xdg-open` via `fork`+`execvp`. On Windows it is dispatched to `ShellExecuteW`.
 *
 * @param[in] iUrl The URL to open. Empty input is a no-op.
 */
void OWL_API openExternalUrl(std::string_view iUrl);

}// namespace owl::platform
