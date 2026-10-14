/**
 * @file Input.h
 * @author Silmaen
 * @date 06/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "input/Input.h"

namespace owl::input::glfw {
/**
 * @brief
 *  Specialization class for inputs using glfw.
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
	 *  Keyboard pressed check, private implementation.
	 * @param[in] iKeycode The Key to check.
	 * @return True if pressed.
	 */
	auto isKeyPressed_impl(KeyCode iKeycode) -> bool override;

	/**
	 * @brief
	 *  Mouse button pressed check, private implementation.
	 * @param[in] iMouseCode Mouse button to check.
	 * @return True if pressed.
	 */
	auto isMouseButtonPressed_impl(MouseCode iMouseCode) -> bool override;

	/**
	 * @brief
	 *  Get mouse position, private implementation.
	 * @return Mouse Position.
	 */
	auto getMousePos_impl() -> math::vec2 override;

	/**
	 * @brief
	 *  Fill the keys, buttons and cursor held by the devices of the application window.
	 * @param[out] oState The state to fill.
	 */
	void readDevices_impl(InputState& oState) override;
};

}// namespace owl::input::glfw
