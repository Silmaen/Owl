/**
 * @file EditorLayerProject.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AssetKind.h"
#include "EditorLayer.h"
#include "document/AnimationDocument.h"
#include "document/CodeEditorDocument.h"
#include "document/NodeGraphDocument.h"
#include "document/SceneFlowDocument.h"
#include "document/TilemapDocument.h"
#include "document/TilesetDocument.h"

#include <platform/AtomicFile.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace owl::nest {

void EditorLayer::openAssetFile(const std::filesystem::path& iPath) {
	switch (classifyAsset(iPath)) {
		case AssetKind::Scene:
			openScene(iPath);
			return;
		case AssetKind::NodeGraph:
			openNodeGraphFile(iPath);
			return;
		case AssetKind::Animation:
			openAnimationFile(iPath);
			return;
		case AssetKind::Tilemap:
			openTilemapFile(iPath);
			return;
		case AssetKind::Tileset:
			openTilesetFile(iPath);
			return;
		case AssetKind::Prefab:
			instantiatePrefab(iPath);
			return;
		case AssetKind::Code:
		case AssetKind::Unsupported:
			openCodeFile(iPath);
			return;
	}
}

void EditorLayer::newScene() {
	// Reuse an already-empty untitled tab when possible; otherwise create a new one.
	SceneDocument* target = nullptr;
	if (auto* current = activeSceneDocument(); current != nullptr && current->filePath().empty() && !current->isDirty())
		target = current;
	if (target == nullptr) {
		auto doc = mkUniq<SceneDocument>();
		doc->onAttach(this);
		target = static_cast<SceneDocument*>(m_documents.add(std::move(doc)));
	}
	target->newScene(activeViewportSize());
	m_documents.setActive(target);
	syncActiveDocumentPanels();
}

void EditorLayer::openScene() {
	if (const auto filepath = platform::FileDialog::openFile("Owl Scene (*.owl)|owl\n"); !filepath.empty())
		openScene(filepath);
}

void EditorLayer::openScene(const std::filesystem::path& iScenePath) {
	if (iScenePath.extension().string() != ".owl") {
		OWL_CORE_WARN("Cannot Open file {}: not a scene.", iScenePath.string())
		return;
	}

	// If this scene is already open in a tab, just make it active.
	if (auto* existing = findSceneDocumentByPath(iScenePath); existing != nullptr) {
		m_documents.setActive(existing);
		existing->requestFocus();
		syncActiveDocumentPanels();
		return;
	}

	if (getState() != State::Edit)
		onSceneStop();

	auto state = mkShared<AsyncProgressState>();
	state->setMessage("Loading scene...");
	m_asyncProgress.open("Loading Scene...", state, false);

	auto parsed = mkShared<scene::ParsedScene>();

	app::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, scenePath = iScenePath, parsed]() -> void {
				state->progress.store(0.1f);
				std::ifstream file(scenePath, std::ios::binary | std::ios::ate);
				if (!file.is_open()) {
					state->setError("Failed to open scene: " + scenePath.string());
					return;
				}
				const auto size = static_cast<size_t>(file.tellg());
				file.seekg(0);
				std::vector<uint8_t> bytes(size);
				file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
				state->progress.store(0.4f);
				state->setMessage("Parsing scene...");
				*parsed = scene::SceneSerializer::parseBuffer(bytes, scenePath.string());
				if (!parsed->valid) {
					const auto message = std::format("Cannot open scene '{}': {}. Fix: {}.", scenePath.string(),
													 parsed->error, scene::fixHint(parsed->failure));
					OWL_ERROR("{}", message)
					state->setError(message);
					return;
				}
				state->progress.store(0.8f);
				state->setMessage("Materialising entities...");
			},
			// Termination: runs on main thread — safe to modify scene & GPU.
			[this, state, scenePath = iScenePath, parsed]() -> void {
				if (state->hasError.load()) {
					state->completed.store(true);
					return;
				}
				const auto newScene = mkShared<scene::Scene>();
				if (const scene::SceneSerializer serializer(newScene);
					const auto loaded = serializer.applyParsed(*parsed)) {
					SceneDocument* target = nullptr;
					if (auto* current = activeSceneDocument();
						current != nullptr && current->filePath().empty() && !current->isDirty())
						target = current;
					if (target == nullptr) {
						auto doc = mkUniq<SceneDocument>();
						doc->onAttach(this);
						target = static_cast<SceneDocument*>(m_documents.add(std::move(doc)));
					}
					target->applyLoadedScene(newScene, scenePath, activeViewportSize());
					m_documents.setActive(target);
					syncActiveDocumentPanels();
				} else {
					state->setError(std::format("Cannot open scene '{}': {}. Fix: {}.", scenePath.string(),
												scene::describe(loaded.error()), scene::fixHint(loaded.error())));
				}
				state->completed.store(true);
			}));
}

void EditorLayer::openCodeFile(const std::filesystem::path& iPath) {
	if (iPath.empty() || !exists(iPath))
		return;
	// If already open, re-activate the existing tab.
	for (const auto& docPtr: m_documents.list()) {
		if (docPtr && docPtr->type() == DocumentType::Code && docPtr->filePath() == iPath) {
			m_documents.setActive(docPtr.get());
			docPtr->requestFocus();
			syncActiveDocumentPanels();
			return;
		}
	}
	auto doc = mkUniq<CodeEditorDocument>();
	doc->onAttach(this);
	if (!doc->loadFromFile(iPath)) {
		OWL_CORE_WARN("Failed to open code file: {}.", iPath.string())
		doc->onDetach();
		return;
	}
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::openSceneFlowView() {
	if (!m_project.isLoaded())
		return;
	// Reuse an already-open Scene Flow document if present.
	for (const auto& docPtr: m_documents.list()) {
		if (auto* graph = dynamic_cast<SceneFlowDocument*>(docPtr.get())) {
			graph->refreshFromProject(m_project);
			m_documents.setActive(graph);
			graph->requestFocus();
			syncActiveDocumentPanels();
			return;
		}
	}
	auto doc = mkUniq<SceneFlowDocument>();
	doc->onAttach(this);
	doc->refreshFromProject(m_project);
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::openNodeGraphFile(const std::filesystem::path& iPath) {
	if (iPath.empty() || !exists(iPath))
		return;
	// If already open, re-activate the existing tab.
	for (const auto& docPtr: m_documents.list()) {
		if (docPtr && docPtr->type() == DocumentType::NodeGraph && docPtr->filePath() == iPath) {
			m_documents.setActive(docPtr.get());
			docPtr->requestFocus();
			syncActiveDocumentPanels();
			return;
		}
	}
	auto doc = mkUniq<NodeGraphDocument>();
	doc->onAttach(this);
	if (!doc->loadFromFile(iPath)) {
		OWL_CORE_WARN("Failed to open node-graph file: {}.", iPath.string())
		doc->onDetach();
		return;
	}
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::openAnimationFile(const std::filesystem::path& iPath) {
	if (iPath.empty() || !exists(iPath))
		return;
	for (const auto& docPtr: m_documents.list()) {
		if (docPtr && docPtr->type() == DocumentType::Animation && docPtr->filePath() == iPath) {
			m_documents.setActive(docPtr.get());
			docPtr->requestFocus();
			syncActiveDocumentPanels();
			return;
		}
	}
	auto doc = mkUniq<AnimationDocument>();
	doc->onAttach(this);
	if (!doc->loadFromFile(iPath)) {
		OWL_CORE_WARN("Failed to open animation file: {}.", iPath.string())
		doc->onDetach();
		return;
	}
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::newAnimationClip() {
	auto doc = mkUniq<AnimationDocument>();
	doc->onAttach(this);
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::openTilemapFile(const std::filesystem::path& iPath) {
	if (iPath.empty() || !exists(iPath))
		return;
	for (const auto& docPtr: m_documents.list()) {
		if (docPtr && docPtr->type() == DocumentType::Tilemap && docPtr->filePath() == iPath) {
			m_documents.setActive(docPtr.get());
			docPtr->requestFocus();
			syncActiveDocumentPanels();
			return;
		}
	}
	auto doc = mkUniq<TilemapDocument>();
	doc->onAttach(this);
	if (!doc->loadFromFile(iPath)) {
		OWL_CORE_WARN("Failed to open tilemap file: {}.", iPath.string())
		doc->onDetach();
		return;
	}
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::newTilemapAsset() {
	auto doc = mkUniq<TilemapDocument>();
	doc->onAttach(this);
	// Provide a sensible starting grid so the canvas is non-empty out of the box.
	doc->asset().width = 16;
	doc->asset().height = 16;
	doc->asset().cellSize = 1.f;
	doc->asset().addLayer("layer0");
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::openTilesetFile(const std::filesystem::path& iPath) {
	if (iPath.empty() || !exists(iPath))
		return;
	for (const auto& docPtr: m_documents.list()) {
		if (docPtr && docPtr->type() == DocumentType::Tileset && docPtr->filePath() == iPath) {
			m_documents.setActive(docPtr.get());
			docPtr->requestFocus();
			syncActiveDocumentPanels();
			return;
		}
	}
	auto doc = mkUniq<TilesetDocument>();
	doc->onAttach(this);
	if (!doc->loadFromFile(iPath)) {
		OWL_CORE_WARN("Failed to open tileset file: {}.", iPath.string())
		doc->onDetach();
		return;
	}
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

void EditorLayer::newTilesetAsset() {
	auto doc = mkUniq<TilesetDocument>();
	doc->onAttach(this);
	doc->tileset().resize(4, 4);
	doc->tileset().tileWidth = 32;
	doc->tileset().tileHeight = 32;
	auto* raw = m_documents.add(std::move(doc));
	m_documents.setActive(raw);
	syncActiveDocumentPanels();
}

namespace {
auto resolveAssetAbsolutePath(const std::filesystem::path& iRelative) -> std::filesystem::path {
	if (iRelative.empty())
		return {};
	if (!app::Application::instanced())
		return {};
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (auto candidate = assetsPath / iRelative; exists(candidate))
			return weakly_canonical(candidate);
	}
	if (exists(iRelative))
		return weakly_canonical(iRelative);
	return {};
}
}// namespace

void EditorLayer::onTilemapSaved(const std::filesystem::path& iAbsolutePath) {
	const auto target = weakly_canonical(iAbsolutePath);
	for (const auto& docPtr: m_documents.list()) {
		if (!docPtr)
			continue;
		if (docPtr->type() == DocumentType::Scene) {
			auto* sceneDoc = static_cast<SceneDocument*>(docPtr.get());
			const auto& scene = sceneDoc->getActiveScene();
			if (!scene)
				continue;
			for (const auto view = scene->registry.view<scene::component::Tilemap>(); auto entity: view) {
				auto& tilemap = view.get<scene::component::Tilemap>(entity);
				const auto resolved = resolveAssetAbsolutePath(tilemap.tilemapPath);
				if (!resolved.empty() && resolved == target)
					tilemap.asset.reset();
			}
		}
	}
}

void EditorLayer::onTilesetSaved(const std::filesystem::path& iAbsolutePath) {
	const auto target = weakly_canonical(iAbsolutePath);
	for (const auto& docPtr: m_documents.list()) {
		if (!docPtr)
			continue;
		if (docPtr->type() == DocumentType::Tilemap) {
			auto* tmDoc = static_cast<TilemapDocument*>(docPtr.get());
			if (tmDoc->asset().tileset && !tmDoc->asset().tilesetPath.empty()) {
				if (resolveAssetAbsolutePath(tmDoc->asset().tilesetPath) == target)
					tmDoc->asset().tileset.reset();
			}
		} else if (docPtr->type() == DocumentType::Scene) {
			auto* sceneDoc = static_cast<SceneDocument*>(docPtr.get());
			const auto& scene = sceneDoc->getActiveScene();
			if (!scene)
				continue;
			for (const auto view = scene->registry.view<scene::component::Tilemap>(); auto entity: view) {
				auto& tilemap = view.get<scene::component::Tilemap>(entity);
				if (!tilemap.asset)
					continue;
				if (resolveAssetAbsolutePath(tilemap.asset->tilesetPath) == target)
					tilemap.asset->tileset.reset();
			}
		}
	}
}

auto EditorLayer::loadOrOpenSceneDocument(const std::filesystem::path& iScenePath) -> SceneDocument* {
	if (iScenePath.empty())
		return nullptr;
	if (auto* existing = findSceneDocumentByPath(iScenePath); existing != nullptr)
		return existing;
	if (!std::filesystem::exists(iScenePath))
		return nullptr;

	std::ifstream file(iScenePath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		OWL_CORE_WARN("loadOrOpenSceneDocument: cannot open '{}'.", iScenePath.string())
		return nullptr;
	}
	const auto size = static_cast<size_t>(file.tellg());
	file.seekg(0);
	std::vector<uint8_t> bytes(size);
	file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
	const auto newScene = mkShared<scene::Scene>();
	const scene::SceneSerializer serializer(newScene);
	if (const auto loaded = serializer.deserializeFromBuffer(bytes, iScenePath.string()); !loaded) {
		OWL_CORE_WARN("loadOrOpenSceneDocument: Cannot open scene '{}': {}. Fix: {}.", iScenePath.string(),
					  scene::describe(loaded.error()), scene::fixHint(loaded.error()))
		return nullptr;
	}
	auto doc = mkUniq<SceneDocument>();
	doc->onAttach(this);
	auto* target = static_cast<SceneDocument*>(m_documents.add(std::move(doc)));
	target->applyLoadedScene(newScene, iScenePath, activeViewportSize());
	return target;
}

void EditorLayer::saveSceneAs() {
	const auto* doc = activeSceneDocument();
	const std::string defaultName = (doc != nullptr && !doc->filePath().empty()) ? doc->filePath().filename().string()
																				 : std::string{"Untitled.owl"};
	if (const auto filepath = platform::FileDialog::saveFile("Owl Scene (*.owl)|owl\n", defaultName); !filepath.empty())
		saveSceneAs(filepath);
}

void EditorLayer::saveSceneAs(const std::filesystem::path& iScenePath) {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || !doc->getActiveScene())
		return;

	// Serialize to string on main thread (read-only, safe), write file on background thread.
	const scene::SceneSerializer serializer(doc->getActiveScene());
	auto yamlData = mkShared<std::string>(serializer.serializeToString());
	doc->setScenePath(iScenePath);

	auto state = mkShared<AsyncProgressState>();
	state->setMessage("Saving scene...");
	state->progress.store(0.5f);
	m_asyncProgress.open("Saving...", state, false);

	app::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, yamlData, path = iScenePath]() -> void {
				if (const auto written = platform::writeFileAtomic(path, *yamlData); !written) {
					state->setError(std::format("Failed to save {}: {}.", path.string(), describe(written.error())));
					return;
				}
				state->progress.store(1.0f);
				state->setMessage("Done!");
				OWL_CORE_INFO("Scene saved to {}.", path.string())
			},
			[state]() -> void { state->completed.store(true); }));
	refreshWindowTitle();
}

void EditorLayer::saveCurrentScene() {
	auto* doc = activeSceneDocument();
	if (doc == nullptr)
		return;
	const auto path = doc->filePath();
	if (path.empty())
		saveSceneAs();
	else
		saveSceneAs(path);
	doc->undoManager().markSaved();
}

void EditorLayer::saveActiveDocument() {
	if (getState() != State::Edit)
		return;
	auto* active = m_documents.getActive();
	if (active == nullptr)
		return;
	if (auto* codeDoc = dynamic_cast<CodeEditorDocument*>(active); codeDoc != nullptr) {
		if (codeDoc->filePath().empty())
			return;
		if (codeDoc->save())
			OWL_INFO("Code editor: saved '{}'.", codeDoc->filePath().string())
		else
			OWL_ERROR("Code editor: FAILED to save '{}'.", codeDoc->filePath().string())
		return;
	}
	if (const auto* sceneDoc = dynamic_cast<SceneDocument*>(active); sceneDoc != nullptr) {
		if (!sceneDoc->filePath().empty())
			saveCurrentScene();
	}
}

void EditorLayer::newProject() {
	std::filesystem::path templatesRoot;
	for (const auto& [title, path]: app::Application::get().getAssetDirectories()) {
		if (title == "Engine assets")
			templatesRoot = path / "project_templates";
	}
	const auto parent = m_settings.recentProjects.empty()
								? app::Application::get().getWorkingDirectory()
								: std::filesystem::path{m_settings.recentProjects.front()}.parent_path();
	m_newProjectDialog.open(listProjectTemplates(templatesRoot), parent);
}


void EditorLayer::openProject() {
	const auto dir = platform::FileDialog::pickFolder();
	if (!dir.empty())
		openProject(dir);
}

void EditorLayer::openProject(const std::filesystem::path& iDir) {
	const auto configFile = iDir / "owl_project.yml";
	if (!exists(configFile)) {
		OWL_CORE_WARN("No owl_project.yml found in {}.", iDir.string())
		m_settings.removeRecentProject(iDir);
		return;
	}
	if (m_project.isLoaded())
		closeProject();

	if (!m_project.loadFromFile(configFile)) {
		OWL_ERROR("Open Project: Cannot load {}.", configFile.string())
		return;
	}
	app::Application::get().addAssetDirectory({std::format("Project: {}", m_project.name), m_project.projectDirectory});
	m_contentBrowser.attach();
	refreshWindowTitle();

	// Add to recent projects list.
	m_settings.pushRecentProject(iDir);
	startProjectRecovery();

	if (restoreProjectSession())
		return;
	if (!m_project.firstScene.empty()) {
		const auto scenePath = m_project.projectDirectory / m_project.firstScene;
		if (exists(scenePath))
			openScene(scenePath);
	}
}

void EditorLayer::saveProject() {
	if (!m_project.isLoaded())
		return;
	if (getState() == State::Edit)
		saveCurrentScene();
	if (!m_project.saveToFile(m_project.projectDirectory / "owl_project.yml"))
		OWL_ERROR("Save Project: Cannot write the project file.")
}

void EditorLayer::saveProjectAs() {
	if (!m_project.isLoaded())
		return;
	const auto dest = platform::FileDialog::pickFolder();
	if (dest.empty())
		return;
	if (std::filesystem::weakly_canonical(dest) == std::filesystem::weakly_canonical(m_project.projectDirectory)) {
		OWL_CORE_WARN("Save Project As: destination is the current project directory — ignored.")
		return;
	}
	std::error_code ec;
	if (!exists(dest))
		create_directories(dest, ec);
	// Flush the current yml + active scene into the source tree before copying.
	if (getState() == State::Edit)
		saveCurrentScene();
	if (!m_project.saveToFile(m_project.projectDirectory / "owl_project.yml")) {
		OWL_ERROR("Save Project As: Cannot write the project file, copy aborted.")
		return;
	}
	std::filesystem::copy(m_project.projectDirectory, dest,
						  std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
						  ec);
	if (ec) {
		OWL_CORE_ERROR("Save Project As: copy failed — {}.", ec.message())
		return;
	}
	closeProject();
	openProject(dest);
}

void EditorLayer::closeProject() {
	if (!m_project.isLoaded())
		return;
	app::Application::get().removeAssetDirectory(m_project.projectDirectory);
	m_contentBrowser.attach();
	saveProjectSession();
	m_recovery.autosave(m_documents);
	m_recovery.setDirectory({});
	m_project = {};
	refreshWindowTitle();
}

void EditorLayer::importScene() {
	if (!m_project.isLoaded())
		return;
	const auto filepath = platform::FileDialog::openFile("Owl Scene (*.owl)|owl\n");
	if (filepath.empty())
		return;
	const auto scenesDir = m_project.projectDirectory / "scenes";
	if (!exists(scenesDir))
		create_directories(scenesDir);
	const auto dest = scenesDir / filepath.filename();
	if (exists(dest)) {
		OWL_CORE_WARN("Scene '{}' already exists in project.", filepath.filename().string())
		return;
	}
	std::filesystem::copy_file(filepath, dest);
	OWL_CORE_INFO("Imported scene '{}' into project.", filepath.filename().string())
}

}// namespace owl::nest
