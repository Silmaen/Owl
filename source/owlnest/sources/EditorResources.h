/**
 * @file EditorResources.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl::nest::utils {

/**
 * @brief
 *  Rasterize the editor icons (SVG sources, PNG fallback) into the icon bank, tinted with the current theme.
 */
void buildIconBank();

/**
 * @brief
 *  Load the scene trigger overlay textures drawn by the viewport.
 */
void loadTriggerTextures();

/**
 * @brief
 *  Load the editor sounds (Play click).
 */
void loadSounds();

}// namespace owl::nest::utils
