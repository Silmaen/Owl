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
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  What the editor does with a file it is asked to open, decided by the file extension.
 */
enum struct AssetKind : uint8_t {
	Scene,///< `.owl`: opened in a scene document.
	NodeGraph,///< `.owlflow`, `.owlvs`, `.owlpcg`: opened in a node graph, visual script or PCG graph document.
	Animation,///< `.owlanim`: opened in an animation document.
	Tilemap,///< `.owltilemap`: opened in a tilemap document.
	Tileset,///< `.owltileset`: opened in a tileset document.
	Font,///< `.ttf`, `.otf` or the `.owlfont` sidecar: opened in a font document.
	Image,///< `.png`, `.jpg`, `.jpeg`, `.bmp`, `.tga`: opened in a read-only image viewer.
	Prefab,///< `.owlprefab`: instantiated in the active scene.
	Mesh,///< `.gltf`, `.glb`, `.obj`: placed in the active scene as a `MeshRenderer` entity.
	Hud,///< `.owlhud`: opened in a scene document (the HUD is a scene of widgets).
	Macro,///< `.owlmacro`: replayed on the active scene (one undo step).
	ActionMap,///< `input_actions.yml`: opened in the action map document.
	Material,///< `.owlmat`: opened in a material document.
	SoundCue,///< `.owlsound`: opened in a sound document.
	Audio,///< `.wav`, `.ogg`, `.flac`, `.mp3`: opened in a sound document, its cue beside it or a new one.
	Localisation,///< `localisation.yml`: opened in the localisation document.
	DataTable,///< `.owltable`: opened in a data table document.
	Dialogue,///< `.owldlg`: opened in a dialogue document.
	Sequence,///< `.owlseq`: opened in a sequence document.
	Code,///< Script, source, data or markup text: opened in the code editor.
	Unsupported///< Anything else.
};

/**
 * @brief
 *  Classify a file by its extension (case-sensitive, as the engine writes them); `input_actions.yml` is told by name.
 * @param[in] iPath The file path.
 * @return The kind of asset.
 */
[[nodiscard]] auto classifyAsset(const std::filesystem::path& iPath) -> AssetKind;

/**
 * @brief
 *  List the files of a folder tree that have an extension (what an asset picker offers).
 * @param[in] iDirectory The folder searched recursively (nothing is listed when it does not exist).
 * @param[in] iExtension The extension, with its dot (`.owlhud`).
 * @return The paths relative to the folder, with `/` separators, sorted.
 */
[[nodiscard]] auto listAssets(const std::filesystem::path& iDirectory, std::string_view iExtension)
		-> std::vector<std::string>;

}// namespace owl::nest
