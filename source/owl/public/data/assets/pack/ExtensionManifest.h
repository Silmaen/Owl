/**
 * @file ExtensionManifest.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/expected.h"

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::data::assets::pack {

/// Name of the manifest at the root of an extension pack (`.owlmod` file or source folder).
constexpr std::string_view g_extensionManifestEntry{"extension.yml"};

/// Folder of a project holding its extension packs, one sub-folder per pack.
constexpr std::string_view g_projectExtensionsFolder{"extensions"};

/// File extension of a packed extension: the same `.owlpack` format, holding a manifest.
constexpr std::string_view g_extensionPackExtension{".owlmod"};

/**
 * @brief
 *  What an extension pack adds, which decides what the loader mounts of it.
 */
enum struct ExtensionKind : uint8_t {
	Data,///< Assets only (textures, sounds, scenes, documents); its scripts are not mounted.
	Script///< Assets and scripts (Lua, visual scripts).
};

/**
 * @brief
 *  Name of an extension kind, as the manifest writes it.
 * @param[in] iKind The kind.
 * @return `Data` or `Script`.
 */
[[nodiscard]] OWL_API auto getKindName(ExtensionKind iKind) -> std::string_view;

/**
 * @brief
 *  Parse the name of an extension kind (case sensitive).
 * @param[in] iName The name.
 * @return The kind, or nothing for an unknown name.
 */
[[nodiscard]] OWL_API auto parseKind(std::string_view iName) -> std::optional<ExtensionKind>;

/**
 * @brief
 *  Version of an extension pack: `major.minor.patch`, compared number by number.
 */
struct OWL_API ExtensionVersion {
	/// The three numbers: major, minor, patch.
	std::array<uint32_t, 3> numbers{1, 0, 0};

	/**
	 * @brief
	 *  Parse a version: one to three dot-separated numbers (`2`, `1.4`, `1.4.2`), the missing ones zero.
	 * @param[in] iText The text.
	 * @return The version, or nothing when the text is no version.
	 */
	[[nodiscard]] static auto parse(std::string_view iText) -> std::optional<ExtensionVersion>;

	/**
	 * @brief
	 *  Write the version as `major.minor.patch`.
	 * @return The text.
	 */
	[[nodiscard]] auto toString() const -> std::string;

	/**
	 * @brief
	 *  Compare two versions number by number.
	 * @param[in] iOther The other version.
	 * @return The ordering.
	 */
	auto operator<=>(const ExtensionVersion& iOther) const -> std::strong_ordering = default;

	/**
	 * @brief
	 *  Equality of two versions.
	 * @param[in] iOther The other version.
	 * @return True when every number is equal.
	 */
	auto operator==(const ExtensionVersion& iOther) const -> bool = default;
};

/**
 * @brief
 *  An extension pack another one needs, mounted before it.
 */
struct OWL_API ExtensionDependency {
	/// Id of the pack needed.
	std::string id;
	/// Lowest version of it accepted.
	ExtensionVersion minVersion{.numbers = {0, 0, 0}};

	/**
	 * @brief
	 *  Equality of two dependencies.
	 * @param[in] iOther The other dependency.
	 * @return True when the id and the version are equal.
	 */
	auto operator==(const ExtensionDependency& iOther) const -> bool = default;
};

/**
 * @brief
 *  Why a manifest cannot be read.
 */
enum struct ManifestError : uint8_t {
	Unreadable,///< Not YAML, or no `OwlExtension` map.
	NewerVersion,///< Written by a newer Owl (`FormatVersion`).
	InvalidId,///< No `id`, or one `isValidExtensionId` refuses.
	InvalidVersion,///< A `version` that is no version.
	UnknownKind,///< A `kind` that is neither `Data` nor `Script`.
	InvalidDependency///< A dependency without a valid id or with an invalid version.
};

/**
 * @brief
 *  Human-readable description of a manifest error.
 * @param[in] iError The error.
 * @return A short sentence fragment.
 */
[[nodiscard]] OWL_API auto describe(ManifestError iError) -> std::string_view;

/**
 * @brief
 *  Check an extension id: 1 to 64 characters among lower-case letters, digits, `.`, `_` and `-`, starting with a
 *  letter or a digit (`demo.recolour`).
 * @param[in] iId The id.
 * @return True when valid.
 */
[[nodiscard]] OWL_API auto isValidExtensionId(std::string_view iId) -> bool;

/**
 * @brief
 *  Manifest of an extension pack (`extension.yml`): one format for game mods and for data and script plugins.
 *
 * The same pack is loaded by a game as a mod, by a project or Owl Nest as a plugin; the host decides the role, the
 * manifest only says what the pack is.
 */
struct OWL_API ExtensionManifest {
	/// Unique id (`isValidExtensionId`), what dependencies name.
	std::string id;
	/// Display name; the id when empty.
	std::string name;
	/// Version of the pack.
	ExtensionVersion version;
	/// What the pack adds.
	ExtensionKind kind{ExtensionKind::Data};
	/// Rank among the packs that do not depend on each other: lower first, the later one wins an override.
	int32_t loadOrder{0};
	/// Author or studio.
	std::string author;
	/// Short description.
	std::string description;
	/// Packs mounted before this one.
	std::vector<ExtensionDependency> dependencies;

	/**
	 * @brief
	 *  Read a manifest from its YAML text; an older format version is migrated.
	 * @param[in] iText The YAML text.
	 * @param[in] iSourceName Name of the source, for the log.
	 * @return The manifest, or why it cannot be read.
	 */
	[[nodiscard]] static auto parse(std::string_view iText, std::string_view iSourceName)
			-> expected<ExtensionManifest, ManifestError>;

	/**
	 * @brief
	 *  Write the manifest as YAML (`FormatVersion`, then the `OwlExtension` map).
	 * @return The YAML text.
	 */
	[[nodiscard]] auto serialize() const -> std::string;

	/**
	 * @brief
	 *  Name shown to the user: the name, or the id when it has none.
	 * @return The display name.
	 */
	[[nodiscard]] auto getDisplayName() const -> const std::string& { return name.empty() ? id : name; }

	/**
	 * @brief
	 *  Equality of two manifests.
	 * @param[in] iOther The other manifest.
	 * @return True when every field is equal.
	 */
	auto operator==(const ExtensionManifest& iOther) const -> bool = default;
};

}// namespace owl::data::assets::pack
