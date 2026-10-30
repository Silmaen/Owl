/**
 * @file ParticleCommands_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <renderer/ParticleSystem.h>
#include <renderer/RenderLayer.h>
#include <renderer/RenderLayerFactory.h>
#include <renderer/RenderStack.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/ParticleEmitter.h>
#include <scene/component/Transform.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <variant>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class ParticleCommandTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }

	auto run(const std::string& iName, const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		return m_registry.execute(iName, iArgs, m_scene, m_undo);
	}

	/// Registry under test.
	CommandRegistry m_registry;
	/// Scene changed by the commands.
	shared<scene::Scene> m_scene = mkShared<scene::Scene>();
	/// Undo history of the scene.
	SceneUndoManager m_undo;
};
OWL_DIAG_POP

}// namespace

TEST_F(ParticleCommandTest, EachPresetCreatesAnEmitterAndUndoes) {
	for (const auto& [kind, preset]: {std::pair{std::string{"Fire"}, renderer::ParticlePreset::Fire},
									  std::pair{std::string{"Smoke"}, renderer::ParticlePreset::Smoke},
									  std::pair{std::string{"Sparks"}, renderer::ParticlePreset::Sparks},
									  std::pair{std::string{"Magic"}, renderer::ParticlePreset::Magic},
									  std::pair{std::string{"Snow"}, renderer::ParticlePreset::Snow}}) {
		const auto result =
				run("entity.create_particle_emitter", {{"type", kind}, {"translation", math::vec3{1.f, 2.f, 3.f}}});
		ASSERT_TRUE(result) << result.error().message;
		auto entity = m_scene->findEntityByUUID(result->entity);
		ASSERT_TRUE(entity) << kind;
		EXPECT_EQ(entity.getName(), kind + " Particles");
		ASSERT_TRUE(entity.hasComponent<scene::component::ParticleEmitter>());
		EXPECT_EQ(entity.getComponent<scene::component::ParticleEmitter>().settings.blend,
				  renderer::makeParticlePreset(preset).blend);
		EXPECT_FLOAT_EQ(entity.getComponent<scene::component::Transform>().transform.translation().y(), 2.f);
		ASSERT_TRUE(run("edit.undo", {}));
		EXPECT_FALSE(m_scene->findEntityByUUID(result->entity)) << kind;
		ASSERT_TRUE(run("edit.redo", {}));
		EXPECT_TRUE(m_scene->findEntityByUUID(result->entity)) << kind;
	}
	EXPECT_FALSE(run("entity.create_particle_emitter", {{"type", std::string{"Lava"}}}));
}

TEST_F(ParticleCommandTest, PreviewDrivesTheSystemsWithoutAnUndoEntry) {
	auto entity = m_scene->createEntity("Fx");
	auto& emitter = entity.addComponent<scene::component::ParticleEmitter>();
	emitter.m_slot.system = mkShared<renderer::ParticleSystem>();
	emitter.m_slot.system->play();
	EXPECT_FALSE(m_undo.canUndo());
	ASSERT_TRUE(run("particles.preview", {{"action", std::string{"Pause"}}}));
	EXPECT_FALSE(emitter.m_slot.system->isPlaying());
	ASSERT_TRUE(run("particles.preview", {{"action", std::string{"Play"}}, {"entity", std::string{"Fx"}}}));
	EXPECT_TRUE(emitter.m_slot.system->isPlaying());
	ASSERT_TRUE(run("particles.preview", {{"action", std::string{"Stop"}}}));
	EXPECT_FALSE(emitter.m_slot.system->isEmitting());
	const auto restarted = run("particles.preview", {});
	ASSERT_TRUE(restarted);
	EXPECT_EQ(std::get<std::string>(restarted->data.at("count")), "1");
	EXPECT_TRUE(emitter.m_slot.system->isEmitting());
	ASSERT_TRUE(run("particles.preview", {{"action", std::string{"Burst"}}}));
	EXPECT_FALSE(m_undo.canUndo());
}

TEST_F(ParticleCommandTest, TheFirstEmitterAppendsAParticleLayerToTheProject) {
	if (!renderer::RenderLayerFactory::hasType("RendererParticles"))
		renderer::RenderLayerFactory::registerType(
				"RendererParticles",
				[](const std::string& /*iName*/) -> shared<renderer::RenderLayer> { return nullptr; });
	renderer::RendererStackConfig stack;
	int notified = 0;
	CommandContext context{.scene = m_scene, .undo = &m_undo, .playing = false};
	ASSERT_TRUE(m_registry.execute("entity.create_particle_emitter", {}, context));
	context.projectStack = &stack;
	context.onProjectStackChanged = [&notified]() -> void { ++notified; };
	ASSERT_TRUE(m_registry.execute("entity.create", {{"name", std::string{"Plain"}}}, context));
	ASSERT_TRUE(m_registry.execute("entity.create_particle_emitter", {{"type", std::string{"Snow"}}}, context));
	// The default layer stays first, the particle layer comes last.
	ASSERT_EQ(stack.entries.size(), 2u);
	EXPECT_EQ(stack.entries.back().typeKey, "RendererParticles");
	EXPECT_EQ(notified, 1);
	ASSERT_TRUE(m_registry.execute("entity.create_particle_emitter", {}, context));
	EXPECT_EQ(stack.entries.size(), 2u);
	EXPECT_EQ(notified, 1);
}
