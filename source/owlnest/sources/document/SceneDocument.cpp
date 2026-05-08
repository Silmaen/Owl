/**
 * @file SceneDocument.cpp
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneDocument.h"

#include <physic/PhysicCommand.h>
#include <scene/component/components.h>
#include <sound/SoundCommand.h>
#include <sound/SoundSystem.h>

namespace owl::nest {

SceneDocument::SceneDocument() = default;

auto SceneDocument::title() const -> std::string {
	if (!m_scenePath.empty())
		return m_scenePath.filename().string();
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

void SceneDocument::onImGuiRender() { m_viewport.onRender(); }

auto SceneDocument::save() -> bool {
	// Actual async file write happens from EditorLayer; saving without a path falls back to Save As.
	return !m_scenePath.empty();
}

auto SceneDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	m_scenePath = iPath;
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

	const auto& app = core::Application::get();
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

	const auto previousGameState = m_activeScene->getGameState();
	m_activeScene->onEndRuntime();

	const auto newScene = mkShared<scene::Scene>();
	if (const scene::SceneSerializer sc(newScene); !sc.deserialize(levelPath)) {
		OWL_CORE_ERROR("Teleport: failed to load level '{}'.", request.levelName)
		return;
	}
	newScene->getGameState() = previousGameState;
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
					physic::PhysicCommand::applySnapshot(entity, snap);
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
			physic::PhysicCommand::setTransform(
					player, {targetTransform.translation().x(), targetTransform.translation().y()}, targetRotation);
			physic::PhysicCommand::setVelocity(player, finalVelocity);
			auto& playerTransform = player.getComponent<scene::component::Transform>().transform;
			playerTransform.translation().x() = targetTransform.translation().x();
			playerTransform.translation().y() = targetTransform.translation().y();
			playerTransform.rotation().z() = targetRotation;
			break;
		}
	}
}

}// namespace owl::nest
