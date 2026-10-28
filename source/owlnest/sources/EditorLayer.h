/**
 * @file EditorLayer.h
 * @author Silmaen
 * @date 21/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owlgui.h>

#include "ActionRegistry.h"
#include "DockLayout.h"
#include "EditorSettings.h"
#include "GamePackager.h"
#include "Project.h"
#include "RecoveryManager.h"
#include "RibbonBuilder.h"
#include "VoxelBrush.h"
#include "commands/CommandForm.h"
#include "commands/CommandRegistry.h"
#include "document/DocumentManager.h"
#include "document/SceneDocument.h"
#include "mcp/McpServer.h"
#include "panel/AssetImport.h"
#include "panel/AsyncProgressModal.h"
#include "panel/Backstage.h"
#include "panel/CommandFormWindow.h"
#include "panel/CommandPalette.h"
#include "panel/ConsolePanel.h"
#include "panel/ScriptDebugPanel.h"
#include "panel/ContentBrowser.h"
#include "panel/FrameMeter.h"
#include "panel/FullLoadProbe.h"
#include "panel/GpuCounterMonitor.h"
#include "panel/HelpPanel.h"
#include "panel/LogPanel.h"
#include "panel/NewProjectDialog.h"
#include "panel/Parameters.h"
#include "panel/ProjectSettings.h"
#include "panel/RecoveryPrompt.h"
#include "panel/SceneHierarchy.h"
#include "panel/SceneSettings.h"
#include "panel/SettingsPanel.h"
#include "panel/StatusBar.h"
#include "panel/VoxelPalette.h"
#include "session/EditorSession.h"

#include <gui/widgets/Toast.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  Indicator state of an MCP server activity.
 * @param[in] iStatus The server activity.
 * @return The status bar state.
 */
[[nodiscard]] auto getMcpIndicator(mcp::HttpStatus iStatus) -> panel::StatusInfo::Mcp;

/**
 * @brief
 *  Class EditorLayer.
 */
class EditorLayer final : public app::layer::Layer, public session::EditorSession {
	friend class RibbonBuilder;

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
	 *  Read access to the loaded project (used by side panels).
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
	 *  New HUD: an untitled scene document saved as an `.owlhud`, holding a full-screen canvas.
	 */
	void newHud();

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
	 *  Open a file in the document its extension calls for (see classifyAsset); prefabs are instantiated and
	 *  unknown files open in the code editor.
	 * @param[in] iPath Absolute path of the file.
	 */
	void openAssetFile(const std::filesystem::path& iPath);

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
	 *  Open a font asset (`.ttf`, `.otf` or its `.owlfont` sidecar) in a `FontDocument` (or switch to it).
	 * @param[in] iPath Absolute path to the file.
	 */
	void openFontFile(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Open an image asset in a read-only `ImageDocument` (or switch to it).
	 * @param[in] iPath The image file.
	 */
	void openImageFile(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Open a mesh asset in a read-only `MeshDocument` (or switch to it).
	 * @param[in] iPath The mesh file.
	 */
	void openMeshFile(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Open a `.owlmat` material in a `MaterialDocument` (or switch to it).
	 * @param[in] iPath The material file.
	 */
	void openMaterialFile(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Open an `input_actions.yml` in an `ActionMapDocument` (or switch to it); a missing file starts from the built-in
	 *  actions and is created by the first save.
	 * @param[in] iPath Absolute path to the file.
	 */
	void openActionMapFile(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Open the action map of the project (`input_actions.yml` at its root).
	 */
	void openActionMapEditor();

	/**
	 * @brief
	 *  Notify the editor that an action map has just been saved: the one of the project is reloaded in the engine
	 *  context, so a running Play session reads the new bindings on its next frame.
	 * @param[in] iAbsolutePath The absolute path of the saved file.
	 */
	void onActionMapSaved(const std::filesystem::path& iAbsolutePath);

	/**
	 * @brief
	 *  Suspend the editor shortcuts (a binding capture needs every key).
	 * @param[in] iSuspended True to suspend them.
	 */
	void setShortcutsSuspended(const bool iSuspended) { m_actionRegistry.setSuspended(iSuspended); }

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
	 *  Hot reload: take a file changed on disk into account in the open documents.
	 *
	 * Tilesets, tilemaps and Lua scripts are reloaded in every open scene (edited and played copies); a scene file
	 * reloads its document when it has no unsaved edit and is not playing (otherwise a warning keeps the edits).
	 * @param[in] iFile The changed file.
	 */
	void onAssetFileChanged(const std::filesystem::path& iFile);

	/**
	 * @brief
	 *  Synchronously open (or return) a `SceneDocument` for the given on-disk scene.
	 * @param[in] iScenePath Absolute or project-relative path to a `.owl` file.
	 * @return Pointer to the live `SceneDocument`, or `nullptr` on missing/invalid file.
	 *
	 * Unlike `openScene`, this does **not** route through the async progress modal and does
	 * **not** switch the active document — it is meant for editor-side actions (cross-scene
	 * tooling) that need to mutate a scene's entities without yanking
	 * the user's focus. The file is read and deserialized synchronously on the main thread.
	 */
	auto loadOrOpenSceneDocument(const std::filesystem::path& iScenePath) -> SceneDocument*;

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
	 *  Context menu of a document tab: save, close, close the others, reveal the file in the Content Browser, copy its
	 *  path. Call right after the `ImGui::Begin` of the document window, whose tab is then the last item.
	 * @param[in,out] ioDocument The document of the tab.
	 */
	void renderDocumentTabMenu(Document& ioDocument);

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
	 *  Close the project: its documents close (unsaved changes go to recovery), its asset directory leaves the
	 *  search paths and everything loaded from it is unloaded (textures, sounds, fonts, HUDs, previews).
	 */
	void closeProject();

	/**
	 * @brief
	 *  Switch project at the start of the next frame, when no widget of the current one draws a texture any more.
	 * @param[in] iDir The project directory to open, empty to close the project only.
	 */
	void requestProjectSwitch(const std::filesystem::path& iDir);

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
	 *  Create an entity drawing a mesh file in the active scene (`mesh.create`), tagged for the first `RendererMesh`
	 *  layer of the render stack when that layer is not the default one, and select it; without such a layer, a modal
	 *  first offers to add one to the project stack.
	 * @param[in] iMeshPath Absolute path to the `.gltf`, `.glb` or `.obj` file.
	 * @param[in] iAssetRelativePath Asset path stored in the `MeshRenderer` (auto-derived when empty).
	 * @param[in] iPosition World position of the entity; the origin when absent.
	 */
	void placeMesh(const std::filesystem::path& iMeshPath, const std::string& iAssetRelativePath = {},
				   const std::optional<math::vec3>& iPosition = std::nullopt);

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
	 *  Project to open when the layer is attached (`OwlNest --project <dir>`), instead of showing the File screen.
	 * @param[in] iProject Project folder or `owl_project.yml`.
	 */
	void setStartupProject(const std::filesystem::path& iProject) { m_startupProject = iProject; }

	/**
	 * @brief
	 *  Render the snap-step preset popup (triggered from the viewport snap button).
	 *
	 * Shows fraction / multiple presets when a `Tilemap` exists in the active scene, or raw
	 * world-unit presets otherwise. Selecting an entry updates `m_settings.snapMultiplier`
	 * (tilemap path) or `m_settings.snapStep` (raw path).
	 */
	void renderSnapStepPopup();

	/**
	 * @brief
	 *  Run an editor action by its identifier ("scene.play"), as its shortcut would.
	 * @param[in] iId The action identifier.
	 * @return False if no action has this identifier.
	 */
	auto runAction(const std::string& iId) const -> bool { return m_actionRegistry.run(iId); }

	/**
	 * @brief
	 *  Shortcut of an editor action, for tooltips.
	 * @param[in] iId The action identifier.
	 * @return The shortcut ("Ctrl+S"), empty when unbound.
	 */
	[[nodiscard]] auto getActionShortcut(const std::string& iId) const -> std::string {
		return m_actionRegistry.getShortcutString(iId);
	}

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
	 *  Mutable access to the editor settings (for toggles wired from non-EditorLayer panels).
	 * @return The settings.
	 */
	[[nodiscard]] auto getSettings() -> EditorSettings& { return m_settings; }

	/**
	 * @brief
	 *  Access the frame costs averaged for the frame chip and the Stats panel.
	 * @return The frame meter.
	 */
	[[nodiscard]] auto getFrameMeter() const -> const panel::FrameMeter& { return m_frameMeter; }

	/**
	 * @brief
	 *  Check if a viewport hidden behind another tab still draws its scene: MCP reads the viewport back (captures,
	 *  bounded plays), so it stays drawn while the MCP server is on.
	 * @return True when hidden viewports draw.
	 */
	[[nodiscard]] auto rendersHiddenViewports() const -> bool {
		return m_settings.mcpEnabled || m_playDriver.isActive();
	}

	/**
	 * @brief
	 *  Open an asset named by an inspector field (relative to an asset folder) in its editor.
	 * @param[in] iPath The path stored in the component.
	 */
	void openAssetFromInspector(const std::filesystem::path& iPath, uint32_t iLine = 0);

	/**
	 * @brief
	 *  Select a line of an open code document and scroll to it.
	 * @param[in] iPath The file of the document.
	 * @param[in] iLine One-based line.
	 */
	void revealCodeLine(const std::filesystem::path& iPath, uint32_t iLine);

	/**
	 * @brief
	 *  Register the camera actions of the active viewport: reset, frame scene, axis views and camera viewpoint.
	 */
	void registerCameraActions();

	/**
	 * @brief
	 *  Register the actions that make the text larger, smaller or back to its size (Ctrl+=, Ctrl+-, Ctrl+0).
	 */
	void registerTextScaleActions();

	/**
	 * @brief
	 *  Route a Content Browser file drop to `openScene` / `openCodeFile` / prefab instantiation / mesh placement.
	 * @param[in] iRelativePath Path of the dropped file, relative to the browsed folder.
	 * @param[in] iDropPoint World point under the cursor of a viewport drop, where a mesh is placed.
	 */
	void handleContentBrowserDrop(const std::filesystem::path& iRelativePath,
								  const std::optional<math::vec3>& iDropPoint = std::nullopt);

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

	// --- EditorSession: the active scene document (EditorLayerEditorSession.cpp) ---------------------------------

	/**
	 * @brief
	 *  Get the scene of the active scene document (its running copy while it plays).
	 * @return The scene, null without a scene document.
	 */
	[[nodiscard]] auto getSessionScene() const -> shared<scene::Scene> override { return getActiveScene(); }

	/**
	 * @brief
	 *  Get the file of the active scene document.
	 * @return The path, empty when never saved.
	 */
	[[nodiscard]] auto getSessionScenePath() const -> std::filesystem::path override;

	/**
	 * @brief
	 *  Get the directory of the open project.
	 * @return The directory, empty without a project.
	 */
	[[nodiscard]] auto getSessionProjectDirectory() const -> std::filesystem::path override {
		return m_project.isLoaded() ? m_project.projectDirectory : std::filesystem::path{};
	}

	/**
	 * @brief
	 *  Get the undo history of the active scene document.
	 * @return The undo manager, null without a scene document.
	 */
	[[nodiscard]] auto getSessionUndo() -> SceneUndoManager* override { return activeUndoManager(); }

	/**
	 * @brief
	 *  Get the editor command registry.
	 * @return The registry.
	 */
	[[nodiscard]] auto getRegistry() const -> const commands::CommandRegistry& override { return m_commands; }

	/**
	 * @brief
	 *  Build the command context of the active scene document; edits are refused while it plays.
	 * @return The context.
	 */
	[[nodiscard]] auto makeCommandContext() -> commands::CommandContext override;

	/**
	 * @brief
	 *  Check whether the active scene document plays.
	 * @return True in Play or Pause.
	 */
	[[nodiscard]] auto isScenePlaying() const -> bool override { return getState() != State::Edit; }

	/**
	 * @brief
	 *  Start a bounded play of the active scene document, played over the next frames of the main loop.
	 * @param[in] iRequest The request.
	 * @return Nothing on success, or why the play did not start.
	 */
	auto startPlay(const session::PlayRequest& iRequest) -> expected<void, session::SessionFailure> override;

	/**
	 * @brief
	 *  Check whether a bounded play is running.
	 * @return True until its bound is reached.
	 */
	[[nodiscard]] auto isPlayRunning() const -> bool override { return m_playDriver.isActive(); }

	/**
	 * @brief
	 *  Take the report of the last bounded play.
	 * @return The report, once.
	 */
	auto takePlayReport() -> std::optional<session::PlayReport> override;

	/**
	 * @brief
	 *  Stop the active scene document.
	 * @return Nothing on success, `NotPlaying` in Edit.
	 */
	auto stopPlay() -> expected<void, session::SessionFailure> override;

	/**
	 * @brief
	 *  Read the last frame of the active viewport back.
	 * @return The image, or why it cannot be read.
	 */
	auto captureFrame() -> expected<session::FrameCapture, session::SessionFailure> override;

	/**
	 * @brief
	 *  Get the frame measurements of the active viewport.
	 * @return The statistics.
	 */
	[[nodiscard]] auto getStatistics() const -> session::SessionStatistics override;

private:
	/**
	 * @brief
	 *  Update the active scene document (its viewport renders it), at the fixed step of a bounded play if one runs.
	 * @param[in,out] ioDocument The active scene document.
	 * @param[in] iTimeStep Duration of the frame.
	 */
	void updateActiveDocument(SceneDocument& ioDocument, const core::Timestep& iTimeStep);

	/**
	 * @brief
	 *  Check that the document of the bounded play is still open and plays.
	 * @return True when it is open, in Play or Pause.
	 */
	[[nodiscard]] auto isPlayDocumentPlaying() const -> bool;

	/**
	 * @brief
	 *  End the bounded play: report, capture, then stop or pause the scene.
	 * @param[in] iInterrupted True when the play stops before its bound.
	 */
	void finishBoundedPlay(bool iInterrupted);

	/**
	 * @brief
	 *  Render stats.
	 * @param[in] iTimeStep Frame time delta.
	 */
	void renderStats(const core::Timestep& iTimeStep);

	/**
	 * @brief
	 *  Draw the Vulkan queue counters of the Stats panel (greyed out on another backend).
	 */
	void renderGpuCounters();

	/**
	 * @brief
	 *  Feed the frame meter with the last frame and cap the frame rate: the foreground limit while the editor has
	 *  the focus or runs a play driven from MCP, the background one otherwise.
	 */
	void updateFramePacing();

	/**
	 * @brief
	 *  Check if the next frame may wait for an input: every scene in Edit, no gesture, key or button held, no
	 *  notification shown, no task, preview or font render pending.
	 * @return True when nothing changes on screen without an input.
	 */
	[[nodiscard]] auto isIdle() const -> bool;

	/**
	 * @brief
	 *  Lift every frame cap (vsync, frame rate limit, idle wait) for a few seconds and measure the frame rate.
	 */
	void startFullLoadMeasure();


	/**
	 * @brief
	 *  Draw the File screen when it is open.
	 */
	void renderBackstage();

	/**
	 * @brief
	 *  Draw the command palette when open, listing the editor actions and the entities of the active scene.
	 */
	void renderCommandPalette();

	/**
	 * @brief
	 *  Draw the argument form of a registry command and run it once confirmed.
	 */
	void renderCommandForm();

	/**
	 * @brief
	 *  Run a registry command chosen by the user and report its result in a toast.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Command arguments.
	 * @param[in] iSource Who asks, recorded by the journal.
	 */
	void runCommandFromUi(const std::string& iName, const commands::CommandArgs& iArgs,
						  commands::CommandSource iSource = commands::CommandSource::Palette);

	/**
	 * @brief
	 *  Switch the panel arrangement and rebuild the dock tree on the next frame.
	 * @param[in] iPreset The arrangement.
	 */
	void applyLayoutPreset(LayoutPreset iPreset);

	/**
	 * @brief
	 *  Gather the state shown by the status bar.
	 * @return The status bar state.
	 */
	[[nodiscard]] auto buildStatusInfo() const -> panel::StatusInfo;

	/**
	 * @brief
	 *  Draw the status bar and route its requests (open the Log, the MCP settings).
	 */
	void renderStatusBar();

	/**
	 * @brief
	 *  Follow the MCP settings (start, stop, port, token) and run the queued MCP requests unless the editor is busy.
	 */
	void updateMcp();

	/**
	 * @brief
	 *  Check whether an MCP request must wait: a gesture (gizmo drag, inspector edit, held widget) or a popup or
	 *  modal is in progress.
	 * @return True while busy.
	 */
	[[nodiscard]] auto isMcpBusy() const -> bool;


	/**
	 * @brief
	 *  Render the dirty-close confirmation modal for `m_pendingCloseDocId`.
	 */
	void renderCloseDocumentModal();

	/**
	 * @brief
	 *  Render the modal offering to add a mesh layer to the project stack for `m_pendingMesh`.
	 */
	void renderMeshLayerModal();

	/**
	 * @brief
	 *  Run `mesh.create` for a mesh file, tagged for the given layer, and select the new entity.
	 * @param[in] iMeshPath Absolute path to the mesh file.
	 * @param[in] iAssetRelativePath Asset path stored in the `MeshRenderer` (auto-derived when empty).
	 * @param[in] iPosition World position of the entity; the origin when absent.
	 * @param[in] iLayer Render-stack layer recorded in a `RendererTag`; none when empty.
	 */
	void createMeshEntity(const std::filesystem::path& iMeshPath, const std::string& iAssetRelativePath,
						  const std::optional<math::vec3>& iPosition, const std::string& iLayer);


	/**
	 * @brief
	 *  Access the active SceneDocument, creating an empty one when none is active.
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
	 *  Pause the game on a breakpoint reached by a script, then draw the Script Debug panel and run its request.
	 */
	void renderScriptDebug();

	/**
	 * @brief
	 *  Open the visual script of the active scene or HUD on the custom event of a handler, creating the script and the
	 *  event when missing (the "Edit in Graph" button of the inspector).
	 * @param[in] iFunction Lua name of the handler.
	 */
	void editHandlerInGraph(const std::string& iFunction);

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

	/// Builds and draws the ribbon.
	RibbonBuilder m_ribbonBuilder{*this};
	/// 2D camera controller wired to the active viewport for pan/zoom.
	renderer::CameraOrthoController m_cameraController;
	/// The File screen (projects, packaging, settings, help); shown at start-up without a project.
	panel::Backstage m_backstage;
	/// Folder of the Engine Demo shipped with the editor, offered by the File screen; empty when not found.
	std::filesystem::path m_sampleProject;
	/// Folders of the sample games shipped beside the Engine Demo, offered by the File screen.
	std::vector<std::filesystem::path> m_sampleGames;
	/// Project opened at attach time; empty to show the File screen instead.
	std::filesystem::path m_startupProject;
	/// Pending document id to close (awaits dirty-confirmation modal). 0 when idle.
	core::UUID m_pendingCloseDocId{0};
	/// True when the close-confirmation modal should open on the next frame.
	bool m_openCloseDocModal = false;
	/// A mesh drop waiting for the answer of the mesh-layer modal.
	struct PendingMesh {
		/// Absolute path to the mesh file.
		std::filesystem::path path;
		/// Asset path stored in the `MeshRenderer`.
		std::string assetPath;
		/// World position of the entity.
		std::optional<math::vec3> position;
	};
	/// Mesh dropped while no `RendererMesh` layer draws it; empty when idle.
	std::optional<PendingMesh> m_pendingMesh;
	/// True when the mesh-layer modal should open on the next frame.
	bool m_openMeshLayerModal = false;
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
	/// Line to reveal once the deferred file is open, by absolute path.
	std::map<std::filesystem::path, uint32_t> m_deferredOpenLines;
	/// Visual scripts being opened for a handler, by asset path: the custom event to select once open.
	std::map<std::string, std::string> m_deferredGraphEvents;
	/// Project to switch to at the start of the next frame (empty path: close only), set by `requestProjectSwitch`.
	std::optional<std::filesystem::path> m_pendingProjectSwitch;
	/// The currently loaded project (or empty when no project is open).
	Project m_project;
	/// Persistent editor preferences (window layout, recent projects, etc.).
	EditorSettings m_settings;
#ifdef OWL_TRACKER_ACTIVE
	/// Allocation counter from the previous frame, for the memory profiler readout.
	size_t m_lastAllocCalls = 0;
	/// Deallocation counter from the previous frame, for the memory profiler readout.
	size_t m_lastDeallocCalls = 0;
#endif
	/// Default dock tree, presets and the decision to rebuild it.
	DockLayout m_dockLayout;
	/// One-line bar under the docked panels.
	panel::StatusBar m_statusBar;
	/// Short notifications in the bottom-right corner (saves, exports, failures).
	gui::widgets::ToastQueue m_toasts;
	/// True once the renderer fallback (Vulkan replaced by OpenGL) has been told in a toast.
	bool m_fallbackNotified = false;
	/// Search over the actions, the entities and the assets, opened by pressing Shift twice.
	panel::CommandPalette m_commandPalette;
	/// Argument form of the registry command chosen in the palette.
	panel::CommandFormWindow m_commandForm;
	/// Asset files listed by the palette, with their name relative to their asset folder; refreshed on each opening.
	std::vector<std::pair<std::filesystem::path, std::string>> m_paletteAssets;
	/// When the last autosave wrote something; empty before the first one.
	std::optional<std::chrono::steady_clock::time_point> m_lastAutosave;
	/// Per-frame Vulkan counter deltas and their 2 s maxima, for the Stats panel.
	panel::GpuCounterMonitor m_gpuCounters;
	/// CPU and GPU cost of the frames, the waits for the display excluded.
	panel::FrameMeter m_frameMeter;
	/// Frame rate of the editor running flat out, measured on request from the Stats panel.
	panel::FullLoadProbe m_fullLoadProbe;
	/// Open documents (scenes, HUDs, assets, scripts, node graphs...).
	DocumentManager m_documents;
	/// Id of the hot reload listener registered at attach.
	uint32_t m_hotReloadListener = 0;
	/// Autosave of the modified documents of the open project (crash recovery).
	RecoveryManager m_recovery;
	/// Modal offering the autosaves of a session that ended with unsaved changes.
	panel::RecoveryPrompt m_recoveryPrompt;
	/// Modal asking for the name, the folder and the template of a new project.
	panel::NewProjectDialog m_newProjectDialog;

	/**
	 * @brief
	 *  Create the project the new-project dialogue asked for, then open it.
	 * @param[in] iRequest The request.
	 */
	void createProject(const panel::NewProjectRequest& iRequest);

	/**
	 * @brief
	 *  Point the autosave at the open project and offer the autosaves its previous session left.
	 */
	void startProjectRecovery();

	/**
	 * @brief
	 *  Remember the open tabs, the active tab and the selection of the open project in the settings.
	 */
	void saveProjectSession();

	/**
	 * @brief
	 *  Reopen the tabs of the open project's last session, its active tab and its selection.
	 * @return True when at least one document was reopened.
	 */
	auto restoreProjectSession() -> bool;

	/**
	 * @brief
	 *  Apply the user's choice in the recovery modal.
	 * @param[in] iChoice The choice made this frame.
	 */
	void handleRecoveryChoice(panel::RecoveryPrompt::Choice iChoice);

	/**
	 * @brief
	 *  Open (or create) the document an autosave belongs to.
	 * @param[in] iEntry The autosave.
	 * @return The document, or null when its file no longer exists.
	 */
	auto openDocumentForRecovery(const RecoveryEntry& iEntry) -> Document*;

	/**
	 * @brief
	 *  Find an open document of a type by its file.
	 * @param[in] iType The document type.
	 * @param[in] iPath The file the document edits.
	 * @return The document, or null when none is open.
	 */
	[[nodiscard]] auto findDocument(DocumentType iType, const std::filesystem::path& iPath) const -> Document*;

	/**
	 * @brief
	 *  Find an open SceneDocument whose on-disk path matches `iPath`.
	 * @return The matching scene document by path, or empty when nothing was found.
	 */
	[[nodiscard]] auto findSceneDocumentByPath(const std::filesystem::path& iPath) const -> SceneDocument*;

	/**
	 * @brief
	 *  Make the document whose window holds the keyboard focus the active one, whatever its type.
	 */
	void followFocusedDocument();

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
	/// Import settings of the asset selected in the content browser.
	panel::AssetImport m_assetImport;
	/// Log output panel (filterable).
	panel::LogPanel m_logPanel;
	/// Command console and journal of the session.
	panel::ConsolePanel m_consolePanel{*this};
	/// Breakpoint hit and variables of the running scripts.
	panel::ScriptDebugPanel m_scriptDebugPanel;
	/// Editor preferences panel (theme, key bindings).
	panel::SettingsPanel m_settingsPanel;
	/// Modal that surfaces progress for long-running async work (packaging, asset scans).
	panel::AsyncProgressModal m_asyncProgress;
	/// In-editor help / documentation panel.
	panel::HelpPanel m_helpPanel;
	/// Voxel brush palette (block / eraser / structure picker).
	panel::VoxelPalette m_voxelPalette;
	/// Shared voxel-brush state edited by the palette and consumed by the viewport.
	VoxelBrush m_voxelBrush;

public:
	/**
	 * @brief
	 *  Mutable access to the shared voxel-brush state (read by the viewport, edited by the palette).
	 * @return The voxel brush state.
	 */
	[[nodiscard]] auto getVoxelBrush() -> VoxelBrush& { return m_voxelBrush; }

private:
	/**
	 * @brief
	 *  Open the in-editor help on the contextual page for the component currently hovered
	 *        in the SceneHierarchy inspector, or the editor overview when none is hovered.
	 */
	void onContextualHelp();

	/// Maps menu / keyboard / ribbon actions to their handlers.
	ActionRegistry m_actionRegistry;
	/// Named, undoable scene commands, issued by the panels, the shortcuts and the tests.
	commands::CommandRegistry m_commands;
	/// Bounded play of the session in progress.
	session::PlayDriver m_playDriver;
	/// Document the bounded play runs in (compared, never dereferenced once it may be closed).
	const SceneDocument* mp_playDocument{nullptr};
	/// Scene the bounded play last updated.
	shared<scene::Scene> m_playScene;
	/// Report of the last bounded play, until taken.
	std::optional<session::PlayReport> m_playReport;
	/// Frame measurements of the active viewport.
	session::SessionStatistics m_sessionStatistics;
	/// Scene and game packaging (wizard, validation, background export).
	GamePackager m_packager{m_project, m_asyncProgress};
	/// MCP server of the session (off unless the setting enables it); created on attach, destroyed on detach.
	uniq<mcp::McpServer> mp_mcp;
};
}// namespace owl::nest
