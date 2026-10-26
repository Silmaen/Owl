/**
 * @file FloatingOriginSettings.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/FloatingOriginSettings.h"

#include "core/Macros.h"

#include <algorithm>
#include <cmath>

namespace owl::scene {

auto FloatingOriginSettings::clamped() const -> FloatingOriginSettings {
	FloatingOriginSettings out = *this;
	out.shiftRadius =
			std::isfinite(shiftRadius) ? std::max(shiftRadius, minShiftRadius) : FloatingOriginSettings{}.shiftRadius;
	return out;
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
auto FloatingOriginSettings::operator==(const FloatingOriginSettings& iOther) const -> bool = default;
OWL_DIAG_POP

}// namespace owl::scene
