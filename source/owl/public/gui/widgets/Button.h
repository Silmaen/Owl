/**
 * @file Button.h
 * @author Silmaen
 * @date 10/26/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "Widget.h"
#include "math/vectors.h"
#include <functional>

namespace owl::gui::widgets {

/**
	 * @brief Data associated with Button.
	 */
struct ButtonData : WidgetData {
	std::string icon{""};
	std::string replacementText{""};
	std::function<bool()> isSelected{[] { return false; }};
	std::function<void()> onClick{[] {}};
	math::vec2 size{0, 0};
};

/**
 * @brief Class describing a button gui widget.
 */
class OWL_API Button final: public Widget<ButtonData> {
public:
	/**
	 * @brief Default destructor.
	 */
	~Button() override;

	Button() = default;
	Button(const Button&) = default;
	Button(Button&&) = default;
	auto operator=(const Button&) -> Button& = default;
	auto operator=(Button&&) -> Button& = default;

private:
	/**
	 * @brief Render the button.
	 */
	void onRenderBase() const override;
};

}// namespace owl::gui::widgets
