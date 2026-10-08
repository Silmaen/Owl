/**
 * @file RecoveryManager.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "RecoveryManager.h"

#include "document/DocumentManager.h"

#include <platform/AtomicFile.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

#include <array>
#include <chrono>
#include <exception>
#include <format>
#include <fstream>
#include <functional>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace owl::nest {

namespace {

constexpr std::array<core::MigrationStep, 0> g_recoveryMigrations{};
constexpr core::DocumentFormat g_recoveryFormat{.name = "Recovery", .migrations = g_recoveryMigrations};

constexpr std::array<std::pair<DocumentType, const char*>, 6> g_typeNames{{
		{DocumentType::Scene, "Scene"},
		{DocumentType::Code, "Code"},
		{DocumentType::NodeGraph, "NodeGraph"},
		{DocumentType::Animation, "Animation"},
		{DocumentType::Tilemap, "Tilemap"},
		{DocumentType::Tileset, "Tileset"},
}};

auto typeName(const DocumentType iType) -> std::string {
	for (const auto& [type, name]: g_typeNames) {
		if (type == iType)
			return name;
	}
	return "Scene";
}

auto typeFromName(const std::string& iName) -> std::optional<DocumentType> {
	for (const auto& [type, name]: g_typeNames) {
		if (iName == name)
			return type;
	}
	return std::nullopt;
}

auto nowUtc() -> std::string {
	return std::format("{:%Y-%m-%d %H:%M:%S} UTC",
					   std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

// FNV-1a: stable across runs and compilers, unlike std::hash.
auto stableHash(const std::string& iText) -> uint64_t {
	uint64_t hash = 14695981039346656037ull;
	for (const char c: iText) {
		hash ^= static_cast<uint8_t>(c);
		hash *= 1099511628211ull;
	}
	return hash;
}

auto readText(const std::filesystem::path& iFile) -> std::optional<std::string> {
	const std::ifstream in(iFile, std::ios::binary);
	if (!in.is_open())
		return std::nullopt;
	std::stringstream buffer;
	buffer << in.rdbuf();
	return buffer.str();
}

auto isStale(const RecoveryEntry& iEntry, const std::filesystem::path& iData) -> bool {
	std::error_code ec;
	if (iEntry.originalPath.empty() || !exists(iEntry.originalPath, ec))
		return false;
	const auto original = last_write_time(iEntry.originalPath, ec);
	if (ec)
		return false;
	const auto snapshot = last_write_time(iData, ec);
	return !ec && original > snapshot;
}

}// namespace

auto RecoveryManager::format() -> const core::DocumentFormat& { return g_recoveryFormat; }

auto RecoveryManager::directoryFor(const std::filesystem::path& iRoot, const std::filesystem::path& iProjectDirectory)
		-> std::filesystem::path {
	auto normal = iProjectDirectory.lexically_normal();
	if (!normal.has_filename())
		normal = normal.parent_path();
	return iRoot / "OwlNest_recovery" /
		   std::format("{}-{:016x}", normal.filename().string(), stableHash(normal.generic_string()));
}

void RecoveryManager::setDirectory(const std::filesystem::path& iDirectory) {
	m_directory = iDirectory;
	m_written.clear();
	m_elapsed = 0.f;
}

auto RecoveryManager::onUpdate(const float iDeltaSeconds) -> bool {
	if (m_directory.empty() || m_suspended || m_interval <= 0.f)
		return false;
	m_elapsed += iDeltaSeconds;
	if (m_elapsed < m_interval)
		return false;
	m_elapsed = 0.f;
	return true;
}

auto RecoveryManager::autosave(const DocumentManager& iDocuments) -> size_t {
	if (m_directory.empty() || m_suspended)
		return 0;
	std::error_code ec;
	create_directories(m_directory, ec);
	if (ec) {
		OWL_WARN("Recovery: Cannot create '{}': {}.", m_directory.string(), ec.message())
		return 0;
	}
	size_t written = 0;
	std::vector<RecoveryEntry> entries;
	std::unordered_set<uint64_t> live;
	for (const auto& doc: iDocuments.list()) {
		if (!doc || !doc->isDirty())
			continue;
		const auto snapshot = doc->recoverySnapshot();
		if (!snapshot)
			continue;
		const auto id = static_cast<uint64_t>(doc->id());
		const auto dataFile = std::format("{}.snapshot", id);
		const auto hash = std::hash<std::string>{}(*snapshot);
		auto& last = m_written[id];
		if (last.savedAt.empty() || last.hash != hash || !exists(m_directory / dataFile, ec)) {
			if (const auto ok = platform::writeFileAtomic(m_directory / dataFile, *snapshot); !ok) {
				OWL_WARN("Recovery: Cannot autosave '{}': {}.", doc->title(), describe(ok.error()))
				m_written.erase(id);
				continue;
			}
			last = {.hash = hash, .savedAt = nowUtc()};
			++written;
		}
		live.insert(id);
		entries.push_back({.type = doc->type(),
						   .originalPath = doc->filePath(),
						   .title = doc->title(),
						   .dataFile = dataFile,
						   .savedAt = last.savedAt});
	}
	std::erase_if(m_written, [&](const auto& iItem) -> bool {
		if (live.contains(iItem.first))
			return false;
		std::error_code removeError;
		remove(m_directory / std::format("{}.snapshot", iItem.first), removeError);
		return true;
	});
	if (entries.empty()) {
		remove(m_directory / g_manifestName, ec);
		return written;
	}
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << std::string{core::g_FormatVersionKey} << YAML::Value << g_recoveryFormat.currentVersion();
	out << YAML::Key << "Recovery" << YAML::Value << YAML::BeginSeq;
	for (const auto& entry: entries) {
		out << YAML::BeginMap;
		out << YAML::Key << "type" << YAML::Value << typeName(entry.type);
		out << YAML::Key << "path" << YAML::Value << entry.originalPath.generic_string();
		out << YAML::Key << "title" << YAML::Value << entry.title;
		out << YAML::Key << "data" << YAML::Value << entry.dataFile;
		out << YAML::Key << "savedAt" << YAML::Value << entry.savedAt;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;
	if (const auto ok = platform::writeFileAtomic(m_directory / g_manifestName, out.c_str()); !ok)
		OWL_WARN("Recovery: Cannot write the manifest in '{}': {}.", m_directory.string(), describe(ok.error()))
	return written;
}

auto RecoveryManager::getPendingEntries() const -> std::vector<RecoveryEntry> {
	std::vector<RecoveryEntry> entries;
	if (m_directory.empty())
		return entries;
	const auto manifest = m_directory / g_manifestName;
	auto text = readText(manifest);
	if (!text)
		return entries;
	if (const auto version = core::upgradeDocumentText(g_recoveryFormat, *text, manifest.string()); !version) {
		OWL_WARN("Recovery: Cannot read '{}': {}.", manifest.string(), describe(version.error()))
		return entries;
	}
	try {
		const auto root = YAML::Load(*text);
		const auto list = root["Recovery"];
		if (!list || !list.IsSequence())
			return entries;
		for (const auto& node: list) {
			const auto type = typeFromName(node["type"].as<std::string>(""));
			RecoveryEntry entry{.type = type.value_or(DocumentType::Scene),
								.originalPath = node["path"].as<std::string>(""),
								.title = node["title"].as<std::string>(""),
								.dataFile = node["data"].as<std::string>(""),
								.savedAt = node["savedAt"].as<std::string>("")};
			const auto data = m_directory / entry.dataFile;
			std::error_code ec;
			if (!type || entry.dataFile.empty() || !exists(data, ec))
				continue;
			if (isStale(entry, data)) {
				OWL_INFO("Recovery: Autosave of '{}' skipped, the file was saved after it.", entry.title)
				continue;
			}
			entries.push_back(std::move(entry));
		}
	} catch (const std::exception& iEx) {
		OWL_WARN("Recovery: Cannot read '{}': {}.", manifest.string(), iEx.what())
		entries.clear();
	}
	return entries;
}

auto RecoveryManager::readSnapshot(const RecoveryEntry& iEntry) const -> std::optional<std::string> {
	if (m_directory.empty() || iEntry.dataFile.empty())
		return std::nullopt;
	auto text = readText(m_directory / iEntry.dataFile);
	if (!text)
		OWL_WARN("Recovery: Cannot read the autosave of '{}'.", iEntry.title)
	return text;
}

void RecoveryManager::clear() {
	m_written.clear();
	if (m_directory.empty())
		return;
	std::error_code ec;
	remove_all(m_directory, ec);
	if (ec)
		OWL_WARN("Recovery: Cannot empty '{}': {}.", m_directory.string(), ec.message())
}

auto RecoveryManager::keepAside() -> std::filesystem::path {
	m_written.clear();
	std::error_code ec;
	if (m_directory.empty() || !exists(m_directory, ec))
		return {};
	const auto seconds =
			std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
					.count();
	auto kept = m_directory;
	kept += std::format("-kept-{}", seconds);
	rename(m_directory, kept, ec);
	if (ec) {
		OWL_WARN("Recovery: Cannot move '{}' aside: {}.", m_directory.string(), ec.message())
		return m_directory;
	}
	return kept;
}

}// namespace owl::nest
