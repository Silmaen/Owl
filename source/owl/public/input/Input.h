/**
 * @file Input.h
 * @author Silmaen
 * @date 06/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "KeyCodes.h"
#include "MouseCode.h"
#include "Window.h"
#include "core/Core.h"

namespace owl::input {
/**
 * @brief Base class for input management.
 */
class OWL_API Input {
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
	virtual ~Input();

	/**
	 * @brief Initialize the input;
	 * @param[in] iType the type of input
	 */
	static void init(const Type& iType = Type::Glfw);

	/**
	 * @brief Destroy the inout instance.
	 */
	static void invalidate();

	/**
	 * @brief Keyboard pressed check.
	 * @param[in] iKeycode The Key to check.
	 * @return True if pressed.
	 */
	static auto isKeyPressed(KeyCode iKeycode) -> bool;

	/**
	 * @brief Mouse button pressed check.
	 * @param[in] iMouseCode Mouse button to check.
	 * @return True if pressed.
	 */
	static auto isMouseButtonPressed(MouseCode iMouseCode) -> bool;

	/**
	 * @brief Get mouse X position.
	 * @return Mouse X Position.
	 */
	static auto getMouseX() -> float;

	/**
	 * @brief Get mouse Y position.
	 * @return Mouse Y Position.
	 */
	static auto getMouseY() -> float;

	/**
	 * @brief Get mouse position.
	 * @return Mouse Position.
	 */
	static auto getMousePos() -> math::vec2;

	/**
	 * @brief Simulate key toggle.
	 * @param[in] iKeycode the key to press/release.
	 */
	static void injectKey(KeyCode iKeycode);

	/**
	 * @brief Simulate mouse key toggle.
	 * @param[in] iMouseCode the key to press/release.
	 */
	static void injectMouseButton(MouseCode iMouseCode);

	/**
	 * @brief Simulate mouse movement.
	 * @param[in] iMousePos The new mouse pos.
	 */
	static void injectMousePos(const math::vec2& iMousePos);

	/**
	 * @brief Clear the previously defined injections.
	 */
	static void resetInjection();

private:
	/// The Input type.
	static Type m_type;
	/// The input instance.
	static uniq<Input> m_instance;

	/**
	 * @brief Keyboard pressed check, private implementation.
	 * @param[in] iKeycode The Key to check.
	 * @return True if pressed.
	 */
	virtual auto isKeyPressed_impl(KeyCode iKeycode) -> bool = 0;
	/**
	 * @brief Mouse button pressed check, private implementation.
	 * @param[in] iMouseCode Mouse button to check.
	 * @return True if pressed.
	 */
	virtual auto isMouseButtonPressed_impl(MouseCode iMouseCode) -> bool = 0;
	/**
	 * @brief Get mouse position, private implementation.
	 * @return Mouse Position.
	 */
	virtual auto getMousePos_impl() -> math::vec2 = 0;
	/**
	 * @brief Simulate key toggle, private implementation.
	 * @param[in] iKeycode the key to press/release.
	 */
	virtual void injectKey_impl(KeyCode iKeycode) = 0;

	/**
	 * @brief Simulate mouse key toggle, private implementation.
	 * @param[in] iMouseCode the key to press/release.
	 */
	virtual void injectMouseButton_impl(MouseCode iMouseCode) = 0;
	/**
	 * @brief Simulate mouse movement, private implementation.
	 * @param[in] iMousePos The new mouse pos.
	 */
	virtual void injectMousePos_impl(const math::vec2& iMousePos) = 0;

	/**
	 * @brief Clear the previously defined injections.
	 */
	virtual void resetInjection_impl() = 0;
};

}// namespace owl::input
