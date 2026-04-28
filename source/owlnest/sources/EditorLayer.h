/**
 * @file EditorLayer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <gui/widgets/Ribbon.h>
#include <io/pack/AssetScanner.h>
#include <owl.h>

#include "ActionRegistry.h"
#include "EditorSettings.h"
#include "Project.h"
#include "document/DocumentManager.h"
#include "document/SceneDocument.h"
#include "panel/AsyncProgressModal.h"
#include "panel/ContentBrowser.h"
#include "panel/LogPanel.h"
#include "panel/Parameters.h"
#include "panel/ProjectSettings.h"
#include "panel/SceneHierarchy.h"
#include "panel/SettingsPanel.h"

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

	/// @brief Read access to the loaded project (used by side panels / Scene Flow document).
	[[nodiscard]] auto getProject() const -> const Project& { return m_project; }

	/// @brief Scene-state alias — the active document owns the authoritative value.
	using State = SceneDocument::State;
	[[nodiscard]] auto getState() const -> State;

	void newScene();
	void openScene();
	void openScene(const std::filesystem::path& iScenePath);

	/// @brief Open a text/code file as a new `CodeEditorDocument` (or switch to one already open).
	void openCodeFile(const std::filesystem::path& iPath);

	/// @brief Open a `.owlflow` node-graph file as a new `NodeGraphDocument` (or switch to one already open).
	void openNodeGraphFile(const std::filesystem::path& iPath);

	/// @brief Open a `.owlanim` animation clip as a new `AnimationDocument` (or switch to one already open).
	void openAnimationFile(const std::filesystem::path& iPath);

	/// @brief Create a new untitled `AnimationDocument`.
	void newAnimationClip();

	/// @brief Open (or refresh) the Scene Flow view — a `NodeGraphDocument` subclass populated from the current project.
	void openSceneFlowView();
	void saveSceneAs();
	void saveSceneAs(const std::filesystem::path& iScenePath);
	void saveCurrentScene();

	void newProject();
	void openProject();
	void openProject(const std::filesystem::path& iDir);
	void saveProject();
	/// @brief Duplicate the current project to a user-picked directory and switch to it.
	void saveProjectAs();
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
	[[nodiscard]] auto activeUndoManager() -> SceneUndoManager*;

	/// @brief Access the document manager (used by the Viewport tab bar).
	[[nodiscard]] auto getDocumentManager() -> DocumentManager& { return m_documents; }
	/// @brief Access the editor settings (font size, theme, etc.).
	[[nodiscard]] auto getSettings() const -> const EditorSettings& { return m_settings; }
	/// @brief Route a Content Browser file drop to `openScene` / `openCodeFile` / prefab instantiation.
	void handleContentBrowserDrop(const std::filesystem::path& iRelativePath);
	/// @brief Close the document with the given id (handles dirty + tab sync).
	void closeDocument(core::UUID iId);
	/// @brief Request closing a document. Prompts for confirmation when dirty.
	void requestCloseDocument(core::UUID iId);
	/// @brief Close the currently active document (with dirty prompt if needed).
	void requestCloseActiveDocument();

private:
	void renderStats(const core::Timestep& iTimeStep);
	/// Populate `m_ribbon` with the File, Edit, and contextual tabs bound to ActionRegistry.
	/// The contextual tab contents depend on the active document type.
	void buildRibbon();
	/// Contextual tab for scene documents (Playback, Gizmo, Scene file ops, Package).
	void buildSceneTab();
	void buildNodeGraphTab();
	/// Contextual tab for animation-clip documents (Playback, Frame, File).
	void buildAnimationTab();
	/// Contextual tab for code / text documents (Save, Save As, Close, language…).
	void buildCodeTab();
	/// Rebuild the ribbon when the active document type changes.
	void refreshRibbonForActiveDoc();
	/// Render the welcome screen shown when no project is loaded.
	void renderWelcomeScreen();
	/// Render the packaging wizard dialog (shown before running pack).
	void renderPackWizardModal();
	/// Render the pre-packaging validation modal (missing assets confirmation).
	void renderPackValidationModal();
	/// Render the dirty-close confirmation modal for `m_pendingCloseDocId`.
	void renderCloseDocumentModal();
	/// Render the recent-projects popup (triggered from the ribbon File > Recent button).
	void renderRecentProjectsPopup();
	/// Launch the async validation + pack pipeline with the current wizard settings.
	void launchPackValidation();
	/// Start the async packaging process (called after validation).
	void startPackGame();

	/// @brief Access (or create) the single SceneDocument during Phase 1.
	auto ensureActiveSceneDocument() -> SceneDocument&;
	/// @brief Get the active SceneDocument, or nullptr if the active doc is not a scene.
	[[nodiscard]] auto activeSceneDocument() const -> SceneDocument*;
	/// @brief Shortcut to the active document's Viewport, or nullptr.
	[[nodiscard]] auto activeViewport() const -> panel::Viewport*;
	/// @brief Active viewport size, or a sensible default (1280x720) when no viewport exists yet.
	[[nodiscard]] auto activeViewportSize() const -> math::vec2ui;

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

	gui::widgets::Ribbon m_ribbon;
	/// Last document type for which the ribbon's contextual tab was populated.  When the active
	/// document switches to a different type, we rebuild the ribbon.
	std::optional<DocumentType> m_lastRibbonDocType;

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

	/// Pending document id to close (awaits dirty-confirmation modal). 0 when idle.
	core::UUID m_pendingCloseDocId{0};
	/// True when the close-confirmation modal should open on the next frame.
	bool m_openCloseDocModal = false;
	/// Set by the ribbon's "Recent" button; drives `renderRecentProjectsPopup`.
	bool m_openRecentProjectsPopup = false;
	/// Documents that must actually be closed at the start of the next frame.  Deferring the
	/// destruction avoids a use-after-free on the Viewport's Vulkan color-attachment: during the
	/// current frame, ImGui's draw list still references the texture and only samples it at the
	/// `UiLayer::end()` flush.
	std::vector<core::UUID> m_deferredCloseIds;

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
	panel::Parameters m_parameters;
	panel::ProjectSettings m_projectSettings;
	panel::LogPanel m_logPanel;
	panel::SettingsPanel m_settingsPanel;
	panel::AsyncProgressModal m_asyncProgress;

	// Action registry
	ActionRegistry m_actionRegistry;
};
}// namespace owl::nest
