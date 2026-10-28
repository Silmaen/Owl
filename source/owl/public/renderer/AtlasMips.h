/**
 * @file AtlasMips.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "gpu/Texture.h"
#include "math/vectors.h"

#include <cstdint>
#include <span>

namespace owl::renderer {

/**
 * @brief
 *  Number of mipmap levels of a tile atlas in which every texel stays inside one cell.
 *
 * A level halves the cells: the chain stops at the level where a cell side would no longer be a whole number of
 * texels, at one texel per cell at most.
 * @param[in] iCell Size of one cell in texels of the finest level.
 * @return The level count, 1 for a cell with an odd side.
 */
[[nodiscard]] OWL_API auto cellMipLevelCount(const math::vec2ui& iCell) -> uint32_t;

/**
 * @brief
 *  Build the mipmap chain of a tile atlas cell by cell: each texel of a level averages the 2x2 texels below it, all
 *  of one cell, the colours weighted by their alpha so transparent texels do not darken the edges.
 * @param[in] iRgba Pixels of the finest level, RGBA8, tightly packed.
 * @param[in] iSize Size of the finest level.
 * @param[in] iCell Size of one cell in texels.
 * @return The levels, finest first, `cellMipLevelCount(iCell)` of them (fewer when the atlas is smaller).
 */
[[nodiscard]] OWL_API auto buildCellMips(std::span<const uint8_t> iRgba, const math::vec2ui& iSize,
										 const math::vec2ui& iCell) -> gpu::TextureLevels;

}// namespace owl::renderer
