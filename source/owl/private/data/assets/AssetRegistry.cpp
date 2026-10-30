/**
 * @file AssetRegistry.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/AssetRegistry.h"

#include "core/external/yaml.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <map>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace owl::data::assets {

namespace {

using ReadLock = std::shared_lock<std::shared_mutex>;
using WriteLock = std::unique_lock<std::shared_mutex>;

auto normalized(const std::filesystem::path& iPath) -> std::filesystem::path {
	std::error_code error;
	auto absolute = std::filesystem::absolute(iPath, error);
	if (error)
		absolute = iPath;
	return absolute.lexically_normal();
}

auto keyOf(const std::filesystem::path& iPath) -> std::string { return normalized(iPath).generic_string(); }

auto isUnder(const std::filesystem::path& iPath, const std::filesystem::path& iDirectory) -> bool {
	const auto relative = iPath.lexically_relative(iDirectory);
	return !relative.empty() && *relative.begin() != "..";
}

auto isHidden(const std::filesystem::path& iRelative) -> bool {
	return std::ranges::any_of(iRelative, [](const std::filesystem::path& iPart) -> bool {
		const auto name = iPart.string();
		return name.size() > 1 && name.front() == '.' && name != "..";
	});
}

auto isMetaFile(const std::filesystem::path& iFile) -> bool { return iFile.extension() == g_MetaExtension; }

auto assetOfMeta(const std::filesystem::path& iMetaFile) -> std::filesystem::path {
	auto asset = iMetaFile;
	asset.replace_extension();
	return asset;
}

auto writeTime(const std::filesystem::path& iFile) -> std::filesystem::file_time_type {
	std::error_code error;
	const auto time = std::filesystem::last_write_time(iFile, error);
	return error ? std::filesystem::file_time_type::min() : time;
}

auto moveOnDisk(const std::filesystem::path& iFrom, const std::filesystem::path& iTo) -> bool {
	std::error_code error;
	std::filesystem::create_directories(iTo.parent_path(), error);
	std::filesystem::rename(iFrom, iTo, error);
	if (!error)
		return true;
	std::filesystem::copy(iFrom, iTo, std::filesystem::copy_options::recursive, error);
	if (error) {
		OWL_CORE_WARN("Assets: Cannot move '{}' to '{}': {}.", iFrom.string(), iTo.string(), error.message())
		return false;
	}
	std::filesystem::remove_all(iFrom, error);
	return true;
}

// Keys whose value is an asset reference in an Owl document.
constexpr std::array<std::string_view, 25> g_ReferenceKeys{"texture",        "normalTexture", "roughnessTexture",
														   "opacityTexture", "baseColorMap",  "metallicMap",
														   "roughnessMap",   "normalMap",     "occlusionMap",
														   "emissiveMap",    "mesh",          "material",
														   "parent",         "scriptPath",    "hdri",
														   "tilesetPath",    "tilemapPath",   "prefabAssetPath",
														   "atlasPath",      "soundAsset",    "asset",
														   "stinger",        "LevelName",     "Hud",
														   "Tileset"};

auto isReferenceKey(const std::string_view iKey) -> bool {
	return std::ranges::find(g_ReferenceKeys, iKey) != g_ReferenceKeys.end();
}

auto trimmedRight(std::string_view iText) -> std::string_view {
	while (!iText.empty() && (iText.back() == ' ' || iText.back() == '\r')) iText.remove_suffix(1);
	return iText;
}

auto findUnquoted(const std::string_view iText, const char iCharacter, const size_t iFrom) -> size_t {
	char quote = 0;
	for (size_t index = iFrom; index < iText.size(); ++index) {
		const char character = iText[index];
		if (quote != 0) {
			if (character == quote)
				quote = 0;
		} else if (character == '\'' || character == '"') {
			quote = character;
		} else if (character == iCharacter) {
			return index;
		}
	}
	return std::string_view::npos;
}

}// namespace

AssetRegistry::AssetRegistry() = default;

AssetRegistry::~AssetRegistry() = default;

auto AssetRegistry::rootOf(const std::filesystem::path& iFile) const -> const Root* {
	for (const auto& root: m_roots)
		if (isUnder(iFile, root.path))
			return &root;
	return nullptr;
}

// NOLINTNEXTLINE(misc-no-recursion): one level at most, the copy of a duplicated GUID is registered once.
auto AssetRegistry::registerLocked(const Root& iRoot, const std::filesystem::path& iFile, ScanReport& ioReport)
		-> std::optional<AssetGuid> {
	const auto importer = importerOf(iFile);
	if (!importer)
		return std::nullopt;
	const auto metaFile = metaPathOf(iFile);
	AssetMeta meta;
	if (std::error_code error; std::filesystem::exists(metaFile, error)) {
		auto read = readMeta(metaFile);
		if (!read) {
			OWL_CORE_WARN("Assets: '{}' is left without GUID until its sidecar is fixed ({}).", iFile.string(),
						  describe(read.error()))
			return std::nullopt;
		}
		meta = *read;
	} else {
		if (!iRoot.editable)
			return std::nullopt;
		meta = AssetMeta{.guid = AssetGuid{}, .importer = *importer, .texture = {}, .mesh = {}};
		if (!writeMeta(metaFile, meta))
			return std::nullopt;
		++ioReport.created;
	}

	const auto key = keyOf(iFile);
	if (const auto existing = m_entries.find(meta.guid); existing != m_entries.end()) {
		const auto existingFile = existing->second.root / existing->second.path;
		if (keyOf(existingFile) != key) {
			// The most recently written of two files sharing a GUID is the copy: it takes a new GUID.
			const auto* existingRoot = rootOf(existingFile);
			const bool existingIsCopy = existingRoot != nullptr && existingRoot->editable &&
										(!iRoot.editable || writeTime(iFile) < writeTime(existingFile));
			if (existingIsCopy) {
				const Root copyRoot = *existingRoot;
				m_byAbsolutePath.erase(keyOf(existingFile));
				m_entries.erase(existing);
				m_entries.emplace(meta.guid, Entry{.meta = meta,
												   .root = iRoot.path,
												   .path = iFile.lexically_relative(iRoot.path).generic_string()});
				m_byAbsolutePath[key] = meta.guid;
				std::error_code error;
				std::filesystem::remove(metaPathOf(existingFile), error);
				std::ignore = registerLocked(copyRoot, existingFile, ioReport);
				++ioReport.reassigned;
				return meta.guid;
			}
			if (!iRoot.editable) {
				OWL_CORE_WARN("Assets: '{}' shares the GUID of '{}', ignored.", iFile.string(), existingFile.string())
				return std::nullopt;
			}
			meta.guid = AssetGuid{};
			if (!writeMeta(metaFile, meta))
				return std::nullopt;
			++ioReport.reassigned;
			OWL_CORE_INFO("Assets: '{}' was a copy of '{}', given a new GUID.", iFile.string(), existingFile.string())
		}
	}
	m_entries.insert_or_assign(
			meta.guid,
			Entry{.meta = meta, .root = iRoot.path, .path = iFile.lexically_relative(iRoot.path).generic_string()});
	m_byAbsolutePath[key] = meta.guid;
	++ioReport.assets;
	return meta.guid;
}

void AssetRegistry::rebuildPathIndex() {
	m_byRelativePath.clear();
	m_byAbsolutePath.clear();
	for (const auto& [guid, entry]: m_entries) {
		if (entry.root.empty())
			m_byRelativePath.emplace(entry.path, guid);
		else
			m_byAbsolutePath[keyOf(entry.root / entry.path)] = guid;
	}
	for (const auto& root: std::views::reverse(m_roots)) {
		for (const auto& [guid, entry]: m_entries)
			if (entry.root == root.path)
				m_byRelativePath[entry.path] = guid;
	}
}

auto AssetRegistry::addDirectory(const std::filesystem::path& iRoot, const bool iEditable) -> ScanReport {
	OWL_PROFILE_FUNCTION()

	ScanReport report;
	const auto rootPath = normalized(iRoot);
	std::error_code error;
	if (!std::filesystem::is_directory(rootPath, error)) {
		OWL_CORE_WARN("Assets: Cannot scan '{}', it is not a directory.", rootPath.string())
		return report;
	}
	const WriteLock lock(m_mutex);
	m_roots.remove_if([&rootPath](const Root& iRootEntry) -> bool { return iRootEntry.path == rootPath; });
	m_roots.push_front({.path = rootPath, .editable = iEditable});
	const auto& root = m_roots.front();
	std::vector<std::filesystem::path> metaFiles;
	for (auto it = std::filesystem::recursive_directory_iterator(
				 rootPath, std::filesystem::directory_options::skip_permission_denied, error);
		 !error && it != std::filesystem::recursive_directory_iterator{}; it.increment(error)) {
		const auto& entry = *it;
		if (isHidden(entry.path().lexically_relative(rootPath))) {
			if (entry.is_directory(error))
				it.disable_recursion_pending();
			continue;
		}
		if (!entry.is_regular_file(error))
			continue;
		if (isMetaFile(entry.path()))
			metaFiles.push_back(entry.path());
		else
			std::ignore = registerLocked(root, entry.path(), report);
	}
	for (const auto& metaFile: metaFiles) {
		if (!std::filesystem::exists(assetOfMeta(metaFile), error)) {
			OWL_CORE_WARN("Assets: Sidecar '{}' has no asset next to it.", metaFile.string())
			++report.orphans;
		}
	}
	rebuildPathIndex();
	OWL_CORE_INFO("Assets: {} asset(s) in '{}' ({} sidecar(s) created, {} copy(ies) given a new GUID).", report.assets,
				  rootPath.string(), report.created, report.reassigned)
	return report;
}

void AssetRegistry::removeDirectory(const std::filesystem::path& iRoot) {
	const auto rootPath = normalized(iRoot);
	const WriteLock lock(m_mutex);
	m_roots.remove_if([&rootPath](const Root& iRootEntry) -> bool { return iRootEntry.path == rootPath; });
	std::erase_if(m_entries, [&rootPath](const auto& iEntry) -> bool { return iEntry.second.root == rootPath; });
	rebuildPathIndex();
}

auto AssetRegistry::registerFile(const std::filesystem::path& iFile) -> std::optional<AssetGuid> {
	const auto file = normalized(iFile);
	const WriteLock lock(m_mutex);
	const auto* root = rootOf(file);
	if (root == nullptr || isHidden(file.lexically_relative(root->path)) || isMetaFile(file))
		return std::nullopt;
	if (const auto known = m_byAbsolutePath.find(keyOf(file)); known != m_byAbsolutePath.end())
		return known->second;
	ScanReport report;
	const auto guid = registerLocked(*root, file, report);
	rebuildPathIndex();
	return guid;
}

void AssetRegistry::forgetFile(const std::filesystem::path& iFile) {
	const auto file = normalized(iFile);
	const WriteLock lock(m_mutex);
	std::erase_if(m_entries, [&file](const auto& iEntry) -> bool {
		const auto entryFile = (iEntry.second.root / iEntry.second.path).lexically_normal();
		return !iEntry.second.root.empty() && (entryFile == file || isUnder(entryFile, file));
	});
	rebuildPathIndex();
}

auto AssetRegistry::moveFile(const std::filesystem::path& iFrom, const std::filesystem::path& iTo) -> bool {
	const auto from = normalized(iFrom);
	const auto to = normalized(iTo);
	std::error_code error;
	const bool isFolder = std::filesystem::is_directory(from, error);
	if (!moveOnDisk(from, to))
		return false;
	if (!isFolder && std::filesystem::exists(metaPathOf(from), error))
		std::ignore = moveOnDisk(metaPathOf(from), metaPathOf(to));

	const WriteLock lock(m_mutex);
	const auto* newRoot = rootOf(to);
	for (auto it = m_entries.begin(); it != m_entries.end();) {
		auto& entry = it->second;
		const auto entryFile = (entry.root / entry.path).lexically_normal();
		if (entry.root.empty() || (entryFile != from && !isUnder(entryFile, from))) {
			++it;
			continue;
		}
		if (newRoot == nullptr) {
			it = m_entries.erase(it);
			continue;
		}
		const auto movedFile = entryFile == from ? to : to / entryFile.lexically_relative(from);
		entry.root = newRoot->path;
		entry.path = movedFile.lexically_relative(newRoot->path).generic_string();
		++it;
	}
	rebuildPathIndex();
	return true;
}

auto AssetRegistry::deleteFile(const std::filesystem::path& iFile) -> bool {
	const auto file = normalized(iFile);
	std::error_code error;
	const bool isFolder = std::filesystem::is_directory(file, error);
	if (isFolder)
		std::filesystem::remove_all(file, error);
	else
		std::filesystem::remove(file, error);
	if (error) {
		OWL_CORE_WARN("Assets: Cannot delete '{}': {}.", file.string(), error.message())
		return false;
	}
	if (!isFolder)
		std::filesystem::remove(metaPathOf(file), error);
	forgetFile(file);
	return true;
}

auto AssetRegistry::copyFile(const std::filesystem::path& iFrom, const std::filesystem::path& iTo)
		-> std::optional<AssetGuid> {
	const auto from = normalized(iFrom);
	const auto to = normalized(iTo);
	std::error_code error;
	std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, error);
	if (error) {
		OWL_CORE_WARN("Assets: Cannot copy '{}' to '{}': {}.", from.string(), to.string(), error.message())
		return std::nullopt;
	}
	const auto importer = importerOf(to);
	const WriteLock lock(m_mutex);
	const auto* root = rootOf(to);
	if (!importer || root == nullptr || !root->editable)
		return std::nullopt;
	auto meta = AssetMeta{.guid = AssetGuid{}, .importer = *importer, .texture = {}, .mesh = {}};
	if (const auto original = m_byAbsolutePath.find(keyOf(from)); original != m_byAbsolutePath.end()) {
		meta = m_entries.at(original->second).meta;
		meta.guid = AssetGuid{};
	}
	if (!writeMeta(metaPathOf(to), meta))
		return std::nullopt;
	m_entries.insert_or_assign(
			meta.guid,
			Entry{.meta = meta, .root = root->path, .path = to.lexically_relative(root->path).generic_string()});
	rebuildPathIndex();
	return meta.guid;
}

auto AssetRegistry::setMeta(const AssetMeta& iMeta) -> bool {
	const WriteLock lock(m_mutex);
	const auto entry = m_entries.find(iMeta.guid);
	if (entry == m_entries.end()) {
		OWL_CORE_WARN("Assets: Cannot set the import settings of unknown GUID {}.", formatGuid(iMeta.guid))
		return false;
	}
	const auto* root = rootOf(entry->second.root / entry->second.path);
	if (root == nullptr || !root->editable) {
		OWL_CORE_WARN("Assets: '{}' lies in a read-only directory, its import settings stay.", entry->second.path)
		return false;
	}
	if (!writeMeta(metaPathOf(entry->second.root / entry->second.path), iMeta))
		return false;
	entry->second.meta = iMeta;
	return true;
}

auto AssetRegistry::loadIndex(const std::string_view iText) -> size_t {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iText});
	} catch (const YAML::Exception& exception) {
		OWL_CORE_WARN("Assets: Cannot parse the pack index: {}.", exception.what())
		return 0;
	}
	if (!root["Assets"] || !root["Assets"].IsMap()) {
		OWL_CORE_WARN("Assets: The pack index has no 'Assets' map.")
		return 0;
	}
	const WriteLock lock(m_mutex);
	size_t count = 0;
	for (const auto& item: root["Assets"]) {
		const auto guid = parseGuid(item.first.as<std::string>());
		if (!guid) {
			OWL_CORE_WARN("Assets: Pack index entry '{}' is no GUID, skipped.", item.first.as<std::string>())
			continue;
		}
		const bool detailed = item.second.IsMap();
		const auto path = detailed ? item.second["Path"].as<std::string>("") : item.second.as<std::string>();
		AssetMeta meta{.guid = *guid, .importer = importerOf(path).value_or(Importer::Copy), .texture = {}, .mesh = {}};
		if (detailed)
			get(item.second, "Nearest", meta.texture.nearest);
		m_entries.insert_or_assign(*guid, Entry{.meta = meta, .root = {}, .path = path});
		++count;
	}
	rebuildPathIndex();
	return count;
}

auto AssetRegistry::serializeIndex(const std::vector<std::string>& iPaths) const -> std::string {
	std::map<std::string, Entry> sorted;
	for (const auto& path: iPaths)
		if (const auto guid = findGuid(path); guid)
			if (auto entry = find(*guid); entry)
				sorted.emplace(path, std::move(*entry));
	YAML::Emitter out;
	out << YAML::BeginMap << YAML::Key << "Assets" << YAML::Value << YAML::BeginMap;
	for (const auto& [path, entry]: sorted) {
		out << YAML::Key << formatGuid(entry.meta.guid) << YAML::Value;
		if (entry.meta.texture.nearest)
			out << YAML::Flow << YAML::BeginMap << YAML::Key << "Path" << YAML::Value << path << YAML::Key << "Nearest"
				<< YAML::Value << true << YAML::EndMap;
		else
			out << path;
	}
	out << YAML::EndMap << YAML::EndMap;
	return std::string{out.c_str()} + "\n";
}

auto AssetRegistry::find(const AssetGuid iGuid) const -> std::optional<Entry> {
	const ReadLock lock(m_mutex);
	if (const auto entry = m_entries.find(iGuid); entry != m_entries.end())
		return entry->second;
	return std::nullopt;
}

auto AssetRegistry::findGuid(const std::string_view iPath) const -> std::optional<AssetGuid> {
	const std::filesystem::path path{iPath};
	const ReadLock lock(m_mutex);
	if (path.is_absolute()) {
		if (const auto guid = m_byAbsolutePath.find(keyOf(path)); guid != m_byAbsolutePath.end())
			return guid->second;
		return std::nullopt;
	}
	if (const auto guid = m_byRelativePath.find(path.lexically_normal().generic_string());
		guid != m_byRelativePath.end())
		return guid->second;
	return std::nullopt;
}

auto AssetRegistry::toAssetPath(const std::string_view iReference) const -> std::string {
	auto parsed = parseReference(iReference);
	if (!parsed.guid)
		return std::move(parsed.path);
	if (const auto entry = find(*parsed.guid); entry)
		return entry->path;
	OWL_CORE_TRACE("Assets: GUID {} unknown, falling back to '{}'.", formatGuid(*parsed.guid), parsed.path)
	return std::move(parsed.path);
}

auto AssetRegistry::toAssetReference(const std::string_view iPath) const -> std::string {
	if (iPath.empty() || iPath.starts_with(g_GuidReferencePrefix))
		return std::string{iPath};
	const auto guid = findGuid(iPath);
	if (!guid)
		return std::string{iPath};
	const auto entry = find(*guid);
	return formatReference(*guid, entry ? entry->path : std::string{iPath});
}

auto AssetRegistry::rewriteReferences(const std::string_view iText) const -> std::string {
	const auto rewriteValue = [this](const std::string_view iValue) -> std::string {
		const auto value = trimmedRight(iValue);
		const bool quoted =
				value.size() >= 2 && (value.front() == '\'' || value.front() == '"') && value.back() == value.front();
		const auto bare = quoted ? value.substr(1, value.size() - 2) : value;
		std::string reference;
		if (bare.starts_with(g_TextureNamePrefix))
			reference = toAssetReference(bare.substr(g_TextureNamePrefix.size()));
		else if (bare.starts_with("pat:"))
			reference = toAssetReference(bare.substr(4));
		else if (!bare.starts_with(g_GuidReferencePrefix))
			reference = toAssetReference(bare);
		if (!reference.starts_with(g_GuidReferencePrefix))
			return {};
		const auto quote = quoted ? std::string(1, value.front()) : std::string{};
		return std::format("{}{}{}{}", quote, reference, quote, iValue.substr(value.size()));
	};
	const auto rewriteFlowMap = [&rewriteValue](const std::string_view iEntries) -> std::string {
		std::string out;
		bool changed = false;
		size_t cursor = 0;
		while (cursor <= iEntries.size()) {
			const auto comma = std::min(iEntries.size(), findUnquoted(iEntries, ',', cursor));
			const auto entry = iEntries.substr(cursor, comma - cursor);
			const auto separator = entry.find(": ");
			std::string rewritten;
			if (separator != std::string_view::npos) {
				const auto key = entry.substr(0, separator);
				if (isReferenceKey(key.substr(std::min(key.size(), key.find_first_not_of(' ')))))
					rewritten = rewriteValue(entry.substr(separator + 2));
			}
			changed |= !rewritten.empty();
			out += rewritten.empty() ? std::string{entry}
									 : std::format("{}{}", entry.substr(0, separator + 2), rewritten);
			if (comma < iEntries.size())
				out += ',';
			cursor = comma + 1;
		}
		return changed ? out : std::string{};
	};
	const auto headerEnd = std::min(iText.size(), iText.find("\n...\n"));
	std::string out;
	out.reserve(iText.size());
	size_t cursor = 0;
	while (cursor < headerEnd) {
		const auto lineEnd = std::min(headerEnd, iText.find('\n', cursor));
		const auto line = iText.substr(cursor, lineEnd - cursor);
		cursor = lineEnd == headerEnd ? headerEnd : lineEnd + 1;
		const auto separator = line.find(": ");
		auto keyStart = line.find_first_not_of(' ');
		if (keyStart != std::string_view::npos && line.substr(keyStart).starts_with("- "))
			keyStart += 2;
		const auto content = trimmedRight(line);
		std::string rewritten;
		if (keyStart < content.size() && content[keyStart] == '{' && content.ends_with('}')) {
			if (const auto entries = rewriteFlowMap(content.substr(keyStart + 1, content.size() - keyStart - 2));
				!entries.empty())
				rewritten =
						std::format("{}{}{}", line.substr(0, keyStart + 1), entries, line.substr(content.size() - 1));
		} else if (separator != std::string_view::npos && keyStart != std::string_view::npos && keyStart < separator &&
				   isReferenceKey(line.substr(keyStart, separator - keyStart))) {
			if (const auto value = rewriteValue(line.substr(separator + 2)); !value.empty())
				rewritten = std::format("{}{}", line.substr(0, separator + 2), value);
		}
		out += rewritten.empty() ? std::string{line} : rewritten;
		if (lineEnd != headerEnd)
			out += '\n';
	}
	out.append(iText.substr(headerEnd));
	return out;
}

auto AssetRegistry::getEditableDirectories() const -> std::vector<std::filesystem::path> {
	const ReadLock lock(m_mutex);
	std::vector<std::filesystem::path> directories;
	for (const auto& root: m_roots)
		if (root.editable)
			directories.push_back(root.path);
	return directories;
}

auto AssetRegistry::getEntries() const -> std::vector<Entry> {
	const ReadLock lock(m_mutex);
	std::vector<Entry> entries;
	entries.reserve(m_entries.size());
	for (const auto& entry: m_entries | std::views::values) entries.push_back(entry);
	return entries;
}

auto AssetRegistry::size() const -> size_t {
	const ReadLock lock(m_mutex);
	return m_entries.size();
}

void AssetRegistry::clear() {
	const WriteLock lock(m_mutex);
	m_roots.clear();
	m_entries.clear();
	m_byRelativePath.clear();
	m_byAbsolutePath.clear();
}

}// namespace owl::data::assets
