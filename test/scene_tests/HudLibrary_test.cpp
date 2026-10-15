/**
 * @file HudLibrary_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/FormatVersion.h>
#include <core/Log.h>
#include <platform/AtomicFile.h>
#include <scene/Entity.h>
#include <scene/HudLibrary.h>
#include <scene/SaveManager.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/UiInputSystem.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace owl;
using scene::DocumentKind;
using scene::component::UiRect;

namespace {

class HudLibraryTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() /
				("owl_hud_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		create_directories(m_dir);
	}

	void TearDown() override {
		std::error_code ec;
		std::filesystem::remove_all(m_dir, ec);
		core::Log::invalidate();
	}

	/// Write an `.owlhud` holding a canvas, a progress bar and a button; returns its path.
	[[nodiscard]] auto writeHud(const std::string& iName = "gameplay.owlhud") const -> std::string {
		const auto hud = mkShared<scene::Scene>();
		auto canvas = hud->createEntityWithUUID(core::UUID{101}, "Canvas");
		canvas.addComponent<scene::component::Canvas>().scaleMode = scene::component::Canvas::ScaleMode::ConstantPixels;
		auto bar = hud->createEntityWithUUID(core::UUID{102}, "HealthBar");
		bar.addComponent<UiRect>().size = {100.f, 20.f};
		bar.addComponent<scene::component::UiProgressBar>().value = 0.25f;
		hud->setParent(bar, canvas);
		auto button = hud->createEntityWithUUID(core::UUID{103}, "Pause");
		auto& rect = button.addComponent<UiRect>();
		rect.size = {80.f, 30.f};
		button.addComponent<scene::component::UiButton>();
		hud->setParent(button, canvas);
		const auto path = m_dir / iName;
		EXPECT_TRUE(scene::SceneSerializer(hud, DocumentKind::Hud).serialize(path));
		return path.string();
	}

	std::filesystem::path m_dir;
};

auto slurp(const std::string& iPath) -> std::string {
	const std::ifstream in(iPath, std::ios::binary);
	return {std::istreambuf_iterator<char>(in.rdbuf()), std::istreambuf_iterator<char>()};
}

}// namespace

TEST_F(HudLibraryTest, hudFileHasItsOwnRootKeyAndFormat) {
	const auto path = writeHud();
	const auto text = slurp(path);
	EXPECT_TRUE(text.contains("Hud: untitled"));
	EXPECT_FALSE(text.contains("Scene:"));
	EXPECT_TRUE(text.contains(std::string{core::g_FormatVersionKey}));
	const auto& format = scene::SceneSerializer::format(DocumentKind::Hud);
	EXPECT_EQ(format.name, "Hud");
	EXPECT_EQ(format.currentVersion(), 1u);
	EXPECT_EQ(scene::documentKindOf("huds/gameplay.owlhud"), DocumentKind::Hud);
	EXPECT_EQ(scene::documentKindOf("scenes/level.owl"), DocumentKind::Scene);
}

TEST_F(HudLibraryTest, hudRoundTripKeepsTheWidgets) {
	const auto path = writeHud();
	const auto loaded = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer(loaded, DocumentKind::Hud).deserialize(path).has_value());
	EXPECT_EQ(loaded->getEntityCount(), 3u);
	const auto bar = loaded->findEntityByUUID(core::UUID{102});
	ASSERT_TRUE(static_cast<bool>(bar));
	EXPECT_FLOAT_EQ(bar.getComponent<scene::component::UiProgressBar>().value, 0.25f);
	EXPECT_EQ(bar.getComponent<scene::component::Hierarchy>().parentId, core::UUID{101});
}

TEST_F(HudLibraryTest, aSceneIsNotAHudAndAHudIsNotAScene) {
	const auto hudPath = writeHud();
	const auto sceneObj = mkShared<scene::Scene>();
	sceneObj->createEntity("Alone");
	const auto scenePath = m_dir / "level.owl";
	ASSERT_TRUE(scene::SceneSerializer(sceneObj).serialize(scenePath));
	const auto target = mkShared<scene::Scene>();
	const auto asScene = scene::SceneSerializer(target).deserialize(hudPath);
	ASSERT_FALSE(asScene.has_value());
	EXPECT_EQ(asScene.error(), scene::SceneLoadError::NotAScene);
	const auto asHud = scene::SceneSerializer(target, DocumentKind::Hud).deserialize(scenePath);
	ASSERT_FALSE(asHud.has_value());
	EXPECT_EQ(asHud.error(), scene::SceneLoadError::NotAScene);
	EXPECT_EQ(target->getEntityCount(), 0u);
}

TEST_F(HudLibraryTest, aNewerHudFormatIsRefused) {
	const auto path = writeHud();
	auto text = slurp(path);
	const auto key = std::string{core::g_FormatVersionKey} + ": 1";
	const auto at = text.find(key);
	ASSERT_NE(at, std::string::npos);
	text.replace(at, key.size(), std::string{core::g_FormatVersionKey} + ": 99");
	const std::vector<uint8_t> bytes(text.begin(), text.end());
	const auto target = mkShared<scene::Scene>();
	const auto loaded = scene::SceneSerializer(target, DocumentKind::Hud).deserializeFromBuffer(bytes, "future.owlhud");
	ASSERT_FALSE(loaded.has_value());
	EXPECT_EQ(loaded.error(), scene::SceneLoadError::NewerFormatVersion);
}

TEST_F(HudLibraryTest, libraryCachesPerPathAndSharesTheScene) {
	app::EngineContext context;
	const auto path = writeHud();
	auto& library = context.getHudLibrary();
	const auto first = library.get(path);
	const auto second = library.get(path);
	ASSERT_TRUE(first);
	EXPECT_EQ(first, second);
	EXPECT_EQ(library.getLoadCount(), 1u);
	EXPECT_TRUE(library.isLoaded(path));
	EXPECT_EQ(library.size(), 1u);
	EXPECT_EQ(first->getHudPath(), std::string{});
}

TEST_F(HudLibraryTest, libraryReportsAMissingFileOnceAndRetriesAfterInvalidate) {
	app::EngineContext context;
	auto& library = context.getHudLibrary();
	const auto missing = (m_dir / "nothing.owlhud").string();
	EXPECT_EQ(library.get(missing), nullptr);
	EXPECT_EQ(library.get(missing), nullptr);
	EXPECT_EQ(library.getLoadCount(), 1u);
	EXPECT_EQ(library.get(""), nullptr);
	EXPECT_EQ(library.getLoadCount(), 1u);
	const auto path = writeHud("nothing.owlhud");
	library.invalidate(missing);
	EXPECT_NE(library.get(missing), nullptr);
	EXPECT_EQ(library.getLoadCount(), 2u);
	library.clear();
	EXPECT_EQ(library.size(), 0u);
	EXPECT_NE(library.get(path), nullptr);
	EXPECT_EQ(library.getLoadCount(), 3u);
}

TEST_F(HudLibraryTest, libraryRejectsAFileOfAnotherKind) {
	app::EngineContext context;
	const auto sceneObj = mkShared<scene::Scene>();
	const auto scenePath = m_dir / "level.owl";
	ASSERT_TRUE(scene::SceneSerializer(sceneObj).serialize(scenePath));
	EXPECT_EQ(context.getHudLibrary().get(scenePath.string()), nullptr);
}

TEST_F(HudLibraryTest, twoScenesShareTheHudAndItsValuesSurviveATransition) {
	app::EngineContext context;
	context.setDefaultHud(writeHud());
	const auto first = mkShared<scene::Scene>();
	first->setEngineContext(&context);
	const auto second = mkShared<scene::Scene>();
	second->setEngineContext(&context);
	const auto hud = first->getHud();
	ASSERT_TRUE(hud);
	hud->findEntityByUUID(core::UUID{102}).getComponent<scene::component::UiProgressBar>().value = 0.9f;
	EXPECT_EQ(second->getHud(), hud);
	EXPECT_FLOAT_EQ(
			second->getHud()->findEntityByUUID(core::UUID{102}).getComponent<scene::component::UiProgressBar>().value,
			0.9f);
	EXPECT_EQ(context.getHudLibrary().getLoadCount(), 1u);
}

TEST_F(HudLibraryTest, sceneHudPathOverridesTheDefaultAndEmptyMeansNone) {
	app::EngineContext context;
	const auto defaultHud = writeHud("default.owlhud");
	const auto ownHud = writeHud("own.owlhud");
	context.setDefaultHud(defaultHud);
	const auto sc = mkShared<scene::Scene>();
	EXPECT_EQ(sc->getHud(), nullptr);
	sc->setEngineContext(&context);
	EXPECT_EQ(sc->getEffectiveHudPath(), defaultHud);
	EXPECT_TRUE(sc->getHud());
	sc->setHudPath(ownHud);
	EXPECT_EQ(sc->getEffectiveHudPath(), ownHud);
	EXPECT_NE(sc->getHud(), context.getHudLibrary().get(defaultHud));
	sc->setHudPath(std::string{});
	EXPECT_EQ(sc->getHud(), nullptr);
	sc->setHudPath(std::nullopt);
	EXPECT_EQ(sc->getEffectiveHudPath(), defaultHud);
}

TEST_F(HudLibraryTest, sceneKeepsItsHudKeyThroughSaveCopyAndNoneIsExplicit) {
	const auto sc = mkShared<scene::Scene>();
	EXPECT_FALSE(scene::SceneSerializer(sc).serializeToString().contains("Hud:"));
	sc->setHudPath("huds/gameplay.owlhud");
	const auto text = scene::SceneSerializer(sc).serializeToString();
	EXPECT_TRUE(text.contains("Hud: huds/gameplay.owlhud"));
	const auto loaded = mkShared<scene::Scene>();
	const std::vector<uint8_t> bytes(text.begin(), text.end());
	ASSERT_TRUE(scene::SceneSerializer(loaded).deserializeFromBuffer(bytes).has_value());
	EXPECT_EQ(loaded->getHudPath(), std::string{"huds/gameplay.owlhud"});
	EXPECT_EQ(scene::Scene::copy(loaded)->getHudPath(), std::string{"huds/gameplay.owlhud"});

	sc->setHudPath(std::string{});
	const auto noneText = scene::SceneSerializer(sc).serializeToString();
	const auto none = mkShared<scene::Scene>();
	const std::vector<uint8_t> noneBytes(noneText.begin(), noneText.end());
	ASSERT_TRUE(scene::SceneSerializer(none).deserializeFromBuffer(noneBytes).has_value());
	ASSERT_TRUE(none->getHudPath().has_value());
	EXPECT_TRUE(none->getHudPath()->empty());
}

TEST_F(HudLibraryTest, hudInputIsRoutedBeforeTheScene) {
	app::EngineContext context;
	context.setDefaultHud(writeHud());
	const auto sc = mkShared<scene::Scene>();
	sc->setEngineContext(&context);
	auto canvas = sc->createEntity("Canvas");
	canvas.addComponent<scene::component::Canvas>().scaleMode = scene::component::Canvas::ScaleMode::ConstantPixels;
	auto sceneButton = sc->createEntity("SceneButton");
	auto& sceneRect = sceneButton.addComponent<UiRect>();
	sceneRect.size = {80.f, 30.f};
	sceneButton.addComponent<scene::component::UiButton>();
	sc->setParent(sceneButton, canvas);
	sc->status = scene::Scene::Status::Playing;

	// Both buttons sit at the centre: the HUD's one is drawn over the scene's and takes the mouse.
	scene::UiInputSystem::update(sc.get(), {400u, 300u}, {200.f, 150.f}, false);
	EXPECT_TRUE(scene::UiInputSystem::isUIConsuming(*sc));
	const auto hudButton = sc->getHud()->findEntityByUUID(core::UUID{103});
	EXPECT_EQ(hudButton.getComponent<scene::component::UiButton>().state, scene::component::UiButton::State::Hovered);
	EXPECT_EQ(sceneButton.getComponent<scene::component::UiButton>().state, scene::component::UiButton::State::Normal);

	scene::UiInputSystem::update(sc.get(), {400u, 300u}, {10.f, 10.f}, false);
	EXPECT_FALSE(scene::UiInputSystem::isUIConsuming(*sc));
	EXPECT_EQ(hudButton.getComponent<scene::component::UiButton>().state, scene::component::UiButton::State::Normal);
}

TEST_F(HudLibraryTest, saveKeepsTheHudWidgetValues) {
	app::EngineContext context;
	const auto hudPath = writeHud();
	context.setDefaultHud(hudPath);
	scene::SaveManager::setGameName("OwlHudTest_" +
									std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	const auto sc = mkShared<scene::Scene>();
	sc->setEngineContext(&context);
	sc->getHud()->findEntityByUUID(core::UUID{102}).getComponent<scene::component::UiProgressBar>().value = 0.75f;
	ASSERT_TRUE(scene::SaveManager::save(1, sc, "scenes/level.owl"));

	context.getHudLibrary().clear();
	const auto restored = mkShared<scene::Scene>();
	restored->setEngineContext(&context);
	ASSERT_TRUE(scene::SaveManager::load(1, restored).success);
	EXPECT_EQ(restored->getEffectiveHudPath(), hudPath);
	const auto bar = restored->getHud()->findEntityByUUID(core::UUID{102});
	ASSERT_TRUE(static_cast<bool>(bar));
	EXPECT_FLOAT_EQ(bar.getComponent<scene::component::UiProgressBar>().value, 0.75f);
	scene::SaveManager::deleteSave(1);
	std::filesystem::remove_all(scene::SaveManager::getSaveDirectory().parent_path());
}
