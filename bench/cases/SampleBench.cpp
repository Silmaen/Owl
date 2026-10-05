/**
 * @file SampleBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <renderer/CameraEditor.h>
#include <renderer/Renderer2D.h>
#include <scene/Entity.h>
#include <scene/SceneSerializer.h>

#include <format>

namespace owl::bench {

void runSampleBenches(Runner& ioRunner) {
	if (!ioRunner.wants("sample"))
		return;
	const auto scenes = std::filesystem::path(OWL_BENCH_SOURCE_DIR) / "sample_project" / "scenes";
	if (!exists(scenes))
		return;
	std::vector<std::filesystem::path> files;
	for (const auto& entry: std::filesystem::directory_iterator(scenes))
		if (entry.path().extension() == ".owl")
			files.push_back(entry.path());
	std::ranges::sort(files);
	core::Timestep step;
	step.forceUpdate(std::chrono::microseconds(16667));
	const renderer::CameraEditor camera;
	for (const auto& file: files) {
		const std::string label = file.stem().string();
		shared<scene::Scene> scn;
		ioRunner.measureWithSetup(
				std::format("sample/load/{}", label), 1, [&]() -> void { scn = mkShared<scene::Scene>(); },
				[&]() -> void {
					const scene::SceneSerializer loader(scn);
					doNotOptimize(loader.deserialize(file));
				});
		if (scn == nullptr)
			continue;
		ioRunner.metric(std::format("sample/entities/{}", label), static_cast<double>(scn->getAllEntities().size()),
						"entities");
		ioRunner.metric(std::format("sample/file_bytes/{}", label), static_cast<double>(file_size(file)), "bytes");
		scn->onViewportResize({1280, 720});
		ioRunner.measure(std::format("sample/editor_update/{}", label), 1,
						 [&]() -> void { scn->onUpdateEditor(step, camera); });
		ioRunner.metric(std::format("sample/editor_update/{}_quads", label), renderer::Renderer2D::getStats().quadCount,
						"Renderer2D quads (last frame)");
	}
}

}// namespace owl::bench
