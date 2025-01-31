
#include "testHelper.h"

#include <renderer/Texture.h>
#include <renderer/null/Texture.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

using namespace owl::scene;

TEST(SceneSerializer, SaveLoad) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto sc = owl::mkShared<Scene>();
	sc->createEntityWithUUID(5, "bobObject");
	const SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / "tempSave.yml";
	saver.serialize(fs);

	ASSERT_TRUE(exists(fs));
	const auto sc2 = owl::mkShared<Scene>();
	const SceneSerializer loader(sc2);
	EXPECT_TRUE(loader.deserialize(fs));

	EXPECT_EQ(sc2->getEntityCount(), sc->getEntityCount());
	remove(fs);
	EXPECT_FALSE(exists(fs));
	owl::core::Log::invalidate();
}

TEST(SceneSerializer, SaveLoadFULL) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto sc = owl::mkShared<Scene>();
	auto ent = sc->createEntityWithUUID(5, "bobObject");
	ent.addOrReplaceComponent<component::Camera>();
	ent.addOrReplaceComponent<component::CircleRenderer>();
	ent.addOrReplaceComponent<component::SpriteRenderer>();
	ent.addOrReplaceComponent<component::Player>();
	auto ent2 = sc->createEntityWithUUID(7, "bobObject2");
	auto& spr = ent2.addOrReplaceComponent<component::SpriteRenderer>();
	ent2.addOrReplaceComponent<component::PhysicBody>();
	ent2.addOrReplaceComponent<component::Text>();
	ent2.addOrReplaceComponent<component::EntityLink>();
	ent2.addOrReplaceComponent<component::Trigger>();
	spr.texture =
			owl::mkShared<owl::renderer::null::Texture2D>(owl::renderer::Texture2D::Specification{.size = {1, 1}});
	spr.tilingFactor = 12.3f;

	const SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / "tempSave.yml";
	saver.serialize(fs);

	ASSERT_TRUE(exists(fs));
	const auto sc2 = owl::mkShared<Scene>();
	const SceneSerializer loader(sc2);
	EXPECT_TRUE(loader.deserialize(fs));

	EXPECT_EQ(sc2->getEntityCount(), sc->getEntityCount());
	remove(fs);
	EXPECT_FALSE(exists(fs));
	owl::core::Log::invalidate();
}

TEST(SceneSerializer, badScene) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto fs = std::filesystem::temp_directory_path() / "tempSave.yml";

	// write bad file
	{
		std::ofstream file(fs);
		file << "Bob: toto\n";
		file.close();
		const auto sc = owl::mkShared<Scene>();
		const SceneSerializer loader(sc);
		EXPECT_FALSE(loader.deserialize(fs));
		remove(fs);
	}
	// write bad file
	{
		std::ofstream file(fs);
		file << "Scene: untitled\n";
		file << "  - je suis une fougère.:\n";
		file.close();
		const auto sc = owl::mkShared<Scene>();
		const SceneSerializer loader(sc);
		EXPECT_FALSE(loader.deserialize(fs));
		remove(fs);
	}
	// write another bad file
	{
		std::ofstream file(fs);
		file << "Scene: untitled\n";
		file << "Entities:\n";
		file << "  - Entity: 1\n";
		file << "  - Entity: 2\n";
		file << "    SpriteRenderer:\n";
		file << "      color: 0.949019611\n";
		file << "  - Entity: 3\n";
		file.close();
		const auto sc = owl::mkShared<Scene>();
		const SceneSerializer loader(sc);
		EXPECT_FALSE(loader.deserialize(fs));
		remove(fs);
	}
	// write another bad file
	{
		std::ofstream file(fs);
		file << "Scene: untitled\n";
		file << "Entities:\n";
		file << "  - Entity: 1\n";
		file << "  - Entity: 2\n";
		file << "    Transform:\n";
		file << "      translation: 0.353553385\n";
		file.close();
		const auto sc = owl::mkShared<Scene>();
		const SceneSerializer loader(sc);
		EXPECT_FALSE(loader.deserialize(fs));
		remove(fs);
	}
	// write another bad file
	{
		std::ofstream file(fs);
		file << "Scene: untitled\n";
		file << "Entities:\n";
		file << "  - Entity: 1\n";
		file << "  - Entity: 3\n";
		file << "    Transform:\n";
		file << "      translation: [0.353553385, 0]\n";
		file.close();
		const auto sc = owl::mkShared<Scene>();
		const SceneSerializer loader(sc);
		EXPECT_FALSE(loader.deserialize(fs));
		remove(fs);
	}
	// write another bad file
	{
		std::ofstream file(fs);
		file << "Scene: untitled\n";
		file << "Entities:\n";
		file << "  - Entity: 1\n";
		file << "  - Entity: 3\n";
		file << "    SpriteRenderer:\n";
		file << "      color: [0, 0.949019611]\n";
		file.close();
		const auto sc = owl::mkShared<Scene>();
		const SceneSerializer loader(sc);
		EXPECT_FALSE(loader.deserialize(fs));
		remove(fs);
	}
	EXPECT_FALSE(exists(fs));
	owl::core::Log::invalidate();
}
