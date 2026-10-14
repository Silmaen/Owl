/**
 * @file tilemap_fuzzer.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include <core/Log.h>
#include <scene/TilemapAsset.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" auto LLVMFuzzerTestOneInput(const uint8_t* iData, size_t iSize) -> int;

extern "C" auto LLVMFuzzerTestOneInput(const uint8_t* iData, const size_t iSize) -> int {
	static const bool s_logReady = []() -> bool {
		owl::core::Log::init(owl::core::Log::Level::Off);
		return true;
	}();
	static_cast<void>(s_logReady);
	owl::scene::TilemapAsset asset;
	// A tilemap file from a pack is untrusted: any input is either read or refused, never a crash.
	if (asset.deserializeFromString(std::string_view{reinterpret_cast<const char*>(iData), iSize}))
		static_cast<void>(asset.serializeToString());
	return 0;
}
