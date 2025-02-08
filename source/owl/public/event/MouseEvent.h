/**
 * @file MouseEvent.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "event/Event.h"
#include "input/MouseCode.h"

namespace owl::event {
/**
 * @brief Mouse move event.
 */
class OWL_API MouseMovedEvent final : public Event {
public:
	/**
	 * @brief Constructor.
	 * @param[in] iX X mouse position.
	 * @param[in] iY Y mouse position.
	 */
	MouseMovedEvent(const float iX, const float iY) : m_mouseX(iX), m_mouseY(iY) {}

	/**
	 * @brief Get X mouse position.
	 * @return X mouse position.
	 */
	[[nodiscard]] auto getX() const -> float { return m_mouseX; }

	/**
	 * @brief Get X mouse position.
	 * @return X mouse position.
	 */
	[[nodiscard]] auto getY() const -> float { return m_mouseY; }

	/**
	 * @brief Get the event as string.
	 * @return String of the event.
	 */
	[[nodiscard]] auto toString() const -> std::string override;

	/**
	 * @brief Get the event's name.
	 * @return Event's name.
	 */
	[[nodiscard]] auto getName() const -> std::string override;

	/**
	 * @brief Get the event's category flags.
	 * @return Event's category flags.
	 */
	[[nodiscard]] auto getCategoryFlags() const -> uint8_t override;

	/**
	 * @brief Get the event's static type.
	 * @return Event's static type.
	 */
	[[nodiscard]] static auto getStaticType() -> Type { return Type::MouseMoved; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }

private:
	/// X mouse position.
	float m_mouseX;
	/// Y mouse position
	float m_mouseY;
};

/**
 * @brief Event when mouse is scrolled.
 */
class OWL_API MouseScrolledEvent final : public Event {
public:
	/**
	 * @brief Constructor.
	 * @param[in] iXOffset X wheel offset.
	 * @param[in] iYOffset Y wheel offset.
	 */
	MouseScrolledEvent(const float iXOffset, const float iYOffset) : m_xOffset(iXOffset), m_yOffset(iYOffset) {}

	/**
	 * @brief Get X wheel offset.
	 * @return X wheel offset.
	 */
	[[nodiscard]] auto getXOff() const -> float { return m_xOffset; }

	/**
	 * @brief Get Y wheel offset.
	 * @return Y wheel offset.
	 */
	[[nodiscard]] auto getYOff() const -> float { return m_yOffset; }

	/**
	 * @brief Get the event as string.
	 * @return String of the event.
	 */
	[[nodiscard]] auto toString() const -> std::string override;

	/**
	 * @brief Get the event's name.
	 * @return Event's name.
	 */
	[[nodiscard]] auto getName() const -> std::string override;

	/**
	 * @brief Get the event's static type.
	 * @return Event's static type.
	 */
	[[nodiscard]] static auto getStaticType() -> Type { return Type::MouseScrolled; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }

	/**
	 * @brief Get the event's category flags.
	 * @return Event's category flags.
	 */
	[[nodiscard]] auto getCategoryFlags() const -> uint8_t override;

private:
	/// X wheel offset.
	float m_xOffset;
	/// Y wheel offset.
	float m_yOffset;
};

/**
 * @brief Event for mouse button.
 */
class OWL_API MouseButtonEvent : public Event {
public:
	/**
	 * @brief Get the mouse button involved in Event.
	 * @return The mouse button.
	 */
	[[nodiscard]] auto getMouseButton() const -> input::MouseCode { return m_mouseButton; }

	/**
	 * @brief Get the event's category flags.
	 * @return Event's category flags.
	 */
	[[nodiscard]] auto getCategoryFlags() const -> uint8_t override;

protected:
	/**
	 * @brief Constructor.
	 * @param[in] iXOffset The mouse button.
	 */
	explicit MouseButtonEvent(const input::MouseCode iXOffset) : m_mouseButton(iXOffset) {}

	/// The mouse button of the event.
	input::MouseCode m_mouseButton;
};

/**
 * @brief Event for Mouse button pressed.
 */
class OWL_API MouseButtonPressedEvent final : public MouseButtonEvent {
public:
	/**
	 * @brief Constructor.
	 * @param[in] iButton The mouse button.
	 */
	explicit MouseButtonPressedEvent(const input::MouseCode iButton) : MouseButtonEvent(iButton) {}

	/**
	 * @brief Get the event as string.
	 * @return String of the event.
	 */
	[[nodiscard]] auto toString() const -> std::string override;

	/**
	 * @brief Get the event's name.
	 * @return Event's name.
	 */
	[[nodiscard]] auto getName() const -> std::string override;

	/**
	 * @brief Get the event's static type.
	 * @return Event's static type.
	 */
	[[nodiscard]] static auto getStaticType() -> Type { return Type::MouseButtonPressed; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }
};

/**
 * @brief Event for mouse button released.
 */
class OWL_API MouseButtonReleasedEvent final : public MouseButtonEvent {
public:
	/**
	 * @brief Constructor.
	 * @param[in] iButton The mouse button.
	 */
	explicit MouseButtonReleasedEvent(const input::MouseCode iButton) : MouseButtonEvent(iButton) {}

	/**
	 * @brief Get the event as string.
	 * @return String of the event.
	 */
	[[nodiscard]] auto toString() const -> std::string override;

	/**
	 * @brief Get the event's name.
	 * @return Event's name.
	 */
	[[nodiscard]] auto getName() const -> std::string override;

	/**
	 * @brief Get the event's static type.
	 * @return Event's static type.
	 */
	[[nodiscard]] static auto getStaticType() -> Type { return Type::MouseButtonReleased; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }
};
}// namespace owl::event
