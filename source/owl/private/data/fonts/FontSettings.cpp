/**
 * @file FontSettings.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/fonts/FontSettings.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "platform/AtomicFile.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <span>
#include <sstream>

namespace owl::data::fonts {

namespace {
constexpr std::array<core::MigrationStep, 0> g_fontSettingsMigrations{};
constexpr core::DocumentFormat g_fontSettingsFormat{.name = "FontSettings", .migrations = g_fontSettingsMigrations};

constexpr char32_t g_maxCodePoint = 0x10FFFF;

constexpr std::array<CodePointRange, 5> g_defaultRanges{{
		{.first = 0x0020, .last = 0x007E},// Basic Latin
		{.first = 0x00A0, .last = 0x00FF},// Latin-1 Supplement
		{.first = 0x0100, .last = 0x017F},// Latin Extended-A
		{.first = 0x2000, .last = 0x206F},// General Punctuation
		{.first = 0x20AC, .last = 0x20AC},// Euro sign
}};
}// namespace

auto FontSettings::defaultRanges() noexcept -> std::span<const CodePointRange> { return g_defaultRanges; }

auto FontSettings::format() -> const core::DocumentFormat& { return g_fontSettingsFormat; }

auto FontSettings::getCharsetRanges() const -> std::vector<CodePointRange> {
	std::vector<CodePointRange> ranges{g_defaultRanges.begin(), g_defaultRanges.end()};
	for (const auto& [first, last]: extraRanges) {
		const char32_t low = std::min(first, g_maxCodePoint);
		const char32_t high = std::min(last, g_maxCodePoint);
		ranges.push_back({.first = std::min(low, high), .last = std::max(low, high)});
	}
	return ranges;
}

auto FontSettings::sidecarPath(const std::filesystem::path& iFontPath) -> std::filesystem::path {
	auto path = iFontPath;
	path.replace_extension(fileExtension());
	return path;
}

auto FontSettings::serializeToString() const -> std::string {
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "FontSettings" << YAML::Value << YAML::BeginMap << YAML::EndMap;
	core::emitFormatVersion(emitter, g_fontSettingsFormat);
	emitter << YAML::Key << "extraRanges" << YAML::Value << YAML::BeginSeq;
	for (const auto& [first, last]: extraRanges)
		emitter << YAML::Flow << YAML::BeginSeq << static_cast<uint32_t>(first) << static_cast<uint32_t>(last)
				<< YAML::EndSeq;
	emitter << YAML::EndSeq;
	emitter << YAML::EndMap;
	return std::string{emitter.c_str()};
}

auto FontSettings::deserializeFromString(const std::string_view iYaml) -> bool {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("FontSettings: Failed to parse YAML ({}).", e.what())
		return false;
	}
	if (!root || !root.IsMap() || !root["FontSettings"]) {
		OWL_CORE_WARN("FontSettings: Document is not a font settings file.")
		return false;
	}
	if (!core::upgradeYamlDocument(g_fontSettingsFormat, root, "<buffer>")) {
		OWL_CORE_ERROR("FontSettings: Document cannot be read.")
		return false;
	}
	FontSettings parsed;
	if (const auto ranges = root["extraRanges"]; ranges && ranges.IsSequence()) {
		for (const auto& entry: ranges) {
			if (!entry.IsSequence() || entry.size() != 2) {
				OWL_CORE_WARN("FontSettings: Ignoring a malformed code-point range.")
				continue;
			}
			try {
				const auto first = std::min(entry[0].as<uint32_t>(), static_cast<uint32_t>(g_maxCodePoint));
				const auto last = std::min(entry[1].as<uint32_t>(), static_cast<uint32_t>(g_maxCodePoint));
				parsed.extraRanges.push_back({.first = static_cast<char32_t>(std::min(first, last)),
											  .last = static_cast<char32_t>(std::max(first, last))});
			} catch (const YAML::Exception& e) {
				OWL_CORE_WARN("FontSettings: Ignoring a code-point range that is not a number ({}).", e.what())
			}
		}
	}
	*this = std::move(parsed);
	return true;
}

auto FontSettings::saveToFile(const std::filesystem::path& iPath) const -> bool {
	if (const auto written = platform::writeFileAtomic(iPath, serializeToString()); !written) {
		OWL_CORE_ERROR("FontSettings: Failed to write '{}': {}.", iPath.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto FontSettings::loadFromFile(const std::filesystem::path& iPath) -> bool {
	std::ifstream in(iPath, std::ios::binary);
	if (!in.is_open()) {
		OWL_CORE_ERROR("FontSettings: Failed to open '{}' for reading.", iPath.string())
		return false;
	}
	std::stringstream buffer;
	buffer << in.rdbuf();
	return deserializeFromString(buffer.str());
}

auto FontSettings::loadForFont(const std::filesystem::path& iFontPath) -> FontSettings {
	FontSettings settings;
	if (const auto sidecar = sidecarPath(iFontPath); exists(sidecar) && !settings.loadFromFile(sidecar))
		OWL_CORE_WARN("FontSettings: Sidecar '{}' ignored, default charset only.", sidecar.string())
	return settings;
}

}// namespace owl::data::fonts
