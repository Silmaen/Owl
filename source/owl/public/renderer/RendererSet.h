/**
 * @file RendererSet.h
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::renderer {

/**
 * @brief
 *  Renderers an application initialises, with the shader folders they load.
 *
 * The 2D family (`Renderer2D`, tilemaps, background, colour encoding, outline, post-process) is always initialised;
 * each flag adds one optional family. An exported game initialises the families of its renderer stack only, and its
 * pack holds the shaders of those families only.
 */
struct OWL_API RendererSet {
	/// `RendererRaycast` and its GPU passes (DDA, sprite sort).
	bool raycast{true};
	/// `RendererIsometric`.
	bool isometric{true};
	/// `Renderer3D` with `RendererMesh`, `RendererVoxel` and the mesh culling passes.
	bool renderer3D{true};

	/**
	 * @brief
	 *  The 2D family alone.
	 * @return A set with every optional family off.
	 */
	[[nodiscard]] static auto base() -> RendererSet;

	/**
	 * @brief
	 *  The families one layer type of the renderer stack draws with.
	 * @param[in] iTypeKey Layer type key (`RendererStackEntry::typeKey`).
	 * @return The set of that layer, the 2D family alone for `Renderer2D` or an unknown key.
	 */
	[[nodiscard]] static auto forLayerType(std::string_view iTypeKey) -> RendererSet;

	/**
	 * @brief
	 *  Add the families of another set.
	 * @param[in] iOther The set to add.
	 * @return A reference to this set.
	 */
	auto operator|=(const RendererSet& iOther) -> RendererSet&;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other set.
	 * @return True when both sets hold the same families.
	 */
	auto operator==(const RendererSet& iOther) const -> bool = default;

	/**
	 * @brief
	 *  Names of the optional families of the set, as written in `runner.yml`.
	 * @return `RendererRaycast`, `RendererIsometric` and `Renderer3D`, for the families on.
	 */
	[[nodiscard]] auto getNames() const -> std::vector<std::string>;

	/**
	 * @brief
	 *  Build a set from family names.
	 * @param[in] iNames Names as given by getNames(); unknown names are ignored.
	 * @return The 2D family plus the named families.
	 */
	[[nodiscard]] static auto fromNames(std::span<const std::string> iNames) -> RendererSet;

	/**
	 * @brief
	 *  Shader folders the families of the set load, under `shaders/`.
	 * @return Folder names, the 2D family and `common` first.
	 */
	[[nodiscard]] auto getShaderFolders() const -> std::vector<std::string>;
};

}// namespace owl::renderer
