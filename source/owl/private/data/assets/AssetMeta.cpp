/**
 * @file AssetMeta.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/AssetMeta.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "platform/AtomicFile.h"
#include "platform/FileUtils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace owl::data::assets {

namespace {
constexpr std::array<core::MigrationStep, 0> g_metaMigrations{};
constexpr core::DocumentFormat g_metaFormat{.name = "Asset meta", .migrations = g_metaMigrations};

constexpr std::array<std::pair<std::string_view, Importer>, 30> g_importerByExtension{{
		{".png", Importer::Texture},
		{".jpg", Importer::Texture},
		{".jpeg", Importer::Texture},
		{".bmp", Importer::Texture},
		{".tga", Importer::Texture},
		{".hdr", Importer::Copy},
		{".glb", Importer::Mesh},
		{".gltf", Importer::Mesh},
		{".obj", Importer::Mesh},
		{".fbx", Importer::Mesh},
		{".wav", Importer::Sound},
		{".ogg", Importer::Sound},
		{".mp3", Importer::Sound},
		{".flac", Importer::Sound},
		{".lua", Importer::Script},
		{".owlvs", Importer::Script},
		{".ttf", Importer::Font},
		{".otf", Importer::Font},
		{".owl", Importer::Document},
		{".owlmat", Importer::Document},
		{".owltileset", Importer::Document},
		{".owltilemap", Importer::Document},
		{".owlhud", Importer::Document},
		{".owlprefab", Importer::Document},
		{".owlanim", Importer::Document},
		{".owlsound", Importer::Document},
		{".owltable", Importer::Document},
		{".owldlg", Importer::Document},
		{".owlseq", Importer::Document},
		{".owlvoxstruct", Importer::Document},
}};

constexpr std::array<std::pair<Importer, std::string_view>, 7> g_importerNames{{
		{Importer::Copy, "Copy"},
		{Importer::Texture, "Texture"},
		{Importer::Mesh, "Mesh"},
		{Importer::Sound, "Sound"},
		{Importer::Script, "Script"},
		{Importer::Font, "Font"},
		{Importer::Document, "Document"},
}};

auto importerFromName(const std::string_view iName) -> std::optional<Importer> {
	for (const auto& [importer, name]: g_importerNames)
		if (name == iName)
			return importer;
	return std::nullopt;
}

auto lowerExtension(const std::filesystem::path& iFile) -> std::string {
	auto extension = iFile.extension().string();
	std::ranges::transform(extension, extension.begin(),
						   [](const unsigned char iChar) -> char { return static_cast<char>(std::tolower(iChar)); });
	return extension;
}
}// namespace

auto describe(const MetaError iError) -> std::string_view {
	switch (iError) {
		case MetaError::Unreadable:
			return "the sidecar cannot be read";
		case MetaError::InvalidGuid:
			return "the sidecar holds no valid GUID";
		case MetaError::NewerVersion:
			return "the sidecar was written by a newer Owl";
		case MetaError::WriteFailed:
			return "the sidecar cannot be written";
	}
	return "unknown sidecar error";
}

auto importerOf(const std::filesystem::path& iFile) -> std::optional<Importer> {
	const auto extension = lowerExtension(iFile);
	for (const auto& [ext, importer]: g_importerByExtension)
		if (ext == extension)
			return importer;
	return std::nullopt;
}

auto importerName(const Importer iImporter) -> std::string_view {
	for (const auto& [importer, name]: g_importerNames)
		if (importer == iImporter)
			return name;
	return "Copy";
}

auto metaPathOf(const std::filesystem::path& iFile) -> std::filesystem::path {
	return std::filesystem::path{iFile.string() + std::string{g_MetaExtension}};
}

auto parseMeta(const std::string_view iText, const std::string_view iSourceName) -> expected<AssetMeta, MetaError> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iText});
	} catch (const YAML::Exception& exception) {
		OWL_CORE_WARN("Assets: Cannot parse sidecar '{}': {}.", iSourceName, exception.what())
		return unexpected{MetaError::Unreadable};
	}
	if (!root.IsMap()) {
		OWL_CORE_WARN("Assets: Sidecar '{}' is not a YAML map.", iSourceName)
		return unexpected{MetaError::Unreadable};
	}
	if (const auto version = core::upgradeYamlDocument(g_metaFormat, root, iSourceName); !version) {
		OWL_CORE_WARN("Assets: Cannot read sidecar '{}': {}.", iSourceName, core::describe(version.error()))
		return unexpected{MetaError::NewerVersion};
	}
	AssetMeta meta;
	try {
		const auto guid = root["Guid"] ? parseGuid(root["Guid"].as<std::string>()) : std::nullopt;
		if (!guid) {
			OWL_CORE_WARN("Assets: Sidecar '{}' has no valid 'Guid'.", iSourceName)
			return unexpected{MetaError::InvalidGuid};
		}
		meta.guid = *guid;
		if (root["Importer"]) {
			if (const auto importer = importerFromName(root["Importer"].as<std::string>()); importer)
				meta.importer = *importer;
			else
				OWL_CORE_WARN("Assets: Sidecar '{}' names an unknown importer, read as Copy.", iSourceName)
		}
		if (const auto settings = root["Settings"]; settings && meta.importer == Importer::Texture) {
			get(settings, "Nearest", meta.texture.nearest);
			get(settings, "Mips", meta.texture.mips);
			if (const auto compression = settings["Compression"]; compression) {
				const auto name = compression.as<std::string>();
				if (const auto value = magic_enum::enum_cast<TextureCompression>(name); value)
					meta.texture.compression = *value;
				else
					OWL_CORE_WARN("Assets: Sidecar '{}' names an unknown compression '{}', read as None.", iSourceName,
								  name)
			}
		}
		if (const auto settings = root["Settings"]; settings && meta.importer == Importer::Mesh) {
			get(settings, "Lods", meta.mesh.lods);
			get(settings, "LodReduction", meta.mesh.lodReduction);
			get(settings, "LodScreenSize", meta.mesh.lodScreenSize);
		}
	} catch (const YAML::Exception& exception) {
		OWL_CORE_WARN("Assets: Cannot read sidecar '{}': {}.", iSourceName, exception.what())
		return unexpected{MetaError::Unreadable};
	}
	return meta;
}

auto serializeMeta(const AssetMeta& iMeta) -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	core::emitFormatVersion(out, g_metaFormat);
	out << YAML::Key << "Guid" << YAML::Value << formatGuid(iMeta.guid);
	out << YAML::Key << "Importer" << YAML::Value << std::string{importerName(iMeta.importer)};
	if (iMeta.importer == Importer::Texture) {
		out << YAML::Key << "Settings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Nearest" << YAML::Value << iMeta.texture.nearest;
		out << YAML::Key << "Compression" << YAML::Value
			<< std::string{magic_enum::enum_name(iMeta.texture.compression)};
		out << YAML::Key << "Mips" << YAML::Value << iMeta.texture.mips;
		out << YAML::EndMap;
	}
	if (iMeta.importer == Importer::Mesh && iMeta.mesh.lods > 0) {
		out << YAML::Key << "Settings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Lods" << YAML::Value << iMeta.mesh.lods;
		out << YAML::Key << "LodReduction" << YAML::Value << iMeta.mesh.lodReduction;
		out << YAML::Key << "LodScreenSize" << YAML::Value << iMeta.mesh.lodScreenSize;
		out << YAML::EndMap;
	}
	out << YAML::EndMap;
	return std::string{out.c_str()} + "\n";
}

auto readMeta(const std::filesystem::path& iMetaFile) -> expected<AssetMeta, MetaError> {
	std::error_code error;
	if (!std::filesystem::is_regular_file(iMetaFile, error)) {
		OWL_CORE_WARN("Assets: Cannot open sidecar '{}'.", iMetaFile.string())
		return unexpected{MetaError::Unreadable};
	}
	return parseMeta(platform::fileToString(iMetaFile), iMetaFile.string());
}

auto writeMeta(const std::filesystem::path& iMetaFile, const AssetMeta& iMeta) -> expected<void, MetaError> {
	if (const auto written = platform::writeFileAtomic(iMetaFile, serializeMeta(iMeta)); !written) {
		OWL_CORE_WARN("Assets: Cannot write sidecar '{}': {}.", iMetaFile.string(), platform::describe(written.error()))
		return unexpected{MetaError::WriteFailed};
	}
	return {};
}

}// namespace owl::data::assets
