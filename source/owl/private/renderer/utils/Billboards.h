/**
 * @file Billboards.h
 * @author Silmaen
 * @date 24/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/matrixCreation.h"
#include "renderer/Renderer2DLayer.h"

#include <cmath>
#include <cstddef>

namespace owl::renderer::utils {

/**
 * @brief
 *  Axes the sprites of a billboard layer take for one camera, and the view axis that sorts them.
 */
struct BillboardFrame {
	/// World direction of the sprite X axis.
	math::vec3 right{1.f, 0.f, 0.f};
	/// World direction of the sprite Y axis.
	math::vec3 up{0.f, 1.f, 0.f};
	/// World direction the sprite faces (toward the camera).
	math::vec3 normal{0.f, 0.f, 1.f};
	/// Camera position.
	math::vec3 eye{0.f, 0.f, 0.f};
	/// Camera view direction (its -Z axis).
	math::vec3 forward{0.f, 0.f, -1.f};
};

/**
 * @brief
 *  The billboard axes of a camera.
 * @param[in] iMode How the sprites turn: `Upright` keeps the world Y axis up, `Full` (and `None`, for the sort axis
 *  only) takes the camera axes.
 * @param[in] iCameraWorld World matrix of the camera (the inverse of its view).
 * @return The frame.
 */
[[nodiscard]] inline auto makeBillboardFrame(const Renderer2DLayer::Billboard iMode, const math::mat4& iCameraWorld)
		-> BillboardFrame {
	BillboardFrame frame;
	const math::vec3 cameraRight{iCameraWorld(0, 0), iCameraWorld(1, 0), iCameraWorld(2, 0)};
	const math::vec3 cameraUp{iCameraWorld(0, 1), iCameraWorld(1, 1), iCameraWorld(2, 1)};
	const math::vec3 cameraBack{iCameraWorld(0, 2), iCameraWorld(1, 2), iCameraWorld(2, 2)};
	frame.eye = math::vec3{iCameraWorld(0, 3), iCameraWorld(1, 3), iCameraWorld(2, 3)};
	frame.forward = cameraBack.normSq() > 1e-12f ? -1.f * cameraBack.normalized() : math::vec3{0.f, 0.f, -1.f};
	if (iMode == Renderer2DLayer::Billboard::Upright) {
		math::vec3 flatRight{cameraRight.x(), 0.f, cameraRight.z()};
		// Looking straight down, the camera up gives the heading instead of its right axis.
		if (flatRight.normSq() < 1e-8f)
			flatRight = math::vec3{cameraUp.z(), 0.f, -cameraUp.x()};
		frame.right = flatRight.normSq() > 1e-12f ? flatRight.normalized() : math::vec3{1.f, 0.f, 0.f};
		frame.up = math::vec3{0.f, 1.f, 0.f};
		frame.normal = frame.right ^ frame.up;
		return frame;
	}
	if (cameraRight.normSq() > 1e-12f && cameraUp.normSq() > 1e-12f && cameraBack.normSq() > 1e-12f) {
		frame.right = cameraRight.normalized();
		frame.up = cameraUp.normalized();
		frame.normal = cameraBack.normalized();
	}
	return frame;
}

/**
 * @brief
 *  World matrix of a sprite turned to the camera.
 * @param[in] iFrame The billboard axes.
 * @param[in] iPosition World position of the sprite centre.
 * @param[in] iScale Size of the sprite (x, y) and its depth scale (z).
 * @param[in] iRoll Rotation of the sprite in its plane, in radians (an arrow along its flight).
 * @return The world matrix.
 */
[[nodiscard]] inline auto billboardMatrix(const BillboardFrame& iFrame, const math::vec3& iPosition,
										  const math::vec3& iScale, const float iRoll) -> math::mat4 {
	const float cosine = std::cos(iRoll);
	const float sine = std::sin(iRoll);
	const math::vec3 axisX = (iFrame.right * cosine + iFrame.up * sine) * iScale.x();
	const math::vec3 axisY = (iFrame.up * cosine - iFrame.right * sine) * iScale.y();
	const math::vec3 axisZ = iFrame.normal * iScale.z();
	math::mat4 matrix = math::identity<float, 4>();
	for (size_t row = 0; row < 3; ++row) {
		matrix(row, 0) = axisX[row];
		matrix(row, 1) = axisY[row];
		matrix(row, 2) = axisZ[row];
		matrix(row, 3) = iPosition[row];
	}
	return matrix;
}

/**
 * @brief
 *  Sort key of a primitive of a depth-tested layer: larger is nearer the camera, so the batch draws back to front.
 * @param[in] iFrame The camera axes.
 * @param[in] iPosition World position of the primitive.
 * @return Minus its distance along the view direction.
 */
[[nodiscard]] inline auto viewSortKey(const BillboardFrame& iFrame, const math::vec3& iPosition) -> float {
	return -((iPosition - iFrame.eye) * iFrame.forward);
}

}// namespace owl::renderer::utils
