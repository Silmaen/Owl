/**
 * @file trigonometry.cpp
 * @author Silmaen
 * @date 02/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "math/trigonometry.h"

namespace owl::math {

auto atan2(const float iY, const float iX) -> float { return std::atan2(iY, iX); }

}// namespace owl::math
