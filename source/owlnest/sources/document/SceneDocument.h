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

namespace owl::nest {

/**
 * @brief Document wrapping a `.owl` scene edited in the editor.
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

	SceneDocument();

	// --- Document interface --------------------------------------------------

	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Scene; }
	[[nodiscard]] auto title() const -> std::string override;
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_scenePath; }
	[[nodiscard]] auto isDirty() const -> bool override { return m_undoManager.isDirty(); }

	void onAttach(EditorLayer* iEditor) override;
	void onDetach() override;
	void onUpdate(const core::Timestep& iTimeStep) override;
	void onEvent(event::Event& ioEvent) override;
	void onImGuiRender() override;

	auto save() -> bool override;
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	[[nodiscard]] auto undoManager() -> UndoManager& override { return m_undoManager; }
	[[nodiscard]] auto undoManager() const -> const UndoManager& override { return m_undoManager; }

	// --- Scene-specific API --------------------------------------------------

	/// @brief Reset to a fresh empty scene (clears undo).
	void newScene(const math::vec2ui& iViewportSize);

	/// @brief Install a freshly deserialized scene as both editor and active (main thread only).
	void applyLoadedScene(const shared<scene::Scene>& iScene, const std::filesystem::path& iPath,
						  const math::vec2ui& iViewportSize);

	/// @brief Get the active scene (the one currently edited or running).
	[[nodiscard]] auto getActiveScene() const -> const shared<scene::Scene>& { return m_activeScene; }
	/// @brief Get the editor-side scene (design snapshot, untouched by Play mode).
	[[nodiscard]] auto getEditorScene() const -> const shared<scene::Scene>& { return m_editorScene; }
	/// @brief Set the scene path (used after Save As).
	void setScenePath(const std::filesystem::path& iPath) { m_scenePath = iPath; }

	// --- Playback ------------------------------------------------------------

	[[nodiscard]] auto state() const -> State { return m_state; }
	void onScenePlay();
	void onScenePause() { m_state = State::Pause; }
	void onSceneResume() { m_state = State::Play; }
	void onSceneStop();
	void onSceneStep() { m_stepRequested = true; }
	auto consumeStepRequest() -> bool;

	/// @brief Request the editor to stop Play mode (triggered by `scene.quit()` Lua).
	void requestStop() { m_stopRequested = true; }
	/// @brief True when Lua asked the engine to stop playing (consumed by EditorLayer on next update).
	[[nodiscard]] auto isStopRequested() const -> bool { return m_stopRequested; }
	void clearStopRequest() { m_stopRequested = false; }

	/// @brief Process any pending teleport request posted by Lua on the active scene.
	void handleTeleportRequest(const math::vec2ui& iViewportSize);
	/// @brief Process any pending save/load request posted by Lua on the active scene.
	void handleSaveLoadRequest(const math::vec2ui& iViewportSize);

	/// @brief Apply a pending teleport velocity on the player spawned in the new scene.
	void applyPendingTeleportVelocity();

private:
	shared<scene::Scene> m_editorScene;
	shared<scene::Scene> m_activeScene;
	std::filesystem::path m_scenePath;

	State m_state = State::Edit;
	bool m_stepRequested = false;
	bool m_stopRequested = false;

	bool m_pendingTeleportVelocity = false;
	math::vec2f m_teleportVelocity = {0.f, 0.f};
	std::string m_teleportTargetName;

	UndoManager m_undoManager;
	EditorLayer* mp_editor = nullptr;
};

}// namespace owl::nest
