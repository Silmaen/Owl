/**
 * @file ScenePicking.h
 * @author Silmaen
 * @date 23/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "renderer/RendererIsometric.h"
#include "scene/Entity.h"

namespace owl::scene {

class Scene;

/**
 * @brief
 *  Camera the isometric layer of the render stack draws a running scene with: the layer's presentation and zoom,
 *  centred on the primary camera.
 *
 * The default presentation when the stack holds no isometric layer; a 1280x720 viewport when the scene has none.
 * @param[in] iScene The scene.
 * @return The camera.
 */
[[nodiscard]] OWL_API auto makeRuntimeIsometricCamera(const Scene& iScene) -> renderer::IsometricCamera;

/**
 * @brief
 *  Entity drawn under a viewport pixel of a running scene, the nearest to the viewer.
 *
 * Tests the visible `SpriteRenderer`, `AnimatedSpriteRenderer`, `CircleRenderer` and `MeshRenderer` entities (not
 * the UI). An entity routed to an isometric layer of the render stack is tested on the screen rectangle of its
 * sprite, through `makeRuntimeIsometricCamera`; any other one on the ray of the primary camera through the pixel:
 * a sprite on its unit quad, a circle on its disc, a mesh on its bounding box. A hit in a later layer of the stack
 * (drawn over) wins, then the nearest hit.
 * @param[in,out] ioScene The scene (its world transforms are read).
 * @param[in] iPixel The pixel, in viewport pixels from the top left.
 * @return The entity, or an invalid entity when nothing is drawn there.
 */
[[nodiscard]] OWL_API auto pickEntity(Scene& ioScene, const math::vec2& iPixel) -> Entity;

}// namespace owl::scene
