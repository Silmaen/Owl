/**
 * @file Button.h
 * @author Silmaen
 * @date 10/26/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "Widget.h"
#include "math/vectors.h"
#include <functional>
#include <string>

namespace owl::gui::widgets {
/**
 * @brief
 *  Data associated with Button.
	 */
struct ButtonData : WidgetData {
	/// Name of the icon in the `IconBank`.
	std::string icon;
	/// Text shown when the icon is missing.
	std::string replacementText;
	/// Tell whether the button is drawn as selected.
	std::function<bool()> isSelected{[] -> bool { return false; }};
	/**
	 * @brief
	 *  Action run on click.
	 */
	std::function<void()> onClick{[] -> void {}};
	/// Size of the button (zero: automatic).
	math::vec2 size{0, 0};
	/// Tooltip text.
	std::string tooltip;
};

/**
 * @brief
 *  Class describing a button gui widget.
 */
class OWL_API Button final : public Widget<ButtonData> {
public:
	/**
	 * @brief
	 *  Default destructor.
	 */
	~Button() override;

	Button() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	Button(const Button&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	Button(Button&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const Button&) -> Button& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(Button&&) -> Button& = default;

private:
	/**
	 * @brief
	 *  Render the button.
	 */
	void onRenderBase() const override;
};

}// namespace owl::gui::widgets
