/**
 * @file LuaSequence_test.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Core.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <data/sequence/Sequence.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>
#include <script/ScriptInstance.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;
using data::sequence::Sequence;

namespace {

auto makeSequence() -> Sequence {
	Sequence sequence;
	sequence.duration = 1.f;
	sequence.cameraTracks.push_back(
			{.name = "Shot",
			 .keys = {{.time = 0.f, .position = {0.f, 0.f, 0.f}}, {.time = 1.f, .position = {1.f, 0.f, 0.f}}},
			 .muted = false});
	sequence.eventTracks.push_back(
			{.name = "Cues",
			 .keys = {{.time = 0.f, .function = "on_start_cue"}, {.time = 0.45f, .function = "on_cue"}}});
	return sequence;
}

void run(scene::Scene& ioScene, const int iFrames) {
	core::Timestep step;
	for (int frame = 0; frame < iFrames; ++frame) {
		step.forceUpdate(std::chrono::milliseconds(100));
		ioScene.onUpdateRuntime(step, false);
	}
}

auto attachScript(const shared<scene::Scene>& iScene, scene::Entity& ioEntity, const std::string& iScript)
		-> const ScriptInstance& {
	auto& luaScript = ioEntity.addComponent<scene::component::LuaScript>();
	luaScript.instance = mkUniq<ScriptInstance>();
	luaScript.instance->setScene(iScene.get());
	const std::vector<uint8_t> data(iScript.begin(), iScript.end());
	EXPECT_TRUE(luaScript.instance->createFromBuffer(data, "sequence_test", static_cast<uint64_t>(ioEntity.getUUID())));
	return *luaScript.instance;
}

}// namespace

TEST(LuaBindings, sequenceEventsAndPlayback) {
	core::Log::init(core::Log::Level::Off);
	const auto path = std::filesystem::temp_directory_path() / "lua_cutscene.owlseq";
	ASSERT_TRUE(makeSequence().saveToFile(path));
	const auto scn = mkShared<scene::Scene>();
	app::EngineContext context;
	scn->setEngineContext(&context);
	scn->status = scene::Scene::Status::Playing;
	auto camera = scn->createEntity("Cut");
	camera.addComponent<scene::component::Camera>().primary = false;
	auto entity = scn->createEntity("Cutscene");
	auto& player = entity.addComponent<scene::component::SequencePlayer>();
	player.sequence = path.generic_string();
	player.camera = camera.getUUID();
	player.playOnStart = false;
	player.onFinished = "on_done";
	// Signed, as Lua reads it: an id above 2^63 written unsigned would be a float.
	const auto id = static_cast<int64_t>(static_cast<uint64_t>(entity.getUUID()));
	const std::string script = std::format("starts = 0\n"
										   "cues = 0\n"
										   "finished = 0\n"
										   "function on_start_cue() starts = starts + 1 end\n"
										   "function on_cue() cues = cues + 1 end\n"
										   "function on_done() finished = finished + 1 end\n"
										   "function begin()\n"
										   "  local me = {}\n"
										   "  idle = sequence.is_playing(me) == false\n"
										   "  duration = sequence.get_duration(me)\n"
										   "  played = sequence.play(me)\n"
										   "  playing = sequence.is_playing(me)\n"
										   "  missing = sequence.is_playing(123456) == nil\n"
										   "  sped = sequence.set_speed(me, 1)\n"
										   "end\n"
										   "function jump()\n"
										   "  sought = sequence.seek({}, 0.2)\n"
										   "  time = sequence.get_time({})\n"
										   "  stopped = sequence.stop({})\n"
										   "  again = sequence.stop({})\n"
										   "end\n",
										   id, id, id, id, id);
	const auto& inst = attachScript(scn, entity, script);
	ASSERT_TRUE(inst.callFunction("begin"));
	EXPECT_TRUE(inst.getPropertyBool("idle").value_or(false));
	EXPECT_NEAR(inst.getPropertyFloat("duration").value_or(0.f), 1.f, 1e-5f);
	EXPECT_TRUE(inst.getPropertyBool("played").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("playing").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("missing").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("sped").value_or(false));
	EXPECT_TRUE(camera.getComponent<scene::component::Camera>().primary);
	run(*scn, 5);
	EXPECT_EQ(inst.getPropertyInt("starts").value_or(0), 1);
	EXPECT_EQ(inst.getPropertyInt("cues").value_or(0), 1);
	ASSERT_TRUE(inst.callFunction("jump"));
	EXPECT_TRUE(inst.getPropertyBool("sought").value_or(false));
	EXPECT_NEAR(inst.getPropertyFloat("time").value_or(0.f), 0.2f, 1e-5f);
	EXPECT_TRUE(inst.getPropertyBool("stopped").value_or(false));
	EXPECT_FALSE(inst.getPropertyBool("again").value_or(true));
	EXPECT_EQ(inst.getPropertyInt("finished").value_or(0), 1);
	EXPECT_EQ(inst.getPropertyInt("cues").value_or(0), 1);// the seek called no event
	scn->setEngineContext(nullptr);
	std::filesystem::remove(path);
	core::Log::invalidate();
}
