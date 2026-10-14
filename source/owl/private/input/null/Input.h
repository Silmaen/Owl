/**
 * @file Input.h
 * @author Silmaen
 * @date 02/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "input/Input.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace owl::input::null {
/**
 * @brief
 *  Input without any device: only the injection of the input layer drives it.
 */
class OWL_API Input final : public input::Input {
public:
	Input(const Input&) = delete;

	Input(Input&&) = delete;

	auto operator=(const Input&) -> Input& = delete;

	auto operator=(Input&&) -> Input& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	Input() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~Input() override = default;

	/**
	 * @brief
	 *  Keyboard pressed check: no device, never pressed.
	 * @param[in] iKeyCode The Key to check.
	 * @return False.
	 */
	auto isKeyPressed_impl(KeyCode iKeyCode) -> bool override;

	/**
	 * @brief
	 *  Mouse button pressed check: no device, never pressed.
	 * @param[in] iMouseCode Mouse button to check.
	 * @return False.
	 */
	auto isMouseButtonPressed_impl(MouseCode iMouseCode) -> bool override;

	/**
	 * @brief
	 *  Get mouse position: no device, the origin.
	 * @return The origin.
	 */
	auto getMousePos_impl() -> math::vec2 override;

	/**
	 * @brief
	 *  Fill the device state: no device, nothing held.
	 * @param[out] oState The state to fill (left untouched).
	 */
	void readDevices_impl(InputState& oState) override;

	/**
	 * @brief
	 *  Gamepad state: no device, disconnected.
	 * @param[in] iPad The gamepad slot.
	 * @return A disconnected state.
	 */
	auto getGamepadState_impl(std::size_t iPad) -> GamepadState override;

	/**
	 * @brief
	 *  Gamepad name: no device, empty.
	 * @param[in] iPad The gamepad slot.
	 * @return An empty name.
	 */
	auto getGamepadName_impl(std::size_t iPad) -> std::string override;

	/**
	 * @brief
	 *  Accept gamepad mappings already checked by the input layer: no device to give them to.
	 * @param[in] iMappings The mapping text.
	 * @return True.
	 */
	auto addGamepadMappings_impl(std::string_view iMappings) -> bool override;
};
}// namespace owl::input::null
