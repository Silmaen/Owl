/**
 * @file ParticleSystems.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "EngineSystems.h"

#include "renderer/RendererParticles.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/ParticleEmitter.h"
#include "scene/component/Trail.h"
#include "scene/component/Transform.h"

namespace owl::scene::systems {

void updateParticles(Scene& ioScene, const SystemContext& iContext) {
	advanceEffects(ioScene, iContext.timeStep.getSeconds(), false);
}

void advanceEffects(Scene& ioScene, const float iSeconds, const bool iEditorMode) {
	OWL_PROFILE_FUNCTION()

	for (const auto view = ioScene.registry.view<component::ParticleEmitter, component::Transform>();
		 const auto handle: view) {
		auto& emitter = view.get<component::ParticleEmitter>(handle);
		auto& system = emitter.m_slot.system;
		if (!system) {
			system = renderer::RendererParticles::createSystem();
			if (iEditorMode || emitter.playOnStart)
				system->play();
		}
		const Entity entity{handle, &ioScene};
		system->advance(emitter.settings, iSeconds, ioScene.getWorldTransform(entity)());
	}
	for (const auto view = ioScene.registry.view<component::Trail, component::Transform>(); const auto handle: view) {
		const Entity entity{handle, &ioScene};
		view.get<component::Trail>(handle).record(ioScene.getWorldTransform(entity).translation(), iSeconds);
	}
}

}// namespace owl::scene::systems
