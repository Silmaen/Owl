/**
 * @file FrameBench_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "FrameBench.h"
#include "FrameBenchStats.h"

#include <core/external/yaml.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

using namespace owl::nest::runner;

TEST(FrameBenchStats, PercentileInterpolates) {
	const std::vector sorted{1.0, 2.0, 3.0, 4.0, 5.0};
	EXPECT_DOUBLE_EQ(percentile(sorted, 0.0), 1.0);
	EXPECT_DOUBLE_EQ(percentile(sorted, 0.5), 3.0);
	EXPECT_DOUBLE_EQ(percentile(sorted, 1.0), 5.0);
	EXPECT_DOUBLE_EQ(percentile(sorted, 0.125), 1.5);
	EXPECT_DOUBLE_EQ(percentile({}, 0.5), 0.0);
}

TEST(FrameBenchStats, SummarizeUnsortedSeries) {
	const auto summary = summarize({5.0, 1.0, 4.0, 2.0, 3.0});
	EXPECT_EQ(summary.count, 5u);
	EXPECT_DOUBLE_EQ(summary.median, 3.0);
	EXPECT_DOUBLE_EQ(summary.iqr, 2.0);
	EXPECT_DOUBLE_EQ(summary.mean, 3.0);
	EXPECT_DOUBLE_EQ(summary.min, 1.0);
	EXPECT_DOUBLE_EQ(summary.max, 5.0);
	EXPECT_DOUBLE_EQ(summary.p95, 4.8);
	EXPECT_EQ(summarize({}).count, 0u);
}

TEST(FrameBenchOptions, ParseAndReject) {
	const auto scene = owl::test::getRootPath() / "sample_project" / "scenes" / "main_menu.owl";
	std::string sceneArg = scene.string();
	std::array<std::string, 12> storage{"OwlRunner", "--frame-bench", sceneArg, "--frames", "42",      "--warmup",
										"3",         "--backend",     "null",   "--size",   "640x480", "--vsync"};
	std::vector<char*> argv;
	for (auto& arg: storage) argv.push_back(arg.data());
	EXPECT_TRUE(hasFrameBenchFlag(static_cast<int>(argv.size()), argv.data()));
	const auto options = parseFrameBenchOptions(static_cast<int>(argv.size()), argv.data(), scene.parent_path());
	ASSERT_TRUE(options.has_value());
	EXPECT_EQ(options->frames, 42u);
	EXPECT_EQ(options->warmup, 3u);
	EXPECT_EQ(options->backend, owl::renderer::gpu::RenderAPI::Type::Null);
	EXPECT_EQ(options->size.x(), 640u);
	EXPECT_EQ(options->size.y(), 480u);
	EXPECT_TRUE(options->vSync);
	EXPECT_EQ(options->project, owl::test::getRootPath() / "sample_project");

	storage[8] = "metal";
	EXPECT_FALSE(parseFrameBenchOptions(static_cast<int>(argv.size()), argv.data(), scene.parent_path()).has_value());
	storage[8] = "null";
	storage[2] = "missing_scene.owl";
	EXPECT_FALSE(parseFrameBenchOptions(static_cast<int>(argv.size()), argv.data(), scene.parent_path()).has_value());
	std::array<std::string, 1> plain{"OwlRunner"};
	std::vector<char*> plainArgv{plain[0].data()};
	EXPECT_FALSE(hasFrameBenchFlag(1, plainArgv.data()));
}

TEST(FrameBenchRun, NullBackendWritesReport) {
#ifndef OWL_RUNNER_EXECUTABLE
	GTEST_SKIP() << "OwlRunner is not built (OWL_BUILD_NEST=OFF).";
#else
	const auto scene = owl::test::getRootPath() / "sample_project" / "scenes" / "platformer_house.owl";
	const auto out = std::filesystem::temp_directory_path() / "owl_frame_bench_test.json";
	std::filesystem::remove(out);
	const auto command = std::format(R"("{}" --frame-bench "{}" --backend null --frames 8 --warmup 2 --out "{}")",
									 OWL_RUNNER_EXECUTABLE, scene.string(), out.string());
	// NOLINTNEXTLINE(concurrency-mt-unsafe,cert-env33-c)
	ASSERT_EQ(std::system(command.c_str()), 0);
	ASSERT_TRUE(exists(out));
	const auto report = YAML::LoadFile(out.string());
	EXPECT_EQ(report["backend"].as<std::string>(), "null");
	EXPECT_EQ(report["frames_measured"].as<uint32_t>(), 8u);
	EXPECT_FALSE(report["interrupted"].as<bool>());
	EXPECT_FALSE(report["gpu_timestamps"].as<bool>());
	const auto summary = report["summary"];
	EXPECT_EQ(summary["cpu_total_ms"]["count"].as<uint32_t>(), 8u);
	EXPECT_GT(summary["cpu_total_ms"]["median"].as<double>(), 0.0);
	EXPECT_EQ(summary["gpu_busy_ms"]["count"].as<uint32_t>(), 0u);
	EXPECT_EQ(summary["queue_wait_idle"]["max"].as<double>(), 0.0);
	EXPECT_GT(summary["draw_calls"]["median"].as<double>(), 0.0);
	const auto samples = report["samples"];
	ASSERT_EQ(samples.size(), 8u);
	EXPECT_TRUE(samples[0]["gpu_busy_ms"].IsNull());
	EXPECT_GE(samples[0]["cpu_total_ms"].as<double>(), samples[0]["cpu_render_prep_ms"].as<double>());
	std::filesystem::remove(out);
#endif
}
