/**
 * @file DocumentKind.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstdint>
#include <filesystem>

namespace owl::scene {

/**
 * @brief
 *  The document a scene is read from and written to: both hold entities in the same format and differ by their root
 *  key and their format descriptor.
 */
enum struct DocumentKind : uint8_t {
	Scene,///< An `.owl` scene: root key `Scene`, renderer overrides and physics settings besides the entities.
	Hud,///< An `.owlhud` HUD: root key `Hud`, the entities only.
	Prefab///< An `.owlprefab` prefab: root key `Prefab`, its name and version, the entities of its subtree.
};

/**
 * @brief
 *  Tell which document a file path holds, from its extension.
 * @param[in] iPath The file path or pack entry name.
 * @return `DocumentKind::Hud` for an `.owlhud`, `DocumentKind::Prefab` for an `.owlprefab`, `DocumentKind::Scene`
 *  otherwise.
 */
[[nodiscard]] OWL_API auto documentKindOf(const std::filesystem::path& iPath) -> DocumentKind;

}// namespace owl::scene
