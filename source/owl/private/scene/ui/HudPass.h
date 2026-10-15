/**
 * @file HudPass.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "scene/ui/HudLayout.h"

namespace owl::scene {
class Scene;
}// namespace owl::scene

namespace owl::scene::ui {

/**
 * @brief
 *  The HUD pass: the implicit last pass of a frame, after the renderer stack and the overlays, before the toasts.
 *
 * It lays out the canvases of the scene into the scene's `HudLayout` and draws every widget in pixels through
 * `Renderer2D`, whatever the layers underneath drew. The canvases embedded in the scene come first, then those of the
 * scene's HUD asset (`Scene::getHud`), which lies on top.
 */
class OWL_API HudPass final {
public:
	HudPass() = delete;

	/**
	 * @brief
	 *  Lay out and draw the HUD of a scene.
	 * @param[in,out] ioScene The scene (its `HudLayout` is recomputed).
	 * @param[in] iViewport The viewport size in pixels.
	 * @param[in] iEditorMode Use the editor visibility.
	 */
	static void render(Scene& ioScene, const math::vec2ui& iViewport, bool iEditorMode);
};

}// namespace owl::scene::ui
