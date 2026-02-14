/**
 * @file Map.cpp
 * @author Silmaen
 * @date 23/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Map.h"

namespace owl::raycaster::game {

Map::Map() = default;

Map::~Map() = default;

void Map::reset(const size_t iWidth, const size_t iHeight) {
	m_width = iWidth;
	m_height = iHeight;
	m_cells.resize(iWidth * iHeight);
	m_playerInitialPosition = {0, 0};
	m_playerInitialDirection = {0, 1};
}

}// namespace owl::raycaster::game
