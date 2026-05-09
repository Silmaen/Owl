/**
 * @file EditorLayer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright (c) 2022 All rights reserved.
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
#include "panel/HelpPanel.h"
#include "panel/LogPanel.h"
#include "panel/Parameters.h"
#include "panel/ProjectSettings.h"
#include "panel/SceneHierarchy.h"
#include "panel/SceneSettings.h"
#include "panel/SettingsPanel.h"

namespace owl::nest {
/**
 * @brief
 *  Class EditorLayer.
 */
class EditorLayer final : public core::layer::Layer {
public:
	EditorLayer(const EditorLayer&) = delete;

	EditorLayer(EditorLayer&&) = delete;

	auto operator=(const EditorLayer&) -> EditorLayer& = delete;

	auto operator=(EditorLayer&&) -> EditorLayer& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	EditorLayer();

	/**
	 * @brief
	 *  Destructor.
	 */
	~EditorLayer() override = default;

	/**
	 * @brief
	 *  Handle the attach event.
	 */
	void onAttach() override;

	/**
	 * @brief
	 *  Handle the detach event.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Handle the update event.
	 * @param[in] iTimeStep Frame time delta.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Handle the event event.
	 * @param[in,out] ioEvent The incoming event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Handle the im gui render event.
	 * @param[in] iTimeStep Frame time delta.
	 */
	void onImGuiRender(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Read access to the loaded project (used by side panels / Scene Flow document).
	 * @return The project.
	 */
	[[nodiscard]] auto getProject() const -> const Project& { return m_project; }

	/**
	 * @brief
	 *  Scene-state alias — the active document owns the authoritative value.
	 */
	using State = SceneDocument::State;

	/**
	 * @brief
	 *  Get the state.
	 * @return The state.
	 */
	[[nodiscard]] auto getState() const -> State;

	/**
	 * @brief
	 *  New scene.
	 */
	void newScene();

	/**
	 * @brief
	 *  Open scene.
	 */
	void openScene();

	/**
	 * @brief
	 *  Open a scene file as a new `SceneDocument` (or switch to one already open).
	 * @param[in] iScenePath Path to the `.owl` scene file.
	 */
	void openScene(const std::filesystem::path& iScenePath);

	/**
	 * @brief
	 *  Open a text/code file as a new `CodeEditorDocument` (or switch to one already open).
	 */
	void openCodeFile(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Open a `.owlflow` node-graph file as a new `NodeGraphDocument` (or switch to one already open).
	 */
	void openNodeGraphFile(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Open a `.owlanim` animation clip as a new `AnimationDocument` (or switch to one already open).
	 */
	void openAnimationFile(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Create a new untitled `AnimationDocument`.
	 */
	void newAnimationClip();

	/**
	 * @brief
	 *  Open a `.owltilemap` asset as a new `TilemapDocument` (or switch to one already open).
	 * @param[in] iPath Absolute path to the `.owltilemap` file.
	 */
	void openTilemapFile(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Create a new untitled `TilemapDocument`.
	 */
	void newTilemapAsset();

	/**
	 * @brief
	 *  Open a `.owltileset` asset as a new `TilesetDocument` (or switch to one already open).
	 * @param[in] iPath Absolute path to the `.owltileset` file.
	 */
	void openTilesetFile(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Create a new untitled `TilesetDocument`.
	 */
	void newTilesetAsset();

	/**
	 * @brief
	 *  Queue an asset path for deferred opening (drained at the start of the next frame).
	 *
	 * Safe to call from in-render contexts (popups, context menus, drag-drop). The dispatch
	 * by extension (`.owl` / `.owltilemap` / `.owltileset` / …) happens when the queue is
	 * drained, mirroring `handleContentBrowserDrop`.
	 * @param[in] iAbsolutePath The absolute path of the asset to open.
	 */
	void requestDeferredOpen(const std::filesystem::path& iAbsolutePath) {
		if (!iAbsolutePath.empty())
			m_deferredOpenPaths.push_back(iAbsolutePath);
	}

	/**
	 * @brief
	 *  Notify the editor that a tilemap asset has just been saved to disk.
	 *
	 * Iterates every open `SceneDocument` and clears the cached `Tilemap.asset` of any
	 * entity whose `tilemapPath` resolves to `iAbsolutePath`, forcing a re-resolve from
	 * disk on the next frame so live scenes pick up the change.
	 * @param[in] iAbsolutePath The absolute path of the saved `.owltilemap` asset.
	 */
	void onTilemapSaved(const std::filesystem::path& iAbsolutePath);

	/**
	 * @brief
	 *  Notify the editor that a tileset asset has just been saved to disk.
	 *
	 * Walks every open `SceneDocument` and `TilemapDocument` and clears every cached
	 * `tileset` whose path matches `iAbsolutePath`, forcing a re-resolve from disk on
	 * the next frame so live tilemaps and scenes pick up the change.
	 * @param[in] iAbsolutePath The absolute path of the saved `.owltileset` asset.
	 */
	void onTilesetSaved(const std::filesystem::path& iAbsolutePath);

	/**
	 * @brief
	 *  Synchronously open (or return) a `SceneDocument` for the given on-disk scene.
	 * @param[in] iScenePath Absolute or project-relative path to a `.owl` file.
	 * @return Pointer to the live `SceneDocument`, or `nullptr` on missing/invalid file.
	 *
	 * Unlike `openScene`, this does **not** route through the async progress modal and does
	 * **not** switch the active document — it is meant for editor-side actions (Scene Flow link
	 * edits, future cross-scene tooling) that need to mutate a scene's entities without yanking
	 * the user's focus. The file is read and deserialized synchronously on the main thread.
	 */
	auto loadOrOpenSceneDocument(const std::filesystem::path& iScenePath) -> SceneDocument*;

	/**
	 * @brief
	 *  Open (or refresh) the Scene Flow view — a `NodeGraphDocument` subclass populated from the current project.
	 */
	void openSceneFlowView();

	/**
	 * @brief
	 *  Save scene as.
	 */
	void saveSceneAs();

	/**
	 * @brief
	 *  Save the active scene to an explicit path and update the document.
	 * @param[in] iScenePath Destination `.owl` file.
	 */
	void saveSceneAs(const std::filesystem::path& iScenePath);

	/**
	 * @brief
	 *  Save current scene.
	 */
	void saveCurrentScene();

	/**
	 * @brief
	 *  Dispatch the Ctrl+S save shortcut to the active document, whichever its kind
	 *        (`SceneDocument` → async scene serialise; `CodeEditorDocument` → write the
	 *        Lua / Markdown / SVG file directly). No-op outside Edit state or when the
	 *        active document has no on-disk path yet.
	 */
	void saveActiveDocument();

	/**
	 * @brief
	 *  New project.
	 */
	void newProject();

	/**
	 * @brief
	 *  Open project.
	 */
	void openProject();

	/**
	 * @brief
	 *  Open a project from disk and switch to it.
	 * @param[in] iDir Path to the project directory containing `owl_project.yml`.
	 */
	void openProject(const std::filesystem::path& iDir);

	/**
	 * @brief
	 *  Save project.
	 */
	void saveProject();

	/**
	 * @brief
	 *  Duplicate the current project to a user-picked directory and switch to it.
	 */
	void saveProjectAs();

	/**
	 * @brief
	 *  Close project.
	 */
	void closeProject();

	/**
	 * @brief
	 *  Import scene.
	 */
	void importScene();

	/**
	 * @brief
	 *  Recompute the application window title from the current project + dirty state and push it to the Window.
	 */
	void refreshWindowTitle();

	/**
	 * @brief
	 *  Pack scene.
	 */
	void packScene();

	/**
	 * @brief
	 *  Pack game.
	 */
	void packGame();

	/**
	 * @brief
	 *  Instantiate a `.owlprefab` into the active scene as a new entity subtree.
	 * @param[in] iPrefabPath Absolute path to the prefab file.
	 * @param[in] iAssetRelativePath Asset-relative path stored in the resulting `PrefabLink` (auto-derived when empty).
	 */
	void instantiatePrefab(const std::filesystem::path& iPrefabPath, const std::string& iAssetRelativePath = {});

	/**
	 * @brief
	 *  Active scene from the currently active document (empty shared if none).
	 * @return The active scene.
	 */
	[[nodiscard]] auto getActiveScene() const -> const shared<scene::Scene>&;

	/**
	 * @brief
	 *  Get the selected entity.
	 * @return The selected entity.
	 */
	[[nodiscard]] auto getSelectedEntity() const -> scene::Entity;

	/**
	 * @brief
	 *  Set the selected entity.
	 * @param[in] iEntity The target entity.
	 */
	void setSelectedEntity(scene::Entity iEntity);

	/**
	 * @brief
	 *  Consume a pending step-frame request from the active document.
	 * @return True on success, false otherwise.
	 */
	auto consumeStepRequest() -> bool;

	/**
	 * @brief
	 *  Handle a cross-level teleport request from the active document's scene.
	 */
	void handleTeleportRequest();

	/**
	 * @brief
	 *  Request to stop play mode on the active document (e.g. from Lua `scene.quit()`).
	 */
	void requestStop();

	/**
	 * @brief
	 *  Handle a save/load request from Lua on the active document's scene.
	 */
	void handleSaveLoadRequest();

	/**
	 * @brief
	 *  Access the active document's undo manager, or nullptr if no doc is open.
	 * @return Pointer to the active undo manager, or `nullptr` when not available.
	 */
	[[nodiscard]] auto activeUndoManager() -> SceneUndoManager*;

	/**
	 * @brief
	 *  Access the document manager (used by the Viewport tab bar).
	 * @return The document manager.
	 */
	[[nodiscard]] auto getDocumentManager() -> DocumentManager& { return m_documents; }

	/**
	 * @brief
	 *  Const access to the document manager (used by panels that only read tab metadata).
	 * @return The document manager.
	 */
	[[nodiscard]] auto getDocumentManager() const -> const DocumentManager& { return m_documents; }

	/**
	 * @brief
	 *  Access the editor settings (font size, theme, etc.).
	 * @return The settings.
	 */
	[[nodiscard]] auto getSettings() const -> const EditorSettings& { return m_settings; }

	/**
	 * @brief
	 *  Route a Content Browser file drop to `openScene` / `openCodeFile` / prefab instantiation.
	 */
	void handleContentBrowserDrop(const std::filesystem::path& iRelativePath);

	/**
	 * @brief
	 *  Close the document with the given id (handles dirty + tab sync).
	 */
	void closeDocument(core::UUID iId);

	/**
	 * @brief
	 *  Request closing a document. Prompts for confirmation when dirty.
	 */
	void requestCloseDocument(core::UUID iId);

	/**
	 * @brief
	 *  Close the currently active document (with dirty prompt if needed).
	 */
	void requestCloseActiveDocument();

private:
	/**
	 * @brief
	 *  Render stats.
	 * @param[in] iTimeStep Frame time delta.
	 */
	void renderStats(const core::Timestep& iTimeStep);

	/**
	 * @brief
	 *  Populate `m_ribbon` with the File, Edit, and contextual tabs bound to ActionRegistry.
	 * The contextual tab contents depend on the active document type.
	 */
	void buildRibbon();

	/**
	 * @brief
	 *  Contextual tab for scene documents (Playback, Gizmo, Scene file ops, Package).
	 */
	void buildSceneTab();

	/**
	 * @brief
	 *  Build node graph tab.
	 */
	void buildNodeGraphTab();

	/**
	 * @brief
	 *  Contextual tab for animation-clip documents (Playback, Frame, File).
	 */
	void buildAnimationTab();

	/**
	 * @brief
	 *  Contextual tab for code / text documents (Save, Save As, Close, language…).
	 */
	void buildCodeTab();

	/**
	 * @brief
	 *  Contextual tab for tilemap documents (Save / Save As / Close + undo helpers).
	 */
	void buildTilemapTab();

	/**
	 * @brief
	 *  Contextual tab for tileset documents (Save / Save As / Close + grid helpers).
	 */
	void buildTilesetTab();

	/**
	 * @brief
	 *  Rebuild the ribbon when the active document type changes.
	 */
	void refreshRibbonForActiveDoc();

	/**
	 * @brief
	 *  Render the welcome screen shown when no project is loaded.
	 */
	void renderWelcomeScreen();

	/**
	 * @brief
	 *  Render the packaging wizard dialog (shown before running pack).
	 */
	void renderPackWizardModal();

	/**
	 * @brief
	 *  Render the pre-packaging validation modal (missing assets confirmation).
	 */
	void renderPackValidationModal();

	/**
	 * @brief
	 *  Render the dirty-close confirmation modal for `m_pendingCloseDocId`.
	 */
	void renderCloseDocumentModal();

	/**
	 * @brief
	 *  Render the recent-projects popup (triggered from the ribbon File > Recent button).
	 */
	void renderRecentProjectsPopup();

	/**
	 * @brief
	 *  Render the snap-step preset popup (triggered from the ribbon Scene > Gizmo > Step button).
	 *
	 * Shows fraction / multiple presets when a `Tilemap` exists in the active scene, or raw
	 * world-unit presets otherwise. Selecting an entry updates `m_settings.snapMultiplier`
	 * (tilemap path) or `m_settings.snapStep` (raw path).
	 */
	void renderSnapStepPopup();

	/**
	 * @brief
	 *  Launch the async validation + pack pipeline with the current wizard settings.
	 */
	void launchPackValidation();

	/**
	 * @brief
	 *  Start the async packaging process (called after validation).
	 */
	void startPackGame();

	/**
	 * @brief
	 *  Access (or create) the single SceneDocument during Phase 1.
	 * @return The SceneDocument.
	 */
	auto ensureActiveSceneDocument() -> SceneDocument&;

	/**
	 * @brief
	 *  Get the active SceneDocument, or nullptr if the active doc is not a scene.
	 * @return Pointer to the active scene document, or `nullptr` when not available.
	 */
	[[nodiscard]] auto activeSceneDocument() const -> SceneDocument*;

	/**
	 * @brief
	 *  Shortcut to the active document's Viewport, or nullptr.
	 * @return Pointer to the active viewport, or `nullptr` when not available.
	 */
	[[nodiscard]] auto activeViewport() const -> panel::Viewport*;

	/**
	 * @brief
	 *  Active viewport size, or a sensible default (1280x720) when no viewport exists yet.
	 * @return The math vec2ui.
	 */
	[[nodiscard]] auto activeViewportSize() const -> math::vec2ui;

	/**
	 * @brief
	 *  Handle the key pressed event.
	 * @param[in,out] ioEvent The incoming event.
	 * @return True when the event was consumed.
	 */
	auto onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool;

	/**
	 * @brief
	 *  Handle the mouse button pressed event.
	 * @param[in,out] ioEvent The incoming event.
	 * @return True when the event was consumed.
	 */
	static auto onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool;

	/**
	 * @brief
	 *  Handle the scene play event.
	 */
	void onScenePlay();

	/**
	 * @brief
	 *  Handle the scene pause event.
	 */
	void onScenePause();

	/**
	 * @brief
	 *  Handle the scene resume event.
	 */
	void onSceneResume();

	/**
	 * @brief
	 *  Handle the scene stop event.
	 */
	void onSceneStop();

	/**
	 * @brief
	 *  Handle the scene step event.
	 */
	void onSceneStep();

	/**
	 * @brief
	 *  Handle the duplicate entity event.
	 */
	void onDuplicateEntity();

	/**
	 * @brief
	 *  Perform undo and restore entity selection from the command's hint.
	 */
	void performUndo();

	/**
	 * @brief
	 *  Perform redo and restore entity selection from the command's hint.
	 */
	void performRedo();

	/// Top-of-window action ribbon (File / Edit / View / Project tabs).
	gui::widgets::Ribbon m_ribbon;
	/**
	 * Last document type for which the ribbon's contextual tab was populated.  When the active
	 * document switches to a different type, we rebuild the ribbon.
	 */
	std::optional<DocumentType> m_lastRibbonDocType;
	/// 2D camera controller wired to the active viewport for pan/zoom.
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
	/**
	 * Documents that must actually be closed at the start of the next frame.  Deferring the
	 * destruction avoids a use-after-free on the Viewport's Vulkan colour-attachment: during the
	 * current frame, ImGui's draw list still references the texture and only samples it at the
	 * `UiLayer::end()` flush.
	 */
	std::vector<core::UUID> m_deferredCloseIds;
	/**
	 * @brief
	 *  Asset paths queued for opening at the start of the next frame (deferred from
	 *  in-render callbacks like SceneHierarchy context menus and document drag-drop).
	 *
	 * Calling `openTilemapFile` / `openTilesetFile` / `openScene` directly inside the
	 * SceneHierarchy popup mutates the document stack and panel state mid-render, which
	 * corrupts the iteration that opened the popup. We push the path here and drain the
	 * queue at the start of `onImGuiRender` before any panel is rendered.
	 */
	std::vector<std::filesystem::path> m_deferredOpenPaths;
	/// The currently loaded project (or empty when no project is open).
	Project m_project;
	/// Persistent editor preferences (window layout, recent projects, etc.).
	EditorSettings m_settings;
	/// Allocation counter from the previous frame, for the memory profiler readout.
	size_t m_lastAllocCalls = 0;
	/// Deallocation counter from the previous frame, for the memory profiler readout.
	size_t m_lastDeallocCalls = 0;
	/// Open documents (scenes for now; later also Lua scripts, node graphs...).
	DocumentManager m_documents;

	/**
	 * @brief
	 *  Find an open SceneDocument whose on-disk path matches `iPath`.
	 * @return The matching scene document by path, or empty when nothing was found.
	 */
	[[nodiscard]] auto findSceneDocumentByPath(const std::filesystem::path& iPath) const -> SceneDocument*;

	/**
	 * @brief
	 *  Keep panels synced with the currently active document.
	 */
	void syncActiveDocumentPanels();

	/**
	 * @brief
	 *  Get the document currently in Play or Pause mode (at most one).
	 * @return The matching playing scene document, or empty when nothing was found.
	 */
	[[nodiscard]] auto findPlayingSceneDocument() const -> SceneDocument*;

	/// Scene hierarchy tree panel (entity list with drag-drop reparenting).
	panel::SceneHierarchy m_sceneHierarchy;
	/// Content browser panel listing project assets.
	panel::ContentBrowser m_contentBrowser;
	/// Component inspector panel for the selected entity / asset.
	panel::Parameters m_parameters;
	/// Project metadata + window-config editor.
	panel::ProjectSettings m_projectSettings;
	/// Scene-wide rendering / physics settings panel.
	panel::SceneSettings m_sceneSettings;
	/// Log output panel (filterable).
	panel::LogPanel m_logPanel;
	/// Editor preferences panel (theme, key bindings).
	panel::SettingsPanel m_settingsPanel;
	/// Modal that surfaces progress for long-running async work (packaging, asset scans).
	panel::AsyncProgressModal m_asyncProgress;
	/// In-editor help / documentation panel.
	panel::HelpPanel m_helpPanel;

public:
	/**
	 * @brief
	 *  Open the in-editor help on the contextual page for the component currently hovered
	 *        in the SceneHierarchy inspector, or the editor overview when none is hovered.
	 */
	void onContextualHelp();

private:
	/// Maps menu / keyboard / ribbon actions to their handlers.
	ActionRegistry m_actionRegistry;
};
}// namespace owl::nest
