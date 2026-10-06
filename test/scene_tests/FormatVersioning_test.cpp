/**
 * @file FormatVersioning_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

// NOLINTBEGIN: include the editor's project TU directly, the test targets do not link the editor.
#include "../../source/owlnest/sources/Project.cpp"
// NOLINTEND

#include "testHelper.h"

#include <core/Log.h>
#include <platform/AtomicFileFault.h>
#include <scene/AnimationClip.h>
#include <scene/Entity.h>
#include <scene/PrefabSerializer.h>
#include <scene/SaveManager.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/SettingsManager.h>
#include <scene/TilemapAsset.h>
#include <scene/Tileset.h>
#include <scene/component/SpriteRenderer.h>
#include <scene/component/Transform.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

auto withVersion(const std::string& iYaml, const std::string& iVersion) -> std::string {
	return std::regex_replace(iYaml, std::regex{"FormatVersion: [0-9]+"}, "FormatVersion: " + iVersion);
}

auto withoutVersion(const std::string& iYaml) -> std::string {
	return std::regex_replace(iYaml, std::regex{"(^|\n)[ \t]*FormatVersion: [0-9]+\n"}, "$1");
}

auto readText(const std::filesystem::path& iPath) -> std::string {
	const std::ifstream in(iPath, std::ios::binary);
	std::stringstream buffer;
	buffer << in.rdbuf();
	return buffer.str();
}

auto findByName(const Scene& iScene, const std::string& iName) -> Entity {
	for (const auto& entity: iScene.getAllEntities())
		if (entity.getName() == iName)
			return entity;
	return {};
}

void writeText(const std::filesystem::path& iPath, const std::string& iText) {
	std::ofstream out(iPath, std::ios::binary);
	out << iText;
}

/// Redirects the user directory (saves, settings) into the test's temporary folder.
class HomeGuard {
public:
	HomeGuard(const HomeGuard&) = delete;
	HomeGuard(HomeGuard&&) = delete;
	auto operator=(const HomeGuard&) -> HomeGuard& = delete;
	auto operator=(HomeGuard&&) -> HomeGuard& = delete;

	explicit HomeGuard(const std::filesystem::path& iHome) {
		// NOLINTNEXTLINE(concurrency-mt-unsafe)
		if (const char* previous = std::getenv(k_Variable); previous != nullptr)
			m_previous = previous;
		set(iHome.string());
	}

	~HomeGuard() {
		if (m_previous)
			set(*m_previous);
	}

private:
#ifdef OWL_PLATFORM_WINDOWS
	static constexpr const char* k_Variable = "APPDATA";
	static void set(const std::string& iValue) { _putenv_s(k_Variable, iValue.c_str()); }
#else
	static constexpr const char* k_Variable = "HOME";
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	static void set(const std::string& iValue) { setenv(k_Variable, iValue.c_str(), 1); }
#endif
	/// Value to restore.
	std::optional<std::string> m_previous;
};

class FormatVersioningTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_format_versioning_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
	}

	void TearDown() override {
		platform::setAtomicWriteFault(std::nullopt);
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	[[nodiscard]] static auto makeScene() -> shared<Scene> {
		auto scene = mkShared<Scene>();
		auto entity = scene->createEntity("Versioned");
		entity.getComponent<component::Transform>().transform.translation() = {1.f, 2.f, 3.f};
		entity.addComponent<component::SpriteRenderer>().color = {0.25f, 0.5f, 0.75f, 1.f};
		return scene;
	}

	[[nodiscard]] static auto loadScene(const std::string& iYaml) -> SceneLoadResult {
		const SceneSerializer serializer(mkShared<Scene>());
		const std::vector<uint8_t> bytes(iYaml.begin(), iYaml.end());
		return serializer.deserializeFromBuffer(bytes, "test");
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(FormatVersioningTest, SceneRoundTripCarriesTheVersion) {
	const auto path = m_dir / "level.owl";
	ASSERT_TRUE(SceneSerializer(makeScene()).serialize(path));
	const auto text = readText(path);
	EXPECT_NE(text.find(std::format("FormatVersion: {}", SceneSerializer::format().currentVersion())),
			  std::string::npos);

	const auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserialize(path));
	const auto entity = findByName(*loaded, "Versioned");
	ASSERT_TRUE(static_cast<bool>(entity));
	EXPECT_FLOAT_EQ(entity.getComponent<component::Transform>().transform.translation().y(), 2.f);
}

TEST_F(FormatVersioningTest, UnversionedSceneLoadsAsVersionOne) {
	const auto text = withoutVersion(SceneSerializer(makeScene()).serializeToString());
	ASSERT_EQ(text.find("FormatVersion"), std::string::npos);
	EXPECT_TRUE(loadScene(text));
	EXPECT_TRUE(loadScene("Scene: legacy\nEntities:\n  - Entity: 42\n    Tag:\n      tag: Old\n"));
}

TEST_F(FormatVersioningTest, FutureSceneIsRefusedWithoutTouchingTheScene) {
	const auto text = withVersion(SceneSerializer(makeScene()).serializeToString(), "999");
	const auto target = mkShared<Scene>();
	target->createEntity("Existing");
	const std::vector<uint8_t> bytes(text.begin(), text.end());
	const auto result = SceneSerializer(target).deserializeFromBuffer(bytes, "future");
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error(), SceneLoadError::NewerFormatVersion);
	EXPECT_NE(describe(result.error()).find("newer version of Owl"), std::string_view::npos);
	EXPECT_EQ(target->getAllEntities().size(), 1u);

	const auto invalid = loadScene("Scene: x\nFormatVersion: nope\nEntities: []\n");
	ASSERT_FALSE(invalid);
	EXPECT_EQ(invalid.error(), SceneLoadError::InvalidFormatVersion);
}

TEST_F(FormatVersioningTest, InterruptedSceneSaveKeepsThePreviousFile) {
	const auto path = m_dir / "level.owl";
	ASSERT_TRUE(SceneSerializer(makeScene()).serialize(path));
	const auto before = readText(path);
	auto bigger = makeScene();
	for (int i = 0; i < 50; ++i) bigger->createEntity(std::format("Extra{}", i));
	platform::setAtomicWriteFault(platform::WriteError::WriteFailed);
	EXPECT_FALSE(SceneSerializer(bigger).serialize(path));
	platform::setAtomicWriteFault(std::nullopt);
	EXPECT_EQ(readText(path), before);
	EXPECT_TRUE(SceneSerializer(mkShared<Scene>()).deserialize(path));
}

TEST_F(FormatVersioningTest, PrefabVersioning) {
	const auto scene = makeScene();
	const auto root = findByName(*scene, "Versioned");
	const auto path = m_dir / "thing.owlprefab";
	ASSERT_TRUE(PrefabSerializer::serialize(root, *scene, path, "Thing"));
	const auto text = readText(path);
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);
	EXPECT_TRUE(static_cast<bool>(PrefabSerializer::instantiate(path, mkShared<Scene>())));

	writeText(path, withoutVersion(text));
	EXPECT_TRUE(static_cast<bool>(PrefabSerializer::instantiate(path, mkShared<Scene>())));
	ASSERT_TRUE(PrefabSerializer::readInfo(path).has_value());

	writeText(path, withVersion(text, "7"));
	EXPECT_FALSE(static_cast<bool>(PrefabSerializer::instantiate(path, mkShared<Scene>())));
	EXPECT_FALSE(PrefabSerializer::readInfo(path).has_value());

	writeText(path, text);
	platform::setAtomicWriteFault(platform::WriteError::RenameFailed);
	EXPECT_FALSE(PrefabSerializer::serialize(root, *scene, path, "Renamed"));
	platform::setAtomicWriteFault(std::nullopt);
	EXPECT_EQ(readText(path), text);
}

TEST_F(FormatVersioningTest, SaveVersioning) {
	const HomeGuard home(m_dir);
	SaveManager::setGameName("OwlFormatTest");
	ASSERT_TRUE(SaveManager::getSaveDirectory().string().starts_with(m_dir.string()));
	const auto scene = makeScene();
	scene->getGameState().set("coins", int64_t{12});
	ASSERT_TRUE(SaveManager::save(1, scene, "scenes/level.owl"));
	const auto path = SaveManager::getSaveDirectory() / "save_1.owl_save";
	const auto text = readText(path);
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);

	const auto loaded = mkShared<Scene>();
	const auto result = SaveManager::load(1, loaded);
	ASSERT_TRUE(result.success);
	EXPECT_TRUE(static_cast<bool>(findByName(*loaded, "Versioned")));

	writeText(path, withoutVersion(text));
	EXPECT_TRUE(SaveManager::load(1, mkShared<Scene>()).success);

	writeText(path, withVersion(text, "3"));
	const auto future = SaveManager::load(1, mkShared<Scene>());
	EXPECT_FALSE(future.success);
	ASSERT_TRUE(future.formatError.has_value());
	EXPECT_EQ(*future.formatError, core::FormatError::NewerVersion);

	writeText(path, text);
	platform::setAtomicWriteFault(platform::WriteError::FlushFailed);
	EXPECT_FALSE(SaveManager::save(1, makeScene(), "scenes/other.owl"));
	platform::setAtomicWriteFault(std::nullopt);
	EXPECT_EQ(readText(path), text);
	SaveManager::setGameName("");
}

TEST_F(FormatVersioningTest, TilesetVersioning) {
	Tileset tileset;
	tileset.columns = 2;
	tileset.rows = 3;
	tileset.tiles.assign(6, TileMeta{});
	tileset.tiles[4].collidable = true;
	const auto path = m_dir / "set.owltileset";
	ASSERT_TRUE(tileset.saveToFile(path));
	const auto text = readText(path);
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);
	Tileset restored;
	ASSERT_TRUE(restored.loadFromFile(path));
	EXPECT_EQ(restored.rows, 3u);
	EXPECT_TRUE(restored.tiles[4].collidable);
	EXPECT_TRUE(Tileset{}.deserializeFromString(withoutVersion(text)));
	EXPECT_FALSE(Tileset{}.deserializeFromString(withVersion(text, "2")));
}

TEST_F(FormatVersioningTest, TilemapVersioning) {
	TilemapAsset asset;
	asset.width = 3;
	asset.height = 2;
	asset.addLayer("ground").tiles[1] = 5;
	const auto path = m_dir / "map.owltilemap";
	ASSERT_TRUE(asset.saveToFile(path));
	const auto text = readText(path);
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);
	TilemapAsset restored;
	ASSERT_TRUE(restored.loadFromFile(path));
	EXPECT_EQ(restored.getTile(0, 1, 0), 5);
	EXPECT_TRUE(TilemapAsset{}.deserializeFromString(withoutVersion(text)));
	EXPECT_FALSE(TilemapAsset{}.deserializeFromString(withVersion(text, "2")));
}

TEST_F(FormatVersioningTest, AnimationClipVersioning) {
	AnimationClip clip;
	clip.columns = 4;
	clip.lastFrame = 3;
	clip.loop = false;
	const auto path = m_dir / "walk.owlanim";
	ASSERT_TRUE(clip.saveToFile(path, "walk"));
	const auto text = readText(path);
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);
	AnimationClip restored;
	ASSERT_TRUE(restored.loadFromFile(path));
	EXPECT_EQ(restored.columns, 4u);
	EXPECT_FALSE(restored.loop);
	EXPECT_TRUE(AnimationClip{}.deserializeFromString(withoutVersion(text)));
	EXPECT_FALSE(AnimationClip{}.deserializeFromString(withVersion(text, "2")));
}

TEST_F(FormatVersioningTest, SettingsVersioning) {
	const HomeGuard home(m_dir);
	SettingsManager::clear();
	SettingsManager::setGameName("OwlFormatTest");
	SettingsManager::set("volume", 0.5f);
	ASSERT_TRUE(SettingsManager::saveUserSettings());
	const auto path = SettingsManager::getSettingsPath();
	ASSERT_TRUE(path.string().starts_with(m_dir.string()));
	const auto text = readText(path);
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);

	SettingsManager::resetAllToDefaults();
	SettingsManager::loadUserSettings();
	EXPECT_FLOAT_EQ(SettingsManager::getAs<float>("volume").value(), 0.5f);

	writeText(path, withoutVersion(text));
	SettingsManager::resetAllToDefaults();
	SettingsManager::loadUserSettings();
	EXPECT_TRUE(SettingsManager::hasOverride("volume"));

	writeText(path, withVersion(text, "4"));
	SettingsManager::resetAllToDefaults();
	SettingsManager::loadUserSettings();
	EXPECT_FALSE(SettingsManager::hasOverride("volume"));

	SettingsManager::loadDefaultsFromString("GameSettings:\n  - {key: speed, type: float, value: 2.5}\n");
	EXPECT_FLOAT_EQ(SettingsManager::getAs<float>("speed").value(), 2.5f);
	SettingsManager::clear();
	SettingsManager::setGameName("");
}

TEST_F(FormatVersioningTest, ProjectVersioning) {
	nest::Project project;
	project.name = "Versioned";
	project.firstScene = "scenes/start.owl";
	project.window.width = 640;
	const auto path = m_dir / "owl_project.yml";
	ASSERT_TRUE(project.saveToFile(path));
	const auto text = readText(path);
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);

	nest::Project restored;
	ASSERT_TRUE(restored.loadFromFile(path));
	EXPECT_EQ(restored.name, "Versioned");
	EXPECT_EQ(restored.window.width, 640u);
	EXPECT_EQ(restored.projectDirectory, m_dir);

	writeText(path, withoutVersion(text));
	nest::Project legacy;
	ASSERT_TRUE(legacy.loadFromFile(path));
	EXPECT_EQ(legacy.firstScene, "scenes/start.owl");

	writeText(path, withVersion(text, "2"));
	nest::Project future;
	future.name = "Untouched";
	EXPECT_FALSE(future.loadFromFile(path));
	EXPECT_EQ(future.name, "Untouched");
	EXPECT_EQ(nest::Project::format().currentVersion(), 1u);
}

TEST_F(FormatVersioningTest, EngineSampleAssetsStillLoad) {
	const auto root = owl::test::getRootPath();
	size_t scenes = 0;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(root / "engine_assets")) {
		if (entry.path().extension() == ".owl") {
			EXPECT_TRUE(SceneSerializer(mkShared<Scene>()).deserialize(entry.path())) << entry.path();
			++scenes;
		} else if (entry.path().extension() == Tileset::fileExtension()) {
			EXPECT_TRUE(Tileset{}.loadFromFile(entry.path())) << entry.path();
		} else if (entry.path().extension() == AnimationClip::fileExtension()) {
			EXPECT_TRUE(AnimationClip{}.loadFromFile(entry.path())) << entry.path();
		}
	}
	EXPECT_GT(scenes, 0u);
}
