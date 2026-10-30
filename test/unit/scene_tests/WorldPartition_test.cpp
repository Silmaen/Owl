/**
 * @file WorldPartition_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <core/task/Scheduler.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/WorldStreamer.h>
#include <scene/component/components.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

constexpr auto g_cellScene = R"(Scene: Cell
FormatVersion: 7
Entities:
  - Entity: 9100000000000000001
    Tag: {tag: Rock}
    Transform: {translation: [1, 2, 3], rotation: [0, 0, 0], scale: [1, 1, 1]}
    Hierarchy: {parentId: 0}
  - Entity: 9100000000000000002
    Tag: {tag: Pebble}
    Transform: {translation: [0, 1, 0], rotation: [0, 0, 0], scale: [1, 1, 1]}
    Hierarchy: {parentId: 9100000000000000001}
  - Entity: 9100000000000000003
    Tag: {tag: Beacon}
    Transform: {translation: [0, 0, 0], rotation: [0, 0, 0], scale: [1, 1, 1]}
    WorldPosition: {position: [5000, 0, 0]}
    Hierarchy: {parentId: 0}
)";

class WorldPartitionTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "worldPartition";
		std::filesystem::create_directories(m_dir);
		std::ofstream{m_dir / "cell.owl"} << g_cellScene;
	}
	void TearDown() override {
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	[[nodiscard]] auto cellPath() const -> std::string { return (m_dir / "cell.owl").string(); }

	[[nodiscard]] auto partition() const -> WorldPartitionSettings {
		return {.enabled = true,
				.cellSize = 100.0,
				.loadRadius = 50.0,
				.unloadRadius = 120.0,
				.cells = {{.coords = {0, 0, 0}, .scene = cellPath()}, {.coords = {3, 0, 0}, .scene = cellPath()}}};
	}

	static auto find(const Scene& iScene, const std::string& iTag) -> Entity {
		for (const auto& entity: iScene.getAllEntities())
			if (entity.getComponent<component::Tag>().tag == iTag)
				return entity;
		return {};
	}

	static void run(Scene& ioScene, const int iFrames) {
		core::Timestep step;
		for (int frame = 0; frame < iFrames; ++frame) {
			step.forceUpdate(std::chrono::milliseconds(10));
			ioScene.onUpdateRuntime(step, false);
		}
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(WorldPartitionTest, SettingsAreClampedAndCellsAreUnique) {
	WorldPartitionSettings settings{.enabled = true,
									.cellSize = 0.1,
									.loadRadius = 200.0,
									.unloadRadius = 50.0,
									.cells = {{.coords = {1, 2, 3}, .scene = "a.owl"},
											  {.coords = {1, 2, 3}, .scene = "b.owl"},
											  {.coords = {0, 0, 0}, .scene = "c.owl"}}};
	const auto clamped = settings.clamped();
	EXPECT_DOUBLE_EQ(clamped.cellSize, WorldPartitionSettings::minCellSize);
	EXPECT_DOUBLE_EQ(clamped.unloadRadius, 200.0);
	ASSERT_EQ(clamped.cells.size(), 2u);
	EXPECT_EQ(clamped.cells[0].scene, "a.owl");
	settings.cellSize = std::numeric_limits<double>::quiet_NaN();
	EXPECT_DOUBLE_EQ(settings.clamped().cellSize, WorldPartitionSettings{}.cellSize);
	EXPECT_EQ(clamped, clamped.clamped());
}

TEST_F(WorldPartitionTest, CellsSitOnWorldCoordinates) {
	const auto settings = partition();
	EXPECT_EQ(settings.getCellOf({-0.5, 250.0, 99.9}), (math::vec3i{-1, 2, 0}));
	EXPECT_EQ(settings.getCellOf({1.5e11, 0.0, 0.0}).x(), 1500000000);
	EXPECT_DOUBLE_EQ(settings.getCellCorner({-2, 1, 0}).x(), -200.0);
	EXPECT_DOUBLE_EQ(settings.getDistanceToCell({0, 0, 0}, {50.0, 50.0, 50.0}), 0.0);
	EXPECT_DOUBLE_EQ(settings.getDistanceToCell({0, 0, 0}, {130.0, 50.0, 50.0}), 30.0);
	EXPECT_NE(settings.findCell({3, 0, 0}), nullptr);
	EXPECT_EQ(settings.findCell({1, 0, 0}), nullptr);
	const std::array sources{StreamPoint{.position = {340.0, 10.0, 10.0}, .radiusScale = 1.0}};
	const auto wanted = WorldStreamer::getWantedCells(settings, sources);
	ASSERT_EQ(wanted.size(), 1u);
	EXPECT_EQ(wanted[0], (math::vec3i{3, 0, 0}));
	const std::array wide{StreamPoint{.position = {200.0, 10.0, 10.0}, .radiusScale = 2.0}};
	EXPECT_EQ(WorldStreamer::getWantedCells(settings, wide).size(), 2u);
}

TEST_F(WorldPartitionTest, TheSceneFileKeepsThePartition) {
	const auto scene = mkShared<Scene>();
	scene->getWorldPartitionSettings() = partition();
	const auto text = SceneSerializer(scene).serializeToString();
	EXPECT_NE(text.find("WorldPartition"), std::string::npos);
	const auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer(std::vector<uint8_t>{text.begin(), text.end()}));
	EXPECT_EQ(loaded->getWorldPartitionSettings(), partition());
	const auto empty = mkShared<Scene>();
	EXPECT_EQ(SceneSerializer(empty).serializeToString().find("WorldPartition"), std::string::npos);
	EXPECT_EQ(Scene::copy(loaded)->getWorldPartitionSettings(), partition());
}

TEST_F(WorldPartitionTest, CellsLoadNearASourceAndUnloadPastTheUnloadRadius) {
	const auto scene = mkShared<Scene>();
	scene->getWorldPartitionSettings() = partition();
	scene->shiftWorldOrigin({10.0, 0.0, 0.0});
	WorldStreamer streamer;
	std::array sources{StreamPoint{.position = {20.0, 20.0, 20.0}, .radiusScale = 1.0}};
	streamer.update(*scene, sources, nullptr);
	EXPECT_EQ(streamer.getState({0, 0, 0}), WorldCellState::Loaded);
	EXPECT_EQ(streamer.getState({3, 0, 0}), WorldCellState::Unloaded);
	EXPECT_EQ(streamer.getCellEntities({0, 0, 0}).size(), 3u);
	const auto rock = find(*scene, "Rock");
	ASSERT_TRUE(rock);
	// Placed from the corner of the cell, around the scene's origin.
	EXPECT_FLOAT_EQ(rock.getComponent<component::Transform>().transform.translation().x(), -9.f);
	const auto pebble = find(*scene, "Pebble");
	ASSERT_TRUE(pebble);
	EXPECT_FLOAT_EQ(pebble.getComponent<component::Transform>().transform.translation().y(), 1.f);
	EXPECT_EQ(scene->getChildren(rock).size(), 1u);
	const auto beacon = find(*scene, "Beacon");
	ASSERT_TRUE(beacon);
	EXPECT_DOUBLE_EQ(beacon.getComponent<component::WorldPosition>().position.x(), 5000.0);
	// Between the two radii the cell stays.
	sources[0].position = {200.0, 20.0, 20.0};
	streamer.update(*scene, sources, nullptr);
	EXPECT_EQ(streamer.getState({0, 0, 0}), WorldCellState::Loaded);
	sources[0].position = {20.0, 260.0, 20.0};
	streamer.update(*scene, sources, nullptr);
	EXPECT_EQ(streamer.getState({0, 0, 0}), WorldCellState::Unloaded);
	EXPECT_FALSE(find(*scene, "Rock"));
	EXPECT_EQ(scene->getEntityCount(), 0u);
	EXPECT_EQ(streamer.getStats().unloads, 1u);
	EXPECT_EQ(streamer.getStats().merges, 1u);
}

TEST_F(WorldPartitionTest, CellsAreReadOnTheSchedulerWorkers) {
	core::task::Scheduler scheduler;
	const auto scene = mkShared<Scene>();
	scene->getWorldPartitionSettings() = partition();
	WorldStreamer streamer;
	const std::array sources{StreamPoint{.position = {340.0, 10.0, 10.0}, .radiusScale = 1.0}};
	streamer.update(*scene, sources, &scheduler);
	EXPECT_EQ(streamer.getState({3, 0, 0}), WorldCellState::Loading);
	EXPECT_EQ(streamer.getStats().loading, 1u);
	WorldStreamer::waitForLoads(&scheduler);
	streamer.update(*scene, sources, &scheduler);
	EXPECT_EQ(streamer.getState({3, 0, 0}), WorldCellState::Loaded);
	EXPECT_EQ(streamer.getLoadedCells().size(), 1u);
	EXPECT_EQ(scene->getEntityCount(), 3u);
	streamer.unloadAll(*scene);
	EXPECT_EQ(scene->getEntityCount(), 0u);
}

TEST_F(WorldPartitionTest, AMissingSceneFailsUntilItIsNoLongerWanted) {
	const auto scene = mkShared<Scene>();
	auto settings = partition();
	settings.cells[0].scene = (m_dir / "missing.owl").string();
	scene->getWorldPartitionSettings() = settings;
	WorldStreamer streamer;
	std::array sources{StreamPoint{.position = {20.0, 20.0, 20.0}, .radiusScale = 1.0}};
	streamer.update(*scene, sources, nullptr);
	EXPECT_EQ(streamer.getState({0, 0, 0}), WorldCellState::Failed);
	streamer.update(*scene, sources, nullptr);
	EXPECT_EQ(streamer.getStats().failures, 1u);
	sources[0].position = {1000.0, 0.0, 0.0};
	streamer.update(*scene, sources, nullptr);
	EXPECT_EQ(streamer.getState({0, 0, 0}), WorldCellState::Unloaded);
}

TEST_F(WorldPartitionTest, TheSystemStreamsAroundTheCameraAndTheSources) {
	auto scene = mkShared<Scene>();
	scene->getWorldPartitionSettings() = partition();
	auto camera = scene->createEntity("Camera");
	camera.addComponent<component::Camera>().primary = true;
	camera.getComponent<component::Transform>().transform.translation() = {20.f, 20.f, 20.f};
	scene->onStartRuntime();
	run(*scene, 2);
	EXPECT_EQ(scene->getWorldStreamer().getState({0, 0, 0}), WorldCellState::Loaded);
	EXPECT_TRUE(find(*scene, "Rock"));
	// A source takes over from the camera.
	auto ship = scene->createEntity("Ship");
	ship.addComponent<component::StreamingSource>();
	ship.getComponent<component::Transform>().transform.translation() = {340.f, 10.f, 10.f};
	run(*scene, 2);
	EXPECT_EQ(scene->getWorldStreamer().getState({3, 0, 0}), WorldCellState::Loaded);
	EXPECT_EQ(scene->getWorldStreamer().getState({0, 0, 0}), WorldCellState::Unloaded);
	// Switched off, the partition empties.
	scene->getWorldPartitionSettings().enabled = false;
	run(*scene, 1);
	EXPECT_TRUE(scene->getWorldStreamer().getLoadedCells().empty());
	EXPECT_FALSE(find(*scene, "Rock"));
	scene->onEndRuntime();
}

TEST_F(WorldPartitionTest, StreamingSourceRoundTripsAndClampsItsScale) {
	const auto scene = mkShared<Scene>();
	auto ship = scene->createEntityWithUUID(42, "Ship");
	ship.addComponent<component::StreamingSource>() = {.enabled = false, .radiusScale = 500.f};
	const auto text = SceneSerializer(scene).serializeToString();
	const auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer(std::vector<uint8_t>{text.begin(), text.end()}));
	const auto copy = loaded->findEntityByUUID(42);
	ASSERT_TRUE(copy.hasComponent<component::StreamingSource>());
	EXPECT_FALSE(copy.getComponent<component::StreamingSource>().enabled);
	EXPECT_FLOAT_EQ(copy.getComponent<component::StreamingSource>().radiusScale,
					component::StreamingSource::maxRadiusScale);
}

TEST_F(WorldPartitionTest, AMergeRefusesAMalformedSceneAndRenamesTakenIdentifiers) {
	const auto scene = mkShared<Scene>();
	scene->createEntityWithUUID(9100000000000000001ULL, "Owner");
	const std::string text{g_cellScene};
	const auto parsed = SceneSerializer::parseBuffer(std::vector<uint8_t>{text.begin(), text.end()}, "cell");
	ASSERT_TRUE(parsed.valid);
	const auto merged = SceneSerializer(scene).mergeParsed(parsed, {0.f, 0.f, 0.f});
	ASSERT_TRUE(merged);
	EXPECT_EQ(merged->size(), 3u);
	EXPECT_EQ(scene->getEntityCount(), 4u);
	EXPECT_NE(merged->front().getUUID(), core::UUID{9100000000000000001ULL});
	// The child of the renamed entity follows it.
	EXPECT_EQ(scene->getChildren(merged->front()).size(), 1u);
	EXPECT_TRUE(scene->getChildren(scene->findEntityByUUID(9100000000000000001ULL)).empty());
	const std::string broken = "Scene: Bad\nFormatVersion: 7\nEntities:\n  - Tag: {tag: NoId}\n";
	const auto bad = SceneSerializer::parseBuffer(std::vector<uint8_t>{broken.begin(), broken.end()}, "bad");
	ASSERT_TRUE(bad.valid);
	EXPECT_FALSE(SceneSerializer(scene).mergeParsed(bad, {0.f, 0.f, 0.f}));
	EXPECT_EQ(scene->getEntityCount(), 4u);
}
