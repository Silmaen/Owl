/**
 * @file AnimationDocument.h
 * @author Silmaen
 * @date 27/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"

#include <gui/widgets/Sequencer.h>
#include <scene/AnimationClip.h>

namespace owl::nest {
/**
 * @brief
 *  Undo manager typed for `scene::AnimationClip` edits.
 */
using AnimationUndoManager = UndoManager<scene::AnimationClip>;

/**
 * @brief
 *  Document editing a reusable `.owlanim` animation asset.
 *
 * Owns one `scene::AnimationClip`, plays it back inside a preview pane and exposes a
 * timeline (`gui::widgets::sequencer`) for visual frame-range editing alongside the usual
 * grid / loop / speed-curve controls. Saves the clip back to disk in the engine's
 * `.owlanim` YAML format; the undo stack is typed over the clip itself so commands can be
 * shared between this document and any future inline-clip editor on entities.
 */
class AnimationDocument final : public Document {
public:
	AnimationDocument(const AnimationDocument&) = delete;

	AnimationDocument(AnimationDocument&&) = delete;

	auto operator=(const AnimationDocument&) -> AnimationDocument& = delete;

	auto operator=(AnimationDocument&&) -> AnimationDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	AnimationDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~AnimationDocument() override;

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Document kind.
	 * @return The document type.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Animation; }

	/**
	 * @brief
	 *  Tab title displayed in the document tab bar.
	 * @return The display title.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  On-disk path of the document.
	 * @return The document file path (empty if never saved).
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Whether the document has unsaved changes.
	 * @return True when the document is dirty.
	 */
	[[nodiscard]] auto isDirty() const -> bool override;

	/**
	 * @brief
	 *  Called once when the document is attached to the manager.
	 * @param[in] iEditor Owning editor layer.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Called once when the document is being removed from the manager.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Per-frame update tick.
	 * @param[in] iTimeStep Frame time delta.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Dispatch an input/system event to the document.
	 * @param[in,out] ioEvent The incoming event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Render the document's panels in the current ImGui frame.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Save the document to its current file path.
	 * @return True on success; false on failure or when no path is set.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Save the document to an explicit path.
	 * @param[in] iPath Destination file.
	 * @return True on success; false on failure.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Access to the document's undo/redo stack.
	 * @return The undo manager.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Const access to the document's undo/redo stack.
	 * @return The undo manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	// --- Animation-specific API ---------------------------------------------
	/**
	 * @brief
	 *  Typed undo manager for `AnimationClip` operations.
	 */
	[[nodiscard]] auto animationUndoManager() -> AnimationUndoManager& { return m_undoManager; }

	/**
	 * @brief
	 *  Const access to the typed undo manager for `AnimationClip` operations.
	 * @return The animation undo manager.
	 */
	[[nodiscard]] auto animationUndoManager() const -> const AnimationUndoManager& { return m_undoManager; }

	/**
	 * @brief
	 *  Direct access to the edited clip.
	 */
	[[nodiscard]] auto clip() -> scene::AnimationClip& { return m_clip; }

	/**
	 * @brief
	 *  Const access to the edited clip.
	 * @return The animation clip.
	 */
	[[nodiscard]] auto clip() const -> const scene::AnimationClip& { return m_clip; }

	/**
	 * @brief
	 *  Load a clip from disk. Returns false on I/O or parse error.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  True while the user hasn't clicked the tab's close X.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Reset the open flag (used by `EditorLayer` after a close is handled or cancelled).
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

	/**
	 * @brief
	 *  Resume frame playback (no-op when already playing).
	 */
	void play() { m_playing = true; }

	/**
	 * @brief
	 *  Pause frame playback.
	 */
	void pause() { m_playing = false; }

	/**
	 * @brief
	 *  Stop playback and rewind to `firstFrame`.
	 */
	void stop();

	/**
	 * @brief
	 *  Jump to the next frame in the active range, pausing playback.
	 */
	void stepNext();

	/**
	 * @brief
	 *  Jump to the previous frame in the active range, pausing playback.
	 */
	void stepPrevious();

	/**
	 * @brief
	 *  True when the preview is currently advancing frames.
	 */
	[[nodiscard]] auto isPlaying() const -> bool { return m_playing; }

	/**
	 * @brief
	 *  Currently displayed frame index.
	 */
	[[nodiscard]] auto currentFrame() const -> uint32_t { return m_currentFrame; }

private:
	/**
	 * @brief
	 *  Render the properties (texture, columns, rows...) sub-panel.
	 */
	void renderProperties();

	/**
	 * @brief
	 *  Render the preview sub-panel (animated thumbnail + transport buttons).
	 */
	void renderPreview();

	/**
	 * @brief
	 *  Render the timeline sub-panel (ImSequencer-backed range bar).
	 */
	void renderTimeline();

	/**
	 * @brief
	 *  Refresh the saved-snapshot baseline used by `isDirty()`.
	 */
	void refreshSavedSnapshot();

	/**
	 * @brief
	 *  Advance the playhead by `iSeconds` of simulated time, clamped to the active range.
	 */
	void advancePlayback(float iSeconds);

	/**
	 * @brief
	 *  Compute UV coordinates for the current frame (row-major, V-flipped to match Renderer2D).
	 */
	[[nodiscard]] auto currentFrameUv() const -> std::pair<math::vec2, math::vec2>;

	/// Disk path for the animation clip backing this document.
	std::filesystem::path m_path;
	/// Animation clip currently being edited.
	scene::AnimationClip m_clip;
	/// Per-document undo/redo manager for animation edits.
	AnimationUndoManager m_undoManager;
	/// YAML snapshot taken at the last save — drives the dirty indicator.
	std::string m_savedSnapshot;
	/// Editor layer back-pointer for menus/dialogs that need editor-wide context.
	EditorLayer* mp_editorLayer{nullptr};

	/// Currently displayed frame index (playback state, not serialized).
	uint32_t m_currentFrame{0};
	/// Time accumulated since the last frame advance, in seconds.
	float m_elapsedTime{0.f};
	/// Whether timeline playback is running.
	bool m_playing{true};

	/// First sequencer-row index currently visible (kept across frames for stable scrolling).
	int32_t m_seqFirstVisibleFrame{0};

	/// Tracks the ImGui "Open" state of the document tab.
	bool m_pOpen{true};

	/// Placeholder unused — `Document::undoManager()` requires returning a `SceneUndoManager&`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
