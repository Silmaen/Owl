/**
 * @file Material.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "gpu/Texture.h"
#include "math/vectors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace owl::renderer {

/**
 * @brief
 *  How a material reacts to the lights.
 */
enum struct ShadingModel : uint8_t {
	Standard,///< Physically based: metallic-roughness with the clearcoat, sheen and anisotropy layers.
	Unlit,///< The base colour as it is, plus the emissive colour; no light, no shadow.
	Toon,///< Banded diffuse from the same lights and shadows, a stepped highlight and a rim.
};

/**
 * @brief
 *  How a material uses the alpha of its base colour.
 */
enum struct AlphaMode : uint8_t {
	Opaque,///< Alpha ignored.
	Mask,///< Fragments below `alphaCutoff` are discarded, the others are opaque.
	Blend,///< Alpha blended, drawn back to front after the opaque meshes.
};

/**
 * @brief
 *  Texture maps of a material.
 *
 * The metallic, roughness and occlusion maps read one channel each (`MaterialParameters::metallicChannel`, …): one
 * packed image (glTF metallic-roughness, ARM) is named by several maps, separate grey images by one map each.
 */
enum struct MaterialMap : uint8_t {
	BaseColor,///< RGB colour (sRGB) and alpha.
	Metallic,///< Metalness in one channel (linear).
	Normal,///< Tangent-space normal (linear).
	Occlusion,///< Ambient occlusion in one channel (linear).
	Emissive,///< Emitted colour (sRGB).
	Roughness,///< Perceptual roughness in one channel (linear).
};

/// Number of texture maps of a material.
constexpr size_t g_materialMapCount = 6;

/**
 * @brief
 *  Direction of the green channel of a normal map.
 */
enum struct NormalConvention : uint8_t {
	OpenGl,///< Green points up the image (glTF, Blender, Poly Haven `nor_gl`).
	DirectX,///< Green points down the image (Unreal, Substance DirectX exports, Poly Haven `nor_dx`).
};

/**
 * @brief
 *  Channel of a texture a single-value map reads.
 */
enum struct TextureChannel : uint8_t {
	R,///< Red.
	G,///< Green.
	B,///< Blue.
	A,///< Alpha.
};

/**
 * @brief
 *  Fields of a material a material instance or a mesh slot can override, one bit each.
 */
enum struct MaterialField : uint32_t {
	ShadingModel = 1u << 0u,///< `shadingModel`.
	AlphaMode = 1u << 1u,///< `alphaMode`.
	AlphaCutoff = 1u << 2u,///< `alphaCutoff`.
	DoubleSided = 1u << 3u,///< `doubleSided`.
	BaseColor = 1u << 4u,///< `baseColor`.
	Metallic = 1u << 5u,///< `metallic`.
	Roughness = 1u << 6u,///< `roughness`.
	NormalScale = 1u << 7u,///< `normalScale`.
	OcclusionStrength = 1u << 8u,///< `occlusionStrength`.
	Emissive = 1u << 9u,///< `emissive`.
	EmissiveLuminance = 1u << 10u,///< `emissiveLuminance`.
	Clearcoat = 1u << 11u,///< `clearcoat`.
	ClearcoatRoughness = 1u << 12u,///< `clearcoatRoughness`.
	SheenColor = 1u << 13u,///< `sheenColor`.
	SheenRoughness = 1u << 14u,///< `sheenRoughness`.
	Anisotropy = 1u << 15u,///< `anisotropy`.
	AnisotropyRotation = 1u << 16u,///< `anisotropyRotation`.
	ToonBands = 1u << 17u,///< `toonBands`.
	ToonRim = 1u << 18u,///< `toonRim`.
	BaseColorMap = 1u << 19u,///< The base-colour map.
	MetallicMap = 1u << 20u,///< The metallic map.
	NormalMap = 1u << 21u,///< The normal map.
	OcclusionMap = 1u << 22u,///< The occlusion map.
	EmissiveMap = 1u << 23u,///< The emissive map.
	RoughnessMap = 1u << 24u,///< The roughness map.
	MetallicChannel = 1u << 25u,///< `metallicChannel`.
	RoughnessChannel = 1u << 26u,///< `roughnessChannel`.
	OcclusionChannel = 1u << 27u,///< `occlusionChannel`.
	Ior = 1u << 28u,///< `ior`.
	ClearcoatIor = 1u << 29u,///< `clearcoatIor`.
	NormalConvention = 1u << 30u,///< `normalConvention`.
};

/// Every overridable field.
constexpr uint32_t g_allMaterialFields = (1u << 31u) - 1u;

/**
 * @brief
 *  Field mask bit of a texture map.
 * @param[in] iMap The map.
 * @return Its field.
 */
[[nodiscard]] constexpr auto mapField(const MaterialMap iMap) -> MaterialField {
	return static_cast<MaterialField>(static_cast<uint32_t>(MaterialField::BaseColorMap)
									  << static_cast<uint32_t>(iMap));
}

/**
 * @brief
 *  The values of a material, as authored.
 */
struct OWL_API MaterialParameters {
	/// Reaction to the lights.
	ShadingModel shadingModel = ShadingModel::Standard;
	/// Use of the base-colour alpha.
	AlphaMode alphaMode = AlphaMode::Opaque;
	/// Alpha below which a `Mask` material discards.
	float alphaCutoff = 0.5f;
	/// Both faces lit (the back face with its normal flipped).
	bool doubleSided = false;
	/// Base colour, sRGB, and alpha; multiplies the base-colour map.
	math::vec4 baseColor{1.f, 1.f, 1.f, 1.f};
	/// 0 dielectric, 1 metal; multiplies the metallic map.
	float metallic = 0.f;
	/// Perceptual roughness, 0 mirror to 1 matte; multiplies the roughness map.
	float roughness = 0.5f;
	/// Strength of the normal map: 1 as baked, 0 flat, above 1 deeper relief.
	float normalScale = 1.f;
	/// Direction of the green channel of the normal map.
	NormalConvention normalConvention = NormalConvention::OpenGl;
	/// Index of refraction of a dielectric (water 1.33, plastic and glass 1.5, diamond 2.42); unused by metals.
	float ior = 1.5f;
	/// Strength of the occlusion map.
	float occlusionStrength = 1.f;
	/// Emitted colour, sRGB; multiplies the emissive map.
	math::vec3 emissive{0.f, 0.f, 0.f};
	/// Luminance in cd/m² (nits) of a white emissive (screen 200, candle flame 10 000, embers 5 000).
	float emissiveLuminance = 1000.f;
	/// Strength of the clearcoat layer, 0 for none.
	float clearcoat = 0.f;
	/// Perceptual roughness of the clearcoat.
	float clearcoatRoughness = 0.f;
	/// Index of refraction of the clearcoat (varnish and car clearcoat 1.5).
	float clearcoatIor = 1.5f;
	/// Sheen colour, sRGB; black for none.
	math::vec3 sheenColor{0.f, 0.f, 0.f};
	/// Perceptual roughness of the sheen.
	float sheenRoughness = 0.f;
	/// Anisotropy strength, -1 to 1, 0 for none.
	float anisotropy = 0.f;
	/// Rotation of the anisotropy direction in the tangent plane, radians.
	float anisotropyRotation = 0.f;
	/// Number of diffuse bands of a `Toon` material.
	uint32_t toonBands = 3;
	/// Strength of the rim of a `Toon` material.
	float toonRim = 0.5f;
	/// Texture maps, by `MaterialMap`, as serialized texture names (empty for none).
	std::array<std::string, g_materialMapCount> maps{};
	/// Channel of the metallic map read as metalness (B as packed by glTF and ARM images).
	TextureChannel metallicChannel = TextureChannel::B;
	/// Channel of the roughness map read as roughness (G as packed by glTF and ARM images).
	TextureChannel roughnessChannel = TextureChannel::G;
	/// Channel of the occlusion map read as occlusion (R as packed by glTF and ARM images).
	TextureChannel occlusionChannel = TextureChannel::R;

	/**
	 * @brief
	 *  Copy the given fields of another set of values.
	 * @param[in] iOther The values to copy from.
	 * @param[in] iFields Mask of `MaterialField` bits to copy.
	 */
	void copyFields(const MaterialParameters& iOther, uint32_t iFields);

	/**
	 * @brief
	 *  Fields whose value differs from another set of values.
	 * @param[in] iOther The values to compare with.
	 * @return Mask of the `MaterialField` bits that differ.
	 */
	[[nodiscard]] auto differingFields(const MaterialParameters& iOther) const -> uint32_t;
};

/**
 * @brief
 *  Values that replace some fields of a material (a material instance, a mesh slot of an entity).
 */
struct OWL_API MaterialOverrides {
	/// Mask of the overridden `MaterialField` bits.
	uint32_t fields = 0;
	/// The overriding values (only the masked fields count).
	MaterialParameters values;

	/**
	 * @brief
	 *  Check whether a field is overridden.
	 * @param[in] iField The field.
	 * @return True when overridden.
	 */
	[[nodiscard]] auto has(MaterialField iField) const -> bool { return (fields & static_cast<uint32_t>(iField)) != 0; }

	/**
	 * @brief
	 *  Mark a field overridden or not.
	 * @param[in] iField The field.
	 * @param[in] iOverridden True to override it.
	 */
	void set(MaterialField iField, bool iOverridden = true);

	/**
	 * @brief
	 *  Apply the overridden fields onto a set of values.
	 * @param[in,out] ioValues The values.
	 */
	void applyTo(MaterialParameters& ioValues) const { ioValues.copyFields(values, fields); }

	/**
	 * @brief
	 *  Check whether nothing is overridden.
	 * @return True without any override.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return fields == 0; }

	/**
	 * @brief
	 *  Write the overridden fields as a YAML map (field names of the `.owlmat` format).
	 * @return The YAML text, empty without any override.
	 */
	[[nodiscard]] auto toYaml() const -> std::string;

	/**
	 * @brief
	 *  Read overridden fields from a YAML map; every key present is an override.
	 * @param[in] iYaml The YAML text.
	 * @return The overrides.
	 */
	[[nodiscard]] static auto fromYaml(const std::string& iYaml) -> MaterialOverrides;

	/**
	 * @brief
	 *  Equality.
	 * @param[in] iOther The other overrides.
	 * @return True when the same fields hold the same values.
	 */
	auto operator==(const MaterialOverrides& iOther) const -> bool;
};

/**
 * @brief
 *  A material ready to draw: its resolved values and its textures.
 */
class OWL_API Material final {
public:
	/**
	 * @brief
	 *  Default constructor: the default `Standard` material.
	 */
	Material() = default;

	/**
	 * @brief
	 *  A material of resolved values; the textures are resolved from the maps' names.
	 * @param[in] iParameters The values.
	 */
	explicit Material(MaterialParameters iParameters);

	/**
	 * @brief
	 *  A material of resolved values and textures given directly (a mesh file's own materials).
	 * @param[in] iParameters The values.
	 * @param[in] iTextures The textures, by `MaterialMap`, null for none.
	 */
	Material(MaterialParameters iParameters, const std::array<shared<gpu::Texture2D>, g_materialMapCount>& iTextures);

	/**
	 * @brief
	 *  Destructor.
	 */
	~Material() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	Material(const Material&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	Material(Material&&) = default;

	/**
	 * @brief
	 *  Copy assignment.
	 * @return This material.
	 */
	auto operator=(const Material&) -> Material& = default;

	/**
	 * @brief
	 *  Move assignment.
	 * @return This material.
	 */
	auto operator=(Material&&) -> Material& = default;

	/**
	 * @brief
	 *  The resolved values.
	 * @return The values.
	 */
	[[nodiscard]] auto getParameters() const -> const MaterialParameters& { return m_parameters; }

	/**
	 * @brief
	 *  A texture map.
	 * @param[in] iMap The map.
	 * @return The texture, null when the material has none.
	 */
	[[nodiscard]] auto getTexture(MaterialMap iMap) const -> const shared<gpu::Texture2D>& {
		return m_textures[static_cast<size_t>(iMap)];
	}

	/**
	 * @brief
	 *  Check whether the material is drawn blended (after the opaque meshes).
	 * @return True for `AlphaMode::Blend`.
	 */
	[[nodiscard]] auto isBlended() const -> bool { return m_parameters.alphaMode == AlphaMode::Blend; }

	/**
	 * @brief
	 *  A copy with overrides applied; the textures of overridden maps are resolved again.
	 * @param[in] iOverrides The overrides.
	 * @return The overridden material.
	 */
	[[nodiscard]] auto withOverrides(const MaterialOverrides& iOverrides) const -> Material;

	/**
	 * @brief
	 *  A copy with one map replaced by a texture given directly.
	 * @param[in] iMap The map.
	 * @param[in] iTexture The texture.
	 * @return The material.
	 */
	[[nodiscard]] auto withTexture(MaterialMap iMap, const shared<gpu::Texture2D>& iTexture) const -> Material;

private:
	/// Resolved values.
	MaterialParameters m_parameters;
	/// Textures, by `MaterialMap`.
	std::array<shared<gpu::Texture2D>, g_materialMapCount> m_textures{};
};

/**
 * @brief
 *  The content of a `.owlmat` file.
 */
struct OWL_API MaterialAsset {
	/// Asset path of the parent material, empty for a root material.
	std::filesystem::path parent;
	/// The values; with a parent, only the fields of `overrides` count.
	MaterialParameters values;
	/// With a parent: the fields this file sets.
	uint32_t overrides = g_allMaterialFields;

	/**
	 * @brief
	 *  Write the `.owlmat` text (versioned YAML); with a parent, only the overridden fields.
	 * @return The text.
	 */
	[[nodiscard]] auto toYaml() const -> std::string;

	/**
	 * @brief
	 *  Read a `.owlmat` text.
	 * @param[in] iYaml The text.
	 * @return The asset, nothing when the text is not a material.
	 */
	[[nodiscard]] static auto fromYaml(const std::string& iYaml) -> std::optional<MaterialAsset>;

	/**
	 * @brief
	 *  Read a `.owlmat` file.
	 * @param[in] iFile Absolute path of the file.
	 * @return The asset, nothing when the file cannot be read.
	 */
	[[nodiscard]] static auto load(const std::filesystem::path& iFile) -> std::optional<MaterialAsset>;

	/**
	 * @brief
	 *  Write a `.owlmat` file atomically.
	 * @param[in] iFile Absolute path of the file.
	 * @return True when written.
	 */
	[[nodiscard]] auto save(const std::filesystem::path& iFile) const -> bool;
};

}// namespace owl::renderer
