/**
 * @file Timestep.cpp
 * @author Silmaen
 * @date 16/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/Timestep.h"

namespace owl::core {

auto Timestep::getStabilizedFps() const -> float {
	return std::accumulate(m_statFps.begin(), m_statFps.end(), 0.f) / static_cast<float>(m_statFps.size());
}

}// namespace owl::core
