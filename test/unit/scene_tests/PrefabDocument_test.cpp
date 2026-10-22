/**
 * @file PrefabDocument_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/PrefabSerializer.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <chrono>
#include <filesystem>
#include <string>

using namespace owl;
using namespace owl::scene;

namespace {

class PrefabDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() /
				("owl_prefab_document_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(m_dir);
	}

	void TearDown() override {
		std::error_code error;
		std::filesystem::remove_all(m_dir, error);
		core::Log::invalidate();
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(PrefabDocumentTest, APrefabFileOpensAsADocumentKeepingItsUuidsNameAndVersion) {
	auto source = mkShared<Scene>();
	auto root = source->createEntity("Door");
	auto leaf = source->createEntity("Handle");
	source->setParent(leaf, root);
	const auto path = m_dir / "door.owlprefab";
	ASSERT_TRUE(PrefabSerializer::serialize(root, *source, path, "Wooden door"));
	EXPECT_EQ(documentKindOf(path), DocumentKind::Prefab);

	auto document = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(document, documentKindOf(path)).deserialize(path));
	EXPECT_EQ(document->getDocumentKind(), DocumentKind::Prefab);
	EXPECT_EQ(document->getPrefabName(), "Wooden door");
	ASSERT_EQ(document->getAllEntities().size(), 2u);
	EXPECT_TRUE(document->findEntityByUUID(root.getUUID()));
	EXPECT_TRUE(document->findEntityByUUID(leaf.getUUID()));
}

TEST_F(PrefabDocumentTest, AnEditedPrefabDocumentSavesBackAsAPrefabThatInstantiates) {
	auto source = mkShared<Scene>();
	auto root = source->createEntity("Lamp");
	const auto path = m_dir / "lamp.owlprefab";
	ASSERT_TRUE(PrefabSerializer::serialize(root, *source, path, "Street lamp"));
	auto document = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(document, documentKindOf(path)).deserialize(path));
	auto lamp = document->findEntityByUUID(root.getUUID());
	ASSERT_TRUE(lamp);
	lamp.getComponent<component::Transform>().transform.translation() = {1.f, 2.f, 3.f};
	auto bulb = document->createEntity("Bulb");
	document->setParent(bulb, lamp);

	const auto saved = m_dir / "saved.owlprefab";
	ASSERT_TRUE(SceneSerializer(document).serialize(saved));
	const auto info = PrefabSerializer::readInfo(saved);
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->name, "Street lamp");
	EXPECT_EQ(info->entityCount, 2u);

	auto target = mkShared<Scene>();
	const auto instance = PrefabSerializer::instantiate(saved, target, "prefabs/lamp.owlprefab");
	ASSERT_TRUE(instance);
	EXPECT_FLOAT_EQ(instance.getComponent<component::Transform>().transform.translation().y(), 2.f);
	EXPECT_EQ(target->getAllEntities().size(), 2u);
}
