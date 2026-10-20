/**
 * @file Core.cpp
 * @author Silmaen
 * @date 1/16/25
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/BuildInfo.h"
#include "core/Core.h"

#include <cstdint>
#include <format>
#include <string>

namespace owl {

auto getVersionCode() -> uint32_t {
	return static_cast<uint32_t>(getVersionMajor() << 24u) | static_cast<uint32_t>(getVersionMinor() << 16u) |
		   static_cast<uint32_t>(getVersionPatch() << 8u);
}

auto getVersionMajor() -> uint8_t { return OWL_MAJOR; }

auto getVersionMinor() -> uint8_t { return OWL_MINOR; }

auto getVersionPatch() -> uint8_t { return OWL_PATCH; }

auto getVersionString() -> std::string {
	return std::format("{}.{}.{}", getVersionMajor(), getVersionMinor(), getVersionPatch());
}

auto getVersionCommit() -> std::string { return OWL_GIT_COMMIT; }

auto getVersionFullString() -> std::string {
	const auto commit = getVersionCommit();
	return commit == "unknown" ? getVersionString() : std::format("{}-{}", getVersionString(), commit);
}

}// namespace owl
