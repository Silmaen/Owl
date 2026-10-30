/**
 * @file WorldStreamingSystem.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "EngineSystems.h"

#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/WorldStreamer.h"
#include "scene/component/StreamingSource.h"

#include <vector>

namespace owl::scene::systems {

void updateWorldStreaming(Scene& ioScene, [[maybe_unused]] const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	if (!ioScene.getWorldPartitionSettings().enabled && !ioScene.hasWorldStreamer())
		return;
	// Kept between frames: the sources of a frame never allocate once the vector has grown.
	thread_local std::vector<StreamPoint> sSources;
	sSources.clear();
	for (const auto [handle, source]: ioScene.registry.view<component::StreamingSource>().each()) {
		if (!source.enabled)
			continue;
		sSources.push_back({.position = ioScene.getEntityWorldPosition(Entity{handle, &ioScene}),
							.radiusScale = static_cast<double>(source.radiusScale)});
	}
	if (sSources.empty()) {
		if (const Entity camera = ioScene.getPrimaryCamera(); camera)
			sSources.push_back({.position = ioScene.getEntityWorldPosition(camera), .radiusScale = 1.0});
	}
	ioScene.getWorldStreamer().update(ioScene, sSources, ioScene.getTaskScheduler());
}

}// namespace owl::scene::systems
