/**
 * @file Input.h
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "input/Input.h"
#include "input/InputFrame.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace owl::input::android {

/**
 * @brief
 *  Owl key code of an Android key code.
 * @param[in] iAndroidKey The `AKEYCODE_*` value.
 * @return The key code, nullopt for a key without equivalent.
 */
[[nodiscard]] auto toKeyCode(int32_t iAndroidKey) -> std::optional<KeyCode>;

/**
 * @brief
 *  Input of the Android native activity: the window feeds the touch screen (as the left mouse button and the cursor
 *  of the first finger) and the keys into a shared state, read here.
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
	 *  Device state shared by the window, which writes it, and the input, which reads it.
	 * @return The state.
	 */
	[[nodiscard]] static auto deviceState() -> InputState&;

	/**
	 * @brief
	 *  Keyboard pressed check.
	 * @param[in] iKeyCode The Key to check.
	 * @return True if pressed.
	 */
	auto isKeyPressed_impl(KeyCode iKeyCode) -> bool override;

	/**
	 * @brief
	 *  Mouse button pressed check: the left button is a finger on the screen.
	 * @param[in] iMouseCode The mouse button to check.
	 * @return True if pressed.
	 */
	auto isMouseButtonPressed_impl(MouseCode iMouseCode) -> bool override;

	/**
	 * @brief
	 *  Position of the first finger, the last one when no finger touches.
	 * @return The position in window pixels.
	 */
	auto getMousePos_impl() -> math::vec2 override;

	/**
	 * @brief
	 *  Copy the device state.
	 * @param[out] oState The state to fill.
	 */
	void readDevices_impl(InputState& oState) override;

	/**
	 * @brief
	 *  Gamepad state: no gamepad on Android yet.
	 * @param[in] iPad The gamepad slot.
	 * @return A disconnected state.
	 */
	auto getGamepadState_impl(std::size_t iPad) -> GamepadState override;

	/**
	 * @brief
	 *  Gamepad name: no gamepad on Android yet.
	 * @param[in] iPad The gamepad slot.
	 * @return An empty name.
	 */
	auto getGamepadName_impl(std::size_t iPad) -> std::string override;

	/**
	 * @brief
	 *  Gamepad mappings: ignored, no gamepad on Android yet.
	 * @param[in] iMappings The SDL mapping lines.
	 * @return True.
	 */
	auto addGamepadMappings_impl(std::string_view iMappings) -> bool override;
};

}// namespace owl::input::android
