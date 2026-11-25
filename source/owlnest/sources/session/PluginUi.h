/**
 * @file PluginUi.h
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <script/ScriptConsole.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace owl::nest::session {

/**
 * @brief
 *  Kind of a widget an Owl Nest plugin draws (`ui.<kind>{...}` in its script).
 */
enum struct PluginWidgetKind : uint8_t {
	Section,///< A section header; the widgets that follow belong to it until the next section.
	Text,///< A paragraph of text.
	Value,///< A read-only row: a label and a text.
	Button,///< A button.
	Checkbox,///< A row with a check box (boolean value).
	Number,///< A row with a dragged number.
	Integer,///< A row with a dragged integer.
	Input,///< A row with a text field.
	Vec3,///< A row with the X / Y / Z fields of the inspector (list of three numbers).
	Color,///< A row with a colour picker (list of four numbers in [0, 1]).
	Choice///< A row with a drop-down list (the chosen text).
};

/**
 * @brief
 *  One widget asked by a plugin script.
 */
struct PluginWidget {
	/// What to draw.
	PluginWidgetKind kind{PluginWidgetKind::Text};
	/// Label of the row, of the button or of the section; the text of a `Text` widget.
	std::string label;
	/// Tooltip of the widget (empty for none).
	std::string tooltip{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Current value, in the type of the kind (none for `Section`, `Text` and `Button`; the text of a `Value`).
	script::HostValue value{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Smallest value of a `Number` or `Integer`.
	std::optional<double> min{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Largest value of a `Number` or `Integer`.
	std::optional<double> max{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Drag step of a `Number` or `Integer`.
	std::optional<double> step{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Texts offered by a `Choice`.
	std::vector<std::string> choices{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Whether a `Text` is dimmed (a hint), a `Button` the main action of its panel.
	bool emphasis{false};
};

/**
 * @brief
 *  What a widget tells the script on the frame it is drawn.
 */
struct PluginWidgetResult {
	/// The value shown: the one given, or the one being edited.
	script::HostValue value{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// An edit ended on this frame (`value` is the new value); a `Button` was clicked; a `Section` is open.
	bool changed{false};
};

/**
 * @brief
 *  The surface an Owl Nest plugin draws its panels, inspector drawers and documents on.
 *
 * A plugin never reaches ImGui: its script asks for widgets, the editor draws them with the Owl Nest panel style
 * (section headers, property grid, the X / Y / Z fields of the inspector).
 */
class PluginUi {
public:
	PluginUi() = default;

	PluginUi(const PluginUi&) = default;

	PluginUi(PluginUi&&) = default;

	auto operator=(const PluginUi&) -> PluginUi& = default;

	auto operator=(PluginUi&&) -> PluginUi& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~PluginUi();

	/**
	 * @brief
	 *  Draw one widget.
	 * @param[in] iWidget The widget.
	 * @return Its value and whether the user acted on it.
	 */
	virtual auto draw(const PluginWidget& iWidget) -> PluginWidgetResult = 0;
};

}// namespace owl::nest::session
