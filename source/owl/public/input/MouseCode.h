/**
 * @file MouseCode.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>


namespace owl::input {
/// Wrap to mouse code.
using MouseCode = uint8_t;
}// namespace owl::input

/**
 * @brief
 *  Namespace for mouse codes.
 */
namespace owl::input::mouse {

enum : MouseCode {
	// From glfw3.h
	Button0 = 0,///< Mouse button 0.
	Button1 = 1,///< Mouse button 1.
	Button2 = 2,///< Mouse button 2.
	Button3 = 3,///< Mouse button 3.
	Button4 = 4,///< Mouse button 4.
	Button5 = 5,///< Mouse button 5.
	Button6 = 6,///< Mouse button 6.
	Button7 = 7,///< Mouse button 7.

	ButtonLast = Button7,///< Last mouse button.
	ButtonLeft = Button0,///< Left mouse button.
	ButtonRight = Button1,///< Right mouse button.
	ButtonMiddle = Button2///< Middle mouse button.
};
}// namespace owl::input::mouse
