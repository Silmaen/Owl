/**
 * @file WorldHash_test.cpp
 * @author Silmaen
 * @date 03/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <math/RandomStream.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/WorldHash.h>
#include <scene/component/components.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

class WorldHashTest : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }

	/// A scene of two entities with fixed UUIDs and a seed.
	[[nodiscard]] static auto makeScene(const uint64_t iSeed) -> shared<Scene> {
		auto scene = mkShared<Scene>();
		scene->setRandomSeed(iSeed);
		auto crate = scene->createEntityWithUUID(core::UUID{11}, "Crate");
		crate.getComponent<component::Transform>().transform.translation() = {1.f, 2.f, 0.f};
		std::ignore = scene->createEntityWithUUID(core::UUID{12}, "Lamp");
		return scene;
	}

	/// UUIDs of the entities a Play session of the scene creates.
	[[nodiscard]] static auto spawnInPlay(const shared<Scene>& ioScene) -> std::vector<uint64_t> {
		ioScene->onStartRuntime();
		std::vector<uint64_t> uuids;
		for (int i = 0; i < 3; ++i) uuids.push_back(static_cast<uint64_t>(ioScene->createEntity("Spark").getUUID()));
		return uuids;
	}
};

}// namespace

TEST_F(WorldHashTest, SameStateGivesSameHash) {
	const auto first = makeScene(5);
	const auto second = makeScene(5);
	EXPECT_EQ(computeWorldHash(*first), computeWorldHash(*second));
}

TEST_F(WorldHashTest, ChangesWithOneBitOfAFloatTheSeedOrAStream) {
	const auto reference = makeScene(5);
	const auto base = computeWorldHash(*reference);
	auto moved = makeScene(5);
	auto& translation =
			moved->findEntityByUUID(core::UUID{11}).getComponent<component::Transform>().transform.translation();
	translation.x() = std::nextafter(translation.x(), 2.f);
	EXPECT_NE(computeWorldHash(*moved), base);
	EXPECT_NE(computeWorldHash(*makeScene(6)), base);
	const auto drawn = makeScene(5);
	std::ignore = drawn->getRandomStream("lua").nextU64();
	EXPECT_NE(computeWorldHash(*drawn), base);
}

TEST_F(WorldHashTest, PlaySessionsOfTheSameSeedCreateTheSameEntities) {
	const auto first = makeScene(5);
	const auto second = makeScene(5);
	const auto uuids = spawnInPlay(first);
	EXPECT_EQ(uuids, spawnInPlay(second));
	EXPECT_EQ(computeWorldHash(*first), computeWorldHash(*second));
	const auto other = makeScene(9);
	EXPECT_NE(uuids, spawnInPlay(other));
	first->onEndRuntime();
	second->onEndRuntime();
	other->onEndRuntime();
}

TEST_F(WorldHashTest, StartingThePlayRestartsTheStreams) {
	const auto scene = makeScene(5);
	std::ignore = scene->getRandomStream("lua").nextU64();
	scene->onStartRuntime();
	EXPECT_TRUE(scene->getRandomStreams().empty());
	EXPECT_EQ(scene->getRandomStream("lua"), (math::RandomStream{5, "lua"}));
	scene->onEndRuntime();
}

TEST_F(WorldHashTest, SeedRoundTripsThroughTheSceneFile) {
	const auto scene = makeScene(1234567890123ull);
	const auto text = SceneSerializer(scene).serializeToString();
	EXPECT_NE(text.find("Simulation"), std::string::npos);
	const auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer(std::vector<uint8_t>(text.begin(), text.end())));
	EXPECT_EQ(loaded->getRandomSeed(), 1234567890123ull);
	EXPECT_EQ(computeWorldHash(*loaded), computeWorldHash(*scene));
	const auto unseeded = makeScene(0);
	EXPECT_EQ(SceneSerializer(unseeded).serializeToString().find("Simulation"), std::string::npos);
}

TEST_F(WorldHashTest, CopyKeepsTheOrderOfTheEntitiesAndTheirComponents) {
	const auto source = makeScene(5);
	for (uint64_t id = 20; id < 26; ++id) {
		auto entity = source->createEntityWithUUID(core::UUID{id}, "Box");
		if (id % 2 == 0)
			entity.addComponent<component::CircleRenderer>();
	}
	const auto order = [](const Scene& iScene) -> std::vector<uint64_t> {
		std::vector<uint64_t> ids;
		for (const auto view = iScene.registry.view<component::ID>(); const auto entity: view)
			ids.push_back(static_cast<uint64_t>(view.get<component::ID>(entity).id));
		for (const auto view = iScene.registry.view<component::ID, component::CircleRenderer>();
			 const auto entity: view)
			ids.push_back(static_cast<uint64_t>(view.get<component::ID>(entity).id));
		return ids;
	};
	const auto copy = Scene::copy(source);
	EXPECT_EQ(order(*copy), order(*source));
	EXPECT_EQ(order(*Scene::copy(copy)), order(*source));
}
