/**
 * @file MouseEvent.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "Event.h"
#include "input/MouseCode.h"
#include <fmt/format.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace owl::event {

/**
 * @brief Mouse move evnet
 */
class OWL_API MouseMovedEvent : public Event {
public:
	MouseMovedEvent(const float x, const float y) : mouseX(x), mouseY(y) {}

	[[nodiscard]] float getX() const { return mouseX; }
	[[nodiscard]] float getY() const { return mouseY; }

	[[nodiscard]] std::string toString() const override {
		return fmt::format("MouseMovedEvent: {}, {}", getX(), getY());
	}
	[[nodiscard]] std::string getName() const override {
		return fmt::format("MouseMovedEvent");
	}

	[[nodiscard]] uint8_t getCategoryFlags() const override {
		return category::Input | category::Mouse;
	}
	[[nodiscard]] static type getStaticType() { return type::MouseMoved; }
	[[nodiscard]] type getType() const override { return getStaticType(); }

private:
	float mouseX;
	float mouseY;
};

/**
 * @brief Event when mouse is scrolled
 */
class OWL_API MouseScrolledEvent : public Event {
public:
	MouseScrolledEvent(const float xOffset, const float yOffset)
		: XOffset(xOffset), YOffset(yOffset) {}

	[[nodiscard]] float getXOff() const { return XOffset; }
	[[nodiscard]] float getYOff() const { return YOffset; }

	[[nodiscard]] std::string toString() const override {
		return fmt::format("MouseScrolledEvent: {}, {}", getXOff(), getYOff());
	}
	[[nodiscard]] std::string getName() const override {
		return fmt::format("MouseScrolledEvent");
	}
	[[nodiscard]] static type getStaticType() { return type::MouseScrolled; }
	[[nodiscard]] type getType() const override { return getStaticType(); }
	[[nodiscard]] uint8_t getCategoryFlags() const override {
		return category::Input | category::Mouse;
	}

private:
	float XOffset;
	float YOffset;
};

/**
 * @brief Event for mouse button
 */
class OWL_API MouseButtonEvent : public Event {
public:
	[[nodiscard]] input::MouseCode GetMouseButton() const { return mouseButton; }

	[[nodiscard]] uint8_t getCategoryFlags() const override {
		return category::Input | category::Mouse | category::MouseButton;
	}

protected:
	explicit MouseButtonEvent(const input::MouseCode button)
		: mouseButton(button) {}

	input::MouseCode mouseButton;
};

/**
 * @brief Event for Mouse button pressed
 */
class OWL_API MouseButtonPressedEvent : public MouseButtonEvent {
public:
	explicit MouseButtonPressedEvent(const input::MouseCode button)
		: MouseButtonEvent(button) {}

	[[nodiscard]] std::string toString() const override {
		return fmt::format("MouseButtonPressedEvent: {}", mouseButton);
	}
	[[nodiscard]] std::string getName() const override {
		return fmt::format("MouseButtonPressedEvent");
	}
	[[nodiscard]] static type getStaticType() { return type::MouseButtonPressed; }
	[[nodiscard]] type getType() const override { return getStaticType(); }
};

/**
 * @brief Event for mouse button released
 */
class OWL_API MouseButtonReleasedEvent : public MouseButtonEvent {
public:
	explicit MouseButtonReleasedEvent(const input::MouseCode button)
		: MouseButtonEvent(button) {}

	[[nodiscard]] std::string toString() const override {
		return fmt::format("MouseButtonReleasedEvent: {}", mouseButton);
	}
	[[nodiscard]] std::string getName() const override {
		return fmt::format("MouseButtonReleasedEvent");
	}
	[[nodiscard]] static type getStaticType() {
		return type::MouseButtonReleased;
	}
	[[nodiscard]] type getType() const override { return getStaticType(); }
};

}// namespace owl::event

#ifdef __clang__
#pragma clang diagnostic pop
#endif
