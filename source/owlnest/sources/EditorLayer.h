/**
 * @file EditorLayer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <io/pack/AssetScanner.h>
#include <owl.h>

#include "ActionRegistry.h"
#include "EditorSettings.h"
#include "Project.h"
#include "document/DocumentManager.h"
#include "document/DocumentTabBar.h"
#include "document/SceneDocument.h"
#include "panel/AsyncProgressModal.h"
#include "panel/ContentBrowser.h"
#include "panel/LogPanel.h"
#include "panel/Parameters.h"
#include "panel/ProjectSettings.h"
#include "panel/SceneHierarchy.h"
#include "panel/SettingsPanel.h"
#include "panel/Viewport.h"

namespace owl::nest {
/**
 * @brief Class EditorLayer
 */
class EditorLayer final : public core::layer::Layer {
public:
	EditorLayer(const EditorLayer&) = delete;
	EditorLayer(EditorLayer&&) = delete;
	auto operator=(const EditorLayer&) -> EditorLayer& = delete;
	auto operator=(EditorLayer&&) -> EditorLayer& = delete;
	/**
	 * @brief Default constructor.
	 */
	EditorLayer();
	/**
	 * @brief Destructor.
	 */
	~EditorLayer() override = default;

	void onAttach() override;
	void onDetach() override;
	void onUpdate(const core::Timestep& iTimeStep) override;
	void onEvent(event::Event& ioEvent) override;
	void onImGuiRender(const core::Timestep& iTimeStep) override;

	/// @brief Scene-state alias — the active document owns the authoritative value.
	using State = SceneDocument::State;
	[[nodiscard]] auto getState() const -> State;

	void newScene();
	void openScene();
	void openScene(const std::filesystem::path& iScenePath);
	void saveSceneAs();
	void saveSceneAs(const std::filesystem::path& iScenePath);
	void saveCurrentScene();

	void newProject();
	void openProject();
	void openProject(const std::filesystem::path& iDir);
	void saveProject();
	void closeProject();
	void importScene();
	void updateWindowTitle();
	void packScene();
	void packGame();
	void instantiatePrefab(const std::filesystem::path& iPrefabPath, const std::string& iAssetRelativePath = {});

	/// @brief Active scene from the currently active document (empty shared if none).
	[[nodiscard]] auto getActiveScene() const -> const shared<scene::Scene>&;
	[[nodiscard]] auto getSelectedEntity() const -> scene::Entity;
	void setSelectedEntity(scene::Entity iEntity);

	/// @brief Consume a pending step-frame request from the active document.
	auto consumeStepRequest() -> bool;

	/// @brief Handle a cross-level teleport request from the active document's scene.
	void handleTeleportRequest();

	/// @brief Request to stop play mode on the active document (e.g. from Lua `scene.quit()`).
	void requestStop();

	/// @brief Handle a save/load request from Lua on the active document's scene.
	void handleSaveLoadRequest();

	/// @brief Access the active document's undo manager, or nullptr if no doc is open.
	[[nodiscard]] auto activeUndoManager() -> UndoManager*;

	/// @brief Access the document manager (used by the Viewport tab bar).
	[[nodiscard]] auto getDocumentManager() -> DocumentManager& { return m_documents; }
	/// @brief Close the document with the given id (handles dirty + tab sync).
	void closeDocument(core::UUID iId);

private:
	void renderStats(const core::Timestep& iTimeStep);
	void renderMenu();
	void renderToolbar();
	/// Render the welcome screen shown when no project is loaded.
	void renderWelcomeScreen();
	/// Render the packaging wizard dialog (shown before running pack).
	void renderPackWizardModal();
	/// Render the pre-packaging validation modal (missing assets confirmation).
	void renderPackValidationModal();
	/// Launch the async validation + pack pipeline with the current wizard settings.
	void launchPackValidation();
	/// Start the async packaging process (called after validation).
	void startPackGame();

	/// @brief Access (or create) the single SceneDocument during Phase 1.
	auto ensureActiveSceneDocument() -> SceneDocument&;
	/// @brief Get the active SceneDocument, or nullptr if the active doc is not a scene.
	[[nodiscard]] auto activeSceneDocument() const -> SceneDocument*;

	auto onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool;
	static auto onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool;
	void onScenePlay();
	void onScenePause();
	void onSceneResume();
	void onSceneStop();
	void onSceneStep();
	void onDuplicateEntity();
	/// Perform undo and restore entity selection from the command's hint.
	void performUndo();
	/// Perform redo and restore entity selection from the command's hint.
	void performRedo();

	gui::widgets::ButtonBar m_controlBar;

	input::CameraOrthoController m_cameraController;

	/// Whether the welcome screen should be shown (hidden when user closes it).
	bool m_showWelcomeScreen = true;

	/// Pending pack destination directory (validated, awaiting user confirmation).
	std::filesystem::path m_pendingPackDestDir;
	/// Warnings collected during pre-packaging validation (empty when no issues).
	std::vector<std::string> m_pendingPackWarnings;
	/// Pre-scanned assets shared between validation and pack tasks (avoids double scan).
	shared<std::vector<io::pack::AssetReference>> m_pendingPackAssets;
	/// True when the validation modal should be shown on the next frame.
	bool m_showPackValidation = false;
	/// True when the packaging wizard modal is visible.
	bool m_showPackWizard = false;
	/// Compress blobs in the output pack (zstd).
	bool m_packCompress = true;
	/// XOR-obfuscate the pack TOC to deter casual inspection.
	bool m_packObfuscate = true;

	// project
	Project m_project;

	// settings
	EditorSettings m_settings;
	size_t m_lastAllocCalls = 0;
	size_t m_lastDeallocCalls = 0;

	// Open documents (scenes for now; later also Lua scripts, node graphs...).
	DocumentManager m_documents;

	/// @brief Find an open SceneDocument whose on-disk path matches `iPath`.
	[[nodiscard]] auto findSceneDocumentByPath(const std::filesystem::path& iPath) const -> SceneDocument*;
	/// @brief Keep panels synced with the currently active document.
	void syncActiveDocumentPanels();
	/// @brief Get the document currently in Play or Pause mode (at most one).
	[[nodiscard]] auto findPlayingSceneDocument() const -> SceneDocument*;

	// Panels
	panel::SceneHierarchy m_sceneHierarchy;
	panel::ContentBrowser m_contentBrowser;
	panel::Viewport m_viewport;
	panel::Parameters m_parameters;
	panel::ProjectSettings m_projectSettings;
	panel::LogPanel m_logPanel;
	panel::SettingsPanel m_settingsPanel;
	panel::AsyncProgressModal m_asyncProgress;

	// Action registry
	ActionRegistry m_actionRegistry;
};
}// namespace owl::nest
