/**
 * @file EngineSceneTemplates_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <string>
#include <vector>

using namespace owl;

namespace {

// Scenes shipped in engine_assets: the first scenes of the project templates.
auto shippedScenes() -> std::vector<std::filesystem::path> {
	const auto root = test::getRootPath() / "engine_assets";
	std::vector<std::filesystem::path> scenes;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(root)) {
		const auto& path = entry.path();
		const auto rel = path.lexically_relative(root).generic_string();
		if (path.extension() == ".owl" && rel.starts_with("project_templates/"))
			scenes.push_back(path);
	}
	return scenes;
}

}// namespace

// Every component and field written in a shipped scene is read back: a key the serializer ignores fails the test.
TEST(EngineSceneTemplates, EveryKeySurvivesALoadSaveRoundTrip) {
	core::Log::init(core::Log::Level::Off);
	const auto scenes = shippedScenes();
	EXPECT_GE(scenes.size(), 3u);
	for (const auto& path: scenes) {
		const auto scn = mkShared<scene::Scene>();
		const scene::SceneSerializer serializer(scn);
		ASSERT_TRUE(serializer.deserialize(path)) << path;
		const auto saved = YAML::Load(serializer.serializeToString());
		std::map<std::string, YAML::Node> savedById;
		for (const auto& entity: saved["Entities"])
			savedById.emplace(entity["Entity"].as<std::string>(), YAML::Node{entity});
		for (const auto& entity: YAML::LoadFile(path.string())["Entities"]) {
			const auto id = entity["Entity"].as<std::string>();
			ASSERT_TRUE(savedById.contains(id)) << path << ": entity " << id;
			const auto& out = savedById.at(id);
			for (const auto& component: entity) {
				const auto name = component.first.as<std::string>();
				// A root's `parentId: 0` is the default, which the serializer does not write.
				if (name == "Hierarchy" && component.second["parentId"].as<uint64_t>(0) == 0)
					continue;
				ASSERT_TRUE(out[name]) << path << ": entity " << id << " lost component " << name;
				if (!component.second.IsMap())
					continue;
				for (const auto& field: component.second) {
					EXPECT_TRUE(out[name][field.first.as<std::string>()])
							<< path << ": entity " << id << " lost " << name << "." << field.first.as<std::string>();
				}
			}
		}
	}
	core::Log::invalidate();
}

// The HUD pass draws every canvas whatever its tag: no shipped scene or HUD names the `ui` layer on a widget any more.
TEST(EngineSceneTemplates, NoShippedWidgetNamesTheUiLayer) {
	const auto root = test::getRootPath();
	const std::regex uiTag{R"(RendererTag:\s*(\{\s*Name:\s*ui\s*\}|\n\s+Name:\s*ui\b))"};
	size_t checked = 0;
	for (const auto* folder: {"engine_assets", "engine_demo"}) {
		for (const auto& entry: std::filesystem::recursive_directory_iterator(root / folder)) {
			const auto& path = entry.path();
			if (path.extension() != ".owl" && path.extension() != ".owlhud")
				continue;
			std::ifstream file{path};
			const std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
			EXPECT_FALSE(std::regex_search(text, uiTag)) << path;
			++checked;
		}
	}
	EXPECT_GE(checked, 10u);
}
