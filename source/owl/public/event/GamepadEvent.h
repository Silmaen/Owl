/**
 * @file GamepadEvent.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "event/Event.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>

namespace owl::event {
/**
 * @brief
 *  A gamepad was connected in a slot (GLFW joystick with a gamepad mapping).
 */
class OWL_API GamepadConnectedEvent final : public Event {
public:
	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iPad The gamepad slot.
	 * @param[in] iName The gamepad name, from its mapping.
	 */
	GamepadConnectedEvent(const std::size_t iPad, std::string iName) : m_pad{iPad}, m_name{std::move(iName)} {}

	/**
	 * @brief
	 *  Get the gamepad slot.
	 * @return The slot.
	 */
	[[nodiscard]] auto getPad() const -> std::size_t { return m_pad; }

	/**
	 * @brief
	 *  Get the gamepad name.
	 * @return The name.
	 */
	[[nodiscard]] auto getGamepadName() const -> const std::string& { return m_name; }

	/**
	 * @brief
	 *  Get the event as string.
	 * @return String of the event.
	 */
	[[nodiscard]] auto toString() const -> std::string override {
		return std::format("GamepadConnectedEvent: {} ({})", m_pad, m_name);
	}

	/**
	 * @brief
	 *  Get the event's name.
	 * @return Event's name.
	 */
	[[nodiscard]] auto getName() const -> std::string override { return std::format("GamepadConnectedEvent"); }

	/**
	 * @brief
	 *  Get the event's category flags.
	 * @return Event's category flags.
	 */
	[[nodiscard]] auto getCategoryFlags() const -> uint8_t override;

	/**
	 * @brief
	 *  Get the event's static type.
	 * @return Event's static type.
	 */
	[[nodiscard]] static auto getStaticType() -> Type { return Type::GamepadConnected; }

	/**
	 * @brief
	 *  Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }

private:
	/// The gamepad slot.
	std::size_t m_pad;
	/// The gamepad name.
	std::string m_name;
};

/**
 * @brief
 *  A joystick or gamepad was disconnected from a slot.
 */
class OWL_API GamepadDisconnectedEvent final : public Event {
public:
	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iPad The gamepad slot.
	 */
	explicit GamepadDisconnectedEvent(const std::size_t iPad) : m_pad{iPad} {}

	/**
	 * @brief
	 *  Get the gamepad slot.
	 * @return The slot.
	 */
	[[nodiscard]] auto getPad() const -> std::size_t { return m_pad; }

	/**
	 * @brief
	 *  Get the event as string.
	 * @return String of the event.
	 */
	[[nodiscard]] auto toString() const -> std::string override {
		return std::format("GamepadDisconnectedEvent: {}", m_pad);
	}

	/**
	 * @brief
	 *  Get the event's name.
	 * @return Event's name.
	 */
	[[nodiscard]] auto getName() const -> std::string override { return std::format("GamepadDisconnectedEvent"); }

	/**
	 * @brief
	 *  Get the event's category flags.
	 * @return Event's category flags.
	 */
	[[nodiscard]] auto getCategoryFlags() const -> uint8_t override;

	/**
	 * @brief
	 *  Get the event's static type.
	 * @return Event's static type.
	 */
	[[nodiscard]] static auto getStaticType() -> Type { return Type::GamepadDisconnected; }

	/**
	 * @brief
	 *  Get the event's type.
	 * @return Event's type.
	 */
	[[nodiscard]] auto getType() const -> Type override { return getStaticType(); }

private:
	/// The gamepad slot.
	std::size_t m_pad;
};

}// namespace owl::event
