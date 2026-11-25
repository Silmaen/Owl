/**
 * @file ExtensionManifest.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/pack/ExtensionManifest.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace owl::data::assets::pack {

namespace {

constexpr std::array<core::MigrationStep, 0> g_manifestMigrations{};
constexpr core::DocumentFormat g_manifestFormat{.name = "Extension manifest", .migrations = g_manifestMigrations};
constexpr std::string_view g_rootKey{"OwlExtension"};
constexpr size_t g_maxIdLength{64};

auto parseDependency(const YAML::Node& iNode) -> std::optional<ExtensionDependency> {
	ExtensionDependency dependency;
	if (iNode.IsScalar()) {
		dependency.id = iNode.as<std::string>();
	} else if (iNode.IsMap()) {
		get(iNode, "id", dependency.id);
		if (const auto version = iNode["version"]; version) {
			const auto parsed = ExtensionVersion::parse(version.as<std::string>());
			if (!parsed)
				return std::nullopt;
			dependency.minVersion = *parsed;
		}
	}
	if (!isValidExtensionId(dependency.id))
		return std::nullopt;
	return dependency;
}

auto readFields(const YAML::Node& iNode, ExtensionManifest& oManifest, const std::string_view iSourceName)
		-> std::optional<ManifestError> {
	get(iNode, "id", oManifest.id);
	if (!isValidExtensionId(oManifest.id)) {
		OWL_CORE_WARN("Extensions: Manifest '{}' has no valid 'id' ('{}').", iSourceName, oManifest.id)
		return ManifestError::InvalidId;
	}
	get(iNode, "name", oManifest.name);
	get(iNode, "author", oManifest.author);
	get(iNode, "description", oManifest.description);
	get(iNode, "loadOrder", oManifest.loadOrder);
	get(iNode, "script", oManifest.script);
	get(iNode, "editorScript", oManifest.editorScript);
	if (const auto version = iNode["version"]; version) {
		const auto parsed = ExtensionVersion::parse(version.as<std::string>());
		if (!parsed) {
			OWL_CORE_WARN("Extensions: Manifest '{}' has an invalid 'version'.", iSourceName)
			return ManifestError::InvalidVersion;
		}
		oManifest.version = *parsed;
	}
	if (const auto kind = iNode["kind"]; kind) {
		const auto parsed = parseKind(kind.as<std::string>());
		if (!parsed) {
			OWL_CORE_WARN("Extensions: Manifest '{}' names an unknown kind '{}'.", iSourceName, kind.as<std::string>())
			return ManifestError::UnknownKind;
		}
		oManifest.kind = *parsed;
	}
	if (const auto dependencies = iNode["dependencies"]; dependencies && dependencies.IsSequence()) {
		for (const auto& item: dependencies) {
			const auto dependency = parseDependency(item);
			if (!dependency) {
				OWL_CORE_WARN("Extensions: Manifest '{}' has an invalid dependency.", iSourceName)
				return ManifestError::InvalidDependency;
			}
			oManifest.dependencies.push_back(*dependency);
		}
	}
	return std::nullopt;
}

}// namespace

auto getKindName(const ExtensionKind iKind) -> std::string_view {
	switch (iKind) {
		case ExtensionKind::Data:
			return "Data";
		case ExtensionKind::Script:
			return "Script";
	}
	return "Data";
}

auto parseKind(const std::string_view iName) -> std::optional<ExtensionKind> {
	if (iName == "Data")
		return ExtensionKind::Data;
	if (iName == "Script")
		return ExtensionKind::Script;
	return std::nullopt;
}

auto ExtensionVersion::parse(const std::string_view iText) -> std::optional<ExtensionVersion> {
	ExtensionVersion version{.numbers = {0, 0, 0}};
	size_t index = 0;
	std::string_view rest = iText;
	while (true) {
		if (index >= version.numbers.size() || rest.empty())
			return std::nullopt;
		const auto dot = rest.find('.');
		const auto part = rest.substr(0, dot);
		uint32_t value = 0;
		if (const auto [ptr, error] = std::from_chars(part.data(), part.data() + part.size(), value);
			part.empty() || error != std::errc{} || ptr != part.data() + part.size())
			return std::nullopt;
		version.numbers.at(index++) = value;
		if (dot == std::string_view::npos)
			return version;
		rest = rest.substr(dot + 1);
	}
}

auto ExtensionVersion::toString() const -> std::string {
	return std::format("{}.{}.{}", numbers[0], numbers[1], numbers[2]);
}

auto describe(const ManifestError iError) -> std::string_view {
	switch (iError) {
		case ManifestError::Unreadable:
			return "the manifest is no YAML `OwlExtension` map";
		case ManifestError::NewerVersion:
			return "the manifest was written by a newer Owl";
		case ManifestError::InvalidId:
			return "the manifest has no valid id";
		case ManifestError::InvalidVersion:
			return "the manifest version is no `major.minor.patch`";
		case ManifestError::UnknownKind:
			return "the manifest kind is neither Data nor Script";
		case ManifestError::InvalidDependency:
			return "a dependency has no valid id or version";
	}
	return "unknown manifest error";
}

auto isValidExtensionId(const std::string_view iId) -> bool {
	if (iId.empty() || iId.size() > g_maxIdLength)
		return false;
	const auto isAlnum = [](const char iChar) -> bool {
		return (iChar >= 'a' && iChar <= 'z') || (iChar >= '0' && iChar <= '9');
	};
	if (!isAlnum(iId.front()))
		return false;
	return std::ranges::all_of(iId, [&isAlnum](const char iChar) -> bool {
		return isAlnum(iChar) || iChar == '.' || iChar == '_' || iChar == '-';
	});
}

auto ExtensionManifest::parse(const std::string_view iText, const std::string_view iSourceName)
		-> expected<ExtensionManifest, ManifestError> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iText});
	} catch (const YAML::Exception& exception) {
		OWL_CORE_WARN("Extensions: Cannot parse manifest '{}': {}.", iSourceName, exception.what())
		return unexpected{ManifestError::Unreadable};
	}
	if (!root.IsMap()) {
		OWL_CORE_WARN("Extensions: Manifest '{}' is not a YAML map.", iSourceName)
		return unexpected{ManifestError::Unreadable};
	}
	if (const auto version = core::upgradeYamlDocument(g_manifestFormat, root, iSourceName); !version) {
		OWL_CORE_WARN("Extensions: Cannot read manifest '{}': {}.", iSourceName, core::describe(version.error()))
		return unexpected{ManifestError::NewerVersion};
	}
	const auto node = root[std::string{g_rootKey}];
	if (!node || !node.IsMap()) {
		OWL_CORE_WARN("Extensions: Manifest '{}' has no '{}' map.", iSourceName, g_rootKey)
		return unexpected{ManifestError::Unreadable};
	}
	ExtensionManifest manifest;
	try {
		if (const auto error = readFields(node, manifest, iSourceName); error)
			return unexpected{*error};
	} catch (const YAML::Exception& exception) {
		OWL_CORE_WARN("Extensions: Cannot read manifest '{}': {}.", iSourceName, exception.what())
		return unexpected{ManifestError::Unreadable};
	}
	return manifest;
}

auto ExtensionManifest::serialize() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	core::emitFormatVersion(out, g_manifestFormat);
	out << YAML::Key << std::string{g_rootKey} << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "id" << YAML::Value << id;
	if (!name.empty())
		out << YAML::Key << "name" << YAML::Value << name;
	out << YAML::Key << "version" << YAML::Value << version.toString();
	out << YAML::Key << "kind" << YAML::Value << std::string{getKindName(kind)};
	out << YAML::Key << "loadOrder" << YAML::Value << loadOrder;
	if (!author.empty())
		out << YAML::Key << "author" << YAML::Value << author;
	if (!description.empty())
		out << YAML::Key << "description" << YAML::Value << description;
	if (!script.empty())
		out << YAML::Key << "script" << YAML::Value << script;
	if (!editorScript.empty())
		out << YAML::Key << "editorScript" << YAML::Value << editorScript;
	if (!dependencies.empty()) {
		out << YAML::Key << "dependencies" << YAML::Value << YAML::BeginSeq;
		for (const auto& dependency: dependencies) {
			out << YAML::BeginMap;
			out << YAML::Key << "id" << YAML::Value << dependency.id;
			out << YAML::Key << "version" << YAML::Value << dependency.minVersion.toString();
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}
	out << YAML::EndMap;
	out << YAML::EndMap;
	return std::string{out.c_str()} + "\n";
}

}// namespace owl::data::assets::pack
