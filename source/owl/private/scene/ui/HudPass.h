/**
 * @file HudPass.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/Transform.h"
#include "renderer/Renderer2D.h"
#include "scene/component/UiStyle.h"
#include "scene/ui/HudLayout.h"

#include <cstdint>

namespace owl::scene {
class Scene;
}// namespace owl::scene

namespace owl::scene::ui {

/**
 * @brief
 *  Parts of a widget box a shape of the widget keeps.
 */
enum struct BoxPart : uint8_t {
	Whole,///< The widget's box itself: everything.
	NoShadow,///< A slider handle, a list row: no shadow of its own.
	Inner,///< A fill or a title bar: inside the border, no shadow, border nor texture.
};

/**
 * @brief
 *  A widget box as the HUD pass draws it.
 */
struct WidgetBox {
	/// The rounded box (only `transform` is set for a plain one).
	renderer::BoxData box;
	/// Whether the box is a flat rectangle, drawn as a plain quad.
	bool plain = true;
};

/**
 * @brief
 *  The draw data of the box of a widget: lengths in reference pixels scaled by the canvas, the shadow offset turned
 *  Y-up, the bottom colour the fill times the bottom tint, the nine-slice insets as pixels and texture fractions.
 * @param[in] iTransform Transform of the widget rectangle (Y-up pixels).
 * @param[in] iColor Fill colour.
 * @param[in] iBox Box of the widget.
 * @param[in] iPart Part of the widget drawn.
 * @param[in] iScale Scale of the canvas.
 * @return The data, flagged plain when nothing but a flat rectangle is left.
 */
[[nodiscard]] OWL_API auto makeWidgetBox(const math::Transform& iTransform, const math::vec4& iColor,
										 const component::UiBox& iBox, BoxPart iPart, float iScale) -> WidgetBox;

/**
 * @brief
 *  The HUD pass: the implicit last pass of a frame, after the renderer stack and the overlays, before the toasts.
 *
 * It lays out the canvases of the scene into the scene's `HudLayout` and draws every widget in pixels through
 * `Renderer2D`, whatever the layers underneath drew. The canvases embedded in the scene come first, then those of the
 * scene's HUD asset (`Scene::getHud`), which lies on top. The `HudMarker` of the world entities are drawn first,
 * under every canvas.
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
