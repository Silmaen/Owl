/**
 * @file ExecutableIcon.h
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/expected.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::data::assets::pack {

/**
 * @brief
 *  Categorised reasons reading or patching the resources of a Windows executable may fail.
 */
enum struct ExecutableIconError : uint8_t {
	ReadFailed,///< The executable or the image file could not be read.
	NotExecutable,///< The bytes are not a PE image (no `MZ` / `PE` signature, unknown optional header).
	Corrupted,///< A header, a section or the resource tree points outside the file or loops.
	Signed,///< The executable carries an Authenticode signature, which a patch would break.
	NoHeaderRoom,///< The headers have no room for the section that receives the new resources.
	ImageDecode,///< The icon image could not be decoded.
	ImageEncode,///< An icon entry could not be encoded.
	WriteFailed,///< The patched executable could not be written.
};

/**
 * @brief
 *  Human-readable text of an executable icon error.
 * @param[in] iError The error.
 * @return The message.
 */
[[nodiscard]] OWL_API auto getErrorMessage(ExecutableIconError iError) -> std::string_view;

/**
 * @brief
 *  Identifier of a resource type or name: a number, or a string when `name` is not empty.
 */
struct ResourceId {
	uint16_t id = 0;///< Numeric identifier, meaningful when `name` is empty.
	std::u16string name;///< String identifier (UTF-16), empty for a numeric identifier.

	/**
	 * @brief
	 *  Comparison of two identifiers.
	 * @param[in] iOther The other identifier.
	 * @return True when both are the same number or the same string.
	 */
	[[nodiscard]] auto operator==(const ResourceId& iOther) const -> bool = default;
};

/// Resource type of one icon image.
constexpr uint16_t g_resourceTypeIcon = 3;
/// Resource type of an icon group, the directory of the images of one icon.
constexpr uint16_t g_resourceTypeGroupIcon = 14;
/// Resource type of the version information.
constexpr uint16_t g_resourceTypeVersion = 16;
/// Resource type of the application manifest.
constexpr uint16_t g_resourceTypeManifest = 24;
/// Pixel sizes of the icon images built from a project icon.
constexpr std::array<uint16_t, 6> g_executableIconSizes = {16, 24, 32, 48, 64, 256};

/**
 * @brief
 *  One resource of a Windows executable: a leaf of its type / name / language tree.
 */
struct ExecutableResource {
	ResourceId type;///< Resource type (`g_resourceTypeIcon`, …, or a named type).
	ResourceId name;///< Resource name.
	uint16_t language = 0;///< Language identifier (0x0409 for US English).
	uint32_t codePage = 0;///< Code page of the data, usually 0.
	std::vector<uint8_t> data;///< Raw bytes of the resource.
};

/**
 * @brief
 *  Read every resource of a Windows executable.
 * @param[in] iExecutable The bytes of the executable.
 * @return The resources in the order of the tree (types, names, languages), empty when it has none, or the reason
 *  the file was refused.
 */
[[nodiscard]] OWL_API auto readExecutableResources(std::span<const uint8_t> iExecutable)
		-> expected<std::vector<ExecutableResource>, ExecutableIconError>;

/**
 * @brief
 *  Rebuild the resources of a Windows executable.
 *
 * The resource section is rewritten in place when it is the last section of the image, otherwise a new section is
 * appended and the old one is left unused. The image size and the checksum are recomputed.
 * @param[in] iExecutable The bytes of the executable.
 * @param[in] iResources The complete list of resources the executable must carry.
 * @return The bytes of the patched executable, or the reason the patch failed.
 */
[[nodiscard]] OWL_API auto writeExecutableResources(std::span<const uint8_t> iExecutable,
													const std::vector<ExecutableResource>& iResources)
		-> expected<std::vector<uint8_t>, ExecutableIconError>;

/**
 * @brief
 *  Build the images of a Windows icon from an image file.
 *
 * The image is fitted, centred and transparent-padded, in a square of each size of `g_executableIconSizes`: the
 * 256-pixel image is a PNG, the others 32-bit bitmaps with their transparency mask, as an `.ico` stores them.
 * @param[in] iImageFile The bytes of an image file stb_image decodes (PNG, JPEG, BMP, TGA, …).
 * @return The encoded image of each size, in the order of `g_executableIconSizes`, or the reason it failed.
 */
[[nodiscard]] OWL_API auto encodeIconImages(std::span<const uint8_t> iImageFile)
		-> expected<std::vector<std::vector<uint8_t>>, ExecutableIconError>;

/**
 * @brief
 *  Replace the icon of a Windows executable, the one Explorer and the taskbar show.
 *
 * The first icon group and the images it references give way to the images of `encodeIconImages`, under the same
 * group name and language; the other resources (version information, manifest, …) are kept as they are.
 * @param[in] iExecutable The bytes of the executable.
 * @param[in] iImageFile The bytes of the icon image file.
 * @return The bytes of the patched executable, or the reason the patch failed.
 */
[[nodiscard]] OWL_API auto setExecutableIcon(std::span<const uint8_t> iExecutable, std::span<const uint8_t> iImageFile)
		-> expected<std::vector<uint8_t>, ExecutableIconError>;

/**
 * @brief
 *  Replace the icon of a Windows executable file, in place.
 * @param[in] iExecutable The executable file.
 * @param[in] iImageFile The icon image file.
 * @return Empty success, or the reason the patch failed (the file is then left untouched).
 */
[[nodiscard]] OWL_API auto setExecutableIcon(const std::filesystem::path& iExecutable,
											 const std::filesystem::path& iImageFile)
		-> expected<void, ExecutableIconError>;

}// namespace owl::data::assets::pack
