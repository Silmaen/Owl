/**
 * @file SerializeBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <scene/Entity.h>
#include <scene/PrefabSerializer.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

namespace owl::bench {

namespace {

void runSceneRoundTrip(Runner& ioRunner) {
	for (const uint32_t count: {1000U, 10000U}) {
		if (!ioRunner.wants("serialize/scene"))
			return;
		const auto scn = makeSpriteScene(count, Shape::Forest);
		const scene::SceneSerializer saver(scn);
		std::string yaml;
		ioRunner.measureWithSetup(
				std::format("serialize/scene_to_string/{}", count), count, [&]() -> void { yaml.clear(); },
				[&]() -> void { yaml = saver.serializeToString(); });
		yaml = saver.serializeToString();
		ioRunner.metric(std::format("serialize/scene_yaml_size/{}", count), static_cast<double>(yaml.size()), "bytes");
		const std::vector<uint8_t> buffer(yaml.begin(), yaml.end());
		shared<scene::Scene> target;
		ioRunner.measureWithSetup(
				std::format("serialize/scene_from_string/{}", count), count,
				[&]() -> void {
					target.reset();
					target = mkShared<scene::Scene>();
				},
				[&]() -> void {
					const scene::SceneSerializer loader(target);
					doNotOptimize(loader.deserializeFromBuffer(buffer));
				});
		ioRunner.measure(std::format("serialize/scene_parse_only/{}", count), count,
						 [&]() -> void { doNotOptimize(scene::SceneSerializer::parseBuffer(buffer).valid); });
	}
}

void runEntitySnapshot(Runner& ioRunner) {
	for (const uint32_t count: {1U, 10000U}) {
		if (!ioRunner.wants("serialize/entity"))
			return;
		const auto scn = makeSpriteScene(count, Shape::Flat);
		const auto ent = scn->getAllEntities().front();
		ioRunner.measure(std::format("serialize/entity_to_string/scene{}", count), 1,
						 [&]() -> void { doNotOptimize(scene::SceneSerializer::serializeEntityToString(ent)); });
		const std::string yaml = scene::SceneSerializer::serializeEntityToString(ent);
		ioRunner.metric("serialize/entity_yaml_size", static_cast<double>(yaml.size()), "bytes");
		shared<scene::Scene> target;
		ioRunner.measureWithSetup(
				std::format("serialize/entity_from_string/scene{}", count), 1,
				[&]() -> void { target = makeSpriteScene(count, Shape::Flat); },
				[&]() -> void { doNotOptimize(scene::SceneSerializer::deserializeEntityFromString(target, yaml)); });
	}
}

void runPrefab(Runner& ioRunner) {
	if (!ioRunner.wants("prefab"))
		return;
	const auto dir = std::filesystem::temp_directory_path() / "owl_bench_prefab";
	std::filesystem::create_directories(dir);
	for (const uint32_t size: {10U, 100U}) {
		const auto source = makeSpriteScene(size, Shape::Forest);
		const auto root = source->getRootEntities().front();
		const auto path = dir / std::format("prefab{}.owlprefab", size);
		if (!scene::PrefabSerializer::serialize(root, *source, path, std::format("prefab{}", size)))
			continue;
		for (const uint32_t sceneSize: {0U, 10000U}) {
			shared<scene::Scene> target;
			ioRunner.measureWithSetup(
					std::format("prefab/instantiate/{}_entities/scene{}", size, sceneSize), size,
					[&]() -> void {
						target = sceneSize == 0 ? mkShared<scene::Scene>() : makeSpriteScene(sceneSize, Shape::Flat);
					},
					[&]() -> void {
						doNotOptimize(scene::PrefabSerializer::instantiate(path, target, "bench.owlprefab"));
					});
		}
	}
	std::filesystem::remove_all(dir);
}

}// namespace

void runSerializeBenches(Runner& ioRunner) {
	runSceneRoundTrip(ioRunner);
	runEntitySnapshot(ioRunner);
	runPrefab(ioRunner);
}

}// namespace owl::bench
