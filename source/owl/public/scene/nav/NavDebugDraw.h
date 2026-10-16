/**
 * @file NavDebugDraw.h
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::scene {

class Scene;
class TilemapAsset;
}// namespace owl::scene

namespace owl::scene::nav {

/**
 * @brief
 *  The centre of a cell, on its mean height, a little above the ground.
 * @param[in] iAsset The tilemap.
 * @param[in] iOrigin World position of the tilemap entity.
 * @param[in] iX Column of the cell.
 * @param[in] iY Row of the cell.
 * @return The world position.
 */
OWL_API auto getCellCenter(const TilemapAsset& iAsset, const math::vec3& iOrigin, uint32_t iX, uint32_t iY)
		-> math::vec3;

/**
 * @brief
 *  Draw the navigation graph of every tilemap of the scene with the 2D line renderer (a scene must be begun): two-way
 *  moves in green, one-way moves in orange with an arrow head.
 * @param[in,out] ioScene The scene.
 */
OWL_API void drawNavigationGraph(Scene& ioScene);

/**
 * @brief
 *  Draw the routes scripts gave to `NavWorld::setDebugRoute`, one colour each, from the first cell to the last, which is
 *  marked.
 * @param[in,out] ioScene The scene.
 */
OWL_API void drawNavigationRoutes(Scene& ioScene);

}// namespace owl::scene::nav
