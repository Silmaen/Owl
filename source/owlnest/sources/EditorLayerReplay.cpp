/**
 * @file EditorLayerReplay.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"

#include <scene/Replay.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace owl::nest {

namespace {

constexpr std::string_view g_replayFolder = "replays";

// A scene path as the project names it, the path itself outside the project.
auto projectRelative(const std::filesystem::path& iPath, const std::filesystem::path& iProject) -> std::string {
	if (iProject.empty() || iPath.empty())
		return iPath.generic_string();
	const auto relative = iPath.lexically_relative(iProject);
	return relative.empty() || *relative.begin() == ".." ? iPath.generic_string() : relative.generic_string();
}

}// namespace

void EditorLayer::saveRecording(SceneDocument& ioDocument) {
	auto replay = ioDocument.takeRecording();
	if (!replay)
		return;
	const auto project = getSessionProjectDirectory();
	const std::filesystem::path scenePath{replay->scene};
	replay->scene = projectRelative(scenePath, project);
	const auto folder = (project.empty() ? scenePath.parent_path() : project) / g_replayFolder;
	const auto stamp =
			std::format("{:%Y%m%d-%H%M%S}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
	const auto stem = scenePath.empty() ? std::string{"untitled"} : scenePath.stem().string();
	const auto file = folder / std::format("{}-{}{}", stem, stamp, ".owlreplay");
	std::error_code error;
	std::filesystem::create_directories(folder, error);
	if (error || !replay->saveFile(file)) {
		m_toasts.push(gui::widgets::ToastKind::Error, "Recording not saved", file.string());
		return;
	}
	OWL_INFO("Replay: Session of {} frames recorded to {}.", replay->frames.size(), file.string())
	m_toasts.push(gui::widgets::ToastKind::Success, std::format("Recorded {:.1f} s", replay->getSeconds()),
				  projectRelative(file, project));
}

void EditorLayer::playReplayFile(const std::filesystem::path& iPath) {
	auto replay = scene::Replay::loadFile(iPath);
	if (!replay) {
		m_toasts.push(gui::widgets::ToastKind::Error, "Replay not read", replay.error());
		return;
	}
	const std::filesystem::path recorded{replay->scene};
	const auto project = getSessionProjectDirectory();
	const auto scenePath = recorded.is_absolute() || project.empty() ? recorded : project / recorded;
	if (replay->scene.empty() || !exists(scenePath)) {
		OWL_WARN("Replay: Scene '{}' of {} not found.", replay->scene, iPath.string())
		m_toasts.push(gui::widgets::ToastKind::Error, "Replay scene not found", replay->scene);
		return;
	}
	openScene(scenePath);
	auto* doc = activeSceneDocument();
	std::error_code error;
	if (doc == nullptr || !std::filesystem::equivalent(doc->filePath(), scenePath, error))
		return;
	if (doc->state() != SceneDocument::State::Edit)
		onSceneStop();
	doc->setReplayForNextPlay(mkShared<const scene::Replay>(std::move(*replay)));
	onScenePlay();
}

}// namespace owl::nest
