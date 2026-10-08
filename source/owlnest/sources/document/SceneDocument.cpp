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
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>
#include <sound/SoundCommand.h>
#include <sound/SoundSystem.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
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

	std::string resolvedName = request.levelName;
	if (std::filesystem::path(resolvedName).extension() != ".owl")
		resolvedName += ".owl";

	const auto& app = app::Application::get();
	std::filesystem::path levelPath;
	for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
		if (exists(assetsPath / resolvedName)) {
			levelPath = assetsPath / resolvedName;
			break;
		}
		if (exists(assetsPath / "scenes" / resolvedName)) {
			levelPath = assetsPath / "scenes" / resolvedName;
			break;
		}
	}
	if (levelPath.empty()) {
		OWL_CORE_ERROR("Teleport: level '{}' not found.", resolvedName)
		return;
	}

	using clk = std::chrono::steady_clock;
	const auto t0 = clk::now();

	std::ifstream file(levelPath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		OWL_CORE_ERROR("Teleport: failed to open level '{}'.", levelPath.string())
		return;
	}
	const auto size = static_cast<size_t>(file.tellg());
	file.seekg(0);
	std::vector<uint8_t> bytes(size);
	file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
	OWL_CORE_INFO("SceneDocument::handleTeleportRequest: read {} bytes from '{}' {:.1f} ms.", bytes.size(),
				  levelPath.string(), std::chrono::duration<double, std::milli>{clk::now() - t0}.count())

	const auto parsed = scene::SceneSerializer::parseBuffer(bytes, levelPath.string());
	if (!parsed.valid) {
		OWL_CORE_ERROR("Teleport: failed to parse level '{}': {}.", request.levelName, parsed.error)
		return;
	}
	const auto newScene = mkShared<scene::Scene>();
	const scene::SceneSerializer serializer(newScene);
	if (const auto loaded = serializer.applyParsed(parsed); !loaded) {
		OWL_CORE_ERROR("Teleport: Failed to load level '{}': {}.", request.levelName, scene::describe(loaded.error()))
		return;
	}
	// The running level is only torn down once the target loaded, so a failed teleport keeps playing.
	newScene->getGameState() = m_activeScene->getGameState();
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
	if (slr.isLoad) {
		auto newScene = mkShared<scene::Scene>();
		if (auto loadResult = scene::SaveManager::load(slr.slot, newScene); loadResult.success) {
			m_activeScene->onEndRuntime();
			m_activeScene = newScene;
			m_activeScene->onViewportResize(iViewportSize);
			m_activeScene->onStartRuntime();
			for (const auto& [uuid, snap]: loadResult.physicsSnapshots)
				if (auto entity = m_activeScene->findEntityByUUID(core::UUID{uuid}); entity)
					physics::PhysicCommand::applySnapshot(entity, snap);
			m_sceneSwapped = true;
		}
	} else {
		std::ignore = scene::SaveManager::save(slr.slot, m_activeScene, m_scenePath.string());
	}
}

void SceneDocument::applyPendingTeleportVelocity() {
	if (!m_pendingTeleportVelocity || !m_activeScene)
		return;
	m_pendingTeleportVelocity = false;
	const scene::Entity player = m_activeScene->getPrimaryPlayer();
	if (!player)
		return;
	for (const auto view = m_activeScene->registry.view<scene::component::Tag, scene::component::Transform>();
		 const auto ent: view) {
		if (view.get<scene::component::Tag>(ent).tag == m_teleportTargetName) {
			const auto& targetTransform = view.get<scene::component::Transform>(ent).transform;
			const float targetRotation = targetTransform.rotation().z();
			const float cosR = std::cos(targetRotation);
			const float sinR = std::sin(targetRotation);
			const math::vec2f finalVelocity = {m_teleportVelocity.x() * cosR - m_teleportVelocity.y() * sinR,
											   m_teleportVelocity.x() * sinR + m_teleportVelocity.y() * cosR};
			physics::PhysicCommand::setTransform(
					player, {targetTransform.translation().x(), targetTransform.translation().y()}, targetRotation);
			physics::PhysicCommand::setVelocity(player, finalVelocity);
			auto& playerTransform = player.getComponent<scene::component::Transform>().transform;
			playerTransform.translation().x() = targetTransform.translation().x();
			playerTransform.translation().y() = targetTransform.translation().y();
			playerTransform.rotation().z() = targetRotation;
			break;
		}
	}
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
