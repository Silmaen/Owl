/**
 * @file mathHelpers.h
 * @author Silmaen
 * @date 01/07/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include <core/Core.h>


static constexpr bool matrixCompare(const owl::math::mat4& gmat, const owl::math::mat4& mmat) {
	bool same = true;
	for (size_t col = 0; col < 4; ++col) {
		for (size_t row = 0; row < 4; ++row) {
			if (std::abs(gmat(row, col) - mmat(row, col)) > 0.001f)
				same = false;
		}
	}
	return same;
}

static constexpr bool vectorCompare(const owl::math::vec4& gvec, const owl::math::vec4& mvec) {
	bool same = true;
	for (size_t col = 0; col < 4; ++col) {
		if (std::abs(gvec[col] - mvec[col]) > 0.001f)
			same = false;
	}
	return same;
}

static constexpr bool quatCompare(const owl::math::quat& gvec, const owl::math::quat& mvec) {
	bool same = true;
	for (size_t col = 0; col < 4; ++col) {
		if (std::abs(gvec[col] - mvec[col]) > 0.001f)
			same = false;
	}
	return same;
}
