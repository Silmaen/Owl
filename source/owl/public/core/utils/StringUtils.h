/**
 * @file StringUtils.h
 * @author Silmaen
 * @date 29/04/25
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/Core.h>
#include <cstdint>
#include <string>

namespace owl::core::utils {
/**
 * @brief
 *  Convert a byte count to a human-readable string.
 * @param[in] iSize The size in octets (64-bit: a GPU or a file can exceed the size_t of a 32-bit target).
 * @return The human-readable string.
 */
auto OWL_API sizeToString(uint64_t iSize) -> std::string;

}// namespace owl::core::utils
