/**
 * @file UiStyle.h
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "renderer/gpu/Texture.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace owl::scene::component {

/**
 * @brief
 *  Look of the box of a HUD widget: rounded corners, vertical gradient, border, soft shadow, nine-slice texture.
 *
 * A box with none of them (`isPlain`) is drawn as the flat rectangle of a plain quad; any of them draws it through
 * the rounded-box shader (`renderer::Renderer2D::drawBox`). Lengths are in reference pixels (scaled with the canvas).
 */
struct OWL_API UiBox {
	/// Radius of the corners (0 = square), clamped to half the shorter side.
	float cornerRadius = 0.f;
	/// Multiplied with the fill colour at the bottom edge: a vertical gradient from the top (white = flat).
	math::vec4 bottomTint{1.f, 1.f, 1.f, 1.f};
	/// Colour of the border.
	math::vec4 borderColor{1.f, 1.f, 1.f, 1.f};
	/// Width of the border drawn inside the box (0 = none).
	float borderWidth = 0.f;
	/// Colour of the shadow drawn outside the box (a zero alpha draws none).
	math::vec4 shadowColor{0.f, 0.f, 0.f, 0.f};
	/// Offset of the shadow, right and down.
	math::vec2 shadowOffset{0.f, 4.f};
	/// Softness of the shadow edge (0 = sharp).
	float shadowBlur = 8.f;
	/// Nine-slice texture of the background, tinted by the fill colour (none when null).
	shared<renderer::gpu::Texture2D> texture;
	/// Borders of the nine-slice texture kept unstretched, in texture pixels: left, top, right, bottom.
	math::vec4 sliceInsets{0.f, 0.f, 0.f, 0.f};

	/**
	 * @brief
	 *  Check whether the box is a flat rectangle.
	 * @return True when no corner, gradient, border, shadow or texture is set.
	 */
	[[nodiscard]] auto isPlain() const -> bool;

	/**
	 * @brief
	 *  Compare two boxes (the texture by identity).
	 * @param[in] iOther The other box.
	 * @return True when every field is the same.
	 */
	auto operator==(const UiBox& iOther) const -> bool;
};

/**
 * @brief
 *  Properties a style of a `scene::HudTheme` may set on a widget, one bit each in `UiStyleRef::overrides`.
 */
enum struct UiStyleProperty : uint8_t {
	Background,///< Fill colour: panel background, button normal, slider track, bar background, list row, window body.
	Hover,///< Button and list row under the mouse.
	Pressed,///< Button held down.
	Disabled,///< Button not interactive.
	Accent,///< Slider and bar fill, selected list row, window title bar.
	Secondary,///< Slider handle, alternate list row.
	TextColor,///< Text colour of a text, a list and a window title.
	Font,///< Font of a text.
	FontSize,///< Font size of a text, a list and a window title.
	Outline,///< Outline colour and width of a text.
	Padding,///< Padding of the `UiLayout` of a styled panel.
	CornerRadius,///< `UiBox::cornerRadius`.
	Gradient,///< `UiBox::bottomTint`.
	Border,///< `UiBox::borderColor` and `UiBox::borderWidth`.
	Shadow,///< `UiBox::shadowColor`, `UiBox::shadowOffset` and `UiBox::shadowBlur`.
	NineSlice,///< `UiBox::texture` and `UiBox::sliceInsets`.
};

/// Number of `UiStyleProperty` values.
inline constexpr uint8_t g_uiStylePropertyCount = 16;

/**
 * @brief
 *  Bit of a property in a style mask.
 * @param[in] iProperty The property.
 * @return Its bit.
 */
[[nodiscard]] constexpr auto styleBit(const UiStyleProperty iProperty) -> uint32_t {
	return 1u << static_cast<uint32_t>(iProperty);
}

/**
 * @brief
 *  Name of a style property, as written in the files (`Background`, `CornerRadius`...).
 * @param[in] iProperty The property.
 * @return Its name.
 */
[[nodiscard]] OWL_API auto getStylePropertyName(UiStyleProperty iProperty) -> std::string_view;

/**
 * @brief
 *  Style of the theme a widget follows, and the properties it keeps its own.
 *
 * Precedence: a property in `overrides` keeps the widget field; else the style sets it when it defines it; else the
 * widget field stays. An empty name follows no style.
 */
struct OWL_API UiStyleRef {
	/// Name of the style in the theme of the HUD (empty = none).
	std::string name;
	/// Properties the widget keeps (`styleBit` mask).
	uint32_t overrides = 0;

	/**
	 * @brief
	 *  Check whether the style sets a property of the widget.
	 * @param[in] iProperty The property.
	 * @return True when a style is named and the property is not overridden.
	 */
	[[nodiscard]] auto follows(const UiStyleProperty iProperty) const -> bool {
		return !name.empty() && (overrides & styleBit(iProperty)) == 0;
	}

	/**
	 * @brief
	 *  Compare two references.
	 * @param[in] iOther The other reference.
	 * @return True when the name and the overrides are the same.
	 */
	auto operator==(const UiStyleRef& iOther) const -> bool = default;
};

}// namespace owl::scene::component
