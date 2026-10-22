/**
 * @file Light2D_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/ComponentRegistry.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <string>

using namespace owl;

namespace {
class Light2DTest : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
	static void TearDownTestSuite() { core::Log::invalidate(); }
};

// The entity read back into a new scene, which the caller keeps alive while it uses the entity.
auto roundTrip(const scene::Entity& iEntity, const shared<scene::Scene>& ioCopy) -> scene::Entity {
	const auto yaml = scene::SceneSerializer::serializeEntityToString(iEntity);
	EXPECT_TRUE(scene::SceneSerializer::deserializeEntityFromString(ioCopy, yaml));
	return ioCopy->findEntityByUUID(iEntity.getUUID());
}
}// namespace

TEST_F(Light2DTest, serializationRoundTripsEveryField) {
	const auto sc = mkShared<scene::Scene>();
	auto torch = sc->createEntity("Torch");
	auto& light = torch.addComponent<scene::component::Light2D>();
	light.type = scene::component::Light2D::Type::Spot;
	light.enabled = false;
	light.color = {0.25f, 0.5f, 0.75f};
	light.intensity = 2.5f;
	light.radius = 7.f;
	light.falloff = 1.5f;
	light.height = 0.4f;
	light.innerAngle = 0.2f;
	light.outerAngle = 0.6f;
	light.castShadows = true;
	light.shadowSoftness = 2.f;
	const auto copy = mkShared<scene::Scene>();
	const auto restored = roundTrip(torch, copy);
	ASSERT_TRUE(restored);
	ASSERT_TRUE(restored.hasComponent<scene::component::Light2D>());
	const auto& read = restored.getComponent<scene::component::Light2D>();
	EXPECT_EQ(read.type, scene::component::Light2D::Type::Spot);
	EXPECT_FALSE(read.enabled);
	EXPECT_EQ(read.color, (math::vec3{0.25f, 0.5f, 0.75f}));
	EXPECT_FLOAT_EQ(read.intensity, 2.5f);
	EXPECT_FLOAT_EQ(read.radius, 7.f);
	EXPECT_FLOAT_EQ(read.falloff, 1.5f);
	EXPECT_FLOAT_EQ(read.height, 0.4f);
	EXPECT_FLOAT_EQ(read.innerAngle, 0.2f);
	EXPECT_FLOAT_EQ(read.outerAngle, 0.6f);
	EXPECT_TRUE(read.castShadows);
	EXPECT_FLOAT_EQ(read.shadowSoftness, 2.f);
}

TEST_F(Light2DTest, ambientTypeRoundTrips) {
	const auto sc = mkShared<scene::Scene>();
	auto night = sc->createEntity("Night");
	night.addComponent<scene::component::Light2D>().type = scene::component::Light2D::Type::Ambient;
	const auto copy = mkShared<scene::Scene>();
	const auto restored = roundTrip(night, copy);
	ASSERT_TRUE(restored);
	EXPECT_EQ(restored.getComponent<scene::component::Light2D>().type, scene::component::Light2D::Type::Ambient);
}

TEST_F(Light2DTest, isAnOptionalComponentOfTheRegistry) {
	const auto* desc = scene::ComponentRegistry::find("Light2D");
	ASSERT_NE(desc, nullptr);
	EXPECT_TRUE(desc->optional);
}

TEST_F(Light2DTest, spriteLightingFieldsRoundTripAndDefaultToLitWithoutShadow) {
	const auto sc = mkShared<scene::Scene>();
	auto wall = sc->createEntity("Wall");
	auto& sprite = wall.addComponent<scene::component::SpriteRenderer>();
	EXPECT_TRUE(sprite.lit);
	EXPECT_FALSE(sprite.castShadows);
	sprite.castShadows = true;
	sprite.lit = false;
	const auto copy = mkShared<scene::Scene>();
	const auto restored = roundTrip(wall, copy);
	ASSERT_TRUE(restored);
	const auto& read = restored.getComponent<scene::component::SpriteRenderer>();
	EXPECT_TRUE(read.castShadows);
	EXPECT_FALSE(read.lit);
	EXPECT_EQ(read.normalMap, nullptr);
}
