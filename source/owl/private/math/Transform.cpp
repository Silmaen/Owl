/**
 * @file Transform.cpp
 * @author Silmaen
 * @date 30/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "math/Transform.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>

namespace owl::math {

namespace {
constexpr float g_zero = 0.f;
constexpr float g_one = 1.f;
constexpr float g_epsi = epsilon<float>();

}// namespace

Transform::Transform(const vec3& iTranslation, const vec3& iRotation, const vec3& iScale)
	: m_translation{iTranslation}, m_rotation{iRotation}, m_scale{iScale} {}

Transform::Transform(const vec3& iTranslation, const vec3& iRotation)
	: m_translation{iTranslation}, m_rotation{iRotation} {}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
Transform::Transform(const mat4& iTransform) {
	// From glm::decompose in matrix_decompose.inl
	mat4 localMatrix(iTransform);
	// normalize the matrix
	if (epsilonEqual(localMatrix(3, 3), g_zero, g_epsi))
		return;
	for (uint8_t i = 0; i < 4; ++i) {
		for (uint8_t j = 0; j < 4; ++j) localMatrix(j, i) /= localMatrix(3, 3);
	}
	// first Suppress perspective.
	if (epsilonNotEqual(localMatrix(3, 0), g_zero, g_epsi) || epsilonNotEqual(localMatrix(3, 1), g_zero, g_epsi) ||
		epsilonNotEqual(localMatrix(3, 2), g_zero, g_epsi)) {
		// clear the perspective part
		localMatrix(3, 0) = localMatrix(3, 1) = localMatrix(3, 2) = g_zero;
		localMatrix(3, 3) = g_one;
	}
	// Next take care of translation (easy).
	m_translation = vec3{localMatrix(0, 3), localMatrix(1, 3), localMatrix(2, 3)};
	localMatrix.setColumn(3, vec4{0, 0, 0, localMatrix(3, 3)});

	vec3 row[3];

	// Now get scale and shear.
	for (size_t i = 0; i < 3; ++i) { row[i] = localMatrix.column(i); }
	// Compute X scale factor and normalize first row.
	m_scale.x() = row[0].norm();
	row[0].normalize();

	// Compute XY shear factor and make 2nd row orthogonal to 1st.
	row[1] = row[1] - (row[0] * row[1]) * row[0];

	// Now, compute Y scale and normalize 2nd row.
	m_scale.y() = row[1].norm();
	row[1].normalize();

	// Compute XZ and YZ shears, orthogonalize 3rd row.
	row[2] = row[2] - (row[0] * row[2]) * row[0];
	row[2] = row[2] - (row[1] * row[2]) * row[1];

	// Next, get Z scale and normalize 3rd row.
	m_scale.z() = row[2].norm();
	row[2].normalize();

	if (const vec3 pdum3 = row[1] ^ row[2]; row[0] * pdum3 < 0) {
		for (size_t i = 0; i < 3; i++) {
			m_scale[i] *= -g_one;
			row[i] *= -g_one;
		}
	}

	m_rotation.y() = -std::asin(row[0][2]);

	if (const float cy = std::cos(m_rotation.y()); epsilonNotEqual(cy, g_zero, g_epsi)) {
		m_rotation.x() = atan2(row[1][2] / cy, row[2][2] / cy);
		m_rotation.z() = atan2(row[0][1] / cy, row[0][0] / cy);
	} else {
		m_rotation.z() = 0;
		m_rotation.x() = atan2(-row[2][1], row[1][1]);
	}
}
OWL_DIAG_POP

auto Transform::operator()() const -> mat4 {
	// Closed form of translate * rotateZ * rotateY * rotateX * scale; a null angle skips its sin / cos (2D case).
	const auto sinCos = [](const float iAngle) -> std::pair<float, float> {
		// Bit test for +0 / -0: cheaper than a float compare chain and free of -Wfloat-equal.
		if ((std::bit_cast<uint32_t>(iAngle) << 1u) == 0u)
			return {g_zero, g_one};
		return {std::sin(iAngle), std::cos(iAngle)};
	};
	const auto [sx, cx] = sinCos(m_rotation[0]);
	const auto [sy, cy] = sinCos(m_rotation[1]);
	const auto [sz, cz] = sinCos(m_rotation[2]);
	mat4 result;
	result(0, 0) = cz * cy * m_scale[0];
	result(1, 0) = sz * cy * m_scale[0];
	result(2, 0) = -sy * m_scale[0];
	result(3, 0) = g_zero;
	result(0, 1) = (cz * sy * sx - sz * cx) * m_scale[1];
	result(1, 1) = (sz * sy * sx + cz * cx) * m_scale[1];
	result(2, 1) = cy * sx * m_scale[1];
	result(3, 1) = g_zero;
	result(0, 2) = (cz * sy * cx + sz * sx) * m_scale[2];
	result(1, 2) = (sz * sy * cx - cz * sx) * m_scale[2];
	result(2, 2) = cy * cx * m_scale[2];
	result(3, 2) = g_zero;
	result(0, 3) = m_translation[0];
	result(1, 3) = m_translation[1];
	result(2, 3) = m_translation[2];
	result(3, 3) = g_one;
	return result;
}

}// namespace owl::math
