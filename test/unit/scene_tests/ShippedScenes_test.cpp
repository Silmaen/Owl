/**
 * @file ShippedScenes_test.cpp
 * @author Silmaen
 * @date 23/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/FormatVersion.h>
#include <core/YamlNode.h>
#include <scene/EntityLinkMigration.h>
#include <scene/SceneSerializer.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace owl;

// A shipped scene at an older format pays its migration (a full YAML parse and emit) at every load, which the sample
// load benchmarks see: each scene of the Engine Demo and of the sample games is saved at the current format.
TEST(ShippedScenes, EveryShippedSceneIsAtTheCurrentFormatAndReadsWithoutMigration) {
	const auto root = test::getRootPath();
	const auto& format = scene::SceneSerializer::format();
	std::vector<std::string> stale;
	size_t checked = 0;
	for (const auto& folder: {root / "engine_demo" / "scenes", root / "samples"}) {
		for (const auto& entry: std::filesystem::recursive_directory_iterator(folder)) {
			if (entry.path().extension() != ".owl")
				continue;
			++checked;
			const std::ifstream file{entry.path()};
			std::stringstream text;
			text << file.rdbuf();
			const core::YamlDocument document{text.str(), entry.path().string()};
			const auto version = document.getRoot()[core::g_FormatVersionKey];
			if (!version.isScalar() || version.as<int64_t>(0) != static_cast<int64_t>(format.currentVersion()) ||
				!scene::canReadWithoutMigration(document.getRoot(), format))
				stale.push_back(std::filesystem::relative(entry.path(), root).generic_string());
		}
	}
	EXPECT_GT(checked, 20u);
	std::string report;
	for (const auto& path: stale) report += " " + path;
	EXPECT_TRUE(stale.empty()) << "scenes to save at FormatVersion " << format.currentVersion() << ":" << report;
}
