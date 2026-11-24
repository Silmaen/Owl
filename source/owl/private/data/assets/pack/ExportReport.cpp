/**
 * @file ExportReport.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/pack/ExportReport.h"

#include "core/external/yaml.h"
#include "data/assets/AssetMeta.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace owl::data::assets::pack {

namespace {

auto isSharedLibrary(const std::filesystem::path& iFile) -> bool {
	const auto ext = iFile.extension().string();
	return ext == ".so" || ext == ".dll" || ext == ".dylib" ||
		   iFile.filename().string().find(".so.") != std::string::npos;
}

auto normalized(const std::filesystem::path& iPath) -> std::filesystem::path {
	std::error_code ec;
	auto result = std::filesystem::weakly_canonical(iPath, ec);
	return ec ? iPath.lexically_normal() : result;
}

auto isInside(const std::filesystem::path& iFile, const std::filesystem::path& iDirectory) -> bool {
	const auto relative = iFile.lexically_relative(iDirectory);
	return !relative.empty() && *relative.begin() != "..";
}

void emitList(YAML::Emitter& ioOut, const char* iKey, const std::vector<std::string>& iValues) {
	ioOut << YAML::Key << iKey << YAML::Value << YAML::BeginSeq;
	for (const auto& value: iValues) ioOut << value;
	ioOut << YAML::EndSeq;
}

}// namespace

void ExportReport::addWarnings(const std::vector<std::string>& iWarnings) {
	for (const auto& warning: iWarnings)
		if (std::ranges::find(warnings, warning) == warnings.end())
			warnings.push_back(warning);
}

auto ExportReport::toYaml() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap << YAML::Key << "ExportReport" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "GameDirectory" << YAML::Value << gameDirectory.string();
	out << YAML::Key << "Platform" << YAML::Value << platform;
	out << YAML::Key << "Sizes" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "Total" << YAML::Value << sizes.getTotal();
	out << YAML::Key << "Pack" << YAML::Value << sizes.pack;
	out << YAML::Key << "Runner" << YAML::Value << sizes.runner;
	out << YAML::Key << "Libraries" << YAML::Value << sizes.libraries;
	out << YAML::Key << "Other" << YAML::Value << sizes.other;
	out << YAML::EndMap;
	out << YAML::Key << "Assets" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "Packed" << YAML::Value << assetCount;
	out << YAML::Key << "Cooked" << YAML::Value << cookedAssets;
	out << YAML::Key << "CacheHits" << YAML::Value << cacheHits;
	out << YAML::EndMap;
	out << YAML::Key << "Renderers" << YAML::Value << YAML::Flow << renderers;
	out << YAML::Key << "Shaders" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "Baked" << YAML::Value << shadersBaked;
	out << YAML::Key << "Targets" << YAML::Value << YAML::Flow << shaderTargets;
	out << YAML::Key << "EngineFilesSkipped" << YAML::Value << engineFilesSkipped;
	out << YAML::EndMap;
	std::vector<std::string> modNames;
	modNames.reserve(mods.size());
	for (const auto& mod: mods) modNames.push_back(mod.filename().string());
	emitList(out, "Mods", modNames);
	emitList(out, "UnusedAssets", unusedAssets);
	emitList(out, "Warnings", warnings);
	out << YAML::EndMap << YAML::EndMap;
	return std::string{out.c_str()} + "\n";
}

auto ExportReport::getSummary() const -> std::string {
	auto summary = std::format("Game written to '{}' ({}).\n", gameDirectory.string(), platform);
	summary += std::format("Size: {} (pack {}, runner {}, libraries {}, other {}).\n", formatByteSize(sizes.getTotal()),
						   formatByteSize(sizes.pack), formatByteSize(sizes.runner), formatByteSize(sizes.libraries),
						   formatByteSize(sizes.other));
	std::string targets;
	for (const auto& target: shaderTargets) targets += (targets.empty() ? "" : ", ") + target;
	summary += std::format("Assets: {} packed; shaders: {} baked for {}.\n", assetCount, shadersBaked,
						   targets.empty() ? std::string{"no target"} : targets);
	summary += std::format("Unused assets: {}; warnings: {}.", unusedAssets.size(), warnings.size());
	return summary;
}

auto ExportReport::write() const -> bool {
	const auto file = gameDirectory / g_exportReportFile;
	std::ofstream out(file, std::ios::binary);
	if (!out) {
		OWL_CORE_WARN("ExportReport: Cannot write '{}'.", file.string())
		return false;
	}
	out << toYaml();
	if (!out) {
		OWL_CORE_WARN("ExportReport: Cannot write '{}'.", file.string())
		return false;
	}
	return true;
}

auto classifyExportFile(const std::filesystem::path& iFile, const std::filesystem::path& iExecutable)
		-> ExportFileKind {
	const auto ext = iFile.extension().string();
	if (ext == ".owlpack" || ext == ".owlmod")
		return ExportFileKind::Pack;
	if ((!iExecutable.empty() && iFile.filename() == iExecutable.filename()) ||
		iFile.filename().string().starts_with("OwlGameRunner."))
		return ExportFileKind::Runner;
	if (isSharedLibrary(iFile))
		return ExportFileKind::Library;
	return ExportFileKind::Other;
}

auto measureExportSizes(const std::filesystem::path& iGameDirectory, const std::filesystem::path& iExecutable)
		-> ExportSizes {
	ExportSizes sizes;
	std::error_code ec;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(iGameDirectory, ec)) {
		std::error_code fileEc;
		if (entry.is_symlink(fileEc) || !entry.is_regular_file(fileEc))
			continue;
		const auto bytes = entry.file_size(fileEc);
		if (fileEc)
			continue;
		const bool topLevel = entry.path().parent_path() == iGameDirectory;
		switch (classifyExportFile(entry.path(), topLevel ? iExecutable : std::filesystem::path{})) {
			case ExportFileKind::Pack:
				sizes.pack += bytes;
				break;
			case ExportFileKind::Runner:
				sizes.runner += bytes;
				break;
			case ExportFileKind::Library:
				sizes.libraries += bytes;
				break;
			case ExportFileKind::Other:
				sizes.other += bytes;
				break;
		}
	}
	if (ec)
		OWL_CORE_WARN("ExportReport: Cannot measure '{}': {}.", iGameDirectory.string(), ec.message())
	return sizes;
}

auto findUnusedAssets(const std::filesystem::path& iProjectDirectory, const std::vector<AssetReference>& iUsed,
					  const std::vector<std::filesystem::path>& iExcluded) -> std::vector<std::string> {
	std::set<std::filesystem::path> used;
	for (const auto& ref: iUsed) used.insert(normalized(ref.diskPath));
	std::vector<std::filesystem::path> excluded;
	excluded.reserve(iExcluded.size());
	for (const auto& path: iExcluded)
		if (!path.empty())
			excluded.push_back(normalized(path));
	const auto root = normalized(iProjectDirectory);
	std::vector<std::string> unused;
	std::error_code ec;
	for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
		 !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
		const auto& path = it->path();
		const bool hidden = path.filename().string().starts_with('.');
		const bool skipped = std::ranges::any_of(excluded, [&path](const std::filesystem::path& iExcludedPath) -> bool {
			return path == iExcludedPath || isInside(path, iExcludedPath);
		});
		std::error_code typeEc;
		if (it->is_directory(typeEc)) {
			if (hidden || skipped)
				it.disable_recursion_pending();
			continue;
		}
		if (hidden || skipped || !it->is_regular_file(typeEc) || !importerOf(path).has_value())
			continue;
		if (!used.contains(path))
			unused.push_back(path.lexically_relative(root).generic_string());
	}
	if (ec)
		OWL_CORE_WARN("ExportReport: Cannot list the assets of '{}': {}.", root.string(), ec.message())
	std::ranges::sort(unused);
	return unused;
}

auto formatByteSize(const uintmax_t iBytes) -> std::string {
	constexpr std::array<const char*, 3> units{"KiB", "MiB", "GiB"};
	if (iBytes < 1024)
		return std::format("{} B", iBytes);
	auto value = static_cast<double>(iBytes) / 1024.0;
	size_t unit = 0;
	while (value >= 1024.0 && unit + 1 < units.size()) {
		value /= 1024.0;
		++unit;
	}
	if (value < 10.0)
		return std::format("{:.2f} {}", value, units[unit]);
	if (value < 100.0)
		return std::format("{:.1f} {}", value, units[unit]);
	return std::format("{:.0f} {}", value, units[unit]);
}

}// namespace owl::data::assets::pack
