/**
 * @file ExtensionPack.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/pack/ExtensionPack.h"

#include "data/assets/AssetMeta.h"
#include "data/assets/pack/PackWriter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace owl::data::assets::pack {

namespace {

auto isHiddenEntry(const std::string_view iPath) -> bool {
	return iPath.starts_with('.') || iPath.find("/.") != std::string_view::npos;
}

auto isEntryOf(const ExtensionKind iKind, const std::string& iPath) -> bool {
	if (iPath == g_extensionManifestEntry || iPath.ends_with(g_MetaExtension) || isHiddenEntry(iPath) ||
		!isSafeEntryPath(iPath))
		return false;
	return iKind == ExtensionKind::Script || importerOf(iPath) != Importer::Script;
}

auto readFile(const std::filesystem::path& iPath) -> std::optional<std::vector<uint8_t>> {
	std::ifstream file(iPath, std::ios::binary);
	if (!file.is_open())
		return std::nullopt;
	return std::vector<uint8_t>{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

auto parseManifest(const std::vector<uint8_t>& iBytes, const std::filesystem::path& iSource)
		-> expected<ExtensionManifest, ExtensionLoadError> {
	auto manifest = ExtensionManifest::parse(
			std::string_view{reinterpret_cast<const char*>(iBytes.data()), iBytes.size()}, iSource.string());
	if (!manifest) {
		OWL_CORE_WARN("Extensions: '{}' is skipped: {}.", iSource.string(), describe(manifest.error()))
		return unexpected{ExtensionLoadError::InvalidManifest};
	}
	return *manifest;
}

auto listFolder(const std::filesystem::path& iFolder) -> std::vector<std::string> {
	std::vector<std::string> files;
	std::error_code error;
	for (auto it = std::filesystem::recursive_directory_iterator(
				 iFolder, std::filesystem::directory_options::skip_permission_denied, error);
		 !error && it != std::filesystem::recursive_directory_iterator{}; it.increment(error)) {
		if (it->is_regular_file(error))
			files.push_back(it->path().lexically_relative(iFolder).generic_string());
	}
	return files;
}

}// namespace

auto describe(const ExtensionLoadError iError) -> std::string_view {
	switch (iError) {
		case ExtensionLoadError::CannotOpen:
			return "neither a readable .owlmod nor a folder";
		case ExtensionLoadError::NoManifest:
			return "no extension.yml at its root";
		case ExtensionLoadError::InvalidManifest:
			return "its manifest cannot be read";
		case ExtensionLoadError::DuplicateId:
			return "another pack has the same id";
		case ExtensionLoadError::MissingDependency:
			return "a dependency is missing";
		case ExtensionLoadError::DependencyTooOld:
			return "a dependency is older than required";
		case ExtensionLoadError::DependencyRejected:
			return "a dependency is not loaded";
		case ExtensionLoadError::DependencyCycle:
			return "its dependencies form a cycle";
		case ExtensionLoadError::WriteFailed:
			return "the .owlmod cannot be written";
	}
	return "unknown extension error";
}

auto getPackAssetType(const std::filesystem::path& iPath) -> AssetType {
	if (iPath.extension() == ".owl")
		return AssetType::Scene;
	const auto importer = importerOf(iPath);
	if (!importer)
		return AssetType::Other;
	switch (*importer) {
		case Importer::Texture:
			return AssetType::Texture;
		case Importer::Mesh:
			return AssetType::Mesh;
		case Importer::Sound:
			return AssetType::Sound;
		case Importer::Script:
			return AssetType::Script;
		case Importer::Font:
			return AssetType::Font;
		case Importer::Copy:
		case Importer::Document:
			return AssetType::Other;
	}
	return AssetType::Other;
}

ExtensionPack::~ExtensionPack() = default;

auto ExtensionPack::readManifest(const std::filesystem::path& iSource)
		-> expected<ExtensionManifest, ExtensionLoadError> {
	std::error_code error;
	if (std::filesystem::is_directory(iSource, error)) {
		const auto bytes = readFile(iSource / g_extensionManifestEntry);
		if (!bytes) {
			OWL_CORE_WARN("Extensions: '{}' is skipped: {}.", iSource.string(),
						  describe(ExtensionLoadError::NoManifest))
			return unexpected{ExtensionLoadError::NoManifest};
		}
		return parseManifest(*bytes, iSource);
	}
	PackReader reader;
	if (const auto opened = reader.tryOpen(iSource); !opened) {
		OWL_CORE_WARN("Extensions: Cannot open '{}': {}.", iSource.string(), describe(opened.error()))
		return unexpected{ExtensionLoadError::CannotOpen};
	}
	const auto bytes = reader.readEntry(std::string{g_extensionManifestEntry});
	if (!bytes) {
		OWL_CORE_WARN("Extensions: '{}' is skipped: {}.", iSource.string(), describe(ExtensionLoadError::NoManifest))
		return unexpected{ExtensionLoadError::NoManifest};
	}
	return parseManifest(*bytes, iSource);
}

auto ExtensionPack::open(const std::filesystem::path& iSource) -> expected<ExtensionPack, ExtensionLoadError> {
	auto manifest = readManifest(iSource);
	if (!manifest)
		return unexpected{manifest.error()};
	ExtensionPack pack;
	pack.m_manifest = std::move(*manifest);
	pack.m_source = iSource;
	std::vector<std::string> candidates;
	if (std::error_code error; std::filesystem::is_directory(iSource, error)) {
		candidates = listFolder(iSource);
	} else {
		if (const auto opened = pack.m_reader.tryOpen(iSource); !opened) {
			OWL_CORE_WARN("Extensions: Cannot open '{}': {}.", iSource.string(), describe(opened.error()))
			return unexpected{ExtensionLoadError::CannotOpen};
		}
		candidates = pack.m_reader.listEntries();
	}
	for (auto& candidate: candidates) {
		if (isEntryOf(pack.m_manifest.kind, candidate))
			pack.m_entries.push_back(std::move(candidate));
	}
	std::ranges::sort(pack.m_entries);
	pack.m_entrySet.insert(pack.m_entries.begin(), pack.m_entries.end());
	OWL_CORE_DEBUG("Extensions: '{}' {} opened from '{}' ({} entries).", pack.m_manifest.id,
				   pack.m_manifest.version.toString(), iSource.string(), pack.m_entries.size())
	return pack;
}

auto ExtensionPack::contains(const std::string& iPath) const -> bool { return m_entrySet.contains(iPath); }

auto ExtensionPack::readEntry(const std::string& iPath) const -> std::optional<std::vector<uint8_t>> {
	if (!contains(iPath))
		return std::nullopt;
	if (m_reader.isOpen())
		return m_reader.readEntry(iPath);
	return readFile(m_source / iPath);
}

auto isEditorEntry(const ExtensionManifest& iManifest, const std::string_view iPath) -> bool {
	return iPath.starts_with("editor/") || (!iManifest.editorScript.empty() && iPath == iManifest.editorScript);
}

auto writeExtensionPack(const std::filesystem::path& iSourceFolder, const std::filesystem::path& iOutputFile,
						const PackFlags iFlags, const ExtensionContent iContent)
		-> expected<size_t, ExtensionLoadError> {
	std::error_code error;
	if (!std::filesystem::is_directory(iSourceFolder, error)) {
		OWL_CORE_WARN("Extensions: Cannot export '{}', it is not a folder.", iSourceFolder.string())
		return unexpected{ExtensionLoadError::CannotOpen};
	}
	auto pack = ExtensionPack::open(iSourceFolder);
	if (!pack)
		return unexpected{pack.error()};
	PackWriter writer;
	size_t written = 0;
	if (iContent == ExtensionContent::Game && !pack->getManifest().editorScript.empty()) {
		auto manifest = pack->getManifest();
		manifest.editorScript.clear();
		const auto text = manifest.serialize();
		writer.addData({text.begin(), text.end()}, std::string{g_extensionManifestEntry}, AssetType::Other);
	} else {
		writer.addFile(iSourceFolder / g_extensionManifestEntry, std::string{g_extensionManifestEntry},
					   AssetType::Other);
	}
	for (const auto& entry: pack->listEntries()) {
		if (iContent == ExtensionContent::Game && isEditorEntry(pack->getManifest(), entry))
			continue;
		writer.addFile(iSourceFolder / entry, entry, getPackAssetType(entry));
		++written;
	}
	if (iOutputFile.has_parent_path())
		std::filesystem::create_directories(iOutputFile.parent_path(), error);
	if (!writer.write(iOutputFile, iFlags)) {
		OWL_CORE_WARN("Extensions: Cannot write '{}'.", iOutputFile.string())
		return unexpected{ExtensionLoadError::WriteFailed};
	}
	OWL_CORE_INFO("Extensions: '{}' {} written to '{}' ({} entries).", pack->getManifest().id,
				  pack->getManifest().version.toString(), iOutputFile.string(), written)
	return written;
}

}// namespace owl::data::assets::pack
