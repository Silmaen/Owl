/**
 * @file Input.h
 * @author Silmaen
 * @date 06/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "input/InputFrame.h"
#include "input/KeyCodes.h"
#include "input/MouseCode.h"
#include "math/vectors.h"

#include <cstdint>

/**
 * @brief
 *  Windowing back-ends (declared here so `input` does not depend on the `window` module).
 */
namespace owl::window {
enum struct Type : uint8_t;
}// namespace owl::window

namespace owl::input {
/**
 * @brief
 *  Input layer: polling of the device back-end (GLFW or Null) merged with the injected input.
 *
 * Injection lives here, not in the back-ends, so scenarios, tests and tools drive the game on any window type.
 */
class OWL_API Input {
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
	virtual ~Input();

	/**
	 * @brief
	 *  Initialize the input for the GLFW windowing back-end.
	 */
	static void init();

	/**
	 * @brief
	 *  Initialize the input;
	 * @param[in] iType the type of input.
	 */
	static void init(const window::Type& iType);

	/**
	 * @brief
	 *  Destroy the inout instance.
	 */
	static void invalidate();

	/**
	 * @brief
	 *  Keyboard pressed check, live (device or injection): game code reads `scene::Scene::getInputFrame()`.
	 * @param[in] iKeycode The Key to check.
	 * @return True if pressed.
	 */
	static auto isKeyPressed(KeyCode iKeycode) -> bool;

	/**
	 * @brief
	 *  Mouse button pressed check, live (device or injection).
	 * @param[in] iMouseCode Mouse button to check.
	 * @return True if pressed.
	 */
	static auto isMouseButtonPressed(MouseCode iMouseCode) -> bool;

	/**
	 * @brief
	 *  Get mouse X position.
	 * @return Mouse X Position.
	 */
	static auto getMouseX() -> float;

	/**
	 * @brief
	 *  Get mouse Y position.
	 * @return Mouse Y Position.
	 */
	static auto getMouseY() -> float;

	/**
	 * @brief
	 *  Get mouse position.
	 * @return Mouse Position.
	 */
	static auto getMousePos() -> math::vec2;

	/**
	 * @brief
	 *  Snapshot of the devices merged with the injected input; the wheel is the offset of the current frame.
	 * @return The current input state.
	 */
	static auto readState() -> InputState;

	/**
	 * @brief
	 *  Start a frame: the wheel offset added since the previous call becomes the one of this frame.
	 *
	 * Called once per frame by `app::Application::run`, before the layers update.
	 */
	static void beginFrame();

	/**
	 * @brief
	 *  Add a wheel offset to the current frame interval (scroll events, injection).
	 * @param[in] iOffset The offset (x horizontal, y vertical).
	 */
	static void addWheel(const math::vec2& iOffset);

	/**
	 * @brief
	 *  Simulate key toggle, on any back-end: the key reads as held until toggled again or reset.
	 * @param[in] iKeycode the key to press/release.
	 */
	static void injectKey(KeyCode iKeycode);

	/**
	 * @brief
	 *  Simulate mouse key toggle, on any back-end.
	 * @param[in] iMouseCode the key to press/release.
	 */
	static void injectMouseButton(MouseCode iMouseCode);

	/**
	 * @brief
	 *  Simulate mouse movement, on any back-end: the injected position replaces the device one until reset.
	 * @param[in] iMousePos The new mouse pos.
	 */
	static void injectMousePos(const math::vec2& iMousePos);

	/**
	 * @brief
	 *  Clear the previously defined injections (keys, buttons, cursor).
	 */
	static void resetInjection();

private:
	/// The Input type.
	static window::Type m_type;
	/// The input instance.
	static uniq<Input> m_instance;

	/**
	 * @brief
	 *  Keyboard pressed check, private implementation.
	 * @param[in] iKeycode The Key to check.
	 * @return True if pressed.
	 */
	virtual auto isKeyPressed_impl(KeyCode iKeycode) -> bool = 0;

	/**
	 * @brief
	 *  Mouse button pressed check, private implementation.
	 * @param[in] iMouseCode Mouse button to check.
	 * @return True if pressed.
	 */
	virtual auto isMouseButtonPressed_impl(MouseCode iMouseCode) -> bool = 0;

	/**
	 * @brief
	 *  Get mouse position, private implementation.
	 * @return Mouse Position.
	 */
	virtual auto getMousePos_impl() -> math::vec2 = 0;

	/**
	 * @brief
	 *  Fill the keys, buttons and cursor held by the devices, private implementation.
	 * @param[out] oState The state to fill (cleared by the caller).
	 */
	virtual void readDevices_impl(InputState& oState) = 0;
};

}// namespace owl::input
