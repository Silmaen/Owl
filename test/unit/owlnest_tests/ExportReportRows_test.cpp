/**
 * @file ExportReportRows_test.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "GamePackager.h"

#include <data/assets/pack/ExportReport.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace owl;

namespace {

auto valueOf(const std::vector<nest::ExportReportRow>& iRows, const std::string& iLabel) -> std::string {
	const auto row = std::ranges::find(iRows, iLabel, &nest::ExportReportRow::label);
	return row == iRows.end() ? std::string{"<missing>"} : row->value;
}

}// namespace

TEST(ExportReportRows, ShowTheSizesAndTheShaders) {
	data::assets::pack::ExportReport report;
	report.gameDirectory = "out/My_Game";
	report.platform = "web-wasm32";
	report.sizes = {.pack = 2048, .runner = 1024 * 1024, .libraries = 0, .other = 10};
	report.assetCount = 5;
	report.cookedAssets = 2;
	report.cacheHits = 3;
	report.renderers = {"Renderer3D"};
	report.shadersBaked = 1;
	report.shaderTargets = {"webgpu"};
	const auto rows = nest::getExportReportRows(report);
	EXPECT_EQ(valueOf(rows, "Platform"), "web-wasm32");
	EXPECT_EQ(valueOf(rows, "Total Size"), "1.00 MiB");
	EXPECT_EQ(valueOf(rows, "Pack"), "2.00 KiB");
	EXPECT_EQ(valueOf(rows, "Libraries"), "0 B");
	EXPECT_EQ(valueOf(rows, "Assets"), "5 packed (2 cooked, 3 from the cache)");
	EXPECT_EQ(valueOf(rows, "Renderers"), "Renderer2D, Renderer3D");
	EXPECT_EQ(valueOf(rows, "Shaders Baked"), "1 for webgpu");
	EXPECT_EQ(valueOf(rows, "Mods"), "<missing>");
	report.mods = {"out/My_Game/mods/a.owlmod", "out/My_Game/mods/b.owlmod"};
	report.shaderTargets.clear();
	const auto withMods = nest::getExportReportRows(report);
	EXPECT_EQ(valueOf(withMods, "Mods"), "a.owlmod, b.owlmod");
	EXPECT_EQ(valueOf(withMods, "Shaders Baked"), "1");
}
