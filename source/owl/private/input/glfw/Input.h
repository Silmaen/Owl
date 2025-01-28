/**
 * @file Input.h
 * @author Silmaen
 * @date 06/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "input/Input.h"

namespace owl::input::glfw {

/**
 * @brief Specialization class for inputs using glfw.
 */
class OWL_API Input final : public input::Input {
public:
	Input(const Input&) = delete;
	Input(Input&&) = delete;
	auto operator=(const Input&) -> Input& = delete;
	auto operator=(Input&&) -> Input& = delete;

	/**
	 * @brief Default constructor.
	 */
	Input() = default;

	/**
	 * @brief Destructor.
	 */
	~Input() override = default;

	/**
	 * @brief Keyboard pressed check, private implementation.
	 * @param[in] iKeycode The Key to check.
	 * @return True if pressed.
	 */
	auto isKeyPressed_impl(KeyCode iKeycode) -> bool override;
	/**
	 * @brief Mouse button pressed check, private implementation.
	 * @param[in] iMouseCode Mouse button to check.
	 * @return True if pressed.
	 */
	auto isMouseButtonPressed_impl(MouseCode iMouseCode) -> bool override;
	/**
	 * @brief Get mouse position, private implementation.
	 * @return Mouse Position.
	 */
	auto getMousePos_impl() -> math::vec2 override;
	/**
	 * @brief Simulate key toggle, private implementation.
	 * @param[in] iKeycode the key to press/release.
	 */
	void injectKey_impl([[maybe_unused]] KeyCode iKeycode) override {}

	/**
	 * @brief Simulate mouse key toggle, private implementation.
	 * @param[in] iMouseCode the key to press/release.
	 */
	void injectMouseButton_impl([[maybe_unused]] MouseCode iMouseCode) override {}
	/**
	 * @brief Simulate mouse movement, private implementation.
	 * @param[in] iMousePos The new mouse pos.
	 */
	void injectMousePos_impl([[maybe_unused]] const math::vec2& iMousePos) override {}

	/**
	 * @brief Clear the previously defined injections.
	 */
	void resetInjection_impl() override {}
};


}// namespace owl::input::glfw
