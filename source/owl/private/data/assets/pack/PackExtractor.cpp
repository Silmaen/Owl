/**
 * @file PackExtractor.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/pack/PackExtractor.h"

#include <algorithm>
#include <fstream>

namespace owl::data::assets::pack {

namespace {
auto isUpToDate(const std::filesystem::path& iFile, const std::optional<uint64_t>& iSize) -> bool {
	std::error_code ec;
	if (!iSize.has_value() || !std::filesystem::is_regular_file(iFile, ec))
		return false;
	const auto size = std::filesystem::file_size(iFile, ec);
	return !ec && size == *iSize;
}
}// namespace

auto resolveEntryPath(const std::filesystem::path& iRoot, const std::string_view iEntryPath)
		-> std::optional<std::filesystem::path> {
	if (!isSafeEntryPath(iEntryPath)) {
		OWL_CORE_WARN("Pack: entry path '{}' is absolute or escapes its root.", iEntryPath)
		return std::nullopt;
	}
	std::error_code ec;
	auto root = std::filesystem::weakly_canonical(iRoot, ec);
	if (ec) {
		OWL_CORE_WARN("Pack: cannot resolve root '{}': {}.", iRoot.string(), ec.message())
		return std::nullopt;
	}
	if (!root.has_filename())
		root = root.parent_path();
	auto target = std::filesystem::weakly_canonical(root / std::filesystem::path(iEntryPath), ec);
	if (ec) {
		OWL_CORE_WARN("Pack: cannot resolve entry '{}': {}.", iEntryPath, ec.message())
		return std::nullopt;
	}
	const auto [rootIt, targetIt] = std::ranges::mismatch(root, target);
	if (rootIt != root.end() || targetIt == target.end()) {
		OWL_CORE_WARN("Pack: entry '{}' resolves to '{}', outside '{}'.", iEntryPath, target.string(), root.string())
		return std::nullopt;
	}
	return target;
}

auto writeEntryFile(const std::filesystem::path& iFile, const std::span<const uint8_t> iData)
		-> owl::expected<void, PackExtractError> {
	std::error_code ec;
	std::filesystem::create_directories(iFile.parent_path(), ec);
	if (ec) {
		OWL_CORE_ERROR("Pack: cannot create directory '{}': {}.", iFile.parent_path().string(), ec.message())
		return owl::unexpected{PackExtractError::CreateDirectoryFailed};
	}
	std::ofstream out(iFile, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) {
		OWL_CORE_ERROR("Pack: cannot open '{}' for writing.", iFile.string())
		return owl::unexpected{PackExtractError::WriteFailed};
	}
	out.write(reinterpret_cast<const char*>(iData.data()), static_cast<std::streamsize>(iData.size()));
	out.close();
	if (out.fail()) {
		OWL_CORE_ERROR("Pack: failed to write {} bytes to '{}'.", iData.size(), iFile.string())
		return owl::unexpected{PackExtractError::WriteFailed};
	}
	return {};
}

auto extractPack(const PackReader& iReader, const std::filesystem::path& iDestDir)
		-> owl::expected<PackExtractStats, PackExtractError> {
	if (!iReader.isOpen()) {
		OWL_CORE_ERROR("Pack: cannot extract, no pack is open.")
		return owl::unexpected{PackExtractError::NotOpen};
	}
	try {
		std::error_code ec;
		std::filesystem::create_directories(iDestDir, ec);
		if (ec) {
			OWL_CORE_ERROR("Pack: cannot create directory '{}': {}.", iDestDir.string(), ec.message())
			return owl::unexpected{PackExtractError::CreateDirectoryFailed};
		}
		PackExtractStats stats;
		for (const auto& entryPath: iReader.listEntries()) {
			const auto destFile = resolveEntryPath(iDestDir, entryPath);
			if (!destFile) {
				OWL_CORE_ERROR("Pack: extraction stopped, entry '{}' escapes '{}'.", entryPath, iDestDir.string())
				return owl::unexpected{PackExtractError::UnsafeEntryPath};
			}
			const bool hasSpirvCache =
					entryPath.ends_with(".slang") && std::filesystem::exists(destFile->string() + ".spv", ec);
			if (hasSpirvCache || isUpToDate(*destFile, iReader.entrySize(entryPath))) {
				++stats.skipped;
				continue;
			}
			const auto data = iReader.readEntry(entryPath);
			if (!data) {
				OWL_CORE_ERROR("Pack: extraction stopped, cannot read entry '{}'.", entryPath)
				return owl::unexpected{PackExtractError::ReadFailed};
			}
			if (auto written = writeEntryFile(*destFile, *data); !written)
				return owl::unexpected{written.error()};
			++stats.written;
		}
		return stats;
	} catch (const std::exception& e) {
		OWL_CORE_ERROR("Pack: exception during extraction into '{}': {}.", iDestDir.string(), e.what())
		return owl::unexpected{PackExtractError::UnexpectedException};
	}
}

}// namespace owl::data::assets::pack
