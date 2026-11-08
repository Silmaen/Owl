/**
 * @file SimulationLodSystem.cpp
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "EngineSystems.h"

#include "physics/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/ID.h"
#include "scene/component/LuaScript.h"
#include "scene/component/PhysicBody.h"
#include "scene/component/SimulationLod.h"
#include "scene/component/StreamingSource.h"
#include "script/ScriptInstance.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <ranges>
#include <utility>
#include <vector>

namespace owl::scene::systems {

namespace {

// The focus points, as the world streaming takes them: the enabled streaming sources, else the primary camera.
void gatherFocus(Scene& ioScene, std::vector<math::vec3d>& oFocus) {
	oFocus.clear();
	for (const auto [handle, source]: ioScene.registry.view<component::StreamingSource>().each())
		if (source.enabled)
			oFocus.push_back(ioScene.getEntityWorldPosition(Entity{handle, &ioScene}));
	if (oFocus.empty())
		if (const Entity camera = ioScene.getPrimaryCamera(); camera)
			oFocus.push_back(ioScene.getEntityWorldPosition(camera));
}

auto nearestFocusSq(const std::vector<math::vec3d>& iFocus, const math::vec3d& iPosition) -> double {
	double nearest = std::numeric_limits<double>::max();
	for (const auto& focus: iFocus) nearest = std::min(nearest, (iPosition - focus).normSq());
	return nearest;
}

void callScript(const Entity& iEntity, const std::string& iHook) {
	if (!iEntity.hasComponent<component::LuaScript>())
		return;
	if (const auto& script = iEntity.getComponent<component::LuaScript>();
		script.instance && script.instance->isValid())
		std::ignore = script.instance->callFunction(iHook);
}

auto hasDynamicBody(const Entity& iEntity) -> bool {
	return iEntity.hasComponent<component::PhysicBody>() &&
		   iEntity.getComponent<component::PhysicBody>().body.type == SceneBody::BodyType::Dynamic;
}

void demote(const Entity& iEntity, component::SimulationLod& ioLod) {
	ioLod.m_demoted = true;
	// Spread the coarse ticks of the demoted entities over the interval.
	ioLod.m_coarseCountdown =
			static_cast<uint32_t>(static_cast<uint64_t>(iEntity.getUUID()) % ioLod.coarseInterval) + 1;
	ioLod.m_velocity = {0.f, 0.f, 0.f};
	if (hasDynamicBody(iEntity)) {
		ioLod.m_velocity = physics::PhysicCommand::getVelocity3D(iEntity);
		physics::PhysicCommand::setSimulated(iEntity, false);
	}
	callScript(iEntity, "on_demoted");
}

void promote(const Entity& iEntity, component::SimulationLod& ioLod) {
	ioLod.m_demoted = false;
	if (hasDynamicBody(iEntity))
		physics::PhysicCommand::setSimulated(iEntity, true, ioLod.m_velocity);
	callScript(iEntity, "on_promoted");
}

void coarseTick(const Entity& iEntity, component::SimulationLod& ioLod, const float iTickSeconds) {
	if (--ioLod.m_coarseCountdown > 0)
		return;
	const uint32_t interval = std::clamp(ioLod.coarseInterval, 1u, component::SimulationLod::maxCoarseInterval);
	ioLod.m_coarseCountdown = interval;
	const float seconds = iTickSeconds * static_cast<float>(interval);
	if (ioLod.model == component::SimulationLod::Model::Rails && hasDynamicBody(iEntity))
		ioLod.m_velocity = physics::PhysicCommand::followRails(iEntity, ioLod.m_velocity, seconds);
	if (iEntity.hasComponent<component::LuaScript>())
		if (const auto& script = iEntity.getComponent<component::LuaScript>();
			script.instance && script.instance->isValid())
			script.instance->onCoarseTick(seconds);
}

}// namespace

auto isSimulationDemoted(const Scene& iScene, const entt::entity iEntity) -> bool {
	const auto* lod = iScene.registry.try_get<component::SimulationLod>(iEntity);
	return lod != nullptr && lod->m_demoted;
}

void tickSimulationLod(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const auto view = ioScene.registry.view<component::SimulationLod, component::ID>();
	if (view.begin() == view.end())
		return;
	// Kept between ticks: no allocation once grown.
	thread_local std::vector<math::vec3d> sFocus;
	thread_local std::vector<std::pair<uint64_t, entt::entity>> sOrder;
	gatherFocus(ioScene, sFocus);
	sOrder.clear();
	for (const auto [handle, lod, id]: view.each()) sOrder.emplace_back(static_cast<uint64_t>(id.id), handle);
	// UUID order: the hooks run in the same order on every run.
	std::ranges::sort(sOrder);
	const float tickSeconds = iContext.timeStep.getSeconds();
	for (const auto& handle: sOrder | std::views::values) {
		const Entity entity{handle, &ioScene};
		if (!ioScene.registry.valid(handle) || !entity.hasComponent<component::SimulationLod>())
			continue;
		auto& lod = entity.getComponent<component::SimulationLod>();
		bool far = false;
		if (lod.enabled && !sFocus.empty()) {
			const double distanceSq = nearestFocusSq(sFocus, ioScene.getEntityWorldPosition(entity));
			const double limit =
					static_cast<double>(lod.radius) + (lod.m_demoted ? 0.0 : static_cast<double>(lod.hysteresis));
			far = distanceSq > limit * limit;
		}
		if (far && !lod.m_demoted)
			demote(entity, lod);
		else if (!far && lod.m_demoted)
			promote(entity, lod);
		else if (lod.m_demoted)
			coarseTick(entity, lod, tickSeconds);
	}
}

}// namespace owl::scene::systems
