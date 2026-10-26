/**
 * @file FloatingOriginSystem.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "EngineSystems.h"

#include "scene/Entity.h"
#include "scene/Scene.h"

namespace owl::scene::systems {

void updateFloatingOrigin(Scene& ioScene, [[maybe_unused]] const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	ioScene.syncWorldPositions();
	const auto settings = ioScene.getFloatingOriginSettings().clamped();
	if (!settings.enabled)
		return;
	const Entity camera = ioScene.getPrimaryCamera();
	if (!camera)
		return;
	const math::vec3 focus = ioScene.getWorldTransform(camera).translation();
	if (static_cast<double>(focus.norm()) <= settings.shiftRadius)
		return;
	const auto& origin = ioScene.getWorldOrigin();
	ioScene.shiftWorldOrigin({origin.x() + static_cast<double>(focus.x()), origin.y() + static_cast<double>(focus.y()),
							  origin.z() + static_cast<double>(focus.z())});
}

}// namespace owl::scene::systems
