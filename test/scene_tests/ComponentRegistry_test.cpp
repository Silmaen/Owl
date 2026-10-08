/**
 * @file ComponentRegistry_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/SerializerImpl.h>
#include <scene/ComponentRegistry.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

/// A component a game would define, unknown to the engine.
struct Score {
	int64_t points = 0;
	static auto key() -> const char* { return "TestScore"; }
	static auto name() -> const char* { return "Test Score"; }
	void serialize(const core::Serializer& iOut) const {
		iOut.getImpl()->emitter << YAML::Key << key() << YAML::Value << YAML::BeginMap;
		iOut.getImpl()->emitter << YAML::Key << "points" << YAML::Value << points;
		iOut.getImpl()->emitter << YAML::EndMap;
	}
	void deserialize(const core::Serializer& iNode) {
		if (const auto node = iNode.getImpl()->node["points"]; node)
			points = node.as<int64_t>();
	}
};

class ComponentRegistryTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		ASSERT_TRUE(ComponentRegistry::registerComponent<Score>());
	}
	void TearDown() override {
		std::ignore = ComponentRegistry::unregisterComponent(Score::key());
		core::Log::invalidate();
	}
};

template<typename... Components>
auto keysOf(const std::tuple<Components...>& /*iList*/) -> std::vector<std::string> {
	return {Components::key()...};
}

}// namespace

TEST(ComponentRegistry, EngineComponentsComeFirstInSerializationOrder) {
	std::vector<std::string> keys;
	for (const auto& desc: ComponentRegistry::getAll())
		if (desc.builtin)
			keys.push_back(desc.key);
	EXPECT_EQ(keys, keysOf(component::SerializableComponents{}));
	const auto* tag = ComponentRegistry::find("Tag");
	ASSERT_NE(tag, nullptr);
	EXPECT_FALSE(tag->optional);
	EXPECT_FALSE(tag->copiable);
	const auto* transform = ComponentRegistry::find("Transform");
	ASSERT_NE(transform, nullptr);
	EXPECT_FALSE(transform->optional);
	EXPECT_TRUE(transform->copiable);
	const auto* circle = ComponentRegistry::find(component::CircleRenderer::name());
	ASSERT_NE(circle, nullptr);
	EXPECT_TRUE(circle->optional);
	EXPECT_EQ(circle->key, component::CircleRenderer::key());
}

TEST(ComponentRegistry, RejectsDuplicatesAndEngineRemoval) {
	core::Log::init(core::Log::Level::Off);
	EXPECT_FALSE(ComponentRegistry::registerDescriptor(ComponentRegistry::describe<component::CircleRenderer>()));
	EXPECT_FALSE(ComponentRegistry::registerDescriptor(ComponentDescriptor{}));
	EXPECT_FALSE(ComponentRegistry::unregisterComponent("Transform"));
	EXPECT_FALSE(ComponentRegistry::unregisterComponent("NoSuchComponent"));
	core::Log::invalidate();
}

TEST_F(ComponentRegistryTest, GameComponentIsSavedAndLoaded) {
	const auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Player");
	entity.addComponent<Score>().points = 42;
	const SceneSerializer serializer(scene);
	const auto yaml = serializer.serializeToString();
	EXPECT_NE(yaml.find("TestScore"), std::string::npos);
	const auto loaded = mkShared<Scene>();
	const SceneSerializer loader(loaded);
	const std::vector<uint8_t> buffer(yaml.begin(), yaml.end());
	ASSERT_TRUE(loader.deserializeFromBuffer(buffer).has_value());
	const auto copy = loaded->findEntityByUUID(entity.getUUID());
	ASSERT_TRUE(copy);
	ASSERT_TRUE(copy.hasComponent<Score>());
	EXPECT_EQ(copy.getComponent<Score>().points, 42);
}

TEST_F(ComponentRegistryTest, GameComponentIsCopiedAndDuplicated) {
	const auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Player");
	entity.addComponent<Score>().points = 7;
	const auto duplicate = scene->duplicateEntity(entity);
	ASSERT_TRUE(duplicate.hasComponent<Score>());
	EXPECT_EQ(duplicate.getComponent<Score>().points, 7);
	const auto playCopy = Scene::copy(scene);
	const auto inCopy = playCopy->findEntityByUUID(entity.getUUID());
	ASSERT_TRUE(inCopy.hasComponent<Score>());
	EXPECT_EQ(inCopy.getComponent<Score>().points, 7);
}

TEST_F(ComponentRegistryTest, EntityStringsCarryTheGameComponent) {
	const auto scene = mkShared<Scene>();
	auto entity = scene->createEntity("Player");
	entity.addComponent<Score>().points = 3;
	const auto withScore = SceneSerializer::serializeEntityToString(entity);
	EXPECT_NE(SceneSerializer::serializeComponentToString(entity, Score::key()).find("points: 3"), std::string::npos);
	entity.removeComponent<Score>();
	ASSERT_TRUE(SceneSerializer::applyEntityFromString(entity, withScore));
	ASSERT_TRUE(entity.hasComponent<Score>());
	EXPECT_EQ(entity.getComponent<Score>().points, 3);
	auto other = scene->createEntity("Other");
	const auto withoutScore = SceneSerializer::serializeEntityToString(other);
	other.addComponent<Score>();
	ASSERT_TRUE(SceneSerializer::applyEntityFromString(other, withoutScore));
	EXPECT_FALSE(other.hasComponent<Score>());
}

TEST_F(ComponentRegistryTest, DescriptorAddsAndRemoves) {
	const auto* desc = ComponentRegistry::find("Test Score");
	ASSERT_NE(desc, nullptr);
	EXPECT_FALSE(desc->builtin);
	Scene scene;
	auto entity = scene.createEntity("e");
	EXPECT_FALSE(desc->has(entity));
	desc->add(entity);
	EXPECT_TRUE(desc->has(entity));
	desc->remove(entity);
	EXPECT_FALSE(desc->has(entity));
	desc->remove(entity);
}
