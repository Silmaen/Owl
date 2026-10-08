/**
 * @file AssetKind.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>
#include <filesystem>

namespace owl::nest {

/**
 * @brief
 *  What the editor does with a file it is asked to open, decided by the file extension.
 */
enum struct AssetKind : uint8_t {
	Scene,///< `.owl`: opened in a scene document.
	NodeGraph,///< `.owlflow`: opened in a node graph document.
	Animation,///< `.owlanim`: opened in an animation document.
	Tilemap,///< `.owltilemap`: opened in a tilemap document.
	Tileset,///< `.owltileset`: opened in a tileset document.
	Prefab,///< `.owlprefab`: instantiated in the active scene.
	Code,///< Script, source, data or markup text: opened in the code editor.
	Unsupported///< Anything else.
};

/**
 * @brief
 *  Classify a file by its extension (case-sensitive, as the engine writes them).
 * @param[in] iPath The file path.
 * @return The kind of asset.
 */
[[nodiscard]] auto classifyAsset(const std::filesystem::path& iPath) -> AssetKind;

}// namespace owl::nest
