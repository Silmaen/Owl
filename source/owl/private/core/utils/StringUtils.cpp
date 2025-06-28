/**
 * @file StringUtils.cpp
 * @author Silmaen
 * @date 29/04/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "core/utils/StringUtils.h"
#include "owlpch.h"

namespace owl::core::utils {

auto sizeToString(const size_t iSize) -> std::string {
	auto size = static_cast<double>(iSize);
	std::string_view unit{"B"};
	if (size > 1024) {
		size /= 1024;
		unit = "KB";
	}
	if (size > 1024) {
		size /= 1024;
		unit = "MB";
	}
	if (size > 1024) {
		size /= 1024;
		unit = "GB";
	}
	if (size > 1024) {
		size /= 1024;
		unit = "TB";
	}
	// Format the size to 2 decimal places
	if (unit == "B") {
		return fmt::format("{} {}", iSize, unit);
	}
	return fmt::format("{:.2f} {}", size, unit);
}

}// namespace owl::core::utils
