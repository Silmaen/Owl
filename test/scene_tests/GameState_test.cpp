/**
 * @file GameState_test.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Serializer.h>
#include <core/SerializerImpl.h>
#include <scene/GameState.h>
#include <scene/Scene.h>

using namespace owl;
using namespace owl::scene;

TEST(GameState, setGetInt) {
	GameState gs;
	gs.set("coins", int64_t{42});
	const auto val = gs.get("coins");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(std::get<int64_t>(val.value()), 42);
}

TEST(GameState, setGetFloat) {
	GameState gs;
	gs.set("speed", 3.14f);
	const auto val = gs.get("speed");
	ASSERT_TRUE(val.has_value());
	EXPECT_NEAR(std::get<float>(val.value()), 3.14f, 0.01f);
}

TEST(GameState, setGetString) {
	GameState gs;
	gs.set("name", std::string("player1"));
	const auto val = gs.get("name");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(std::get<std::string>(val.value()), "player1");
}

TEST(GameState, setGetBool) {
	GameState gs;
	gs.set("unlocked", true);
	const auto val = gs.get("unlocked");
	ASSERT_TRUE(val.has_value());
	EXPECT_TRUE(std::get<bool>(val.value()));
}

TEST(GameState, getDefault) {
	GameState gs;
	const auto val = gs.get("missing", int64_t{99});
	EXPECT_EQ(std::get<int64_t>(val), 99);
	EXPECT_FALSE(gs.get("missing").has_value());
}

TEST(GameState, overwrite) {
	GameState gs;
	gs.set("score", int64_t{10});
	gs.set("score", int64_t{20});
	EXPECT_EQ(std::get<int64_t>(gs.get("score").value()), 20);
}

TEST(GameState, remove) {
	GameState gs;
	gs.set("temp", int64_t{1});
	EXPECT_TRUE(gs.get("temp").has_value());
	gs.remove("temp");
	EXPECT_FALSE(gs.get("temp").has_value());
}

TEST(GameState, clearAndEmpty) {
	GameState gs;
	EXPECT_TRUE(gs.empty());
	EXPECT_EQ(gs.size(), 0);
	gs.set("a", int64_t{1});
	gs.set("b", 2.f);
	EXPECT_FALSE(gs.empty());
	EXPECT_EQ(gs.size(), 2);
	gs.clear();
	EXPECT_TRUE(gs.empty());
	EXPECT_EQ(gs.size(), 0);
}

TEST(GameState, keys) {
	GameState gs;
	gs.set("x", int64_t{1});
	gs.set("y", 2.f);
	gs.set("z", std::string("three"));
	const auto k = gs.keys();
	EXPECT_EQ(k.size(), 3);
	EXPECT_TRUE(std::ranges::find(k, "x") != k.end());
	EXPECT_TRUE(std::ranges::find(k, "y") != k.end());
	EXPECT_TRUE(std::ranges::find(k, "z") != k.end());
}

#ifndef OWL_BUILD_SHARED
// Serialization tests use Serializer/SerializerImpl (private, not exported in shared builds).
TEST(GameState, serializeRoundTrip) {
	core::Log::init(core::Log::Level::Off);

	GameState original;
	original.set("coins", int64_t{42});
	original.set("speed", 3.14f);
	original.set("name", std::string("hero"));
	original.set("alive", true);

	// Serialize.
	core::Serializer serializer;
	serializer.getImpl()->emitter << YAML::BeginMap;
	original.serialize(serializer);
	serializer.getImpl()->emitter << YAML::EndMap;

	// Deserialize.
	const YAML::Node root = YAML::Load(serializer.getImpl()->emitter.c_str());
	core::Serializer deserCtx;
	deserCtx.getImpl()->node = root;

	GameState loaded;
	loaded.deserialize(deserCtx);

	EXPECT_EQ(loaded.size(), 4);
	EXPECT_EQ(std::get<int64_t>(loaded.get("coins").value()), 42);
	EXPECT_NEAR(std::get<float>(loaded.get("speed").value()), 3.14f, 0.01f);
	EXPECT_EQ(std::get<std::string>(loaded.get("name").value()), "hero");
	EXPECT_TRUE(std::get<bool>(loaded.get("alive").value()));

	core::Log::invalidate();
}
#endif

TEST(GameState, sceneCopyPreservesState) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	scn->getGameState().set("level", int64_t{3});
	scn->getGameState().set("progress", 0.75f);

	auto copy = Scene::copy(scn);
	EXPECT_EQ(std::get<int64_t>(copy->getGameState().get("level").value()), 3);
	EXPECT_NEAR(std::get<float>(copy->getGameState().get("progress").value()), 0.75f, 0.01f);

	// Modifications to copy don't affect original.
	copy->getGameState().set("level", int64_t{4});
	EXPECT_EQ(std::get<int64_t>(scn->getGameState().get("level").value()), 3);

	core::Log::invalidate();
}
