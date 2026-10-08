/**
 * @file SceneDocument.cpp
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneDocument.h"

#include <imgui.h>
#include <physics/PhysicCommand.h>
#include <scene/LevelTransition.h>
#include <scene/SaveManager.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>
#include <sound/SoundCommand.h>
#include <sound/SoundSystem.h>

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
}

void SceneDocument::onDetach() {
	m_viewport.detach();
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
	if (const auto loaded = scene::SceneSerializer(restored).deserializeFromBuffer(bytes, source); !loaded) {
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
		OWL_WARN("Hot reload: Scene '{}' changed on disk; stop the play to reload it.", m_scenePath.string())
		return false;
	}
	if (isDirty()) {
		OWL_WARN("Hot reload: Scene '{}' changed on disk but has unsaved edits, kept; save or reopen it to choose.",
				 m_scenePath.string())
		return false;
	}
	std::ifstream file(m_scenePath, std::ios::binary);
	if (!file.is_open()) {
		OWL_WARN("Hot reload: Cannot read scene '{}'.", m_scenePath.string())
		return false;
	}
	const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	if (const std::string text(bytes.begin(), bytes.end());
		text == scene::SceneSerializer(m_editorScene).serializeToString())
		return false;
	const auto fresh = mkShared<scene::Scene>();
	if (const auto loaded = scene::SceneSerializer(fresh).deserializeFromBuffer(bytes, m_scenePath.string()); !loaded) {
		OWL_ERROR("Hot reload: Scene '{}' failed to load ({}), the open version is kept.", m_scenePath.string(),
				  scene::describe(loaded.error()))
		return false;
	}
	applyLoadedScene(fresh, m_scenePath, iViewportSize);
	m_sceneSwapped = true;
	OWL_INFO("Hot reload: Scene '{}' reloaded.", m_scenePath.string())
	return true;
}

void SceneDocument::onScenePlay() {
	if (!m_editorScene)
		return;
	auto& soundLibrary = sound::SoundSystem::getSoundLibrary();
	sound::SoundCommand::playSound(soundLibrary.get("clic.wav"));

	m_state = State::Play;
	m_activeScene = scene::Scene::copy(m_editorScene);
	m_activeScene->onStartRuntime();
}

void SceneDocument::onSceneStop() {
	m_state = State::Edit;
	m_pendingTeleportVelocity = false;
	if (m_activeScene)
		m_activeScene->onEndRuntime();
	m_activeScene = m_editorScene;
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
		OWL_CORE_ERROR("Teleport: Level '{}' not found. Fix: check the target scene of the teleport trigger.",
					   request.levelName)
		return;
	}
	const auto parsed = scene::SceneSerializer::parseBuffer(source->bytes, source->sourceName);
	const auto newScene = scene::LevelTransition::loadLevel(parsed, *m_activeScene);
	if (!newScene)
		return;
	// The running level is only torn down once the target loaded, so a failed teleport keeps playing.
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
