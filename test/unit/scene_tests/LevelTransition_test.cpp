/**
 * @file LevelTransition_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/LevelTransition.h>
#include <scene/SaveManager.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/Player.h>
#include <scene/component/Tag.h>
#include <scene/component/Transform.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <system_error>
#include <tuple>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {
struct TempRoot {
	TempRoot()
		: path{std::filesystem::temp_directory_path() /
			   ("owl_level_transition_" +
				std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))} {
		create_directories(path / "scenes");
	}
	TempRoot(const TempRoot&) = delete;
	TempRoot(TempRoot&&) = delete;
	auto operator=(const TempRoot&) -> TempRoot& = delete;
	auto operator=(TempRoot&&) -> TempRoot& = delete;
	~TempRoot() {
		std::error_code ec;
		std::filesystem::remove_all(path, ec);
	}
	std::filesystem::path path;
};

auto sceneText() -> std::string {
	const auto level = mkShared<Scene>();
	std::ignore = level->createEntity("Arrival");
	const SceneSerializer serializer(level);
	return serializer.serializeToString();
}

void writeText(const std::filesystem::path& iPath, const std::string& iText) {
	std::ofstream out(iPath, std::ios::binary);
	out << iText;
}
}// namespace

TEST(LevelTransition, ResolveLevelNameAddsTheExtensionOnce) {
	EXPECT_EQ(LevelTransition::resolveLevelName("cave"), "cave.owl");
	EXPECT_EQ(LevelTransition::resolveLevelName("cave.owl"), "cave.owl");
}

TEST(LevelTransition, ReadLevelSearchesTheRootThenScenes) {
	core::Log::init(core::Log::Level::Off);
	const TempRoot root;
	writeText(root.path / "scenes" / "cave.owl", "in scenes");
	auto source = LevelTransition::readLevel("cave", {root.path});
	ASSERT_TRUE(source.has_value());
	EXPECT_EQ(std::string(source->bytes.begin(), source->bytes.end()), "in scenes");
	writeText(root.path / "cave.owl", "at root");
	source = LevelTransition::readLevel("cave.owl", {root.path});
	ASSERT_TRUE(source.has_value());
	EXPECT_EQ(std::string(source->bytes.begin(), source->bytes.end()), "at root");
	EXPECT_EQ(source->sourceName, (root.path / "cave.owl").string());
	core::Log::invalidate();
}

TEST(LevelTransition, ReadLevelMissesAnUnknownLevel) {
	core::Log::init(core::Log::Level::Off);
	const TempRoot root;
	EXPECT_FALSE(LevelTransition::readLevel("nowhere", {root.path}).has_value());
	EXPECT_FALSE(LevelTransition::readLevel("nowhere", {}).has_value());
	core::Log::invalidate();
}

TEST(LevelTransition, LoadLevelCarriesTheGameState) {
	core::Log::init(core::Log::Level::Off);
	const auto text = sceneText();
	const std::vector<uint8_t> bytes(text.begin(), text.end());
	const auto parsed = SceneSerializer::parseBuffer(bytes, "cave.owl");
	Scene current;
	current.getGameState().set("coins", int64_t{7});
	const auto level = LevelTransition::loadLevel(parsed, current);
	ASSERT_NE(level, nullptr);
	EXPECT_EQ(std::get<int64_t>(level->getGameState().get("coins", int64_t{0})), 7);
	bool found = false;
	for (const auto view = level->registry.view<component::Tag>(); const auto ent: view)
		found = found || view.get<component::Tag>(ent).tag == "Arrival";
	EXPECT_TRUE(found);
	core::Log::invalidate();
}

TEST(LevelTransition, LoadLevelRefusesAnInvalidFile) {
	core::Log::init(core::Log::Level::Off);
	const std::string text = "not: [a scene";
	const std::vector<uint8_t> bytes(text.begin(), text.end());
	const auto parsed = SceneSerializer::parseBuffer(bytes, "broken.owl");
	const Scene current;
	EXPECT_EQ(LevelTransition::loadLevel(parsed, current), nullptr);
	core::Log::invalidate();
}

TEST(LevelTransition, PlaceArrivalMovesThePlayerOnTheTarget) {
	core::Log::init(core::Log::Level::Off);
	Scene level;
	auto player = level.createEntity("Hero");
	player.addComponent<component::Player>();
	auto target = level.createEntity("Door");
	auto& targetTransform = target.getComponent<component::Transform>().transform;
	targetTransform.translation() = {3.f, -2.f, 0.f};
	targetTransform.rotation().z() = std::numbers::pi_v<float> / 2.f;
	LevelTransition::placeArrival(level, "Door", {1.f, 0.f});
	const auto& placed = player.getComponent<component::Transform>().transform;
	EXPECT_FLOAT_EQ(placed.translation().x(), 3.f);
	EXPECT_FLOAT_EQ(placed.translation().y(), -2.f);
	EXPECT_FLOAT_EQ(placed.rotation().z(), std::numbers::pi_v<float> / 2.f);
	LevelTransition::placeArrival(level, "Missing", {1.f, 0.f});
	EXPECT_FLOAT_EQ(player.getComponent<component::Transform>().transform.translation().x(), 3.f);
	core::Log::invalidate();
}

TEST(LevelTransition, LoadSavedGameKeepsTheLevelWhenTheSlotIsEmpty) {
	core::Log::init(core::Log::Level::Off);
	SaveManager::setGameName("OwlTestLevelTransition_" +
							 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	Scene current;
	std::ignore = current.createEntity("Stays");
	EXPECT_EQ(LevelTransition::loadSavedGame(42, current, {64, 64}), nullptr);
	EXPECT_EQ(current.registry.view<component::Tag>().size(), 1u);
	std::error_code ec;
	std::filesystem::remove_all(SaveManager::getSaveDirectory().parent_path(), ec);
	core::Log::invalidate();
}
