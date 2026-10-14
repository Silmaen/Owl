/**
 * @file Input.h
 * @author Silmaen
 * @date 06/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "input/Gamepad.h"
#include "input/InputFrame.h"
#include "input/KeyCodes.h"
#include "input/MouseCode.h"
#include "math/vectors.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

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
	 *  Gamepad state, live: the injected state of the slot when there is one, else the device.
	 * @param[in] iPad The gamepad slot (0 to 15).
	 * @return The state; disconnected for a slot out of range or without a gamepad.
	 */
	static auto getGamepadState(std::size_t iPad) -> GamepadState;

	/**
	 * @brief
	 *  Whether a gamepad is connected in a slot, live (device or injection).
	 * @param[in] iPad The gamepad slot.
	 * @return True if connected.
	 */
	static auto isGamepadConnected(std::size_t iPad) -> bool;

	/**
	 * @brief
	 *  Name of the gamepad of a slot, from its mapping.
	 * @param[in] iPad The gamepad slot.
	 * @return The name, `Injected gamepad` for an injected slot, empty when not connected.
	 */
	static auto getGamepadName(std::size_t iPad) -> std::string;

	/**
	 * @brief
	 *  Add gamepad mappings in the SDL_GameControllerDB format (one `GUID,name,mapping...` per line).
	 *
	 * On GLFW they go to `glfwUpdateGamepadMappings` (a mapping replaces the built-in one of the same GUID); the Null
	 * back-end only checks them.
	 * @param[in] iMappings The mapping text.
	 * @return True if at least one mapping line was found and accepted.
	 */
	static auto addGamepadMappings(std::string_view iMappings) -> bool;

	/**
	 * @brief
	 *  Read a gamepad mapping file and add its mappings.
	 * @param[in] iFile The file (SDL_GameControllerDB format).
	 * @return True if the file was read and its mappings accepted.
	 */
	static auto loadGamepadMappings(const std::filesystem::path& iFile) -> bool;

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
	 *  Simulate a gamepad button toggle, on any back-end; the slot then reads as an injected, connected gamepad.
	 * @param[in] iPad The gamepad slot.
	 * @param[in] iButton The button to press/release.
	 */
	static void injectGamepadButton(std::size_t iPad, GamepadButton iButton);

	/**
	 * @brief
	 *  Simulate a gamepad axis value, on any back-end; the slot then reads as an injected, connected gamepad.
	 * @param[in] iPad The gamepad slot.
	 * @param[in] iAxis The axis.
	 * @param[in] iValue The raw value (sticks in [-1, 1], triggers in [0, 1]).
	 */
	static void injectGamepadAxis(std::size_t iPad, GamepadAxis iAxis, float iValue);

	/**
	 * @brief
	 *  Replace the state of a gamepad slot, on any back-end (a disconnected state hides the device).
	 * @param[in] iPad The gamepad slot.
	 * @param[in] iState The injected state.
	 */
	static void injectGamepad(std::size_t iPad, const GamepadState& iState);

	/**
	 * @brief
	 *  Clear the previously defined injections (keys, buttons, cursor, gamepads).
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
	 *  Fill the keys, buttons, cursor and gamepads held by the devices, private implementation.
	 * @param[out] oState The state to fill (cleared by the caller).
	 */
	virtual void readDevices_impl(InputState& oState) = 0;

	/**
	 * @brief
	 *  Gamepad state of the device, private implementation.
	 * @param[in] iPad The gamepad slot (in range).
	 * @return The state.
	 */
	virtual auto getGamepadState_impl(std::size_t iPad) -> GamepadState = 0;

	/**
	 * @brief
	 *  Gamepad name of the device, private implementation.
	 * @param[in] iPad The gamepad slot (in range).
	 * @return The name, empty when none.
	 */
	virtual auto getGamepadName_impl(std::size_t iPad) -> std::string = 0;

	/**
	 * @brief
	 *  Hand checked gamepad mappings to the device back-end, private implementation.
	 * @param[in] iMappings The mapping text.
	 * @return True if accepted.
	 */
	virtual auto addGamepadMappings_impl(std::string_view iMappings) -> bool = 0;
};

}// namespace owl::input
