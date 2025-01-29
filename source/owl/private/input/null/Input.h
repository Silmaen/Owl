/**
 * @file Input.h
 * @author Silmaen
 * @date 02/08/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "input/Input.h"

#include <set>

namespace owl::input::null {
/**
 * @brief Specialization class for null input.
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
	 * @param[in] iKeyCode The Key to check.
	 * @return True if pressed.
	 */
	auto isKeyPressed_impl(KeyCode iKeyCode) -> bool override;
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
	 * @param[in] iKeyCode the key to press/release.
	 */
	void injectKey_impl(KeyCode iKeyCode) override;

	/**
	 * @brief Simulate mouse key toggle, private implementation.
	 * @param[in] iMouseCode the key to press/release.
	 */
	void injectMouseButton_impl(MouseCode iMouseCode) override;
	/**
	 * @brief Simulate mouse movement, private implementation.
	 * @param[in] iMousePos The new mouse pos.
	 */
	void injectMousePos_impl(const math::vec2& iMousePos) override;
	/**
	 * @brief Clear the previously defined injections.
	 */
	void resetInjection_impl() override;

	std::set<KeyCode> keyPressed;
	std::set<MouseCode> mouseBtnPressed;
	math::vec2 mousePos{0, 0};
};
}// namespace owl::input::null
