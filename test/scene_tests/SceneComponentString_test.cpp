/**
 * @file SceneComponentString_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <string>

using namespace owl;
using namespace owl::scene;
using namespace owl::scene::component;

TEST(SceneComponentString, SerializesOneComponent) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto entity = sc.createEntity("E");
	entity.addComponent<CircleRenderer>().thickness = 0.25f;
	const auto yaml = SceneSerializer::serializeComponentToString(entity, CircleRenderer::key());
	EXPECT_TRUE(yaml.starts_with("CircleRenderer:"));
	EXPECT_EQ(yaml.find("Transform"), std::string::npos);
	EXPECT_NE(yaml.find("0.25"), std::string::npos);
	EXPECT_EQ(yaml, SceneSerializer::serializeComponentToString(entity, CircleRenderer::key()));
	entity.getComponent<CircleRenderer>().thickness = 0.5f;
	EXPECT_NE(yaml, SceneSerializer::serializeComponentToString(entity, CircleRenderer::key()));
	EXPECT_TRUE(SceneSerializer::serializeComponentToString(entity, Camera::key()).empty());
	EXPECT_TRUE(SceneSerializer::serializeComponentToString(entity, "NotAComponent").empty());
	EXPECT_TRUE(SceneSerializer::serializeComponentToString(Entity{}, Transform::key()).empty());
	core::Log::invalidate();
}

TEST(SceneComponentString, ReplaceRebuildsThePreviousEntityState) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto entity = sc.createEntity("E");
	entity.addComponent<CircleRenderer>().thickness = 0.25f;
	const auto beforeEntity = SceneSerializer::serializeEntityToString(entity);
	const auto beforeComponent = SceneSerializer::serializeComponentToString(entity, CircleRenderer::key());
	entity.getComponent<CircleRenderer>().thickness = 0.75f;
	const auto afterEntity = SceneSerializer::serializeEntityToString(entity);
	const auto rebuilt = SceneSerializer::replaceComponentInString(afterEntity, CircleRenderer::key(), beforeComponent);
	ASSERT_FALSE(rebuilt.empty());
	ASSERT_TRUE(SceneSerializer::applyEntityFromString(entity, rebuilt));
	EXPECT_FLOAT_EQ(entity.getComponent<CircleRenderer>().thickness, 0.25f);
	EXPECT_EQ(SceneSerializer::serializeEntityToString(entity), beforeEntity);

	const auto removed = SceneSerializer::replaceComponentInString(afterEntity, CircleRenderer::key(), "");
	ASSERT_TRUE(SceneSerializer::applyEntityFromString(entity, removed));
	EXPECT_FALSE(entity.hasComponent<CircleRenderer>());
	core::Log::invalidate();
}

TEST(SceneComponentString, ReplaceRejectsInvalidData) {
	core::Log::init(core::Log::Level::Off);
	EXPECT_TRUE(SceneSerializer::replaceComponentInString("- not a map", "Transform", "").empty());
	EXPECT_TRUE(SceneSerializer::replaceComponentInString("Entity: 1", "Transform", "Camera: {}").empty());
	EXPECT_TRUE(SceneSerializer::replaceComponentInString("Entity: [1", "Transform", "").empty());
	core::Log::invalidate();
}
