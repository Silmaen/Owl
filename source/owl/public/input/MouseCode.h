/**
 * @file MouseCode.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once


namespace owl::input {
/// Wrap to mouse code.
using MouseCode = uint8_t;
}// namespace owl::input

/**
 * @brief Namespace for mouse codes.
 */
namespace owl::input::mouse {
enum : MouseCode {
	// From glfw3.h
	Button0 = 0,
	Button1 = 1,
	Button2 = 2,
	Button3 = 3,
	Button4 = 4,
	Button5 = 5,
	Button6 = 6,
	Button7 = 7,

	ButtonLast = Button7,
	ButtonLeft = Button0,
	ButtonRight = Button1,
	ButtonMiddle = Button2
};
}// namespace owl::input::mouse
