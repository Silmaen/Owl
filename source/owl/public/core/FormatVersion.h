/**
 * @file FormatVersion.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Serializer.h"
#include "core/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace owl::core {

/**
 * @brief
 *  Reason why a versioned document cannot be brought to the current format.
 */
enum struct FormatError : uint8_t {
	InvalidVersion,///< The `FormatVersion` field is not a positive integer.
	NewerVersion,///< The file was created by a newer version of Owl.
	MigrationFailed,///< A migration step rejected the document or threw.
};

/**
 * @brief
 *  Human-readable description of a format error.
 * @param[in] iError The error.
 * @return A short sentence fragment describing the error.
 */
[[nodiscard]] OWL_API auto describe(FormatError iError) -> std::string_view;

/// Root key holding the format version in every versioned document.
inline constexpr std::string_view g_FormatVersionKey{"FormatVersion"};

/// Version assumed for a document that carries no `FormatVersion` key (files written before versioning).
inline constexpr uint32_t g_UnversionedFormat = 1;

/**
 * @brief
 *  One migration step: rewrites, in place, a document of version N into version N + 1.
 *
 * The step receives the document root and returns false (after logging why) when it cannot convert it.
 * It must not touch `FormatVersion`: the chain stamps the new version itself.
 */
using MigrationStep = auto (*)(const Serializer& ioDocument) -> bool;

/**
 * @brief
 *  Description of a versioned file format: its name and its ordered migration chain.
 *
 * `migrations[i]` upgrades version `i + 1` to version `i + 2`, so the current version is always
 * `migrations.size() + 1`: adding a migration is what bumps the version.
 */
struct DocumentFormat {
	/// Format name, used in log messages (`"Scene"`, `"Prefab"`...).
	std::string_view name;
	/// Ordered migration steps, the first one upgrading version 1 to version 2.
	std::span<const MigrationStep> migrations;

	/**
	 * @brief
	 *  Version written by this build of the engine.
	 * @return The current format version.
	 */
	[[nodiscard]] constexpr auto currentVersion() const noexcept -> uint32_t {
		return static_cast<uint32_t>(migrations.size()) + 1;
	}
};

/**
 * @brief
 *  Read the format version of a document.
 * @param[in] iDocument The document root.
 * @return The version (`g_UnversionedFormat` when the key is absent), or `FormatError::InvalidVersion`.
 */
[[nodiscard]] OWL_API auto readFormatVersion(const Serializer& iDocument) -> expected<uint32_t, FormatError>;

/**
 * @brief
 *  Bring a document to the current version of its format.
 *
 * An older document goes through every migration step from its version to the current one; a document
 * already current is left as is; a newer one is refused. On success the root `FormatVersion` holds the
 * current version. Every failure is logged with the format name and the source name.
 * @param[in] iFormat The format of the document.
 * @param[in,out] ioDocument The document root, migrated in place.
 * @param[in] iSourceName File or buffer name for the log messages.
 * @return The version the document was written with, or the failure reason.
 */
[[nodiscard]] OWL_API auto upgradeDocument(const DocumentFormat& iFormat, const Serializer& ioDocument,
										   std::string_view iSourceName) -> expected<uint32_t, FormatError>;

/**
 * @brief
 *  Text variant of `upgradeDocument`, for code that parses YAML itself (the editor's project file).
 *
 * The text is only re-emitted when a migration actually ran.
 * @param[in] iFormat The format of the document.
 * @param[in,out] ioYaml The YAML text, replaced by the migrated document when needed.
 * @param[in] iSourceName File or buffer name for the log messages.
 * @return The version the document was written with, or the failure reason (invalid YAML is `InvalidVersion`).
 */
[[nodiscard]] OWL_API auto upgradeDocumentText(const DocumentFormat& iFormat, std::string& ioYaml,
											   std::string_view iSourceName) -> expected<uint32_t, FormatError>;

}// namespace owl::core
