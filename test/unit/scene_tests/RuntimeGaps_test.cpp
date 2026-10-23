/**
 * @file RuntimeGaps_test.cpp
 * @author Silmaen
 * @date 23/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Serializer.h>
#include <core/Timestep.h>
#include <renderer/Renderer.h>
#include <renderer/RendererIsometricLayer.h>
#include <scene/Entity.h>
#include <scene/SaveManager.h>
#include <scene/Scene.h>
#include <scene/ScenePicking.h>
#include <scene/SceneSerializer.h>
#include <scene/TilemapAsset.h>
#include <scene/component/components.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

void stepScene(Scene& ioScene, const int iMilliseconds) {
	core::Timestep timeStep;
	timeStep.forceUpdate(std::chrono::milliseconds(iMilliseconds));
	ioScene.onUpdateRuntime(timeStep, false);
}

auto makeCameraScene() -> shared<Scene> {
	auto scene = mkShared<Scene>();
	auto camera = scene->createEntity("Camera");
	camera.getComponent<component::Transform>().transform.translation() = {0.f, 0.f, 5.f};
	camera.addComponent<component::Camera>().primary = true;
	scene->onViewportResize({800u, 600u});
	return scene;
}

}// namespace

TEST(SceneTimeScale, scaleSpeedsTheClockAndPauseStopsIt) {
	core::Log::init(core::Log::Level::Off);
	const auto scene = makeCameraScene();
	scene->onStartRuntime();
	scene->setTimeScale(2.f);
	stepScene(*scene, 100);
	EXPECT_NEAR(scene->getRuntimeSeconds(), 0.2, 1e-4);
	EXPECT_NEAR(scene->getRealFrameSeconds(), 0.1f, 1e-4f);
	const auto ticks = scene->getSimulationClock().getTotalTickCount();
	EXPECT_GE(ticks, 8u);
	scene->setPaused(true);
	EXPECT_FLOAT_EQ(scene->getEffectiveTimeScale(), 0.f);
	stepScene(*scene, 100);
	EXPECT_EQ(scene->getSimulationClock().getTotalTickCount(), ticks);
	EXPECT_NEAR(scene->getRuntimeSeconds(), 0.2, 1e-4);
	scene->setTimeScale(-3.f);
	EXPECT_FLOAT_EQ(scene->getTimeScale(), 0.f);
	scene->setTimeScale(NAN);
	EXPECT_FLOAT_EQ(scene->getTimeScale(), 0.f);
	scene->onEndRuntime();
	scene->onStartRuntime();
	EXPECT_FLOAT_EQ(scene->getTimeScale(), 1.f);
	EXPECT_FALSE(scene->isPaused());
	scene->onEndRuntime();
	core::Log::invalidate();
}

TEST(SceneTimeScale, pausedAnimationsKeepTheirFrame) {
	core::Log::init(core::Log::Level::Off);
	const auto scene = makeCameraScene();
	auto sprite = scene->createEntity("Anim");
	auto& anim = sprite.addComponent<component::AnimatedSpriteRenderer>();
	anim.columns = 4;
	anim.lastFrame = 3;
	anim.frameDuration = 0.05f;
	scene->onStartRuntime();
	scene->setPaused(true);
	stepScene(*scene, 100);
	EXPECT_EQ(sprite.getComponent<component::AnimatedSpriteRenderer>().m_currentFrame, 0u);
	scene->setPaused(false);
	stepScene(*scene, 100);
	EXPECT_EQ(sprite.getComponent<component::AnimatedSpriteRenderer>().m_currentFrame, 2u);
	scene->onEndRuntime();
	core::Log::invalidate();
}

TEST(UiImageFrame, coordsSelectTheFrameOfTheGrid) {
	component::UiImage image;
	auto coords = image.getFrameCoords();
	EXPECT_FLOAT_EQ(coords[0].x(), 0.f);
	EXPECT_FLOAT_EQ(coords[2].x(), 1.f);
	image.columns = 4;
	image.rows = 2;
	image.frame = 5;
	coords = image.getFrameCoords();
	EXPECT_FLOAT_EQ(coords[0].x(), 0.25f);
	EXPECT_FLOAT_EQ(coords[2].x(), 0.5f);
	EXPECT_FLOAT_EQ(coords[0].y(), 0.f);
	EXPECT_FLOAT_EQ(coords[2].y(), 0.5f);
	image.frame = 99;
	EXPECT_FLOAT_EQ(image.getFrameCoords()[0].x(), 0.75f);
}

TEST(UiImageFrame, gridRoundTripsThroughTheScene) {
	core::Log::init(core::Log::Level::Off);
	const auto scene = mkShared<Scene>();
	auto& image = scene->createEntity("Icon").addComponent<component::UiImage>();
	image.columns = 3;
	image.rows = 2;
	image.frame = 4;
	const auto text = SceneSerializer(scene).serializeToString();
	const auto loaded = mkShared<Scene>();
	const std::vector<uint8_t> data(text.begin(), text.end());
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer(data, "image"));
	bool found = false;
	for (const auto entity: loaded->registry.view<component::UiImage>()) {
		const auto& read = loaded->registry.get<component::UiImage>(entity);
		EXPECT_EQ(read.columns, 3u);
		EXPECT_EQ(read.rows, 2u);
		EXPECT_EQ(read.frame, 4u);
		found = true;
	}
	EXPECT_TRUE(found);
	core::Log::invalidate();
}

namespace {

struct SaveSlotGuard {
	SaveSlotGuard() {
		SaveManager::setGameName("OwlGapsGame_" +
								 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	}
	SaveSlotGuard(const SaveSlotGuard&) = delete;
	SaveSlotGuard(SaveSlotGuard&&) = delete;
	auto operator=(const SaveSlotGuard&) -> SaveSlotGuard& = delete;
	auto operator=(SaveSlotGuard&&) -> SaveSlotGuard& = delete;
	~SaveSlotGuard() {
		std::error_code error;
		std::filesystem::remove_all(SaveManager::getSaveDirectory().parent_path(), error);
	}
};

auto writeMap(const std::filesystem::path& iPath) -> shared<TilemapAsset> {
	auto asset = mkShared<TilemapAsset>();
	asset->resize(40, 8);
	asset->addLayer("ground").fill(1);
	asset->addLayer("roads");
	EXPECT_TRUE(asset->saveToFile(iPath, "map"));
	auto loaded = mkShared<TilemapAsset>();
	EXPECT_TRUE(loaded->loadFromFile(iPath));
	return loaded;
}

}// namespace

TEST(SaveTilemapCells, runtimeEditsAreSavedAndRestored) {
	core::Log::init(core::Log::Level::Off);
	const SaveSlotGuard guard;
	const auto path = std::filesystem::temp_directory_path() / "gaps_map.owltilemap";
	const auto scene = mkShared<Scene>();
	auto map = scene->createEntity("Map");
	auto& tilemap = map.addComponent<component::Tilemap>();
	tilemap.tilemapPath = path;
	tilemap.asset = writeMap(path);
	std::ignore = tilemap.asset->layers[1].setCell(35, 6, {.terrain = 2, .overlay = 7, .flags = 0});
	tilemap.asset->layers[0].fill(component::g_EmptyTileIndex);
	ASSERT_TRUE(SaveManager::save(1, scene, "level.owl"));

	const auto loaded = mkShared<Scene>();
	const auto result = SaveManager::load(1, loaded);
	ASSERT_TRUE(result.success);
	ASSERT_EQ(result.tilemapCells.size(), 1u);
	const auto loadedMap = loaded->findEntityByUUID(map.getUUID());
	ASSERT_TRUE(loadedMap);
	auto& loadedTilemap = loadedMap.getComponent<component::Tilemap>();
	loadedTilemap.asset = mkShared<TilemapAsset>();
	ASSERT_TRUE(loadedTilemap.asset->loadFromFile(path));
	EXPECT_EQ(SaveManager::restoreTilemapCells(*loaded, result.tilemapCells), 1u);
	EXPECT_EQ(loadedTilemap.asset->layers[1].getCell(35, 6).overlay, 7);
	EXPECT_EQ(loadedTilemap.asset->layers[0].getCell(3, 3).terrain, component::g_EmptyTileIndex);
	std::filesystem::remove(path);
	core::Log::invalidate();
}

TEST(SaveTilemapCells, anUnchangedMapIsNotSaved) {
	core::Log::init(core::Log::Level::Off);
	const SaveSlotGuard guard;
	const auto path = std::filesystem::temp_directory_path() / "gaps_same.owltilemap";
	const auto scene = mkShared<Scene>();
	auto& tilemap = scene->createEntity("Map").addComponent<component::Tilemap>();
	tilemap.tilemapPath = path;
	tilemap.asset = writeMap(path);
	ASSERT_TRUE(SaveManager::save(2, scene, "level.owl"));
	const auto loaded = mkShared<Scene>();
	const auto result = SaveManager::load(2, loaded);
	ASSERT_TRUE(result.success);
	EXPECT_TRUE(result.tilemapCells.empty());
	std::filesystem::remove(path);
	core::Log::invalidate();
}

TEST(SaveTilemapCells, aDifferentGridIsNotRestored) {
	core::Log::init(core::Log::Level::Off);
	const auto scene = mkShared<Scene>();
	auto map = scene->createEntity("Map");
	auto& tilemap = map.addComponent<component::Tilemap>();
	tilemap.asset = mkShared<TilemapAsset>();
	tilemap.asset->resize(4, 4);
	tilemap.asset->addLayer("ground");
	const std::unordered_map<uint64_t, SaveManager::TilemapCells> cells{
			{static_cast<uint64_t>(map.getUUID()), {.width = 5, .height = 4, .layerCount = 1, .payload = {}}},
			{42u, {.width = 4, .height = 4, .layerCount = 1, .payload = {}}}};
	EXPECT_EQ(SaveManager::restoreTilemapCells(*scene, cells), 0u);
	core::Log::invalidate();
}

TEST(ScenePicking, theNearestSpriteUnderThePixelIsPicked) {
	core::Log::init(core::Log::Level::Off);
	const auto scene = makeCameraScene();
	auto back = scene->createEntity("Back");
	back.addComponent<component::SpriteRenderer>();
	auto front = scene->createEntity("Front");
	front.getComponent<component::Transform>().transform.translation() = {0.f, 0.f, 1.f};
	front.addComponent<component::SpriteRenderer>();
	EXPECT_EQ(pickEntity(*scene, {400.f, 300.f}), front);
	front.getComponent<component::Visibility>().gameVisible = false;
	EXPECT_EQ(pickEntity(*scene, {400.f, 300.f}), back);
	EXPECT_FALSE(pickEntity(*scene, {2.f, 2.f}));
	core::Log::invalidate();
}

TEST(ScenePicking, aCircleIsPickedOnItsDiscThroughAPerspectiveCamera) {
	core::Log::init(core::Log::Level::Off);
	const auto scene = makeCameraScene();
	for (const auto entity: scene->registry.view<component::Camera>())
		scene->registry.get<component::Camera>(entity).camera.setPerspective(0.8f, 0.1f, 100.f);
	scene->onViewportResize({800u, 600u});
	auto disc = scene->createEntity("Disc");
	disc.getComponent<component::Transform>().transform.translation() = {0.f, 0.f, -2.f};
	disc.addComponent<component::CircleRenderer>();
	EXPECT_EQ(pickEntity(*scene, {400.f, 300.f}), disc);
	EXPECT_FALSE(pickEntity(*scene, {10.f, 10.f}));
	core::Log::invalidate();
}

TEST(ScenePicking, anIsometricSpriteIsPickedOnItsScreenRectangle) {
	core::Log::init(core::Log::Level::Off);
	renderer::RendererIsometricLayer::registerWithFactory();
	renderer::RendererStackConfig config;
	config.entries.push_back(
			{.typeKey = renderer::RendererIsometricLayer::typeKey(), .name = "iso", .defaultConfig = {}});
	renderer::Renderer::setRenderStack(
			renderer::RenderStack::buildFromConfig(config, renderer::EnabledRenderersConfig{}));
	const auto scene = makeCameraScene();
	auto unit = scene->createEntity("Unit");
	unit.getComponent<component::Transform>().transform.translation() = {2.f, 1.f, 0.f};
	unit.addComponent<component::SpriteRenderer>();
	const auto camera = makeRuntimeIsometricCamera(*scene);
	const auto foot = camera.worldToViewport({2.f, 1.f, 0.f});
	EXPECT_EQ(pickEntity(*scene, {foot.x(), foot.y() - 10.f}), unit);
	EXPECT_FALSE(pickEntity(*scene, {foot.x(), foot.y() + 40.f}));
	renderer::Renderer::setRenderStack(renderer::RenderStack{});
	core::Log::invalidate();
}
