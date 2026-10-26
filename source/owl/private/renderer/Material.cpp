/**
 * @file Material.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/AssetReference.h"
#include "renderer/Material.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "math/YamlSerializers.h"
#include "platform/AtomicFile.h"
#include "platform/FileUtils.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace owl::renderer {

namespace {
constexpr std::array<core::MigrationStep, 0> g_materialMigrations{};
constexpr core::DocumentFormat g_materialFormat{.name = "Material", .migrations = g_materialMigrations};

// One overridable field: its bit, its key in the files, and how to copy, write and read it.
struct FieldCodec {
	MaterialField field;
	std::string_view key;
	std::function<void(MaterialParameters&, const MaterialParameters&)> copy;
	std::function<void(YAML::Emitter&, const MaterialParameters&)> emit;
	std::function<void(MaterialParameters&, const YAML::Node&)> read;
	std::function<bool(const MaterialParameters&, const MaterialParameters&)> equal;
};

template<typename T>
auto member(const MaterialField iField, const std::string_view iKey, T MaterialParameters::* iMember) -> FieldCodec {
	return {.field = iField,
			.key = iKey,
			.copy = [iMember](MaterialParameters& ioTo, const MaterialParameters& iFrom) -> void {
				ioTo.*iMember = iFrom.*iMember;
			},
			.emit = [iMember](YAML::Emitter& ioOut, const MaterialParameters& iValues) -> void {
				if constexpr (std::is_enum_v<T>)
					ioOut << std::string{magic_enum::enum_name(iValues.*iMember)};
				else
					ioOut << iValues.*iMember;
			},
			.read = [iMember](MaterialParameters& ioTo, const YAML::Node& iNode) -> void {
				if constexpr (std::is_enum_v<T>) {
					if (const auto parsed = magic_enum::enum_cast<T>(iNode.as<std::string>()); parsed)
						ioTo.*iMember = *parsed;
				} else {
					ioTo.*iMember = iNode.as<T>();
				}
			},
			.equal = [iMember](const MaterialParameters& iA, const MaterialParameters& iB) -> bool {
				// Exact comparison on purpose: an override equal to its base is still an override.
				return std::equal_to<T>{}(iA.*iMember, iB.*iMember);
			}};
}

auto mapCodec(const MaterialMap iMap, const std::string_view iKey) -> FieldCodec {
	const auto index = static_cast<size_t>(iMap);
	return {.field = mapField(iMap),
			.key = iKey,
			.copy = [index](MaterialParameters& ioTo, const MaterialParameters& iFrom) -> void {
				ioTo.maps[index] = iFrom.maps[index];
			},
			.emit = [index](YAML::Emitter& ioOut, const MaterialParameters& iValues) -> void {
				ioOut << data::assets::toTextureReference(iValues.maps[index]);
			},
			.read = [index](MaterialParameters& ioTo, const YAML::Node& iNode) -> void {
				ioTo.maps[index] = data::assets::fromTextureReference(iNode.as<std::string>());
			},
			.equal = [index](const MaterialParameters& iA, const MaterialParameters& iB) -> bool {
				return iA.maps[index] == iB.maps[index];
			}};
}

auto codecs() -> const std::vector<FieldCodec>& {
	using P = MaterialParameters;
	using F = MaterialField;
	static const std::vector<FieldCodec> s_codecs{
			member(F::ShadingModel, "shadingModel", &P::shadingModel),
			member(F::AlphaMode, "alphaMode", &P::alphaMode),
			member(F::AlphaCutoff, "alphaCutoff", &P::alphaCutoff),
			member(F::DoubleSided, "doubleSided", &P::doubleSided),
			member(F::BaseColor, "baseColor", &P::baseColor),
			member(F::Metallic, "metallic", &P::metallic),
			member(F::Roughness, "roughness", &P::roughness),
			member(F::NormalScale, "normalScale", &P::normalScale),
			member(F::OcclusionStrength, "occlusionStrength", &P::occlusionStrength),
			member(F::Emissive, "emissive", &P::emissive),
			member(F::EmissiveLuminance, "emissiveLuminance", &P::emissiveLuminance),
			member(F::Ior, "ior", &P::ior),
			member(F::ClearcoatIor, "clearcoatIor", &P::clearcoatIor),
			member(F::NormalConvention, "normalConvention", &P::normalConvention),
			member(F::Clearcoat, "clearcoat", &P::clearcoat),
			member(F::ClearcoatRoughness, "clearcoatRoughness", &P::clearcoatRoughness),
			member(F::SheenColor, "sheenColor", &P::sheenColor),
			member(F::SheenRoughness, "sheenRoughness", &P::sheenRoughness),
			member(F::Anisotropy, "anisotropy", &P::anisotropy),
			member(F::AnisotropyRotation, "anisotropyRotation", &P::anisotropyRotation),
			member(F::ToonBands, "toonBands", &P::toonBands),
			member(F::ToonRim, "toonRim", &P::toonRim),
			mapCodec(MaterialMap::BaseColor, "baseColorMap"),
			mapCodec(MaterialMap::Metallic, "metallicMap"),
			mapCodec(MaterialMap::Roughness, "roughnessMap"),
			mapCodec(MaterialMap::Normal, "normalMap"),
			mapCodec(MaterialMap::Occlusion, "occlusionMap"),
			mapCodec(MaterialMap::Emissive, "emissiveMap"),
			member(F::MetallicChannel, "metallicChannel", &P::metallicChannel),
			member(F::RoughnessChannel, "roughnessChannel", &P::roughnessChannel),
			member(F::OcclusionChannel, "occlusionChannel", &P::occlusionChannel),
	};
	return s_codecs;
}

void emitFields(YAML::Emitter& ioOut, const MaterialParameters& iValues, const uint32_t iFields) {
	for (const auto& codec: codecs()) {
		if ((iFields & static_cast<uint32_t>(codec.field)) == 0)
			continue;
		ioOut << YAML::Key << std::string{codec.key} << YAML::Value;
		codec.emit(ioOut, iValues);
	}
}

auto readFields(const YAML::Node& iNode, MaterialParameters& ioValues) -> uint32_t {
	uint32_t fields = 0;
	if (!iNode || !iNode.IsMap())
		return fields;
	for (const auto& codec: codecs()) {
		const auto value = iNode[std::string{codec.key}];
		if (!value)
			continue;
		try {
			codec.read(ioValues, value);
			fields |= static_cast<uint32_t>(codec.field);
		} catch (const YAML::Exception& iError) {
			OWL_CORE_WARN("Material: Field '{}' ignored ({}).", codec.key, iError.what())
		}
	}
	return fields;
}

auto resolveTexture(const std::string& iName) -> shared<gpu::Texture2D> {
	if (iName.empty())
		return nullptr;
	return gpu::Texture2D::createFromSerializedForDeserialize(iName);
}

}// namespace

void MaterialParameters::copyFields(const MaterialParameters& iOther, const uint32_t iFields) {
	for (const auto& codec: codecs()) {
		if ((iFields & static_cast<uint32_t>(codec.field)) != 0)
			codec.copy(*this, iOther);
	}
}

auto MaterialParameters::differingFields(const MaterialParameters& iOther) const -> uint32_t {
	uint32_t fields = 0;
	for (const auto& codec: codecs()) {
		if (!codec.equal(*this, iOther))
			fields |= static_cast<uint32_t>(codec.field);
	}
	return fields;
}

void MaterialOverrides::set(const MaterialField iField, const bool iOverridden) {
	if (iOverridden)
		fields |= static_cast<uint32_t>(iField);
	else
		fields &= ~static_cast<uint32_t>(iField);
}

auto MaterialOverrides::toYaml() const -> std::string {
	if (fields == 0)
		return {};
	YAML::Emitter out;
	out << YAML::BeginMap;
	emitFields(out, values, fields);
	out << YAML::EndMap;
	return out.c_str();
}

auto MaterialOverrides::fromYaml(const std::string& iYaml) -> MaterialOverrides {
	MaterialOverrides overrides;
	if (iYaml.empty())
		return overrides;
	try {
		overrides.fields = readFields(YAML::Load(iYaml), overrides.values);
	} catch (const YAML::Exception& iError) { OWL_CORE_WARN("Material: Overrides ignored ({}).", iError.what()) }
	return overrides;
}

auto MaterialOverrides::operator==(const MaterialOverrides& iOther) const -> bool {
	if (fields != iOther.fields)
		return false;
	return std::ranges::all_of(codecs(), [&](const FieldCodec& iCodec) -> bool {
		return (fields & static_cast<uint32_t>(iCodec.field)) == 0 || iCodec.equal(values, iOther.values);
	});
}

Material::Material(MaterialParameters iParameters) : m_parameters{std::move(iParameters)} {
	for (size_t index = 0; index < g_materialMapCount; ++index)
		m_textures[index] = resolveTexture(m_parameters.maps[index]);
}

Material::Material(MaterialParameters iParameters,
				   const std::array<shared<gpu::Texture2D>, g_materialMapCount>& iTextures)
	: m_parameters{std::move(iParameters)}, m_textures{iTextures} {}

auto Material::withOverrides(const MaterialOverrides& iOverrides) const -> Material {
	Material result{*this};
	iOverrides.applyTo(result.m_parameters);
	for (size_t index = 0; index < g_materialMapCount; ++index) {
		if (iOverrides.has(mapField(static_cast<MaterialMap>(index))))
			result.m_textures[index] = resolveTexture(result.m_parameters.maps[index]);
	}
	return result;
}

auto Material::withTexture(const MaterialMap iMap, const shared<gpu::Texture2D>& iTexture) const -> Material {
	Material result{*this};
	result.m_textures[static_cast<size_t>(iMap)] = iTexture;
	return result;
}

auto MaterialAsset::toYaml() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Material" << YAML::Value << YAML::BeginMap;
	core::emitFormatVersion(out, g_materialFormat);
	if (!parent.empty())
		out << YAML::Key << "parent" << YAML::Value << data::assets::toAssetReference(parent.generic_string());
	emitFields(out, values, parent.empty() ? g_allMaterialFields : overrides);
	out << YAML::EndMap << YAML::EndMap;
	return out.c_str();
}

auto MaterialAsset::fromYaml(const std::string& iYaml) -> std::optional<MaterialAsset> {
	YAML::Node root;
	try {
		root = YAML::Load(iYaml);
	} catch (const YAML::Exception& iError) {
		OWL_CORE_WARN("Material: Invalid YAML ({}).", iError.what())
		return std::nullopt;
	}
	auto node = root && root.IsMap() ? root["Material"] : YAML::Node{};
	if (!node || !node.IsMap()) {
		OWL_CORE_WARN("Material: Document is not a material.")
		return std::nullopt;
	}
	if (!core::upgradeYamlDocument(g_materialFormat, node, "<material>")) {
		OWL_CORE_WARN("Material: Document cannot be read.")
		return std::nullopt;
	}
	MaterialAsset asset;
	if (const auto parent = node["parent"]; parent && parent.IsScalar())
		asset.parent = data::assets::toAssetPath(parent.as<std::string>());
	const uint32_t fields = readFields(node, asset.values);
	asset.overrides = asset.parent.empty() ? g_allMaterialFields : fields;
	return asset;
}

auto MaterialAsset::load(const std::filesystem::path& iFile) -> std::optional<MaterialAsset> {
	const auto text = platform::fileToString(iFile);
	if (text.empty()) {
		OWL_CORE_WARN("Material: Cannot read '{}'.", iFile.string())
		return std::nullopt;
	}
	return fromYaml(text);
}

auto MaterialAsset::save(const std::filesystem::path& iFile) const -> bool {
	if (const auto written = platform::writeFileAtomic(iFile, toYaml()); !written) {
		OWL_CORE_ERROR("Material: Failed to write '{}': {}.", iFile.string(), describe(written.error()))
		return false;
	}
	return true;
}

}// namespace owl::renderer
