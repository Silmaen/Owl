/**
 * @file StringUtils.h
 * @author Silmaen
 * @date 29/04/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/Core.h>
#include <string>

namespace owl::core::utils {

/**
 * @brief Convert a size_t to a human-readable string.
 * @param iSize The size in octets.
 * @return The human-readable string.
 */
auto OWL_API sizeToString(size_t iSize) -> std::string;

}// namespace owl::core::utils
