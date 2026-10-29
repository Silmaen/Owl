/**
 * @file Localisation.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/localisation/Localisation.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "platform/AtomicFile.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <ranges>
#include <set>
#include <sstream>
#include <string>

namespace owl::data::localisation {

namespace {

constexpr std::array<core::MigrationStep, 0> g_localisationMigrations{};
constexpr core::DocumentFormat g_localisationFormat{.name = "Localisation", .migrations = g_localisationMigrations};

auto parseLanguage(const YAML::Node& iNode) -> std::optional<Language> {
	const auto code = iNode["code"].as<std::string>("");
	if (code.empty())
		return std::nullopt;
	return Language{.code = code,
					.name = iNode["name"].as<std::string>(code),
					.font = iNode["font"].as<std::string>(""),
					.exported = iNode["export"].as<bool>(true)};
}

auto parseString(const YAML::Node& iNode) -> std::optional<LocalisedString> {
	const auto key = iNode["key"].as<std::string>("");
	if (key.empty())
		return std::nullopt;
	LocalisedString entry{.key = key, .table = iNode["table"].as<std::string>(""), .values = {}};
	if (const auto values = iNode["values"]; values && values.IsMap())
		for (const auto& value: values) entry.values[value.first.as<std::string>()] = value.second.as<std::string>();
	return entry;
}

}// namespace

auto Localisation::format() -> const core::DocumentFormat& { return g_localisationFormat; }

auto Localisation::serializeToString() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Localisation" << YAML::Value << "";
	core::emitFormatVersion(out, g_localisationFormat);
	out << YAML::Key << "fallback" << YAML::Value << fallback;
	out << YAML::Key << "languages" << YAML::Value << YAML::BeginSeq;
	for (const auto& language: languages) {
		out << YAML::Flow << YAML::BeginMap;
		out << YAML::Key << "code" << YAML::Value << language.code;
		out << YAML::Key << "name" << YAML::Value << language.name;
		if (!language.font.empty())
			out << YAML::Key << "font" << YAML::Value << language.font;
		out << YAML::Key << "export" << YAML::Value << language.exported;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::Key << "strings" << YAML::Value << YAML::BeginSeq;
	for (const auto& entry: strings) {
		out << YAML::BeginMap;
		out << YAML::Key << "key" << YAML::Value << entry.key;
		if (!entry.table.empty())
			out << YAML::Key << "table" << YAML::Value << entry.table;
		out << YAML::Key << "values" << YAML::Value << YAML::BeginMap;
		for (const auto& [code, value]: entry.values) out << YAML::Key << code << YAML::Value << value;
		out << YAML::EndMap;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;
	return std::string{out.c_str()};
}

auto Localisation::deserializeFromString(const std::string_view iYaml) -> std::optional<Localisation> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("Localisation: Failed to parse YAML: {}.", e.what())
		return std::nullopt;
	}
	if (!root || !root.IsMap() || !root["Localisation"]) {
		OWL_CORE_WARN("Localisation: Document is not a localisation.")
		return std::nullopt;
	}
	if (!core::upgradeYamlDocument(g_localisationFormat, root, "<buffer>")) {
		OWL_CORE_ERROR("Localisation: Document cannot be read.")
		return std::nullopt;
	}
	try {
		Localisation result{.languages = {}, .fallback = root["fallback"].as<std::string>(""), .strings = {}};
		for (const auto& node: root["languages"])
			if (auto language = parseLanguage(node); language && result.findLanguage(language->code) == nullptr)
				result.languages.push_back(std::move(*language));
		for (const auto& node: root["strings"])
			if (auto entry = parseString(node); entry && result.findString(entry->key) == nullptr)
				result.strings.push_back(std::move(*entry));
		return result;
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("Localisation: Invalid value: {}.", e.what())
		return std::nullopt;
	}
}

auto Localisation::saveToFile(const std::filesystem::path& iPath) const -> bool {
	if (const auto written = platform::writeFileAtomic(iPath, serializeToString()); !written) {
		OWL_CORE_ERROR("Localisation: Failed to write '{}': {}.", iPath.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto Localisation::loadFile(const std::filesystem::path& iPath) -> std::optional<Localisation> {
	const std::ifstream file(iPath);
	if (!file) {
		OWL_CORE_WARN("Localisation: Cannot open '{}'.", iPath.string())
		return std::nullopt;
	}
	std::stringstream text;
	text << file.rdbuf();
	return deserializeFromString(text.str());
}

auto Localisation::findLanguage(const std::string_view iCode) const -> const Language* {
	const auto found = std::ranges::find(languages, iCode, &Language::code);
	return found == languages.end() ? nullptr : &*found;
}

auto Localisation::findString(const std::string_view iKey) const -> const LocalisedString* {
	const auto found = std::ranges::find(strings, iKey, &LocalisedString::key);
	return found == strings.end() ? nullptr : &*found;
}

auto Localisation::translate(const std::string_view iKey, const std::string_view iLanguage) const -> std::string_view {
	const auto* entry = findString(iKey);
	return entry == nullptr ? iKey : getValue(*entry, iLanguage, fallback);
}

auto getValue(const LocalisedString& iEntry, const std::string_view iLanguage, const std::string_view iFallback)
		-> std::string_view {
	for (const auto code: {iLanguage, iFallback})
		if (const auto value = iEntry.values.find(code); value != iEntry.values.end() && !value->second.empty())
			return value->second;
	return iEntry.key;
}

auto Localisation::resolveLanguage(const std::string_view iCode) const -> std::string {
	if (findLanguage(iCode) != nullptr)
		return std::string{iCode};
	if (findLanguage(fallback) != nullptr)
		return fallback;
	return languages.empty() ? std::string{} : languages.front().code;
}

auto Localisation::getTables() const -> std::vector<std::string> {
	std::set<std::string> tables;
	for (const auto& entry: strings) tables.insert(entry.table);
	return {tables.begin(), tables.end()};
}

auto Localisation::getMissing(const std::string_view iCode) const -> std::vector<std::string> {
	std::vector<std::string> missing;
	for (const auto& entry: strings)
		if (const auto value = entry.values.find(iCode); value == entry.values.end() || value->second.empty())
			missing.push_back(entry.key);
	return missing;
}

auto Localisation::withExportedLanguages() const -> Localisation {
	Localisation result{.languages = {}, .fallback = fallback, .strings = strings};
	std::ranges::copy_if(languages, std::back_inserter(result.languages),
						 [](const Language& iLanguage) -> bool { return iLanguage.exported; });
	for (auto& entry: result.strings)
		std::erase_if(entry.values,
					  [&](const auto& iValue) -> bool { return result.findLanguage(iValue.first) == nullptr; });
	if (result.findLanguage(fallback) == nullptr)
		result.fallback = result.languages.empty() ? std::string{} : result.languages.front().code;
	return result;
}

}// namespace owl::data::localisation
