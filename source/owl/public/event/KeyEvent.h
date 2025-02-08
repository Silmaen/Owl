/**
 * @file KeyEvent.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "event/Event.h"
#include "input/KeyCodes.h"

namespace owl::event {

/**
 * @brief Semi-abstracted class for all Keyboard events.
 */
class OWL_API KeyEvent : public Event {
public:
	/**
	 * @brief Get the Key code.
	 * @return The Event Key code.
	 */
	[[nodiscard]] auto getKeyCode() const -> input::KeyCode { return m_keyCode; }

	/**
	 * @brief Get the event's category flags.
	 * @return Event's category flags.
	 */
	[[nodiscard]] auto getCategoryFlags() const -> uint8_t final;

protected:
	explicit KeyEvent(const input::KeyCode iKeyCode) : m_keyCode(iKeyCode) {}
	/// Key code.
	input::KeyCode m_keyCode;
};

/**
 * @brief Event of key presse
 */
class OWL_API KeyPressedEvent final : public KeyEvent {
public:
	/**
	 * @brief Constructor.
	 * @param[in] iKeyCode Code of the key pressed.
	 * @param[in] iRepeatCount Number of repeat.
	 */
	explicit KeyPressedEvent(const input::KeyCode iKeyCode, const uint16_t iRepeatCount)
		: KeyEvent(iKeyCode), m_repeatCount(iRepeatCount) {}

	/**
	 * @brief Check if the key is repeated
	 * @return True if repeated
	 */
	[[nodiscard]] auto getRepeatCount() const -> uint16_t { return m_repeatCount; }

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
	[[nodiscard]] static auto getStaticType() -> Type { return Type::KeyPressed; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }

private:
	/// Counter of repeat.
	uint16_t m_repeatCount;
};

/**
 * @brief Event of key released
 */
class OWL_API KeyReleasedEvent final : public KeyEvent {
public:
	/**
	 * @brief Constructor.
	 * @param[in] iKeyCode Code of the key released.
	 */
	explicit KeyReleasedEvent(const input::KeyCode iKeyCode) : KeyEvent(iKeyCode) {}

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
	[[nodiscard]] static auto getStaticType() -> Type { return Type::KeyReleased; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }
};

/**
 * @brief Event of key typed
 */
class OWL_API KeyTypedEvent final : public KeyEvent {
public:
	/**
	 * @brief Constructor.
	 * @param[in] iKeyCode Code of the key typed.
	 */
	explicit KeyTypedEvent(const input::KeyCode iKeyCode) : KeyEvent(iKeyCode) {}

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
	[[nodiscard]] static auto getStaticType() -> Type { return Type::KeyTyped; }

	/**
	 * @brief Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }
};

}// namespace owl::event
