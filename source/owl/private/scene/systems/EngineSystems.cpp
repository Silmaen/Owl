/**
 * @file EngineSystems.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"

#include "app/Application.h"
#include "math/box.h"
#include "physics/PhysicCommand.h"
#include "renderer/Camera3DController.h"
#include "renderer/Renderer2D.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"
#include "scene/nav/NavWorld.h"
#include "script/ScriptInstance.h"
#include "sound/SoundCommand.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace owl::scene::systems {

namespace {

auto getColliderBox(const Entity& iEntity, const math::Transform& iWorldTransform) -> math::box2f {
	auto halfDiag = math::vec2f{iWorldTransform.scale().x() * 0.5f, iWorldTransform.scale().y() * 0.5f};
	if (iEntity.hasComponent<component::PhysicBody>()) {
		auto& [body] = iEntity.getComponent<component::PhysicBody>();
		halfDiag.x() *= body.colliderSize.x();
		halfDiag.y() *= body.colliderSize.y();
	}
	const math::vec2f center = {iWorldTransform.translation().x(), iWorldTransform.translation().y()};
	return {center - halfDiag, center + halfDiag};
}

// The body of a 3D player in world space: the box of its controller around the eye, or its scale.
auto getBodyBox(const Entity& iEntity, const math::Transform& iWorldTransform) -> math::box3f {
	if (iEntity.hasComponent<component::FirstPersonController>()) {
		const auto& walker = iEntity.getComponent<component::FirstPersonController>();
		const math::vec3 centre = iWorldTransform.translation() + walker.getBodyOffset();
		return {centre - walker.getHalfExtents(), centre + walker.getHalfExtents()};
	}
	if (iEntity.hasComponent<component::VoxelPlayer>()) {
		const auto& half = iEntity.getComponent<component::VoxelPlayer>().halfExtents;
		return {iWorldTransform.translation() - half, iWorldTransform.translation() + half};
	}
	const math::vec3 half = iWorldTransform.scale() * 0.5f;
	return {iWorldTransform.translation() - half, iWorldTransform.translation() + half};
}

auto isWalker(const Entity& iEntity) -> bool {
	return iEntity.hasComponent<component::FirstPersonController>() || iEntity.hasComponent<component::VoxelPlayer>();
}

// The player the triggers test: the primary `Player`, else the first entity walking in 3D.
auto getTriggerPlayer(Scene& ioScene) -> Entity {
	if (const Entity player = ioScene.getPrimaryPlayer(); player)
		return player;
	if (const auto walkers = ioScene.registry.view<component::FirstPersonController>();
		walkers.begin() != walkers.end())
		return Entity{*walkers.begin(), &ioScene};
	if (const auto voxels = ioScene.registry.view<component::VoxelPlayer>(); voxels.begin() != voxels.end())
		return Entity{*voxels.begin(), &ioScene};
	return {};
}

// Whether the player reaches the trigger: aimed at (first-person interaction), body box in 3D, else 2D collider.
auto isTouching(Scene& ioScene, const Entity& iEntity, const Entity& iPlayer, const SceneTrigger::TriggerType iType)
		-> bool {
	if (iType == SceneTrigger::TriggerType::Interaction && iPlayer.hasComponent<component::FirstPersonController>())
		return iPlayer.getComponent<component::FirstPersonController>().m_target == iEntity.getUUID();
	if (isWalker(iPlayer))
		return getBodyBox(iEntity, ioScene.getWorldTransform(iEntity))
				.intersect(getBodyBox(iPlayer, ioScene.getWorldTransform(iPlayer)));
	return getColliderBox(iEntity, ioScene.getWorldTransform(iEntity))
			.intersect(getColliderBox(iPlayer, ioScene.getWorldTransform(iPlayer)));
}

void updateAnimatedSprite(component::AnimatedSpriteRenderer& ioAnim, const core::Timestep& iTimeStep) {
	if (!ioAnim.m_playing || ioAnim.columns == 0 || ioAnim.rows == 0 || ioAnim.frameDuration <= 0.0f)
		return;
	const uint32_t totalFrames = ioAnim.lastFrame >= ioAnim.firstFrame ? ioAnim.lastFrame - ioAnim.firstFrame + 1 : 1;
	float deltaSeconds = iTimeStep.getSeconds();
	if (!ioAnim.speedCurve.empty()) {
		const float progress = totalFrames > 1 ? static_cast<float>(ioAnim.m_currentFrame - ioAnim.firstFrame) /
														 static_cast<float>(totalFrames - 1)
											   : 0.f;
		deltaSeconds *= ioAnim.speedCurve.evaluate(progress);
	}
	ioAnim.m_elapsedTime += deltaSeconds;
	if (ioAnim.m_elapsedTime < ioAnim.frameDuration)
		return;
	const auto framesToAdvance = static_cast<uint32_t>(ioAnim.m_elapsedTime / ioAnim.frameDuration);
	ioAnim.m_elapsedTime -= static_cast<float>(framesToAdvance) * ioAnim.frameDuration;
	if (ioAnim.loop) {
		ioAnim.m_currentFrame =
				ioAnim.firstFrame + (ioAnim.m_currentFrame - ioAnim.firstFrame + framesToAdvance) % totalFrames;
		return;
	}
	const uint32_t newFrame = ioAnim.m_currentFrame + framesToAdvance;
	ioAnim.m_currentFrame = std::min(newFrame, ioAnim.lastFrame);
	if (ioAnim.m_currentFrame >= ioAnim.lastFrame)
		ioAnim.m_playing = false;
}

void updateTrigger(Scene& ioScene, const Entity& iEntity, const Entity& iPlayer, const float iSeconds) {
	auto& trigger = iEntity.getComponent<component::Trigger>().trigger;
	if (!ioScene.isEffectivelyVisible(iEntity, /*iEditorMode=*/false)) {
		// Hidden trigger: cancel any in-progress timer and clear overlap state.
		if (trigger.type == SceneTrigger::TriggerType::Timer)
			trigger.stopTimer();
		if (trigger.wasOverlapping() && iPlayer)
			trigger.onTriggerExit(iPlayer, iEntity);
		trigger.setOverlapping(false);
		return;
	}
	// Timer triggers: update independently of overlap.
	if (trigger.type == SceneTrigger::TriggerType::Timer) {
		trigger.updateTimer(iSeconds, iEntity);
		return;
	}
	if (!iPlayer)
		return;
	const bool overlapping = isTouching(ioScene, iEntity, iPlayer, trigger.type);
	if (overlapping && !trigger.wasOverlapping())
		trigger.onTriggerEnter(iPlayer, iEntity);
	if (overlapping)
		trigger.onTriggered(iPlayer, iEntity);
	if (!overlapping && trigger.wasOverlapping())
		trigger.onTriggerExit(iPlayer, iEntity);
	trigger.setOverlapping(overlapping);
}

}// namespace

void registerEngineSystems(SystemSchedule& ioSchedule) {
	const auto add = [&ioSchedule](const char* iName, const SystemPhase iPhase, SystemFunction&& iUpdate,
								   SystemFunction&& iTick = nullptr) -> void {
		std::ignore = ioSchedule.add(
				{.name = iName, .phase = iPhase, .update = std::move(iUpdate), .tick = std::move(iTick)});
	};
	add("owl.physics_poses", SystemPhase::Scripts, &restorePhysicsPoses);
	add("owl.scripts", SystemPhase::Scripts, &updateScripts, &tickScripts);
	add("owl.oscillators", SystemPhase::PrePhysics, &updateOscillators, &tickOscillators);
	add("owl.patrols", SystemPhase::PrePhysics, &updatePatrols);
	add("owl.fly_cameras", SystemPhase::PrePhysics, &updateFlyCameras);
	add("owl.voxel_players", SystemPhase::PrePhysics, &updateVoxelPlayers);
	add("owl.first_person", SystemPhase::PrePhysics, &updateFirstPersonControllers);
	add("owl.raycast_walls", SystemPhase::PrePhysics, [](Scene& ioScene, const SystemContext& iContext) -> void {
		updateRaycastDynamicWalls(ioScene, iContext.timeStep.getSeconds());
	});
	add("owl.player_input", SystemPhase::PrePhysics, &updatePlayerInput);
	add("owl.physics", SystemPhase::Physics, &updatePhysics, &tickPhysics);
	add("owl.drift", SystemPhase::PostPhysics, &updateDrifts);
	add("owl.follow_target", SystemPhase::PostPhysics, &updateFollowTargets);
	add("owl.animation", SystemPhase::PostPhysics, [](Scene& ioScene, const SystemContext& iContext) -> void {
		ioScene.updateAnimators(iContext.timeStep.getSeconds(), false);
	});
	add("owl.sockets", SystemPhase::PostPhysics,
		[](Scene& ioScene, [[maybe_unused]] const SystemContext& iContext) -> void {
			ioScene.updateSocketAttachments();
		});
	add("owl.day_night", SystemPhase::PostPhysics, &updateDayNightCycles);
	add("owl.light_flicker", SystemPhase::PostPhysics, &updateLightFlickers);
	add("owl.entity_links", SystemPhase::PostPhysics, &updateEntityLinks);
	add("owl.triggers", SystemPhase::PostPhysics, &updateTriggers);
	add("owl.floating_origin", SystemPhase::PostPhysics, &updateFloatingOrigin);
	add("owl.particles", SystemPhase::Late, &updateParticles);
	add("owl.navigation", SystemPhase::Late, &updateNavigation);
	add("owl.sound", SystemPhase::Late, &updateSound);
	add("owl.sprite_animation", SystemPhase::Late, &updateAnimatedSprites);
	add("owl.game_over", SystemPhase::Ended, &renderGameOver);
}

void restorePhysicsPoses(Scene& ioScene, [[maybe_unused]] const SystemContext& iContext) {
	if (physics::PhysicCommand::getInterpolationAlpha(ioScene) < 1.f)
		physics::PhysicCommand::syncSimulatedTransforms(ioScene);
}

void updateScripts(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	for (const auto* script: {&std::as_const(ioScene).getSceneScript(), &ioScene.getHudScript()})
		if (script->instance && script->instance->isValid())
			script->instance->onUpdate(iContext.timeStep.getSeconds());
	ioScene.registry.view<component::NativeScript>().each(
			[&ioScene, &iContext](const entt::entity iEntity, component::NativeScript& ioNsc) -> void {
				if (!ioScene.isEffectivelyVisible(Entity{iEntity, &ioScene}, /*iEditorMode=*/false))
					return;
				if (!ioNsc.instance) {
					ioNsc.instance = ioNsc.instantiateScript();
					ioNsc.instance->entity = Entity{iEntity, &ioScene};
					ioNsc.instance->onCreate();
				}
				ioNsc.instance->onUpdate(iContext.timeStep);
			});
	for (const auto view = ioScene.registry.view<component::LuaScript>(); const auto entity: view) {
		if (!ioScene.isEffectivelyVisible(Entity{entity, &ioScene}, /*iEditorMode=*/false))
			continue;
		if (const auto& luaScript = view.get<component::LuaScript>(entity);
			luaScript.instance && luaScript.instance->isValid())
			luaScript.instance->onUpdate(iContext.timeStep.getSeconds());
	}
}

void tickScripts(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	for (const auto* script: {&std::as_const(ioScene).getSceneScript(), &ioScene.getHudScript()})
		if (script->instance && script->instance->isValid())
			script->instance->onTick(iContext.timeStep.getSeconds());
	for (const auto view = ioScene.registry.view<component::NativeScript>(); const auto entity: view) {
		if (const auto& nsc = view.get<component::NativeScript>(entity);
			nsc.instance != nullptr && ioScene.isEffectivelyVisible(Entity{entity, &ioScene}, /*iEditorMode=*/false))
			nsc.instance->onTick(iContext.timeStep);
	}
	for (const auto view = ioScene.registry.view<component::LuaScript>(); const auto entity: view) {
		if (!ioScene.isEffectivelyVisible(Entity{entity, &ioScene}, /*iEditorMode=*/false))
			continue;
		if (const auto& luaScript = view.get<component::LuaScript>(entity);
			luaScript.instance && luaScript.instance->isValid())
			luaScript.instance->onTick(iContext.timeStep.getSeconds());
	}
}

void updateNavigation(Scene& ioScene, [[maybe_unused]] const SystemContext& iContext) {
	ioScene.getNavigation().update(ioScene);
}

void updateFlyCameras(Scene& ioScene, const SystemContext& iContext) {
	for (const auto view = ioScene.registry.view<component::Transform, component::FlyCamera>();
		 const auto entity: view) {
		auto [transform, fly] = view.get<component::Transform, component::FlyCamera>(entity);
		renderer::Camera3DController controller;
		controller.setMoveSpeed(fly.moveSpeed);
		controller.setLookSpeed(fly.lookSpeed);
		controller.setPosition(transform.transform.translation());
		controller.setEulerRotation(transform.transform.rotation());
		controller.onUpdate(iContext.timeStep);
		transform.transform.translation() = controller.getPosition();
		transform.transform.rotation() = controller.getEulerRotation();
	}
}

void updatePlayerInput(Scene& ioScene, [[maybe_unused]] const SystemContext& iContext) {
	if (const Entity player = ioScene.getPrimaryPlayer()) {
		auto& [primary, iplayer] = player.getComponent<component::Player>();
		iplayer.parseInputs(player, ioScene.getActions());
	}
}

void tickPhysics(Scene& ioScene, const SystemContext& iContext) {
	if (!physics::PhysicCommand::isInitialized(ioScene))
		return;
	physics::PhysicCommand::step(ioScene);
	if (iContext.tickIndex + 1 == iContext.tickCount)
		physics::PhysicCommand::writeFrameTransforms(ioScene);
	else
		physics::PhysicCommand::syncSimulatedTransforms(ioScene);
	ioScene.dispatchCollisionEvents();
}

void updatePhysics(Scene& ioScene, const SystemContext& iContext) {
	if (iContext.tickCount == 0 && physics::PhysicCommand::isInitialized(ioScene))
		physics::PhysicCommand::writeFrameTransforms(ioScene);
}

void updateTriggers(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const Entity player = getTriggerPlayer(ioScene);
	const float seconds = iContext.timeStep.getSeconds();
	for (const auto view = ioScene.registry.view<component::Trigger>(); const auto entity: view)
		updateTrigger(ioScene, Entity{entity, &ioScene}, player, seconds);
}

void updateSound(Scene& ioScene, [[maybe_unused]] const SystemContext& iContext) {
	for (const auto view = ioScene.registry.view<component::Transform, component::SoundListener>();
		 const auto entity: view) {
		if (const auto& [transform, listener] = view.get<component::Transform, component::SoundListener>(entity);
			listener.primary) {
			const auto wt = ioScene.getWorldTransform(Entity{entity, &ioScene});

			sound::SoundCommand::setListenerPosition(
					{wt.translation().x(), wt.translation().y(), wt.translation().z()});
			const float rotZ = wt.rotation().z();

			sound::SoundCommand::setListenerOrientation({std::sin(rotZ), std::cos(rotZ), 0.0f}, {0.0f, 0.0f, 1.0f});
			break;
		}
	}
	for (const auto view = ioScene.registry.view<component::Transform, component::SoundSource>();
		 const auto entity: view) {
		const auto& [soundComp] = view.get<component::SoundSource>(entity);
		if (soundComp.runtimeHandle == sound::invalidSoundHandle || !soundComp.spatial)
			continue;
		const auto wt = ioScene.getWorldTransform(Entity{entity, &ioScene});

		sound::SoundCommand::setPosition(soundComp.runtimeHandle,
										 {wt.translation().x(), wt.translation().y(), wt.translation().z()});
	}
}

void updateAnimatedSprites(Scene& ioScene, const SystemContext& iContext) {
	for (const auto view = ioScene.registry.view<component::AnimatedSpriteRenderer>(); const auto entity: view)
		updateAnimatedSprite(view.get<component::AnimatedSpriteRenderer>(entity), iContext.timeStep);
}

void renderGameOver(Scene& ioScene, const SystemContext& iContext) {
	if (!iContext.render || !app::Application::instanced())
		return;
	const Entity cameraEntity = ioScene.getPrimaryCamera();
	if (!cameraEntity)
		return;
	auto& camera = cameraEntity.getComponent<component::Camera>().camera;
	const math::Transform camTransform = ioScene.getWorldTransform(cameraEntity);
	math::Transform textTransform = camTransform;
	textTransform.translation().z() = 0;
	camera.setTransform(camTransform());

	renderer::Renderer2D::resetStats();

	renderer::Renderer2D::beginScene(camera);

	renderer::Renderer2D::drawString({.transform = textTransform,
									  .text = ioScene.status == Scene::Status::Victory ? "Victory!" : "You loose!",
									  .font = app::Application::get().getFontLibrary().getDefaultFont(),
									  .color = {1, 1, 1, 1},
									  .entityId = 0});

	renderer::Renderer2D::endScene();
}

}// namespace owl::scene::systems
