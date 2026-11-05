/**
 * @file WorldHash.cpp
 * @author Silmaen
 * @date 03/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/WorldHash.h"

#include "core/Serializer.h"
#include "core/SerializerImpl.h"
#include "math/RandomStream.h"
#include "physics/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"
#include "scene/component/ID.h"
#include "scene/component/PhysicBody.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::scene {

namespace {

class Fnv1a final {
public:
	void add(const std::string_view iText) {
		for (const char character: iText) addByte(static_cast<uint8_t>(character));
		addByte(0);
	}

	void add(const uint64_t iValue) {
		for (uint32_t shift = 0; shift < 64; shift += 8) addByte(static_cast<uint8_t>(iValue >> shift));
	}

	void add(const float iValue) { add(static_cast<uint64_t>(std::bit_cast<uint32_t>(iValue))); }

	[[nodiscard]] auto get() const -> uint64_t { return m_hash; }

private:
	void addByte(const uint8_t iByte) { m_hash = (m_hash ^ iByte) * 0x100000001b3ull; }

	uint64_t m_hash = 0xcbf29ce484222325ull;
};

}// namespace

auto computeWorldHash(const Scene& iScene) -> uint64_t {
	OWL_PROFILE_FUNCTION()

	Fnv1a hash;
	hash.add(iScene.getSimulationClock().getTotalTickCount());
	hash.add(iScene.getRandomSeed());
	for (const auto& [name, stream]: iScene.getRandomStreams()) {
		hash.add(name);
		for (const uint64_t word: stream.getState()) hash.add(word);
	}
	{
		const core::Serializer gameState;
		iScene.getGameState().serialize(gameState);
		hash.add(std::string_view{gameState.getImpl()->emitter.c_str()});
	}
	// Serialization only reads the scene; the entities need a mutable owner pointer.
	auto* scene = const_cast<Scene*>(&iScene);
	std::vector<std::pair<uint64_t, entt::entity>> entities;
	for (const auto view = iScene.registry.view<component::ID>(); const auto handle: view)
		entities.emplace_back(static_cast<uint64_t>(view.get<component::ID>(handle).id), handle);
	std::ranges::sort(entities);
	for (const auto& handle: entities | std::views::values) {
		const Entity entity{handle, scene};
		hash.add(SceneSerializer::serializeEntityToString(entity));
		if (entity.hasComponent<component::PhysicBody>()) {
			const auto snapshot = physics::PhysicCommand::getSnapshot(entity);
			hash.add(snapshot.linearVelocity.x());
			hash.add(snapshot.linearVelocity.y());
			hash.add(snapshot.angularVelocity.z());
			if (entity.getComponent<component::PhysicBody>().body.dimension == SceneBody::Dimension::Space) {
				hash.add(snapshot.linearVelocity.z());
				hash.add(snapshot.angularVelocity.x());
				hash.add(snapshot.angularVelocity.y());
			}
			hash.add(static_cast<uint64_t>(snapshot.awake ? 1 : 0));
		}
	}
	return hash.get();
}

}// namespace owl::scene
