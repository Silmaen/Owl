/**
 * @file SceneDocument.h
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"
#include "panel/Viewport.h"

namespace owl::nest {

/**
 * @brief
 *  Document wrapping a `.owl` scene edited in the editor.
 *
 * Holds the two scene shared pointers (`editor` for design, `active` for the
 * running copy during Play), the on-disk path, the playback state (Edit /
 * Play / Pause), step/stop signals, teleport and save-load request plumbing
 * from Lua, plus a per-document undo/redo stack.
 *
 * File I/O stays in `EditorLayer` (it coordinates the async progress modal
 * and the task scheduler); this class only owns the in-memory state.
 */
class SceneDocument final : public Document {
public:
	enum struct State : uint8_t { Edit, Play, Pause };

	/**
	 * @brief
	 *  Default constructor.
	 */
	SceneDocument();

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Document kind.
	 * @return Always `DocumentType::Scene`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Scene; }

	/**
	 * @brief
	 *  Tab title (file stem when saved, "Untitled" otherwise).
	 * @return The display title.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  On-disk path of the scene file.
	 * @return The scene file path (empty if never saved).
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_scenePath; }

	/**
	 * @brief
	 *  Whether the document has unsaved changes.
	 * @return True when the undo manager reports a non-saved state.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return m_undoManager.isDirty(); }

	/**
	 * @brief
	 *  Called once when the document is attached to the manager.
	 * @param[in] iEditor Owning editor layer used to forward events and toolbar requests.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Called once when the document is being removed.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Per-frame update: drive scene runtime/editor logic and viewport rendering.
	 * @param[in] iTimeStep Frame time delta.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Dispatch an event to the document (forwarded from `EditorLayer::onEvent`).
	 * @param[in,out] ioEvent The incoming event; handlers may flag it as handled.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Render this document's panels (the Viewport, mainly) in the current ImGui frame.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Save the scene to its current on-disk path.
	 * @return True on success; false on I/O failure or when no path is set.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Save the scene to an explicit path and update `m_scenePath`.
	 * @param[in] iPath Destination file.
	 * @return True on success; false on I/O failure.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Access to the document's undo/redo stack.
	 * @return The undo manager (mutable).
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_undoManager; }

	/**
	 * @brief
	 *  Const access to the document's undo/redo stack.
	 * @return The undo manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_undoManager; }

	// --- Scene-specific API --------------------------------------------------
	/**
	 * @brief
	 *  Reset to a fresh empty scene (clears undo).
	 */
	void newScene(const math::vec2ui& iViewportSize);

	/**
	 * @brief
	 *  Install a freshly deserialized scene as both editor and active (main thread only).
	 */
	void applyLoadedScene(const shared<scene::Scene>& iScene, const std::filesystem::path& iPath,
						  const math::vec2ui& iViewportSize);

	/**
	 * @brief
	 *  Get the active scene (the one currently edited or running).
	 * @return The active scene.
	 */
	[[nodiscard]] auto getActiveScene() const -> const shared<scene::Scene>& { return m_activeScene; }

	/**
	 * @brief
	 *  Get the editor-side scene (design snapshot, untouched by Play mode).
	 * @return The editor scene.
	 */
	[[nodiscard]] auto getEditorScene() const -> const shared<scene::Scene>& { return m_editorScene; }

	/**
	 * @brief
	 *  Set the scene path (used after Save As).
	 */
	void setScenePath(const std::filesystem::path& iPath) { m_scenePath = iPath; }

	// --- Playback ------------------------------------------------------------
	/**
	 * @brief
	 *  Current playback state of the document.
	 * @return One of `Edit`, `Play`, `Pause`.
	 */
	[[nodiscard]] auto state() const -> State { return m_state; }

	/**
	 * @brief
	 *  Handle the scene play event.
	 */
	void onScenePlay();

	/**
	 * @brief
	 *  Handle the scene pause event.
	 */
	void onScenePause() { m_state = State::Pause; }

	/**
	 * @brief
	 *  Handle the scene resume event.
	 */
	void onSceneResume() { m_state = State::Play; }

	/**
	 * @brief
	 *  Handle the scene stop event.
	 */
	void onSceneStop();

	/**
	 * @brief
	 *  Handle the scene step event.
	 */
	void onSceneStep() { m_stepRequested = true; }

	/**
	 * @brief
	 *  Consume the pending single-step request (set by the toolbar Step button).
	 * @return True if a step was pending (and is now consumed); false otherwise.
	 */
	auto consumeStepRequest() -> bool;

	/**
	 * @brief
	 *  Request the editor to stop Play mode (triggered by `scene.quit()` Lua).
	 */
	void requestStop() { m_stopRequested = true; }

	/**
	 * @brief
	 *  True when Lua asked the engine to stop playing (consumed by EditorLayer on next update).
	 * @return True when the object is stop requested.
	 */
	[[nodiscard]] auto isStopRequested() const -> bool { return m_stopRequested; }

	/**
	 * @brief
	 *  Clear stop request.
	 */
	void clearStopRequest() { m_stopRequested = false; }

	/**
	 * @brief
	 *  Process any pending teleport request posted by Lua on the active scene.
	 */
	void handleTeleportRequest(const math::vec2ui& iViewportSize);

	/**
	 * @brief
	 *  Process any pending save/load request posted by Lua on the active scene.
	 */
	void handleSaveLoadRequest(const math::vec2ui& iViewportSize);

	/**
	 * @brief
	 *  Apply a pending teleport velocity on the player spawned in the new scene.
	 */
	void applyPendingTeleportVelocity();

	/**
	 * @brief
	 *  Whether the active scene has been replaced (teleport / save-load) since
	 * the last `consumeSceneSwapped`. Used by `EditorLayer` to rebuild the
	 * renderer stack from the new scene's `EnabledRenderers` config — without
	 * this poll, an in-Play scene swap inherits the previous scene's stack
	 * and per-scene overrides silently stop applying.
	 * @return True on success, false otherwise.
	 */
	[[nodiscard]] auto consumeSceneSwapped() -> bool {
		const bool wasSwapped = m_sceneSwapped;
		m_sceneSwapped = false;
		return wasSwapped;
	}

	/**
	 * @brief
	 *  Access this document's viewport panel (renders into its own framebuffer + ImGui tab).
	 * @return The viewport panel (mutable).
	 */
	[[nodiscard]] auto getViewport() -> panel::Viewport& { return m_viewport; }

	/**
	 * @brief
	 *  Const access to this document's viewport panel.
	 * @return The viewport panel.
	 */
	[[nodiscard]] auto getViewport() const -> const panel::Viewport& { return m_viewport; }

private:
	/// The persistent editor-side scene (what is saved to disk / restored when leaving Play).
	shared<scene::Scene> m_editorScene;
	/// The scene currently being updated/rendered (a copy of `m_editorScene` while in Play).
	shared<scene::Scene> m_activeScene;
	/// Disk path of the scene file backing this document.
	std::filesystem::path m_scenePath;

	/// Current play / pause / edit state.
	State m_state = State::Edit;
	/// True for one frame after `requestStep()`; consumed by the runtime loop.
	bool m_stepRequested = false;
	/// True for one frame after `requestStop()`; consumed by the runtime loop.
	bool m_stopRequested = false;

	/// Teleport: `m_teleportVelocity` should be re-applied after the next scene load.
	bool m_pendingTeleportVelocity = false;
	/// Velocity in m/s to re-apply to the player after the deferred teleport.
	math::vec2f m_teleportVelocity = {0.f, 0.f};
	/// Optional target-entity name to spawn the player at after teleport.
	std::string m_teleportTargetName;
	/**
	 * Set whenever the active scene is replaced by a teleport / save-load.
	 * Cleared by `consumeSceneSwapped`; lets the editor rebuild renderer
	 * state on the new scene without having to inspect every swap site.
	 */
	bool m_sceneSwapped = false;

	/// Per-document undo/redo manager for scene edits.
	SceneUndoManager m_undoManager;
	/// Editor layer back-pointer (for menus, dialogs, status messages).
	EditorLayer* mp_editor = nullptr;
	/// Viewport panel rendering this scene.
	panel::Viewport m_viewport;
};

}// namespace owl::nest
