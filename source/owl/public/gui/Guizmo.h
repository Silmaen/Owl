/**
 * @file Guizmo.h
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::gui {

class OWL_API Guizmo {
public:
	/**
	 * @brief Types of guizmo.
	 */
	enum struct Type : uint16_t {
		None = 0,///< Nothing displayed.
		Translation = 0b00000000000111,///< Translation control.
		Rotation = 0b00000001111000,///< Rotation control.
		Scale = 0b00001110000000,///< Scale control.
		All = 0b11100001111111,///< All at once.
	};

	static void initialize(const math::vec2& iPosition, const math::vec2& iSize);
	static auto isUsing() -> bool;
	static auto isOver() -> bool;
	static auto isRotate(const Type& iType) -> bool;
	static void manipulate(const math::mat4& iCameraView, const math::mat4& iCameraProjection, const Type& iType,
						   math::mat4& ioTransform, const float& iSnap = 0.0f);
};

}// namespace owl::gui
