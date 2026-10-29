/**
 * @file SkeletalAnimation_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <renderer/MeshCache.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <renderer/RendererMesh.h>
#include <scene/ComponentRegistry.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <string>

using namespace owl;

namespace {

auto armPath() -> std::filesystem::path { return test::getTestAssetsPath("mesh_tests") / "skinned_arm.gltf"; }

auto meshCache() -> renderer::MeshCache& { return app::Application::get().getEngineContext().getMeshCache(); }

auto makeStep(const int iMs) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(iMs));
	return ts;
}

auto bendMachine() -> data::animation::StateMachine {
	data::animation::StateMachine machine;
	machine.states = {{.name = "rest", .clip = "", .speed = 1.f, .loop = true},
					  {.name = "bend", .clip = "bend", .speed = 1.f, .loop = false}};
	machine.parameters = {{.name = "go", .type = data::animation::ParameterType::Trigger, .value = 0.f}};
	machine.transitions = {
			{.from = "rest",
			 .to = "bend",
			 .duration = 0.f,
			 .exitTime = -1.f,
			 .conditions = {{.parameter = "go", .op = data::animation::ConditionOp::IsTrue, .threshold = 0.f}}}};
	return machine;
}

class SkeletalSceneTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "skeletal",
														  .assetsPattern = "",
														  .icon = "",
														  .width = 0,
														  .height = 0,
														  .argCount = 0,
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
	}

	static void TearDownTestSuite() {
		app::Application::invalidate();
		s_app.reset();
		core::Log::invalidate();
	}

	void SetUp() override {
		meshCache().clear();
		renderer::RendererStackConfig config;
		config.entries.push_back({.typeKey = "RendererMesh", .name = "mesh_world", .defaultConfig = {}});
		renderer::Renderer::setRenderStack(
				renderer::RenderStack::buildFromConfig(config, renderer::EnabledRenderersConfig{}));
	}

	void TearDown() override {
		meshCache().clear();
		renderer::Renderer::setRenderStack(renderer::RenderStack{});
	}

	// A camera looking at the arm, which carries an animator and holds an item on its tip.
	auto makeScene() -> shared<scene::Scene> {
		auto sc = mkShared<scene::Scene>();
		auto cam = sc->createEntity("Camera");
		cam.getComponent<scene::component::Transform>().transform.translation() = math::vec3{0.f, 1.f, 10.f};
		auto& camera = cam.addComponent<scene::component::Camera>();
		camera.primary = true;
		camera.camera.setPerspective(0.785f, 0.1f, 1000.f);
		m_arm = sc->createEntity("Arm");
		m_arm.addComponent<scene::component::MeshRenderer>().setMeshPath(armPath());
		m_arm.addComponent<scene::component::Animator>().machine = bendMachine();
		m_item = sc->createEntity("Item");
		sc->onViewportResize({320, 180});
		return sc;
	}

	inline static shared<app::Application> s_app;
	scene::Entity m_arm;
	scene::Entity m_item;
};

}// namespace

TEST_F(SkeletalSceneTest, componentsAreRegisteredAndRoundTrip) {
	for (const char* key: {"Animator", "MeshSockets", "SocketAttachment"}) {
		const auto* desc = scene::ComponentRegistry::find(key);
		ASSERT_NE(desc, nullptr) << key;
		EXPECT_TRUE(desc->optional);
	}
	const auto sc = mkShared<scene::Scene>();
	auto ent = sc->createEntity("Hero");
	auto& animator = ent.addComponent<scene::component::Animator>();
	animator.machine = bendMachine();
	animator.machine.entryState = "rest";
	animator.speed = 1.5f;
	animator.preview = true;
	auto& sockets = ent.addComponent<scene::component::MeshSockets>();
	sockets.sockets.push_back({.name = "hand", .bone = "lower", .offset = {}});
	sockets.sockets.back().offset.translation() = {0.f, 0.5f, 0.f};
	ent.addComponent<scene::component::SocketAttachment>().socket = "back";
	const auto yaml = scene::SceneSerializer::serializeEntityToString(ent);
	const auto copy = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer::deserializeEntityFromString(copy, yaml));
	const auto restored = copy->findEntityByUUID(ent.getUUID());
	ASSERT_TRUE(restored);
	const auto& readAnimator = restored.getComponent<scene::component::Animator>();
	EXPECT_FLOAT_EQ(readAnimator.speed, 1.5f);
	EXPECT_TRUE(readAnimator.preview);
	EXPECT_EQ(readAnimator.machine.entryState, "rest");
	ASSERT_EQ(readAnimator.machine.states.size(), 2u);
	EXPECT_EQ(readAnimator.machine.states[1].clip, "bend");
	EXPECT_FALSE(readAnimator.machine.states[1].loop);
	ASSERT_EQ(readAnimator.machine.transitions.size(), 1u);
	ASSERT_EQ(readAnimator.machine.transitions[0].conditions.size(), 1u);
	EXPECT_EQ(readAnimator.machine.transitions[0].conditions[0].op, data::animation::ConditionOp::IsTrue);
	EXPECT_EQ(readAnimator.machine.parameters[0].type, data::animation::ParameterType::Trigger);
	const auto& readSockets = restored.getComponent<scene::component::MeshSockets>();
	ASSERT_EQ(readSockets.sockets.size(), 1u);
	EXPECT_EQ(readSockets.sockets[0].bone, "lower");
	EXPECT_FLOAT_EQ(readSockets.sockets[0].offset.translation().y(), 0.5f);
	EXPECT_EQ(restored.getComponent<scene::component::SocketAttachment>().socket, "back");
}

TEST_F(SkeletalSceneTest, runtimeFramesSkinTheMeshAndMoveTheAttachedItem) {
	const auto sc = makeScene();
	sc->onStartRuntime();
	sc->onUpdateRuntime(makeStep(16));
	sc->onUpdateRuntime(makeStep(16));
	const auto& animator = m_arm.getComponent<scene::component::Animator>();
	ASSERT_TRUE(animator.hasPose(2));
	EXPECT_GT(renderer::RendererMesh::getStatistics().skinnedDrawCount, 0u);
	const auto names = sc->getSocketNames(m_arm);
	ASSERT_EQ(names.size(), 1u);
	EXPECT_EQ(names[0], "tip");
	ASSERT_TRUE(sc->attachToSocket(m_item, m_arm, "tip"));
	EXPECT_FALSE(sc->attachToSocket(m_arm, m_item, "tip"));
	EXPECT_FALSE(sc->attachToSocket(m_item, m_arm, "missing"));
	EXPECT_EQ(m_item.getComponent<scene::component::Hierarchy>().parentId, m_arm.getUUID());
	// At rest the tip socket sits 2 m up, straight above the root.
	EXPECT_NEAR(m_item.getComponent<scene::component::Transform>().transform.translation().y(), 2.f, 1e-4f);
	auto& player = m_arm.getComponent<scene::component::Animator>();
	ASSERT_TRUE(player.player.setParameter(player.machine, "go", 1.f));
	for (int frame = 0; frame < 80; ++frame) sc->onUpdateRuntime(makeStep(16));
	EXPECT_EQ(player.player.getCurrentState(), size_t{1});
	const auto& tip = m_item.getComponent<scene::component::Transform>().transform.translation();
	EXPECT_NEAR(tip.x(), -1.f, 1e-3f);
	EXPECT_NEAR(tip.y(), 1.f, 1e-3f);
	EXPECT_TRUE(sc->detachFromSocket(m_item));
	EXPECT_FALSE(sc->detachFromSocket(m_item));
	EXPECT_EQ(m_item.getComponent<scene::component::Hierarchy>().parentId, m_arm.getUUID());
	sc->onEndRuntime();
}

TEST_F(SkeletalSceneTest, componentSocketsOverrideMeshSocketsAndEditModeHoldsTheRestPose) {
	const auto sc = makeScene();
	auto& sockets = m_arm.addComponent<scene::component::MeshSockets>();
	sockets.sockets.push_back({.name = "tip", .bone = "", .offset = {}});
	sockets.sockets.back().offset.translation() = {3.f, 0.f, 0.f};
	sc->onRenderRuntime();
	const auto local = sc->getSocketTransform(m_arm, "tip");
	ASSERT_TRUE(local.has_value());
	EXPECT_NEAR((*local)(0, 3), 3.f, 1e-5f);
	EXPECT_FALSE(sc->getSocketTransform(m_arm, "nowhere").has_value());
	sc->updateAnimators(0.5f, true);
	EXPECT_FALSE(m_arm.getComponent<scene::component::Animator>().hasPose(2));
	m_arm.getComponent<scene::component::Animator>().preview = true;
	sc->updateAnimators(0.5f, true);
	EXPECT_TRUE(m_arm.getComponent<scene::component::Animator>().hasPose(2));
}

TEST_F(SkeletalSceneTest, lodsAreBuiltAndPickedByScreenSize) {
	const auto loaded = data::geometry::MeshLoader::loadMesh(armPath());
	const renderer::MeshResource resource{armPath(), loaded, {.levelCount = 2, .reduction = 0.5f, .screenSize = 0.4f}};
	ASSERT_GE(resource.getLodCount(), 1u);
	EXPECT_TRUE(resource.isSkinned());
	EXPECT_LT(resource.getLodTriangleCount(1), resource.getLodTriangleCount(0));
	EXPECT_EQ(resource.selectLod(0.9f), 0u);
	EXPECT_EQ(resource.selectLod(0.3f), 1u);
	EXPECT_EQ(resource.selectLod(0.001f), resource.getLodCount());
	EXPECT_FLOAT_EQ(resource.getLodScreenSize(1), 0.4f);
	EXPECT_FLOAT_EQ(resource.getLodScreenSize(0), 1.f);
	EXPECT_EQ(resource.getLodTriangleCount(9), 0u);
}
