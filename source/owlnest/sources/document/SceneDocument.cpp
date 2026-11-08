/**
 * @file SceneDocument.cpp
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneDocument.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <imgui.h>
#include <physics/PhysicCommand.h>
#include <scene/HudLibrary.h>
#include <scene/LevelTransition.h>
#include <scene/SaveManager.h>
#include <scene/SceneSerializer.h>
#include <scene/UiInputSystem.h>
#include <scene/component/components.h>
#include <script/ScriptDebugger.h>
#include <sound/SoundCommand.h>
#include <sound/SoundSystem.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest {


SceneDocument::SceneDocument() = default;

auto SceneDocument::title() const -> std::string {
	if (!m_scenePath.empty())
		return m_scenePath.stem().string();
	return "Untitled";
}

void SceneDocument::onAttach(EditorLayer* iEditor) {
	mp_editor = iEditor;
	if (!m_activeScene)
		m_activeScene = mkShared<scene::Scene>();
	if (!m_editorScene)
		m_editorScene = m_activeScene;
	m_viewport.attach();
	m_viewport.attachParent(iEditor);
	m_viewport.setDocument(this);
	m_viewport.setUndoManager(&m_undoManager);
	m_hudEditor.attachParent(iEditor);
}

void SceneDocument::onDetach() {
	m_viewport.detach();
	m_hudEditor.detach();
	if (m_state != State::Edit && m_activeScene)
		m_activeScene->onEndRuntime();
	m_activeScene.reset();
	m_editorScene.reset();
	mp_editor = nullptr;
}

void SceneDocument::onUpdate(const core::Timestep& iTimeStep) { m_viewport.onUpdate(iTimeStep); }

void SceneDocument::onEvent(event::Event& ioEvent) { m_viewport.onEvent(ioEvent); }

void SceneDocument::onImGuiRender() {
	if (consumeFocusRequest())
		ImGui::SetNextWindowFocus();
	m_viewport.onRender();
}

auto SceneDocument::save() -> bool {
	// Actual async file write happens from EditorLayer; saving without a path falls back to Save As.
	return !m_scenePath.empty();
}

auto SceneDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	m_scenePath = iPath;
	return true;
}

auto SceneDocument::recoverySnapshot() const -> std::optional<std::string> {
	if (!m_editorScene)
		return std::nullopt;
	return scene::SceneSerializer(m_editorScene).serializeToString();
}

auto SceneDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	const auto restored = mkShared<scene::Scene>();
	const std::vector<uint8_t> bytes(iSnapshot.begin(), iSnapshot.end());
	const auto source = m_scenePath.empty() ? std::string{"autosave"} : m_scenePath.string();
	const auto kind = m_editorScene ? m_editorScene->getDocumentKind() : scene::documentKindOf(m_scenePath);
	if (const auto loaded = scene::SceneSerializer(restored, kind).deserializeFromBuffer(bytes, source); !loaded) {
		OWL_WARN("Recovery: Cannot restore the autosave of '{}': {}.", title(), scene::describe(loaded.error()))
		return false;
	}
	applyLoadedScene(restored, m_scenePath, m_viewport.getSize());
	m_undoManager.markUnsaved();
	return true;
}

void SceneDocument::newScene(const math::vec2ui& iViewportSize) {
	m_activeScene = mkShared<scene::Scene>();
	m_activeScene->onViewportResize(iViewportSize);
	m_editorScene = m_activeScene;
	m_scenePath.clear();
	m_hudMode = false;
	m_state = State::Edit;
	m_stepRequested = false;
	m_stopRequested = false;
	m_pendingTeleportVelocity = false;
	m_undoManager.clear();
}

void SceneDocument::applyLoadedScene(const shared<scene::Scene>& iScene, const std::filesystem::path& iPath,
									 const math::vec2ui& iViewportSize) {
	if (!iScene)
		return;
	m_editorScene = iScene;
	m_editorScene->onViewportResize(iViewportSize);
	m_activeScene = m_editorScene;
	m_scenePath = iPath;
	m_hudMode = m_editorScene->getDocumentKind() == scene::DocumentKind::Hud;
	m_state = State::Edit;
	m_stepRequested = false;
	m_stopRequested = false;
	m_pendingTeleportVelocity = false;
	m_undoManager.clear();
}

auto SceneDocument::reloadFromDisk(const math::vec2ui& iViewportSize) -> bool {
	if (m_scenePath.empty() || !m_editorScene)
		return false;
	if (m_state != State::Edit) {
		OWL_WARN("HotReload: Scene '{}' changed on disk; stop the play to reload it.", m_scenePath.string())
		return false;
	}
	if (isDirty()) {
		OWL_WARN("HotReload: Scene '{}' changed on disk but has unsaved edits, kept; save or reopen it to choose.",
				 m_scenePath.string())
		return false;
	}
	std::ifstream file(m_scenePath, std::ios::binary);
	if (!file.is_open()) {
		OWL_WARN("HotReload: Cannot read scene '{}'.", m_scenePath.string())
		return false;
	}
	const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	if (const std::string text(bytes.begin(), bytes.end());
		text == scene::SceneSerializer(m_editorScene).serializeToString())
		return false;
	const auto fresh = mkShared<scene::Scene>();
	if (const auto loaded = scene::SceneSerializer(fresh, m_editorScene->getDocumentKind())
									.deserializeFromBuffer(bytes, m_scenePath.string());
		!loaded) {
		OWL_WARN("HotReload: Scene '{}' failed to load ({}), the open version is kept.", m_scenePath.string(),
				 scene::describe(loaded.error()))
		return false;
	}
	applyLoadedScene(fresh, m_scenePath, iViewportSize);
	m_sceneSwapped = true;
	OWL_INFO("HotReload: Scene '{}' reloaded.", m_scenePath.string())
	return true;
}

void SceneDocument::onScenePlay() {
	if (!m_editorScene)
		return;
	auto& soundLibrary = sound::SoundSystem::getSoundLibrary();
	sound::SoundCommand::playSound(soundLibrary.get("clic.wav"));

	m_state = State::Play;
	m_recording.reset();
	mp_timeline.reset();
	m_seekTarget.reset();
	script::ScriptDebugger::setEnabled(true);
	startRuntime(std::exchange(mp_nextReplay, nullptr));
}

void SceneDocument::startRuntime(const shared<const scene::Replay>& iPlayback) {
	resetHudValues();
	m_activeScene = scene::Scene::copy(m_editorScene);
	std::ignore = m_activeScene->setPlaybackReplay(iPlayback);
	// Every Play records its timeline, the inputs the rewind plays again.
	std::ignore = m_activeScene->setRecordingReplay(true);
	m_activeScene->onStartRuntime();
}

void SceneDocument::keepRecording(std::optional<scene::Replay> iRecording) {
	if (!iRecording || !m_recordNextPlay)
		return;
	iRecording->scene = m_scenePath.generic_string();
	m_recording = std::move(iRecording);
}

void SceneDocument::onSceneStop() {
	m_state = State::Edit;
	script::ScriptDebugger::setEnabled(false);
	std::ignore = script::ScriptDebugger::takeHit();
	m_pendingTeleportVelocity = false;
	endSeek();
	if (m_activeScene) {
		// Stopped on a sought timeline, the session is the whole timeline: nothing new was played.
		if (mp_timeline)
			keepRecording(*mp_timeline);
		else
			keepRecording(m_activeScene->takeRecordedReplay());
		m_activeScene->onEndRuntime();
	}
	mp_timeline.reset();
	m_activeScene = m_editorScene;
	// The play may have left a teleported level: the editor rebuilds the renderer stack of the edited scene.
	m_sceneSwapped = true;
	resetHudValues();
}

void SceneDocument::resetHudValues() {
	// The HUD scenes are shared: what the game did to their widgets must not outlive the play, nor its conversations.
	if (app::Application::instanced()) {
		app::Application::get().getEngineContext().getHudLibrary().clear();
		app::Application::get().getEngineContext().getDialogues().clear();
	}
}

void SceneDocument::onSceneResume() {
	endSeek();
	branchTimeline();
	m_state = State::Play;
}

void SceneDocument::onSceneStep() {
	if (isSeeking())
		return;
	branchTimeline();
	m_stepRequested = true;
}

auto SceneDocument::getTimelineFrameCount() const -> uint64_t {
	if (m_state == State::Edit || !m_activeScene)
		return 0;
	if (mp_timeline)
		return mp_timeline->frames.size();
	if (const auto& recording = m_activeScene->getRecordedReplay(); recording)
		return recording->frames.size();
	return 0;
}

auto SceneDocument::getTimelineFrame() const -> uint64_t {
	if (mp_timeline)
		return m_activeScene->getReplayProgress().frame;
	return getTimelineFrameCount();
}

auto SceneDocument::timeline() const -> const scene::Replay* {
	if (mp_timeline)
		return mp_timeline.get();
	if (m_state != State::Edit && m_activeScene && m_activeScene->getRecordedReplay())
		return &*m_activeScene->getRecordedReplay();
	return nullptr;
}

auto SceneDocument::getTimelineSeconds(const uint64_t iFrames) const -> double {
	const auto* timeline = this->timeline();
	if (timeline == nullptr)
		return 0.0;
	double seconds = 0.0;
	const auto count = std::min<uint64_t>(iFrames, timeline->frames.size());
	for (uint64_t i = 0; i < count; ++i)
		seconds += std::chrono::duration<double>(timeline->frames[static_cast<size_t>(i)].duration).count();
	return seconds;
}

auto SceneDocument::seek(const uint64_t iFrame) -> bool {
	const uint64_t count = getTimelineFrameCount();
	if (count == 0) {
		OWL_WARN("Rewind: Nothing to seek in this Play session.")
		return false;
	}
	const uint64_t target = std::min(iFrame, count);
	m_state = State::Pause;
	if (!mp_timeline) {
		if (target == count)
			return true;
		mp_timeline = mkShared<const scene::Replay>(*m_activeScene->getRecordedReplay());
		m_activeScene->onEndRuntime();
		startRuntime(mp_timeline);
		m_sceneSwapped = true;
	} else if (target < getTimelineFrame()) {
		m_activeScene->onEndRuntime();
		startRuntime(mp_timeline);
		m_sceneSwapped = true;
	}
	if (!m_seekTarget) {
		// The frames played again are heard once already, and a breakpoint would stop the seek half-way.
		sound::SoundCommand::setListenerGain(0.f);
		script::ScriptDebugger::setEnabled(false);
	}
	m_seekTarget = target;
	return true;
}

auto SceneDocument::rewind(const double iSeconds) -> bool {
	const auto* timeline = this->timeline();
	if (timeline == nullptr)
		return seek(0);
	uint64_t frame = std::min<uint64_t>(isSeeking() ? *m_seekTarget : getTimelineFrame(), timeline->frames.size());
	double back = 0.0;
	while (frame > 0 && back < iSeconds) {
		--frame;
		back += std::chrono::duration<double>(timeline->frames[static_cast<size_t>(frame)].duration).count();
	}
	return seek(frame);
}

auto SceneDocument::advanceSeek(const std::chrono::microseconds iBudget) -> bool {
	if (!m_seekTarget || !m_activeScene)
		return true;
	const auto start = std::chrono::steady_clock::now();
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds{16});
	while (getTimelineFrame() < *m_seekTarget && !m_activeScene->getReplayProgress().finished) {
		scene::UiInputSystem::update(m_activeScene.get(), {64, 64}, {0.f, 0.f}, false, 0.f);
		m_activeScene->onUpdateRuntime(step, /*iRender=*/false);
		if (std::chrono::steady_clock::now() - start >= iBudget)
			break;
	}
	if (getTimelineFrame() < *m_seekTarget && !m_activeScene->getReplayProgress().finished)
		return false;
	endSeek();
	return true;
}

void SceneDocument::endSeek() {
	if (!m_seekTarget)
		return;
	m_seekTarget.reset();
	sound::SoundCommand::setListenerGain(1.f);
	script::ScriptDebugger::setEnabled(m_state != State::Edit);
}

void SceneDocument::branchTimeline() {
	if (!mp_timeline || !m_activeScene)
		return;
	std::ignore = m_activeScene->takeOverReplay();
	mp_timeline.reset();
}

auto SceneDocument::consumeStepRequest() -> bool {
	if (m_stepRequested) {
		m_stepRequested = false;
		return true;
	}
	return false;
}

void SceneDocument::handleTeleportRequest(const math::vec2ui& iViewportSize) {
	if (!m_activeScene || !m_activeScene->teleportRequest.pending)
		return;
	const auto request = m_activeScene->teleportRequest;
	m_activeScene->teleportRequest.pending = false;

	const auto source = scene::LevelTransition::readLevel(request.levelName, scene::LevelTransition::getSearchRoots());
	if (!source) {
		OWL_CORE_ERROR("LevelTransition: Level '{}' not found. Fix: check the target scene of the teleport trigger.",
					   request.levelName)
		return;
	}
	const auto parsed = scene::SceneSerializer::parseBuffer(source->bytes, source->sourceName);
	const auto newScene = scene::LevelTransition::loadLevel(parsed, *m_activeScene);
	if (!newScene)
		return;
	// The running level is only torn down once the target loaded, so a failed teleport keeps playing.
	keepRecording(m_activeScene->takeRecordedReplay());
	mp_timeline.reset();
	m_activeScene->onEndRuntime();
	newScene->onViewportResize(iViewportSize);

	m_pendingTeleportVelocity = true;
	m_teleportVelocity = request.initialVelocity;
	m_teleportTargetName = request.targetName;

	m_activeScene = newScene;
	m_sceneSwapped = true;
}

void SceneDocument::handleSaveLoadRequest(const math::vec2ui& iViewportSize) {
	if (!m_activeScene || !m_activeScene->saveLoadRequest.pending)
		return;
	const auto slr = m_activeScene->saveLoadRequest;
	m_activeScene->saveLoadRequest.pending = false;
	if (!slr.isLoad) {
		std::ignore = scene::SaveManager::save(slr.slot, m_activeScene, m_scenePath.string());
		return;
	}
	if (auto loaded = scene::LevelTransition::loadSavedGame(slr.slot, *m_activeScene, iViewportSize); loaded) {
		mp_timeline.reset();
		m_activeScene = std::move(loaded);
		m_sceneSwapped = true;
	}
}

void SceneDocument::applyPendingTeleportVelocity() {
	if (!m_pendingTeleportVelocity || !m_activeScene)
		return;
	m_pendingTeleportVelocity = false;
	scene::LevelTransition::placeArrival(*m_activeScene, m_teleportTargetName, m_teleportVelocity);
}

auto SceneDocument::canUndo() const -> bool {
	return state() == State::Edit && m_activeScene && m_undoManager.canUndo();
}

auto SceneDocument::canRedo() const -> bool {
	return state() == State::Edit && m_activeScene && m_undoManager.canRedo();
}

void SceneDocument::performUndo() {
	if (!canUndo())
		return;
	m_undoManager.undo(*m_activeScene);
}

void SceneDocument::performRedo() {
	if (!canRedo())
		return;
	m_undoManager.redo(*m_activeScene);
}

}// namespace owl::nest
