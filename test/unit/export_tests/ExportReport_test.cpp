/**
 * @file ExportReport_test.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/external/yaml.h>
#include <data/assets/pack/ExportReport.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::data::assets::pack;

namespace {

void writeFile(const std::filesystem::path& iPath, const std::string& iContent) {
	std::filesystem::create_directories(iPath.parent_path());
	std::ofstream out(iPath, std::ios::binary);
	out << iContent;
}

auto makeReport() -> ExportReport {
	ExportReport report;
	report.platform = "linux-x64";
	report.assetCount = 12;
	report.shadersBaked = 2;
	report.shaderTargets = {"vulkan", "opengl"};
	report.sizes = {.pack = 3000, .runner = 2000, .libraries = 4000, .other = 100};
	report.unusedAssets = {"textures/old.png"};
	report.warnings = {"Icon 'logo.png' not found"};
	report.mods = {"mods/extra.owlmod"};
	return report;
}

}// namespace

TEST(ExportReport, FormatByteSize) {
	EXPECT_EQ(formatByteSize(0), "0 B");
	EXPECT_EQ(formatByteSize(1023), "1023 B");
	EXPECT_EQ(formatByteSize(1024), "1.00 KiB");
	EXPECT_EQ(formatByteSize(12 * 1024 + 300), "12.3 KiB");
	EXPECT_EQ(formatByteSize(uintmax_t{5} * 1024 * 1024), "5.00 MiB");
	EXPECT_EQ(formatByteSize(uintmax_t{300} * 1024 * 1024), "300 MiB");
	EXPECT_EQ(formatByteSize(uintmax_t{3} * 1024 * 1024 * 1024 / 2), "1.50 GiB");
	EXPECT_EQ(formatByteSize(uintmax_t{4096} * 1024 * 1024 * 1024), "4096 GiB");
}

TEST(ExportReport, ClassifyExportFile) {
	const std::filesystem::path game{"game"};
	EXPECT_EQ(classifyExportFile(game / "My_Game.owlpack", game / "My_Game"), ExportFileKind::Pack);
	EXPECT_EQ(classifyExportFile(game / "mods" / "extra.owlmod", game / "My_Game"), ExportFileKind::Pack);
	EXPECT_EQ(classifyExportFile(game / "My_Game", game / "My_Game"), ExportFileKind::Runner);
	EXPECT_EQ(classifyExportFile(game / "OwlGameRunner.wasm", game / "index.html"), ExportFileKind::Runner);
	EXPECT_EQ(classifyExportFile(game / "libOwlEngine.so", {}), ExportFileKind::Library);
	EXPECT_EQ(classifyExportFile(game / "libfmt.so.11.1", {}), ExportFileKind::Library);
	EXPECT_EQ(classifyExportFile(game / "OwlEngine.dll", {}), ExportFileKind::Library);
	EXPECT_EQ(classifyExportFile(game / "runner.yml", game / "My_Game"), ExportFileKind::Other);
	EXPECT_EQ(classifyExportFile(game / "My_Game", {}), ExportFileKind::Other);
}

TEST(ExportReport, MeasureExportSizes) {
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto dir = std::filesystem::temp_directory_path() / std::format("owl_export_report_{}", stamp);
	writeFile(dir / "Game.owlpack", "PACK!");
	writeFile(dir / "mods" / "a.owlmod", "MOD");
	writeFile(dir / "Game", "EXE");
	writeFile(dir / "libx.so", "LIB");
	writeFile(dir / "runner.yml", "RunnerConfig: {}\n");
	writeFile(dir / "data" / "Game", "NOT-THE-EXE");
	const auto sizes = measureExportSizes(dir, dir / "Game");
	EXPECT_EQ(sizes.pack, 8u);
	EXPECT_EQ(sizes.runner, 3u);
	EXPECT_EQ(sizes.libraries, 3u);
	EXPECT_EQ(sizes.other, std::string{"RunnerConfig: {}\n"}.size() + std::string{"NOT-THE-EXE"}.size());
	EXPECT_EQ(sizes.getTotal(), sizes.pack + sizes.runner + sizes.libraries + sizes.other);
	std::filesystem::remove_all(dir);
	EXPECT_EQ(measureExportSizes(dir, {}).getTotal(), 0u);
}

TEST(ExportReport, AddWarningsSkipsTheKnownOnes) {
	auto report = makeReport();
	report.addWarnings({"Icon 'logo.png' not found", "First scene 'x' not found in project"});
	EXPECT_EQ(report.warnings,
			  (std::vector<std::string>{"Icon 'logo.png' not found", "First scene 'x' not found in project"}));
}

TEST(ExportReport, YamlHoldsEverySection) {
	const auto report = makeReport();
	const auto root = YAML::Load(report.toYaml())["ExportReport"];
	ASSERT_TRUE(root);
	EXPECT_EQ(root["Platform"].as<std::string>(), "linux-x64");
	EXPECT_EQ(root["Sizes"]["Total"].as<uintmax_t>(), 9100u);
	EXPECT_EQ(root["Sizes"]["Libraries"].as<uintmax_t>(), 4000u);
	EXPECT_EQ(root["Assets"]["Packed"].as<size_t>(), 12u);
	EXPECT_EQ(root["Shaders"]["Baked"].as<size_t>(), 2u);
	EXPECT_EQ(root["Shaders"]["Targets"].as<std::vector<std::string>>(), report.shaderTargets);
	EXPECT_EQ(root["Mods"].as<std::vector<std::string>>(), std::vector<std::string>{"extra.owlmod"});
	EXPECT_EQ(root["UnusedAssets"].as<std::vector<std::string>>(), report.unusedAssets);
	EXPECT_EQ(root["Warnings"].as<std::vector<std::string>>(), report.warnings);
}

TEST(ExportReport, SummaryAndWrite) {
	auto report = makeReport();
	const auto summary = report.getSummary();
	EXPECT_NE(summary.find("8.89 KiB"), std::string::npos) << summary;
	EXPECT_NE(summary.find("2 baked for vulkan, opengl"), std::string::npos) << summary;
	EXPECT_NE(summary.find("Unused assets: 1; warnings: 1."), std::string::npos) << summary;
	EXPECT_FALSE(summary.ends_with('\n'));

	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	report.gameDirectory = std::filesystem::temp_directory_path() / std::format("owl_export_write_{}", stamp);
	EXPECT_FALSE(report.write());
	std::filesystem::create_directories(report.gameDirectory);
	EXPECT_TRUE(report.write());
	EXPECT_TRUE(exists(report.gameDirectory / g_exportReportFile));
	std::filesystem::remove_all(report.gameDirectory);
}
