/**
 * @file EditorLayerSession.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"

#include "document/SceneFlowDocument.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

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

void EditorLayer::saveProjectSession() {
	if (!m_project.isLoaded())
		return;
	ProjectSession session;
	const auto& dir = m_project.projectDirectory;
	for (const auto& doc: m_documents.list()) {
		if (!doc || doc->filePath().empty() || dynamic_cast<const SceneFlowDocument*>(doc.get()) != nullptr)
			continue;
		session.documents.push_back(ProjectSession::toStored(dir, doc->filePath()));
	}
	if (const auto* active = m_documents.getActive(); active != nullptr && !active->filePath().empty())
		session.activeDocument = ProjectSession::toStored(dir, active->filePath());
	if (const auto selected = getSelectedEntity(); activeSceneDocument() != nullptr && selected)
		session.selectedEntity = static_cast<uint64_t>(selected.getUUID());
	m_settings.setProjectSession(dir, session);
}

auto EditorLayer::restoreProjectSession() -> bool {
	const auto session = m_settings.getProjectSession(m_project.projectDirectory);
	if (!session || session->documents.empty())
		return false;
	const auto& dir = m_project.projectDirectory;
	size_t restored = 0;
	for (const auto& stored: session->documents) {
		const auto path = ProjectSession::resolve(dir, stored);
		if (!exists(path)) {
			OWL_WARN("Session: '{}' no longer exists, its tab is not reopened.", stored)
			continue;
		}
		const auto ext = path.extension().string();
		if (ext == ".owl")
			std::ignore = loadOrOpenSceneDocument(path);
		else if (ext == ".owltilemap")
			openTilemapFile(path);
		else if (ext == ".owltileset")
			openTilesetFile(path);
		else if (ext == ".owlanim")
			openAnimationFile(path);
		else if (ext == ".owlflow")
			openNodeGraphFile(path);
		else
			openCodeFile(path);
		++restored;
	}
	if (restored == 0)
		return false;
	// Drop the blank tab the editor starts with, now that the session filled the strip.
	std::vector<core::UUID> blanks;
	for (const auto& doc: m_documents.list()) {
		if (doc && doc->type() == DocumentType::Scene && doc->filePath().empty() && !doc->isDirty())
			blanks.push_back(doc->id());
	}
	if (blanks.size() < m_documents.size()) {
		for (const auto id: blanks) std::ignore = m_documents.remove(id);
	}
	Document* active = nullptr;
	if (!session->activeDocument.empty()) {
		const auto activePath = ProjectSession::resolve(dir, session->activeDocument);
		for (const auto& doc: m_documents.list()) {
			if (doc && doc->filePath() == activePath)
				active = doc.get();
		}
	}
	if (active == nullptr && !m_documents.empty())
		active = m_documents.list().back().get();
	m_documents.setActive(active);
	if (active != nullptr)
		active->requestFocus();
	syncActiveDocumentPanels();
	if (const auto* scene = activeSceneDocument();
		scene != nullptr && session->selectedEntity != 0 && scene->getEditorScene()) {
		if (const auto entity = scene->getEditorScene()->findEntityByUUID(core::UUID{session->selectedEntity}); entity)
			setSelectedEntity(entity);
	}
	OWL_INFO("Session: Reopened {} document(s) of project '{}'.", restored, m_project.name)
	return true;
}

void EditorLayer::createProject(const panel::NewProjectRequest& iRequest) {
	const auto& dir = iRequest.directory;
	if (iRequest.projectTemplate) {
		if (const auto created = createProjectFromTemplate(*iRequest.projectTemplate, dir, iRequest.name); !created) {
			OWL_ERROR("New Project: Cannot create '{}': {}.", dir.string(), describe(created.error()))
			return;
		}
	} else {
		std::error_code ec;
		create_directories(dir / "scenes", ec);
		Project project;
		project.name = iRequest.name;
		project.projectDirectory = dir;
		if (ec || !project.saveToFile(dir / "owl_project.yml")) {
			OWL_ERROR("New Project: Cannot create the project file in '{}'.", dir.string())
			return;
		}
	}
	openProject(dir);
}

auto EditorLayer::findDocument(const DocumentType iType, const std::filesystem::path& iPath) const -> Document* {
	for (const auto& doc: m_documents.list()) {
		if (doc && doc->type() == iType && doc->filePath() == iPath)
			return doc.get();
	}
	return nullptr;
}

}// namespace owl::nest
