/**
 * @file skeletalAnimation_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/animation/SkeletalClip.h>
#include <data/animation/Skeleton.h>
#include <data/animation/StateMachine.h>
#include <data/geometry/MeshLoader.h>
#include <data/geometry/MeshLod.h>
#include <math/linAlgebra.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numbers>
#include <span>
#include <vector>

using namespace owl;
using namespace owl::data::animation;
using namespace owl::data::geometry;

namespace {

auto armPath() -> std::filesystem::path { return test::getTestAssetsPath("mesh_tests") / "skinned_arm.gltf"; }

auto readBytes(const std::filesystem::path& iPath) -> std::vector<uint8_t> {
	std::ifstream file(iPath, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

auto aboutZ(const float iDegrees) -> math::quat {
	const float half = iDegrees * std::numbers::pi_v<float> / 360.f;
	return {std::cos(half), 0.f, 0.f, std::sin(half)};
}

auto twoBones() -> Skeleton {
	const Bone root{.name = "root",
					.parent = -1,
					.inverseBind = math::identity<float, 4>(),
					.rest = {},
					.anchor = math::identity<float, 4>()};
	const Bone lower{
			.name = "lower",
			.parent = 0,
			.inverseBind = math::translate(math::identity<float, 4>(), math::vec3{0.f, -1.f, 0.f}),
			.rest = {.translation = {0.f, 1.f, 0.f}, .rotation = {1.f, 0.f, 0.f, 0.f}, .scale = {1.f, 1.f, 1.f}},
			.anchor = math::identity<float, 4>()};
	return Skeleton{{root, lower}};
}

auto bendClip() -> SkeletalClip {
	SkeletalClip clip;
	clip.name = "bend";
	clip.duration = 1.f;
	BoneTrack track;
	track.bone = 1;
	track.rotation.times = {0.f, 1.f};
	track.rotation.values = {aboutZ(0.f), aboutZ(90.f)};
	clip.tracks.push_back(track);
	return clip;
}

}// namespace

TEST(SkeletalAnimation, RestPoseGivesIdentitySkin) {
	const auto skeleton = twoBones();
	ASSERT_TRUE(skeleton.isValid());
	std::vector<math::mat4> model(2);
	std::vector<math::mat4> skin(2);
	skeleton.computeModelMatrices(skeleton.getRestPose(), model);
	skeleton.computeSkinMatrices(model, skin);
	for (const auto& matrix: skin) {
		for (size_t row = 0; row < 4; ++row)
			for (size_t col = 0; col < 4; ++col) EXPECT_NEAR(matrix(row, col), row == col ? 1.f : 0.f, 1e-5f);
	}
	EXPECT_EQ(skeleton.findBone("lower"), size_t{1});
	EXPECT_FALSE(skeleton.findBone("missing").has_value());
}

TEST(SkeletalAnimation, InvalidOrderIsRejected) {
	Bone first;
	first.name = "a";
	first.parent = 1;
	Bone second;
	second.name = "b";
	const Skeleton skeleton{{first, second}};
	EXPECT_FALSE(skeleton.isValid());
}

TEST(SkeletalAnimation, ClipSamplesAndBends) {
	const auto skeleton = twoBones();
	const auto clip = bendClip();
	auto pose = skeleton.getRestPose();
	clip.sample(0.5f, false, pose);
	const auto expected = aboutZ(45.f);
	EXPECT_NEAR(std::abs(pose[1].rotation.dot(expected)), 1.f, 1e-4f);
	clip.sample(2.f, false, pose);
	EXPECT_NEAR(std::abs(pose[1].rotation.dot(aboutZ(90.f))), 1.f, 1e-4f);
	EXPECT_NEAR(clip.getLocalTime(1.25f, true), 0.25f, 1e-5f);
	std::vector<math::mat4> model(2);
	std::vector<math::mat4> skin(2);
	skeleton.computeModelMatrices(pose, model);
	skeleton.computeSkinMatrices(model, skin);
	// The tip of the arm, 2 m up at rest, ends 1 m left of the elbow.
	const math::vec4 tip = skin[1] * math::vec4{0.f, 2.f, 0.f, 1.f};
	EXPECT_NEAR(tip.x(), -1.f, 1e-4f);
	EXPECT_NEAR(tip.y(), 1.f, 1e-4f);
}

TEST(SkeletalAnimation, StepTrackHoldsTheKey) {
	VectorTrack track;
	track.times = {0.f, 1.f};
	track.values = {{0.f, 0.f, 0.f}, {2.f, 0.f, 0.f}};
	track.interpolation = Interpolation::Step;
	EXPECT_NEAR(track.sample(0.9f).x(), 0.f, 1e-6f);
	track.interpolation = Interpolation::Linear;
	EXPECT_NEAR(track.sample(0.5f).x(), 1.f, 1e-6f);
}

TEST(SkeletalAnimation, BlendTakesTheShortestArc) {
	const BoneTransform from{.translation = {0.f, 0.f, 0.f}, .rotation = aboutZ(10.f), .scale = {1.f, 1.f, 1.f}};
	BoneTransform to{.translation = {2.f, 0.f, 0.f}, .rotation = aboutZ(30.f), .scale = {3.f, 1.f, 1.f}};
	to.rotation = to.rotation * -1.f;
	const auto mid = BoneTransform::blend(from, to, 0.5f);
	EXPECT_NEAR(mid.translation.x(), 1.f, 1e-6f);
	EXPECT_NEAR(mid.scale.x(), 2.f, 1e-6f);
	EXPECT_NEAR(std::abs(mid.rotation.dot(aboutZ(20.f))), 1.f, 1e-4f);
	Pose out;
	blendPoses({from}, {to}, 1.f, out);
	ASSERT_EQ(out.size(), 1u);
	EXPECT_NEAR(out[0].translation.x(), 2.f, 1e-6f);
}

TEST(SkeletalAnimation, StateMachineFollowsTransitions) {
	StateMachine machine;
	machine.states = {{.name = "idle", .clip = "bend", .speed = 1.f, .loop = true},
					  {.name = "walk", .clip = "bend", .speed = 2.f, .loop = true},
					  {.name = "jump", .clip = "", .speed = 1.f, .loop = false}};
	machine.parameters = {{.name = "speed", .type = ParameterType::Float, .value = 0.f},
						  {.name = "jump", .type = ParameterType::Trigger, .value = 0.f}};
	machine.transitions = {{.from = "idle",
							.to = "walk",
							.duration = 0.5f,
							.exitTime = -1.f,
							.conditions = {{.parameter = "speed", .op = ConditionOp::Greater, .threshold = 0.1f}}},
						   {.from = "*",
							.to = "jump",
							.duration = 0.f,
							.exitTime = -1.f,
							.conditions = {{.parameter = "jump", .op = ConditionOp::IsTrue, .threshold = 0.f}}},
						   {.from = "jump", .to = "idle", .duration = 0.f, .exitTime = -1.f, .conditions = {}}};
	const std::vector<SkeletalClip> clips{bendClip()};
	StateMachinePlayer player;
	EXPECT_FALSE(player.isReady());
	player.reset(machine, clips);
	EXPECT_EQ(player.getCurrentState(), size_t{0});
	player.update(machine, clips, 0.1f);
	EXPECT_EQ(player.getCurrentState(), size_t{0});
	EXPECT_TRUE(player.setParameter(machine, "speed", 1.f));
	EXPECT_FALSE(player.setParameter(machine, "missing", 1.f));
	player.update(machine, clips, 0.1f);
	EXPECT_EQ(player.getCurrentState(), size_t{1});
	player.update(machine, clips, 0.25f);
	EXPECT_NEAR(player.getBlendWeight(), 0.5f, 1e-5f);
	EXPECT_NEAR(player.getStateTime(), 0.5f, 1e-5f);
	const auto skeleton = twoBones();
	Pose pose;
	player.evaluate(machine, skeleton, clips, pose);
	EXPECT_EQ(pose.size(), 2u);
	EXPECT_TRUE(player.setParameter(machine, "jump", 1.f));
	player.update(machine, clips, 0.01f);
	EXPECT_EQ(player.getCurrentState(), size_t{2});
	EXPECT_EQ(player.getParameter(machine, "jump"), 0.f);
	// A state without clip has a zero normalized time: the unconditional exit waits for the end of a clip.
	player.update(machine, clips, 0.01f);
	EXPECT_EQ(player.getCurrentState(), size_t{2});
	EXPECT_TRUE(player.play(machine, "idle", 0.f));
	EXPECT_FALSE(player.play(machine, "nowhere", 0.f));
	EXPECT_EQ(player.getCurrentState(), size_t{0});
}

TEST(SkeletalAnimation, ExitTimeWaitsForTheClip) {
	StateMachine machine;
	machine.states = {{.name = "a", .clip = "bend", .speed = 1.f, .loop = false},
					  {.name = "b", .clip = "bend", .speed = 1.f, .loop = true}};
	machine.transitions = {{.from = "a", .to = "b", .duration = 0.f, .exitTime = 0.8f, .conditions = {}}};
	machine.entryState = "a";
	const std::vector<SkeletalClip> clips{bendClip()};
	StateMachinePlayer player;
	player.reset(machine, clips);
	player.update(machine, clips, 0.5f);
	EXPECT_EQ(player.getCurrentState(), size_t{0});
	player.update(machine, clips, 0.4f);
	EXPECT_EQ(player.getCurrentState(), size_t{1});
}

TEST(SkeletalAnimation, LoaderReadsTheSkinTheClipsAndTheSockets) {
	const auto loaded = MeshLoader::loadMesh(armPath());
	ASSERT_TRUE(loaded.mesh);
	ASSERT_TRUE(loaded.isSkinned());
	ASSERT_EQ(loaded.skeleton->getBoneCount(), 2u);
	EXPECT_EQ(loaded.skeleton->getBones()[1].name, "lower");
	EXPECT_EQ(loaded.skeleton->getBones()[1].parent, 0);
	ASSERT_EQ(loaded.clips.size(), 2u);
	EXPECT_EQ(loaded.clips[0].name, "bend");
	EXPECT_EQ(loaded.clips[1].name, "bent");
	EXPECT_NEAR(loaded.clips[0].duration, 1.f, 1e-5f);
	ASSERT_EQ(loaded.sockets.size(), 1u);
	EXPECT_EQ(loaded.sockets[0].name, "tip");
	EXPECT_EQ(loaded.sockets[0].bone, "lower");
	EXPECT_NEAR(loaded.sockets[0].offset(1, 3), 1.f, 1e-5f);
	size_t upper = 0;
	for (size_t vertex = 0; vertex < loaded.skin.size(); ++vertex) {
		const auto& skin = loaded.skin[vertex];
		EXPECT_NEAR(skin.weights.x() + skin.weights.y() + skin.weights.z() + skin.weights.w(), 1.f, 1e-5f);
		if (loaded.mesh->getVertices()[vertex].getPosition().y() > 1.5f) {
			EXPECT_EQ(skin.joints[0], 1);
			++upper;
		}
	}
	EXPECT_GT(upper, 0u);
	std::vector<math::mat4> model(2);
	std::vector<math::mat4> skin(2);
	auto pose = loaded.skeleton->getRestPose();
	loaded.clips[0].sample(1.f, false, pose);
	loaded.skeleton->computeModelMatrices(pose, model);
	loaded.skeleton->computeSkinMatrices(model, skin);
	const math::vec4 tip = skin[1] * math::vec4{0.f, 2.f, 0.f, 1.f};
	EXPECT_NEAR(tip.x(), -1.f, 1e-4f);
}

TEST(SkeletalAnimation, StaticMeshHasNoSkin) {
	const auto loaded = MeshLoader::loadMesh(test::getTestAssetsPath("mesh_tests") / "simple_triangle.gltf");
	ASSERT_TRUE(loaded.mesh);
	EXPECT_FALSE(loaded.isSkinned());
	EXPECT_TRUE(loaded.skin.empty());
	EXPECT_TRUE(loaded.clips.empty());
}

TEST(MeshLod, ClusteringReducesAGrid) {
	std::vector<math::vec3> corners;
	constexpr size_t side = 32;
	for (size_t y = 0; y < side; ++y) {
		for (size_t x = 0; x < side; ++x) {
			const auto fx = static_cast<float>(x);
			const auto fy = static_cast<float>(y);
			corners.insert(corners.end(), {{fx, fy, 0.f}, {fx + 1.f, fy, 0.f}, {fx + 1.f, fy + 1.f, 0.f}});
			corners.insert(corners.end(), {{fx, fy, 0.f}, {fx + 1.f, fy + 1.f, 0.f}, {fx, fy + 1.f, 0.f}});
		}
	}
	const size_t triangles = corners.size() / 3;
	const auto kept = simplifyCorners(corners, 0.25f);
	EXPECT_EQ(kept.size() % 3, 0u);
	EXPECT_LE(kept.size() / 3, triangles / 4);
	EXPECT_GT(kept.size() / 3, triangles / 32);
	for (const uint32_t corner: kept) EXPECT_LT(corner, corners.size());
	EXPECT_TRUE(simplifyCorners({}, 0.5f).empty());
}

TEST(MeshLod, LevelsAndCookedContainerRoundTrip) {
	const auto loaded = MeshLoader::loadMesh(armPath());
	ASSERT_TRUE(loaded.mesh);
	const auto none = buildMeshLods(loaded, {});
	EXPECT_TRUE(none.empty());
	const auto lods = buildMeshLods(loaded, {.levelCount = 2, .reduction = 0.5f, .screenSize = 0.4f});
	ASSERT_FALSE(lods.empty());
	EXPECT_LT(lods[0].getTriangleCount(), loaded.mesh->getTriangleCount());
	EXPECT_NEAR(lods[0].screenSize, 0.4f, 1e-6f);
	const std::vector<uint8_t> source{1, 2, 3, 4, 5};
	const auto cooked = writeCookedMesh(source, lods);
	EXPECT_TRUE(isCookedMesh(cooked));
	EXPECT_FALSE(isCookedMesh(source));
	const auto back = readCookedMesh(cooked);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(std::vector<uint8_t>(back->source.begin(), back->source.end()), source);
	ASSERT_EQ(back->lods.size(), lods.size());
	EXPECT_EQ(back->lods[0].submeshCorners, lods[0].submeshCorners);
	auto truncated = cooked;
	truncated.resize(truncated.size() - 3);
	EXPECT_FALSE(readCookedMesh(truncated).has_value());
}

TEST(MeshLod, CookedMeshLoadsWithItsLevels) {
	const auto bytes = readBytes(armPath());
	ASSERT_FALSE(bytes.empty());
	const auto loaded = MeshLoader::loadMeshFromMemory(bytes, ".gltf", "arm");
	const auto lods = buildMeshLods(loaded, {.levelCount = 1, .reduction = 0.5f, .screenSize = 0.3f});
	ASSERT_EQ(lods.size(), 1u);
	const auto cooked = writeCookedMesh(bytes, lods);
	const auto again = MeshLoader::loadMeshFromMemory(cooked, ".gltf", "arm");
	ASSERT_TRUE(again.mesh);
	EXPECT_TRUE(again.isSkinned());
	ASSERT_EQ(again.lods.size(), 1u);
	EXPECT_EQ(again.lods[0].getTriangleCount(), lods[0].getTriangleCount());
}
