/**
 * @file HudTheme.h
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/FormatVersion.h"
#include "data/fonts/Font.h"
#include "math/vectors.h"
#include "scene/component/UiStyle.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::scene {
class Scene;

/**
 * @brief
 *  A named style of a HUD theme: the properties it defines replace those of the widgets that follow it.
 */
struct OWL_API HudStyle {
	/// Name widgets refer to (`UiStyleRef::name`).
	std::string name;
	/// Properties the style defines (`component::styleBit` mask); the others leave the widget alone.
	uint32_t defined = 0;
	/// Fill colour (`UiStyleProperty::Background`).
	math::vec4 background{0.2f, 0.2f, 0.2f, 0.9f};
	/// Colour under the mouse.
	math::vec4 hover{0.3f, 0.3f, 0.3f, 1.f};
	/// Colour held down.
	math::vec4 pressed{0.15f, 0.15f, 0.15f, 1.f};
	/// Colour when not interactive.
	math::vec4 disabled{0.15f, 0.15f, 0.15f, 0.5f};
	/// Fill of a bar or slider, selected row, window title bar.
	math::vec4 accent{0.4f, 0.6f, 1.f, 1.f};
	/// Slider handle, alternate list row.
	math::vec4 secondary{1.f, 1.f, 1.f, 1.f};
	/// Text colour.
	math::vec4 textColor{1.f, 1.f, 1.f, 1.f};
	/// Font name in the font library (empty = default font).
	std::string font;
	/// Font size in reference pixels.
	float fontSize = 16.f;
	/// Colour of the text outline.
	math::vec4 outlineColor{0.f, 0.f, 0.f, 1.f};
	/// Width of the text outline in reference pixels.
	float outlineWidth = 0.f;
	/// Padding of the layout of a styled panel.
	float padding = 0.f;
	/// Box look: corners, gradient, border, shadow, nine-slice.
	component::UiBox box;
	/// Font resolved from `font` (runtime only).
	shared<data::fonts::Font> fontCache;

	/**
	 * @brief
	 *  Check whether the style defines a property.
	 * @param[in] iProperty The property.
	 * @return True when defined.
	 */
	[[nodiscard]] auto defines(const component::UiStyleProperty iProperty) const -> bool {
		return (defined & component::styleBit(iProperty)) != 0;
	}

	/**
	 * @brief
	 *  Define or clear a property.
	 * @param[in] iProperty The property.
	 * @param[in] iDefined Whether the style defines it.
	 */
	void setDefined(const component::UiStyleProperty iProperty, const bool iDefined) {
		if (iDefined)
			defined |= component::styleBit(iProperty);
		else
			defined &= ~component::styleBit(iProperty);
	}

	/**
	 * @brief
	 *  Compare two styles (the runtime font and the texture by identity).
	 * @param[in] iOther The other style.
	 * @return True when every field is the same.
	 */
	auto operator==(const HudStyle& iOther) const -> bool;
};

/**
 * @brief
 *  Error of reading a theme.
 */
enum struct HudThemeError : uint8_t {
	NotFound,///< No file at the path, in the pack nor the asset folders.
	Malformed,///< Not YAML, or not a theme document.
	Unsupported,///< Written by a newer Owl, or its migration failed.
};

/**
 * @brief
 *  A HUD theme (`.owlhudtheme`): named styles shared by the HUDs and menus of a project.
 *
 * A scene or a HUD names its theme (`HudTheme:` root key), else the project's `DefaultHudTheme` applies. Each frame the
 * HUD pass writes the styled properties into the widgets (`applyHudTheme`): an override flag of the widget keeps its
 * field, else the style's value wins when the style defines it, else the widget field stays.
 */
class OWL_API HudTheme final {
public:
	/// Name of the theme.
	std::string name = "Theme";
	/// The styles, in authoring order.
	std::vector<HudStyle> styles;

	/**
	 * @brief
	 *  Extension of the theme files.
	 * @return `.owlhudtheme`.
	 */
	[[nodiscard]] static constexpr auto fileExtension() -> std::string_view { return ".owlhudtheme"; }

	/**
	 * @brief
	 *  Check a path by its extension.
	 * @param[in] iPath The path.
	 * @return True for a theme file.
	 */
	[[nodiscard]] static auto isTheme(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Format descriptor (version and migrations) of the theme files.
	 * @return The descriptor.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  Find a style by name.
	 * @param[in] iName The name.
	 * @return The style, or null.
	 */
	[[nodiscard]] auto findStyle(std::string_view iName) const -> const HudStyle*;

	/**
	 * @brief
	 *  Index of a style by name.
	 * @param[in] iName The name.
	 * @return The index, or none.
	 */
	[[nodiscard]] auto findStyleIndex(std::string_view iName) const -> std::optional<size_t>;

	/**
	 * @brief
	 *  Resolve the fonts of the styles in the font library of the application (no-op without one).
	 */
	void resolveFonts();

	/**
	 * @brief
	 *  Write the theme as YAML.
	 * @return The text.
	 */
	[[nodiscard]] auto serializeToString() const -> std::string;

	/**
	 * @brief
	 *  Write one style as a YAML map (the form of an item of `Styles:`).
	 * @param[in] iStyle The style.
	 * @return The text.
	 */
	[[nodiscard]] static auto serializeStyle(const HudStyle& iStyle) -> std::string;

	/**
	 * @brief
	 *  Read one style written by `serializeStyle`.
	 * @param[in] iYaml The text.
	 * @return The style, or none for a malformed text.
	 */
	[[nodiscard]] static auto deserializeStyle(std::string_view iYaml) -> std::optional<HudStyle>;

	/**
	 * @brief
	 *  Read a theme from YAML text, upgrading an older format.
	 * @param[in] iYaml The text.
	 * @param[in] iSourceName Name for the messages.
	 * @return The theme, or why it cannot be read.
	 */
	[[nodiscard]] static auto deserializeFromString(std::string_view iYaml, std::string_view iSourceName = "<buffer>")
			-> expected<HudTheme, HudThemeError>;

	/**
	 * @brief
	 *  Write the theme to a file (atomically).
	 * @param[in] iPath The file.
	 * @return True on success.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iPath) const -> bool;

	/**
	 * @brief
	 *  Read a theme from the pack, the asset folders or the disk.
	 * @param[in] iPath Asset path or disk path.
	 * @return The theme, or why it cannot be read.
	 */
	[[nodiscard]] static auto load(const std::filesystem::path& iPath) -> expected<HudTheme, HudThemeError>;

	/**
	 * @brief
	 *  A small theme to start from: panel, button, title, text, slider, bar, list, window.
	 * @return The theme.
	 */
	[[nodiscard]] static auto makeDefault() -> HudTheme;

	/**
	 * @brief
	 *  Compare two themes.
	 * @param[in] iOther The other theme.
	 * @return True when the name and the styles are the same.
	 */
	auto operator==(const HudTheme& iOther) const -> bool;
};

/**
 * @brief
 *  Write the styled properties of the widgets of a scene from a theme.
 *
 * Every `Ui*` widget naming a style found in the theme takes the properties the style defines, except those it
 * overrides. A widget naming a style the theme lacks is left alone.
 * @param[in,out] ioScene The scene.
 * @param[in] iTheme The theme.
 */
OWL_API void applyHudTheme(Scene& ioScene, const HudTheme& iTheme);

}// namespace owl::scene
