/**
 * @file Input.h
 * @author Silmaen
 * @date 02/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "input/Input.h"

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
};
}// namespace owl::input::null
