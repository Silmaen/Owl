/**
 * @file PhysicalLightMigration_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <renderer/Renderer3D.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/Environment.h>
#include <scene/component/Light.h>
#include <scene/component/Tag.h>

#include <cstdint>
#include <numbers>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

class PhysicalLightMigrationTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

auto load(const std::string& iYaml) -> shared<Scene> {
	auto scene = mkShared<Scene>();
	const std::vector<uint8_t> bytes(iYaml.begin(), iYaml.end());
	if (!SceneSerializer(scene).deserializeFromBuffer(bytes, "test"))
		return nullptr;
	return scene;
}

auto intensityOf(const Scene& iScene, const std::string& iName) -> float {
	for (const auto& entity: iScene.getAllEntities()) {
		if (entity.getName() != iName)
			continue;
		if (entity.hasComponent<component::Light>())
			return entity.getComponent<component::Light>().intensity;
		return entity.getComponent<component::Environment>().intensity;
	}
	return -1.f;
}

// What a factor of 1 gave before, divided by the exposure of the default EV100.
const float g_factor = 1.f / renderer::Renderer3D::exposureOf(renderer::Renderer3D::g_defaultEv100);
constexpr float g_pi = std::numbers::pi_v<float>;

}// namespace

TEST_F(PhysicalLightMigrationTest, oldFactorsBecomePhysicalValuesThatLookTheSame) {
	const auto scene = load("Scene: legacy\nFormatVersion: 5\nEntities:\n"
							"  - Entity: 1\n    Tag: {tag: Sun}\n    Light: {type: Directional, intensity: 2}\n"
							"  - Entity: 2\n    Tag: {tag: Bulb}\n    Light: {type: Point, intensity: 1}\n"
							"  - Entity: 3\n    Tag: {tag: Spot}\n    Light: {type: Spot}\n"
							"  - Entity: 4\n    Tag: {tag: Sky}\n    Environment: {intensity: 0.5}\n");
	ASSERT_NE(scene, nullptr);
	EXPECT_NEAR(intensityOf(*scene, "Sun"), 2.f * g_pi * g_factor, 1.f);
	EXPECT_NEAR(intensityOf(*scene, "Bulb"), 4.f * g_pi * g_pi * g_factor, 10.f);
	EXPECT_NEAR(intensityOf(*scene, "Spot"), g_pi * g_pi * g_factor, 10.f);
	EXPECT_NEAR(intensityOf(*scene, "Sky"), 0.5f * g_factor, 1.f);
}

TEST_F(PhysicalLightMigrationTest, aCurrentSceneKeepsItsValues) {
	const auto scene = load("Scene: current\nFormatVersion: 6\nEntities:\n"
							"  - Entity: 1\n    Tag: {tag: Sun}\n    Light: {type: Directional, intensity: 400}\n"
							"  - Entity: 2\n    Tag: {tag: Sky}\n    Environment: {intensity: 30}\n");
	ASSERT_NE(scene, nullptr);
	EXPECT_FLOAT_EQ(intensityOf(*scene, "Sun"), 400.f);
	EXPECT_FLOAT_EQ(intensityOf(*scene, "Sky"), 30.f);
}
