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

/**
 * @brief Base class for manipulation of guizmos.
 */
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

	/**
	 * @brief Initialize the guizmo drawing.
	 * @param iPosition Position in the scene.
	 * @param iSize Size of the guizmo.
	 */
	static void initialize(const math::vec2& iPosition, const math::vec2& iSize);
	/**
	 * @brief Check if the guizmo is currently be used.
	 * @return True if the user is manipulating the guizmo.
	 */
	static auto isUsing() -> bool;
	/**
	 * @brief Check if the user is hovering the guizmo.
	 * @return True if the user is hovering the guizmo.
	 */
	static auto isOver() -> bool;
	/**
	 * @brief Check if the actual guizmo has rotation action.
	 * @param iType The current type of guizmo used.
	 * @return True if rotation is active in the guizmo.
	 */
	static auto isRotate(const Type& iType) -> bool;

	/**
	 * @brief Do the manipulation.
	 * @param iCameraView The camera view matrix.
	 * @param iCameraProjection Thre camera projection matrix.
	 * @param iType The type of guizmo.
	 * @param ioTransform The object transformation.
	 * @param iSnap The value of the snap.
	 */
	static void manipulate(const math::mat4& iCameraView, const math::mat4& iCameraProjection, const Type& iType,
						   math::mat4& ioTransform, const float& iSnap = 0.0f);
};

}// namespace owl::gui
