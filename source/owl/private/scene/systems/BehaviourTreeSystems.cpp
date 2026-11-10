/**
 * @file BehaviourTreeSystems.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"
#include "app/EngineContext.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/ai/BehaviourTreeInstance.h"
#include "scene/component/components.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <tuple>
#include <vector>

namespace owl::scene::systems {

namespace {

auto loadTree(const Scene& iScene, const std::string& iAsset) -> shared<const ai::BehaviourTree> {
	if (iAsset.empty())
		return nullptr;
	if (auto* context = iScene.getEngineContext(); context != nullptr)
		return context->getBehaviourTrees().get(iAsset);
	auto loaded = ai::loadAsset(iAsset);
	return loaded ? mkShared<const ai::BehaviourTree>(std::move(*loaded)) : nullptr;
}

}// namespace

void tickBehaviourTrees(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const float seconds = iContext.timeStep.getSeconds();
	std::vector<std::tuple<uint64_t, entt::entity>> sorted;
	for (const auto handle: ioScene.registry.view<component::BehaviourTreeAgent, component::ID>())
		sorted.emplace_back(static_cast<uint64_t>(ioScene.registry.get<component::ID>(handle).id), handle);
	if (sorted.empty())
		return;
	std::ranges::sort(sorted);
	for (const auto& [uuid, handle]: sorted) {
		const Entity entity{handle, &ioScene};
		// A tree may destroy or disable an entity: read the component again each time.
		if (!ioScene.registry.valid(handle) || ioScene.isPendingDestructionInTree(entity))
			continue;
		auto& agent = entity.getComponent<component::BehaviourTreeAgent>();
		if (!agent.enabled || isSimulationDemoted(ioScene, handle) ||
			!ioScene.isEffectivelyVisible(entity, /*iEditorMode=*/false)) {
			if (agent.m_instance && agent.m_instance->getStatus() != ai::NodeStatus::Idle)
				agent.m_instance->halt(entity);
			continue;
		}
		if (!agent.m_instance || agent.m_owner != entity.getUUID()) {
			auto tree = loadTree(ioScene, agent.tree);
			if (!tree) {
				OWL_CORE_WARN_ONCE("BehaviourTreeAgent: '{}' cannot read the tree '{}'.", entity.getName(), agent.tree)
				continue;
			}
			agent.m_instance = mkShared<ai::BehaviourTreeInstance>(std::move(tree), std::span{agent.overrides});
			agent.m_owner = entity.getUUID();
		}
		// Keep the instance alive through the tick: a Lua leaf may change the tree of its own agent.
		const auto instance = agent.m_instance;
		std::ignore = instance->tick(entity, seconds);
	}
}

}// namespace owl::scene::systems
