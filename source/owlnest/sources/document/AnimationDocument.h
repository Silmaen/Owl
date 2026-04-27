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

/// @brief Undo manager typed for `scene::AnimationClip` edits.
using AnimationUndoManager = UndoManager<scene::AnimationClip>;

/**
 * @brief Document editing a reusable `.owlanim` animation asset.
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

	AnimationDocument();
	~AnimationDocument() override;

	// --- Document interface --------------------------------------------------

	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Animation; }
	[[nodiscard]] auto title() const -> std::string override;
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }
	[[nodiscard]] auto isDirty() const -> bool override;

	void onAttach(EditorLayer* iEditor) override;
	void onDetach() override;
	void onUpdate(const core::Timestep& iTimeStep) override;
	void onEvent(event::Event& ioEvent) override;
	void onImGuiRender() override;

	auto save() -> bool override;
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	// --- Animation-specific API ---------------------------------------------

	/// @brief Typed undo manager for `AnimationClip` operations.
	[[nodiscard]] auto animationUndoManager() -> AnimationUndoManager& { return m_undoManager; }
	[[nodiscard]] auto animationUndoManager() const -> const AnimationUndoManager& { return m_undoManager; }

	/// @brief Direct access to the edited clip.
	[[nodiscard]] auto clip() -> scene::AnimationClip& { return m_clip; }
	[[nodiscard]] auto clip() const -> const scene::AnimationClip& { return m_clip; }

	/// @brief Load a clip from disk. Returns false on I/O or parse error.
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/// @brief True while the user hasn't clicked the tab's close X.
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }
	/// @brief Reset the open flag (used by `EditorLayer` after a close is handled or cancelled).
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

	/// @brief Resume frame playback (no-op when already playing).
	void play() { m_playing = true; }
	/// @brief Pause frame playback.
	void pause() { m_playing = false; }
	/// @brief Stop playback and rewind to `firstFrame`.
	void stop();
	/// @brief Jump to the next frame in the active range, pausing playback.
	void stepNext();
	/// @brief Jump to the previous frame in the active range, pausing playback.
	void stepPrevious();
	/// @brief True when the preview is currently advancing frames.
	[[nodiscard]] auto isPlaying() const -> bool { return m_playing; }
	/// @brief Currently displayed frame index.
	[[nodiscard]] auto currentFrame() const -> uint32_t { return m_currentFrame; }

private:
	/// Render the properties (texture, columns, rows...) sub-panel.
	void renderProperties();
	/// Render the preview sub-panel (animated thumbnail + transport buttons).
	void renderPreview();
	/// Render the timeline sub-panel (ImSequencer-backed range bar).
	void renderTimeline();
	/// Refresh the saved-snapshot baseline used by `isDirty()`.
	void refreshSavedSnapshot();
	/// Advance the playhead by `iSeconds` of simulated time, clamped to the active range.
	void advancePlayback(float iSeconds);
	/// Compute UV coordinates for the current frame (row-major, V-flipped to match Renderer2D).
	[[nodiscard]] auto currentFrameUv() const -> std::pair<math::vec2, math::vec2>;

	std::filesystem::path m_path;
	scene::AnimationClip m_clip;
	AnimationUndoManager m_undoManager;
	std::string m_savedSnapshot;
	EditorLayer* mp_editorLayer{nullptr};

	// Playback state — never serialized.
	uint32_t m_currentFrame{0};
	float m_elapsedTime{0.f};
	bool m_playing{true};

	// Sequencer view state — kept across frames for stable scrolling.
	int32_t m_seqFirstVisibleFrame{0};

	bool m_pOpen{true};

	/// Placeholder unused — `Document::undoManager()` requires returning a `SceneUndoManager&`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
