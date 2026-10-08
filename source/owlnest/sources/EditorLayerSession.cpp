/**
 * @file EditorLayerSession.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"

#include <filesystem>

namespace owl::nest {

void EditorLayer::startProjectRecovery() {
	m_recovery.setDirectory(
			RecoveryManager::directoryFor(app::Application::get().getWorkingDirectory(), m_project.projectDirectory));
	m_recovery.setInterval(static_cast<float>(m_settings.autosaveIntervalSeconds));
	const auto pending = m_recovery.getPendingEntries();
	m_recovery.setSuspended(!pending.empty());
	if (pending.empty()) {
		m_recovery.clear();
		return;
	}
	OWL_WARN("Recovery: {} autosaved document(s) found for project '{}'.", pending.size(), m_project.name)
	m_recoveryPrompt.open(pending);
}

void EditorLayer::handleRecoveryChoice(const panel::RecoveryPrompt::Choice iChoice) {
	if (iChoice == panel::RecoveryPrompt::Choice::None)
		return;
	if (iChoice == panel::RecoveryPrompt::Choice::Discard) {
		OWL_INFO("Recovery: Autosaves discarded.")
		m_recovery.clear();
		m_recovery.setSuspended(false);
		return;
	}
	size_t failures = 0;
	Document* last = nullptr;
	for (const auto& entry: m_recoveryPrompt.getEntries()) {
		const auto snapshot = m_recovery.readSnapshot(entry);
		auto* doc = snapshot ? openDocumentForRecovery(entry) : nullptr;
		if (doc == nullptr || !doc->restoreRecoverySnapshot(*snapshot)) {
			OWL_ERROR("Recovery: Cannot restore '{}' ({}). Fix: copy its content by hand from the folder named below.",
					  entry.title, entry.originalPath.string())
			++failures;
			continue;
		}
		OWL_INFO("Recovery: Restored '{}' (autosave of {}); save it to keep the changes.", entry.title, entry.savedAt)
		last = doc;
	}
	if (failures > 0) {
		const auto kept = m_recovery.keepAside();
		OWL_WARN("Recovery: {} autosave(s) not restored, kept in '{}'.", failures, kept.string())
	} else {
		m_recovery.clear();
	}
	m_recovery.setSuspended(false);
	if (last != nullptr) {
		m_documents.setActive(last);
		last->requestFocus();
		syncActiveDocumentPanels();
	}
	refreshWindowTitle();
}

auto EditorLayer::openDocumentForRecovery(const RecoveryEntry& iEntry) -> Document* {
	const auto& path = iEntry.originalPath;
	const bool onDisk = !path.empty() && exists(path);
	if (iEntry.type == DocumentType::Scene) {
		if (onDisk)
			return loadOrOpenSceneDocument(path);
		auto doc = mkUniq<SceneDocument>();
		doc->onAttach(this);
		auto* scene = static_cast<SceneDocument*>(m_documents.add(std::move(doc)));
		scene->newScene(activeViewportSize());
		scene->setScenePath(path);
		return scene;
	}
	if (!onDisk) {
		OWL_WARN("Recovery: '{}' no longer exists.", path.string())
		return nullptr;
	}
	switch (iEntry.type) {
		case DocumentType::Code:
			openCodeFile(path);
			break;
		case DocumentType::NodeGraph:
			openNodeGraphFile(path);
			break;
		case DocumentType::Animation:
			openAnimationFile(path);
			break;
		case DocumentType::Tilemap:
			openTilemapFile(path);
			break;
		case DocumentType::Tileset:
			openTilesetFile(path);
			break;
		case DocumentType::Scene:
			break;
	}
	return findDocument(iEntry.type, path);
}

auto EditorLayer::findDocument(const DocumentType iType, const std::filesystem::path& iPath) const -> Document* {
	for (const auto& doc: m_documents.list()) {
		if (doc && doc->type() == iType && doc->filePath() == iPath)
			return doc.get();
	}
	return nullptr;
}

}// namespace owl::nest
