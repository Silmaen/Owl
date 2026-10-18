/**
 * @file pack_reader_fuzzer.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include <data/assets/pack/PackExtractor.h>
#include <data/assets/pack/PackReader.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <random>

extern "C" auto LLVMFuzzerTestOneInput(const uint8_t* iData, size_t iSize) -> int;

namespace {

auto fuzzFile() -> const std::filesystem::path& {
	static const auto file = std::filesystem::temp_directory_path() /
							 std::format("owl_pack_fuzz_{:016x}.owlpack", std::random_device{}());
	return file;
}

}// namespace

extern "C" auto LLVMFuzzerTestOneInput(const uint8_t* iData, const size_t iSize) -> int {
	using namespace owl::data::assets::pack;
	{
		std::ofstream out(fuzzFile(), std::ios::binary | std::ios::trunc);
		out.write(reinterpret_cast<const char*>(iData), static_cast<std::streamsize>(iSize));
	}
	PackReader reader;
	if (!reader.tryOpen(fuzzFile()))
		return 0;
	const auto root = std::filesystem::temp_directory_path() / "owl_pack_fuzz_root";
	size_t budget = 8;
	for (const auto& entry: reader.listEntries()) {
		if (budget-- == 0)
			break;
		if (!isSafeEntryPath(entry))
			__builtin_trap();
		static_cast<void>(reader.readEntry(entry));
		static_cast<void>(resolveEntryPath(root, entry));
	}
	return 0;
}
