/**
 * @file ScriptBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <scene/Entity.h>
#include <script/ScriptEngine.h>
#include <script/ScriptInstance.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::bench {

namespace {

auto toBuffer(const std::string& iText) -> std::vector<uint8_t> { return {iText.begin(), iText.end()}; }

const std::string g_EmptyScript = "function on_update(dt)\nend\n";
const std::string g_MoveScript = "function on_update(dt)\n"
								 "  local x, y, z = transform.get_position(entity_id)\n"
								 "  transform.set_position(entity_id, x + dt, y, z)\n"
								 "end\n";
const std::string g_ArithScript = "function on_update(dt)\n"
								  "  local s = 0\n"
								  "  for i = 1, 100 do s = s + i * dt end\n"
								  "  acc = s\n"
								  "end\n";

void runInstances(Runner& ioRunner, const shared<scene::Scene>& iScene) {
	const auto entities = iScene->getAllEntities();
	const auto uuid = static_cast<uint64_t>(entities.front().getUUID());
	const auto emptyBuffer = toBuffer(g_EmptyScript);
	ioRunner.measure("script/create_instance/empty_script", 1, [&]() -> void {
		const script::ScriptInstance inst;
		inst.setScene(iScene.get());
		doNotOptimize(inst.createFromBuffer(emptyBuffer, "bench_empty", uuid));
	});
	if (ioRunner.wants("script/memory")) {
		std::vector<script::ScriptInstance> pool(1000);
		const size_t before = allocatedBytes();
		for (auto& inst: pool) {
			inst.setScene(iScene.get());
			doNotOptimize(inst.createFromBuffer(emptyBuffer, "bench_empty", uuid));
		}
		ioRunner.metric("script/memory/bytes_per_instance", static_cast<double>(allocatedBytes() - before) / 1000.0,
						"B/instance (lua_State + bindings)");
	}
	for (const auto& [label, source]: {std::pair{"empty", &g_EmptyScript}, std::pair{"get_set_position", &g_MoveScript},
									   std::pair{"arith_100", &g_ArithScript}}) {
		const auto buffer = toBuffer(*source);
		const script::ScriptInstance single;
		single.setScene(iScene.get());
		std::ignore = single.createFromBuffer(buffer, label, uuid);
		ioRunner.measure(std::format("script/on_update/{}/1_instance", label), 1,
						 [&]() -> void { single.onUpdate(0.016f); });
		const script::ScriptInstance unbounded;
		unbounded.setScene(iScene.get());
		unbounded.setQuotas({.memoryBytes = 0, .timePerCallMs = 0});
		std::ignore = unbounded.createFromBuffer(buffer, label, uuid);
		ioRunner.measure(std::format("script/on_update/{}/1_instance_no_quota", label), 1,
						 [&]() -> void { unbounded.onUpdate(0.016f); });
		if (!ioRunner.wants(std::format("script/on_update/{}/1000_instances", label)))
			continue;
		std::vector<script::ScriptInstance> pool(1000);
		for (size_t i = 0; i < pool.size(); ++i) {
			pool[i].setScene(iScene.get());
			std::ignore = pool[i].createFromBuffer(buffer, label, static_cast<uint64_t>(entities[i].getUUID()));
		}
		ioRunner.measure(std::format("script/on_update/{}/1000_instances", label), pool.size(), [&]() -> void {
			for (const auto& inst: pool) inst.onUpdate(0.016f);
		});
	}
}

}// namespace

void runScriptBenches(Runner& ioRunner) {
	if (!ioRunner.wants("script"))
		return;
	const auto scn = makeSpriteScene(10000, Shape::Flat);
	runInstances(ioRunner, scn);
}

}// namespace owl::bench
