/**
 * @file CookCache.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/cook/CookCache.h"

#include "core/external/yaml.h"
#include "platform/AtomicFile.h"

#include <xxhash.h>

#include <format>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace owl::data::assets::cook {

namespace {

constexpr std::string_view g_manifestName{"manifest.yml"};

auto settingsOf(const AssetMeta& iMeta) -> std::string {
	auto settings = std::format("{}:{}", importerName(iMeta.importer), getCookerVersion(iMeta.importer));
	if (iMeta.importer == Importer::Texture)
		settings += std::format(":{}:{}:{}", iMeta.texture.nearest, magic_enum::enum_name(iMeta.texture.compression),
								iMeta.texture.mips);
	return settings;
}

auto readBytes(const std::filesystem::path& iFile) -> std::optional<std::vector<uint8_t>> {
	std::ifstream file(iFile, std::ios::binary);
	if (!file.is_open())
		return std::nullopt;
	return std::vector<uint8_t>{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

auto writeTimeOf(const std::filesystem::path& iFile) -> int64_t {
	std::error_code error;
	const auto time = std::filesystem::last_write_time(iFile, error);
	return error ? 0 : static_cast<int64_t>(time.time_since_epoch().count());
}

auto keyOf(const std::span<const uint8_t> iSource, const std::string& iSettings) -> std::string {
	XXH3_state_t* state = XXH3_createState();
	XXH3_128bits_reset(state);
	XXH3_128bits_update(state, iSource.data(), iSource.size());
	XXH3_128bits_update(state, iSettings.data(), iSettings.size());
	const auto hash = XXH3_128bits_digest(state);
	XXH3_freeState(state);
	return std::format("{:016x}{:016x}", hash.high64, hash.low64);
}

}// namespace

CookCache::CookCache(std::filesystem::path iDirectory) : m_directory{std::move(iDirectory)} {
	std::error_code error;
	std::filesystem::create_directories(m_directory, error);
	const auto manifestFile = m_directory / g_manifestName;
	if (!std::filesystem::exists(manifestFile, error))
		return;
	try {
		const auto root = YAML::LoadFile(manifestFile.string());
		for (const auto& item: root["Sources"]) {
			ManifestEntry entry;
			entry.size = item.second["Size"].as<uintmax_t>();
			entry.writeTime = item.second["Time"].as<int64_t>();
			entry.settings = item.second["Settings"].as<std::string>();
			entry.key = item.second["Key"].as<std::string>();
			m_manifest.emplace(item.first.as<std::string>(), std::move(entry));
		}
	} catch (const YAML::Exception& exception) {
		OWL_CORE_WARN("Cook: Manifest '{}' unreadable ({}), every asset is hashed again.", manifestFile.string(),
					  exception.what())
		m_manifest.clear();
	}
}

CookCache::~CookCache() { std::ignore = saveManifest(); }

auto CookCache::computeKey(const std::span<const uint8_t> iSource, const AssetMeta& iMeta) -> std::string {
	return keyOf(iSource, settingsOf(iMeta));
}

auto CookCache::resultPath(const std::string& iKey) const -> std::filesystem::path {
	return m_directory / iKey.substr(0, 2) / (iKey + ".owlc");
}

auto CookCache::manifestKey(const std::filesystem::path& iSource, const std::string& iSettings) -> std::string {
	std::error_code error;
	const auto size = std::filesystem::file_size(iSource, error);
	if (error)
		return {};
	const std::scoped_lock<std::mutex> lock(m_mutex);
	const auto entry = m_manifest.find(iSource.generic_string());
	if (entry == m_manifest.end() || entry->second.size != size || entry->second.writeTime != writeTimeOf(iSource) ||
		entry->second.settings != iSettings)
		return {};
	return entry->second.key;
}

auto CookCache::isCached(const std::filesystem::path& iSource, const AssetMeta& iMeta) -> bool {
	std::error_code error;
	if (const auto key = manifestKey(iSource, settingsOf(iMeta)); !key.empty())
		return std::filesystem::exists(resultPath(key), error);
	const auto source = readBytes(iSource);
	return source && std::filesystem::exists(resultPath(computeKey(*source, iMeta)), error);
}

auto CookCache::cook(const std::filesystem::path& iSource, const AssetMeta& iMeta, const std::string_view iName)
		-> expected<std::vector<uint8_t>, CookError> {
	const auto settings = settingsOf(iMeta);
	if (const auto key = manifestKey(iSource, settings); !key.empty()) {
		if (auto cached = readBytes(resultPath(key)); cached) {
			const std::scoped_lock<std::mutex> lock(m_mutex);
			++m_stats.hits;
			return std::move(*cached);
		}
	}
	const auto source = readBytes(iSource);
	if (!source) {
		OWL_CORE_WARN("Cook: Cannot read '{}'.", iSource.string())
		const std::scoped_lock<std::mutex> lock(m_mutex);
		++m_stats.failed;
		return unexpected{CookError::Unreadable};
	}
	const auto key = keyOf(*source, settings);
	const auto remember = [&]() -> void {
		std::error_code error;
		const std::scoped_lock<std::mutex> lock(m_mutex);
		m_manifest[iSource.generic_string()] = ManifestEntry{.size = std::filesystem::file_size(iSource, error),
															 .writeTime = writeTimeOf(iSource),
															 .settings = settings,
															 .key = key};
		m_dirty = true;
	};
	if (auto cached = readBytes(resultPath(key)); cached) {
		remember();
		const std::scoped_lock<std::mutex> lock(m_mutex);
		++m_stats.hits;
		return std::move(*cached);
	}
	auto cooked = cookAsset(iMeta, *source, iName);
	if (!cooked) {
		const std::scoped_lock<std::mutex> lock(m_mutex);
		++m_stats.failed;
		return cooked;
	}
	const auto result = resultPath(key);
	std::error_code error;
	std::filesystem::create_directories(result.parent_path(), error);
	if (const auto written = platform::writeFileAtomic(
				result, std::string_view{reinterpret_cast<const char*>(cooked->data()), cooked->size()});
		!written)
		OWL_CORE_WARN("Cook: Cannot store the result of '{}' in the cache: {}.", iName,
					  platform::describe(written.error()))
	else
		remember();
	const std::scoped_lock<std::mutex> lock(m_mutex);
	++m_stats.cooked;
	return cooked;
}

auto CookCache::saveManifest() -> bool {
	const std::scoped_lock<std::mutex> lock(m_mutex);
	if (!m_dirty)
		return true;
	YAML::Emitter out;
	out << YAML::BeginMap << YAML::Key << "Sources" << YAML::Value << YAML::BeginMap;
	for (const auto& [source, entry]: m_manifest) {
		out << YAML::Key << source << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << entry.size;
		out << YAML::Key << "Time" << YAML::Value << entry.writeTime;
		out << YAML::Key << "Settings" << YAML::Value << entry.settings;
		out << YAML::Key << "Key" << YAML::Value << entry.key;
		out << YAML::EndMap;
	}
	out << YAML::EndMap << YAML::EndMap;
	if (const auto written = platform::writeFileAtomic(m_directory / g_manifestName, out.c_str()); !written) {
		OWL_CORE_WARN("Cook: Cannot write the manifest of '{}': {}.", m_directory.string(),
					  platform::describe(written.error()))
		return false;
	}
	m_dirty = false;
	return true;
}

void CookCache::clear() {
	const std::scoped_lock<std::mutex> lock(m_mutex);
	std::error_code error;
	std::filesystem::remove_all(m_directory, error);
	std::filesystem::create_directories(m_directory, error);
	m_manifest.clear();
	m_dirty = false;
}

auto CookCache::getStats() const -> Stats {
	const std::scoped_lock<std::mutex> lock(m_mutex);
	return m_stats;
}

}// namespace owl::data::assets::cook
