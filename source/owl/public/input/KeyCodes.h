/**
 * @file KeyCodes.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>

/**
 * @brief
 *  Namespace for Input management.
 */
namespace owl::input {
/// Wrap Key code size.
using KeyCode = uint16_t;

/**
 * @brief
 *  Namespace for key codes.
 */
namespace key {

enum : KeyCode {
	// From glfw3.h
	Space = 32,///< Space key.
	Apostrophe = 39,///< Apostrophe key (`'`).
	Comma = 44,///< Comma key (`,`).
	Minus = 45,///< Minus key (`-`).
	Period = 46,///< Period key (`.`).
	Slash = 47,///< Slash key (`/`).

	D0 = 48,///< Digit 0 key.
	D1 = 49,///< Digit 1 key.
	D2 = 50,///< Digit 2 key.
	D3 = 51,///< Digit 3 key.
	D4 = 52,///< Digit 4 key.
	D5 = 53,///< Digit 5 key.
	D6 = 54,///< Digit 6 key.
	D7 = 55,///< Digit 7 key.
	D8 = 56,///< Digit 8 key.
	D9 = 57,///< Digit 9 key.

	Semicolon = 59,///< Semicolon key (`;`).
	Equal = 61,///< Equal key (`=`).

	A = 65,///< Key A.
	B = 66,///< Key B.
	C = 67,///< Key C.
	D = 68,///< Key D.
	E = 69,///< Key E.
	F = 70,///< Key F.
	G = 71,///< Key G.
	H = 72,///< Key H.
	I = 73,///< Key I.
	J = 74,///< Key J.
	K = 75,///< Key K.
	L = 76,///< Key L.
	M = 77,///< Key M.
	N = 78,///< Key N.
	O = 79,///< Key O.
	P = 80,///< Key P.
	Q = 81,///< Key Q.
	R = 82,///< Key R.
	S = 83,///< Key S.
	T = 84,///< Key T.
	U = 85,///< Key U.
	V = 86,///< Key V.
	W = 87,///< Key W.
	X = 88,///< Key X.
	Y = 89,///< Key Y.
	Z = 90,///< Key Z.

	LeftBracket = 91,///< Left bracket key (`[`).
	Backslash = 92,///< Backslash key (`\`).
	RightBracket = 93,///< Right bracket key (`]`).
	GraveAccent = 96,///< Grave accent key (```).

	World1 = 161,///< World1 key (`non-us #1`).
	World2 = 162,///< World2 key (`non-us #2`).

	/* Function keys */
	Escape = 256,///< Escape key.
	Enter = 257,///< Enter key.
	Tab = 258,///< Tab key.
	Backspace = 259,///< Backspace key.
	Insert = 260,///< Insert key.
	Delete = 261,///< Delete key.
	Right = 262,///< Right key.
	Left = 263,///< Left key.
	Down = 264,///< Down key.
	Up = 265,///< Up key.
	PageUp = 266,///< Page up key.
	PageDown = 267,///< Page down key.
	Home = 268,///< Home key.
	End = 269,///< End key.
	CapsLock = 280,///< Caps lock key.
	ScrollLock = 281,///< Scroll lock key.
	NumLock = 282,///< Num lock key.
	PrintScreen = 283,///< Print screen key.
	Pause = 284,///< Pause key.
	F1 = 290,///< Function key F1.
	F2 = 291,///< Function key F2.
	F3 = 292,///< Function key F3.
	F4 = 293,///< Function key F4.
	F5 = 294,///< Function key F5.
	F6 = 295,///< Function key F6.
	F7 = 296,///< Function key F7.
	F8 = 297,///< Function key F8.
	F9 = 298,///< Function key F9.
	F10 = 299,///< Function key F10.
	F11 = 300,///< Function key F11.
	F12 = 301,///< Function key F12.
	F13 = 302,///< Function key F13.
	F14 = 303,///< Function key F14.
	F15 = 304,///< Function key F15.
	F16 = 305,///< Function key F16.
	F17 = 306,///< Function key F17.
	F18 = 307,///< Function key F18.
	F19 = 308,///< Function key F19.
	F20 = 309,///< Function key F20.
	F21 = 310,///< Function key F21.
	F22 = 311,///< Function key F22.
	F23 = 312,///< Function key F23.
	F24 = 313,///< Function key F24.
	F25 = 314,///< Function key F25.

	/* Keypad */
	Kp0 = 320,///< Kp0 key.
	Kp1 = 321,///< Kp1 key.
	Kp2 = 322,///< Kp2 key.
	Kp3 = 323,///< Kp3 key.
	Kp4 = 324,///< Kp4 key.
	Kp5 = 325,///< Kp5 key.
	Kp6 = 326,///< Kp6 key.
	Kp7 = 327,///< Kp7 key.
	Kp8 = 328,///< Kp8 key.
	Kp9 = 329,///< Kp9 key.
	KpDecimal = 330,///< Kp decimal key.
	KpDivide = 331,///< Kp divide key.
	KpMultiply = 332,///< Kp multiply key.
	KpSubtract = 333,///< Kp subtract key.
	KpAdd = 334,///< Kp add key.
	KpEnter = 335,///< Kp enter key.
	KpEqual = 336,///< Kp equal key.

	LeftShift = 340,///< Left shift key.
	LeftControl = 341,///< Left control key.
	LeftAlt = 342,///< Left alt key.
	LeftSuper = 343,///< Left super key.
	RightShift = 344,///< Right shift key.
	RightControl = 345,///< Right control key.
	RightAlt = 346,///< Right alt key.
	RightSuper = 347,///< Right super key.
	Menu = 348///< Menu key.
};
}//namespace key
}// namespace owl::input
