
#include "testHelper.h"

#include <renderer/gpu/Texture.h>
#include <renderer/gpu/null/Texture.h>
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
	{
		auto& animSpr = ent.addOrReplaceComponent<component::AnimatedSpriteRenderer>();
		animSpr.columns = 4;
		animSpr.rows = 2;
		animSpr.firstFrame = 1;
		animSpr.lastFrame = 6;
		animSpr.frameDuration = 0.05f;
		animSpr.loop = false;
	}
	ent.addOrReplaceComponent<component::Player>();
	auto ent2 = sc->createEntityWithUUID(7, "bobObject2");
	auto& spr = ent2.addOrReplaceComponent<component::SpriteRenderer>();
	ent2.addOrReplaceComponent<component::PhysicBody>();
	ent2.addOrReplaceComponent<component::Text>();
	ent2.addOrReplaceComponent<component::EntityLink>();
	ent2.addOrReplaceComponent<component::Trigger>();
	spr.texture = owl::mkShared<owl::renderer::gpu::null::Texture2D>(
			owl::renderer::gpu::Texture2D::Specification{.size = {1, 1}});
	spr.tilingFactor = {12.3f, 12.3f};

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

TEST(SceneSerializer, VisibilityRoundTrip) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto sc = owl::mkShared<Scene>();
	auto ent = sc->createEntityWithUUID(42, "visTest");
	auto& vis = ent.getComponent<component::Visibility>();
	vis.gameVisible = false;
	vis.editorVisible = false;

	const SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / "tempVisibility.yml";
	saver.serialize(fs);

	ASSERT_TRUE(exists(fs));
	const auto sc2 = owl::mkShared<Scene>();
	const SceneSerializer loader(sc2);
	EXPECT_TRUE(loader.deserialize(fs));

	EXPECT_EQ(sc2->getEntityCount(), sc->getEntityCount());
	const auto entities = sc2->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	const auto& vis2 = entities[0].getComponent<component::Visibility>();
	EXPECT_FALSE(vis2.gameVisible);// persisted
	EXPECT_TRUE(vis2.editorVisible);// reset to true (not serialized)

	remove(fs);
	EXPECT_FALSE(exists(fs));
	owl::core::Log::invalidate();
}

// `parseBuffer` is the CPU-only half of the async scene-load skeleton:
// it walks the YAML on a worker thread without touching scene / GPU
// state. `applyParsed` then materialises the parsed tree onto a real
// scene from the main thread. End-to-end behaviour must match the
// legacy `deserializeFromBuffer`.
TEST(SceneSerializer, ParseAndApplyMatchesDeserialize) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto source = owl::mkShared<Scene>();
	source->createEntityWithUUID(101, "alpha");
	source->createEntityWithUUID(102, "beta");
	const SceneSerializer saver(source);
	const auto yaml = saver.serializeToString();
	const std::vector<uint8_t> bytes{yaml.begin(), yaml.end()};

	const auto parsed = SceneSerializer::parseBuffer(bytes, "<test>");
	ASSERT_TRUE(parsed.valid);
	EXPECT_FALSE(parsed.sceneName.empty());

	const auto dst = owl::mkShared<Scene>();
	const SceneSerializer applier(dst);
	EXPECT_TRUE(applier.applyParsed(parsed));
	EXPECT_TRUE(dst->findEntityByUUID(owl::core::UUID{101}));
	EXPECT_TRUE(dst->findEntityByUUID(owl::core::UUID{102}));
	owl::core::Log::invalidate();
}

// A malformed YAML buffer must fail at `parseBuffer` without throwing —
// the worker thread reports the failure via `ParsedScene::valid = false`
// and a human-readable `error` field. `applyParsed` on an invalid result
// returns false without touching the destination scene.
TEST(SceneSerializer, ParseBufferRejectsMalformed) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const std::string bad = "not: [valid: yaml: at all";
	const std::vector<uint8_t> bytes{bad.begin(), bad.end()};
	const auto parsed = SceneSerializer::parseBuffer(bytes, "<bad>");
	EXPECT_FALSE(parsed.valid);
	EXPECT_FALSE(parsed.error.empty());

	const auto dst = owl::mkShared<Scene>();
	const SceneSerializer applier(dst);
	EXPECT_FALSE(applier.applyParsed(parsed));
	EXPECT_EQ(dst->getEntityCount(), 0u);
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
