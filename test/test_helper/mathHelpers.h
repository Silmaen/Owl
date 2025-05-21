/**
 * @file mathHelpers.h
 * @author Silmaen
 * @date 01/07/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include <math/math.h>

static constexpr bool matrixCompare(const owl::math::mat4& iFirstMatrix, const owl::math::mat4& iSecondMatrix) {
	bool same = true;
	for (size_t col = 0; col < 4; ++col) {
		for (size_t row = 0; row < 4; ++row) {
			if (std::abs(iFirstMatrix(row, col) - iSecondMatrix(row, col)) > 0.001f)
				same = false;
		}
	}
	return same;
}

static constexpr bool vectorCompare(const owl::math::vec4& iFirstVector, const owl::math::vec4& iSecondVector) {
	bool same = true;
	for (size_t col = 0; col < 4; ++col) {
		if (std::abs(iFirstVector[col] - iSecondVector[col]) > 0.001f)
			same = false;
	}
	return same;
}

static constexpr bool quatCompare(const owl::math::quat& iFirstQuaternion, const owl::math::quat& iSecondQuaternion) {
	bool same = true;
	for (size_t col = 0; col < 4; ++col) {
		if (std::abs(iFirstQuaternion[col] - iSecondQuaternion[col]) > 0.001f)
			same = false;
	}
	return same;
}
