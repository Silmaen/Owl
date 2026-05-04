/**
 * @file AssetField.h
 * @author Silmaen
 * @date 26/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "renderer/Texture.h"

#include <imgui.h>
#include <filesystem>

namespace owl::gui::widgets {

/**
 * @brief Categories of assets a content-browser drop can carry.
 *
 * The same `CONTENT_BROWSER_ITEM` ImGui payload (a relative path, NUL-terminated) is
 * emitted for every asset; the receiver picks an `AssetKind` to filter by extension
 * before accepting the drop.
 */
enum struct AssetKind : uint8_t {
	Texture,///< Image files (`.png`, `.jpg`, `.jpeg`, `.bmp`, `.tga`, `.hdr`).
	Font,///< Font files (`.ttf`, `.otf`).
	Sound,///< Audio files (`.wav`, `.mp3`, `.ogg`, `.flac`).
	LuaScript,///< Lua source (`.lua`).
	AnyScript,///< Any source/script file (`.lua`, `.py`, `.c`, `.cpp`, `.cc`, `.cxx`, `.h`, `.hpp`).
	Scene,///< Scene file (`.owl`).
	Prefab,///< Prefab file (`.owlprefab`).
	Tileset,///< Tileset asset (`.owltileset`).
	Any,///< Accepts any extension.
};

/**
 * @brief Test whether a path's extension matches the given asset kind.
 * @param[in] iPath Path to inspect (the extension is read).
 * @param[in] iKind Asset kind to validate against.
 * @return True when the extension is in the canonical list for `iKind`. Comparison is
 *         case-insensitive (so `.PNG` and `.png` both match `Texture`).
 */
OWL_API auto isPathOfKind(const std::filesystem::path& iPath, AssetKind iKind) -> bool;

/**
 * @brief Wrap `BeginDragDropTarget`/`AcceptDragDropPayload`/`EndDragDropTarget` for
 *        a content-browser asset drop, filtered by kind.
 * @param[in] iKind Accepted asset category.
 * @param[out] oRelativePath Set to the dropped relative path (relative to the content
 *                           browser root) when a valid drop is accepted.
 * @return True when a payload of the expected kind was dropped this frame.
 *
 * Caller must be in the right ImGui state (immediately after the widget that owns the
 * drop target). Wrong-kind drops are silently rejected.
 */
OWL_API auto assetDropTarget(AssetKind iKind, std::filesystem::path& oRelativePath) -> bool;

/**
 * @brief Inspector row for a `Texture2D` field.
 *
 * Renders a thumbnail (or labelled square button when the texture is null) of the given
 * size, attaches a context popup for "Remove texture", and accepts `CONTENT_BROWSER_ITEM`
 * drops filtered to image extensions. Async textures show a state overlay
 * (`(loading...)`/`(failed)`).
 *
 * @param[in] iLabel Field label, used as the ImGui button id.
 * @param[in,out] ioTexture Texture handle to read/write.
 * @param[in] iSize Desired square size in pixels.
 * @return True when the underlying texture pointer changed this frame.
 */
OWL_API auto textureField(const char* iLabel, shared<renderer::Texture2D>& ioTexture,
						  ImVec2 iSize = {100.f, 100.f}) -> bool;

}// namespace owl::gui::widgets
