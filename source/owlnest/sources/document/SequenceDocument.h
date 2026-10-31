/**
 * @file SequenceDocument.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "commands/SequenceCommands.h"

#include <data/sequence/Sequence.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest {

/// Undo history of a sequence document.
using SequenceUndoManager = UndoManager<data::sequence::Sequence>;

/**
 * @brief
 *  The track and keys picked in a sequence document.
 */
struct SequenceSelection {
	/// Whether the track is a camera track (else an event track).
	bool camera = true;
	/// Index of the track.
	size_t track = 0;
	/// Indices of the picked keys of the track, sorted.
	std::vector<size_t> keys;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other selection.
	 * @return True when both are the same.
	 */
	auto operator==(const SequenceSelection& iOther) const -> bool = default;
};

/**
 * @brief
 *  Tab editing a `.owlseq` cutscene: its tracks on a timeline, the keys in the properties panel and a top view of the
 *  camera path. Every edit runs a `sequence.*` command on the document's own history.
 */
class SequenceDocument final : public Document, public commands::SequenceEditor {
public:
	SequenceDocument(const SequenceDocument&) = delete;

	SequenceDocument(SequenceDocument&&) = delete;

	auto operator=(const SequenceDocument&) -> SequenceDocument& = delete;

	auto operator=(SequenceDocument&&) -> SequenceDocument& = delete;

	/**
	 * @brief
	 *  Build an empty document.
	 */
	SequenceDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~SequenceDocument() override;

	// --- Document interface --------------------------------------------------

	/**
	 * @brief
	 *  Kind of the document.
	 * @return `DocumentType::Sequence`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Sequence; }

	/**
	 * @brief
	 *  Title of the tab.
	 * @return The file name without extension.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  File of the document.
	 * @return The path.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Whether the sequence has unsaved edits.
	 * @return True when dirty.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return m_undo.isDirty(); }

	/**
	 * @brief
	 *  Attach the document to the editor.
	 * @param[in] iEditor The editor layer.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Detach the document from the editor.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Advance the preview playback.
	 * @param[in] iTimeStep Duration of the frame.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Events: none handled.
	 * @param[in,out] ioEvent The event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Draw the tab: toolbar, timeline and camera path.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  The document draws its own hierarchy and properties panels.
	 * @return True.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Draw the track list.
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Draw the selected key or track.
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title of the hierarchy panel.
	 * @return "Tracks".
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Tracks"; }

	/**
	 * @brief
	 *  Title of the properties panel.
	 * @return "Key".
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Key"; }

	/**
	 * @brief
	 *  Write the sequence to its file.
	 * @return True on success.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Write the sequence to another file, which becomes the document's.
	 * @param[in] iPath The file.
	 * @return True on success.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Read a sequence file.
	 * @param[in] iPath The file.
	 * @return False when it is not a readable sequence.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  The sequence as YAML, for the autosave.
	 * @return The text.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Restore an autosave.
	 * @param[in] iSnapshot The text of `recoverySnapshot`.
	 * @return False when it cannot be read.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Scene history: unused, a sequence has its own.
	 * @return An empty history.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Scene history: unused, a sequence has its own.
	 * @return An empty history.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Whether an edit can be undone.
	 * @return True when the history has one.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether an edit can be redone.
	 * @return True when the history has one.
	 */
	[[nodiscard]] auto canRedo() const -> bool override { return m_undo.canRedo(); }

	/**
	 * @brief
	 *  Undo the last edit.
	 */
	void performUndo() override;

	/**
	 * @brief
	 *  Redo the last undone edit.
	 */
	void performRedo() override;

	// --- Sequence editor -----------------------------------------------------

	/**
	 * @brief
	 *  The sequence edited.
	 * @return The sequence.
	 */
	[[nodiscard]] auto sequence() const -> const data::sequence::Sequence& override { return m_sequence; }

	/**
	 * @brief
	 *  Replace the sequence as one undo entry.
	 * @param[in] iNext The sequence after the edit.
	 * @param[in] iDescription Description of the entry.
	 * @param[in] iMerge Whether it merges with the previous entry of the same description.
	 * @return False when nothing changes.
	 */
	auto apply(data::sequence::Sequence iNext, const std::string& iDescription, bool iMerge) -> bool override;

	// --- Selection and playhead ----------------------------------------------

	/**
	 * @brief
	 *  The picked track and keys.
	 * @return The selection.
	 */
	[[nodiscard]] auto getSelection() const -> const SequenceSelection& { return m_selection; }

	/**
	 * @brief
	 *  Pick a track and some of its keys; out-of-range indices are dropped.
	 * @param[in] iSelection The selection.
	 */
	void setSelection(SequenceSelection iSelection);

	/**
	 * @brief
	 *  Time of the playhead.
	 * @return The time in seconds.
	 */
	[[nodiscard]] auto getTime() const -> float { return m_time; }

	/**
	 * @brief
	 *  Move the playhead, clamped to the sequence.
	 * @param[in] iTime Time in seconds.
	 */
	void setTime(float iTime);

	/**
	 * @brief
	 *  Whether the preview plays.
	 * @return True while it plays.
	 */
	[[nodiscard]] auto isPlaying() const -> bool { return m_playing; }

	/**
	 * @brief
	 *  Play or pause the preview; playing at the end starts again from 0.
	 * @param[in] iPlaying Whether it plays.
	 */
	void setPlaying(bool iPlaying);

	/**
	 * @brief
	 *  Add a key at the playhead on the selected track (`sequence.add_key`), and select it.
	 * @return False when the document cannot run the command.
	 */
	auto addKeyAtPlayhead() -> bool;

	/**
	 * @brief
	 *  Remove the selected keys (`sequence.remove_keys`).
	 * @return False when nothing is removed.
	 */
	auto deleteSelectedKeys() -> bool;

	/**
	 * @brief
	 *  Add a track (`sequence.add_track`) and select it.
	 * @param[in] iCamera Whether it is a camera track (else an event track).
	 * @return False when the document cannot run the command.
	 */
	auto addTrack(bool iCamera) -> bool;

	// --- Tab -----------------------------------------------------------------

	/**
	 * @brief
	 *  Whether the tab is open.
	 * @return False once its close button was pressed.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Open or close the tab.
	 * @param[in] iOpen Whether it is open.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/**
	 * @brief
	 *  Run a `sequence.*` command on this document.
	 * @param[in] iName The command.
	 * @param[in] iArgs Its arguments, `path` added.
	 * @return The result, nothing when it failed.
	 */
	auto sendCommand(std::string_view iName, commands::CommandArgs iArgs) -> std::optional<commands::CommandResult>;

	/**
	 * @brief
	 *  Arguments naming the selected track (`kind`, `track`).
	 * @return The arguments.
	 */
	[[nodiscard]] auto trackArgs() const -> commands::CommandArgs;

	/**
	 * @brief
	 *  The selected keys as `0,2`.
	 * @return The list.
	 */
	[[nodiscard]] auto selectedKeysText() const -> std::string;

	/**
	 * @brief
	 *  Time of a key of a track.
	 * @param[in] iCamera Camera track (else event track).
	 * @param[in] iTrack Index of the track.
	 * @param[in] iKey Index of the key.
	 * @return The time.
	 */
	[[nodiscard]] auto keyTime(bool iCamera, size_t iTrack, size_t iKey) const -> float;

	/**
	 * @brief
	 *  Draw the playback toolbar.
	 */
	void renderToolbar();

	/**
	 * @brief
	 *  Draw the tracks on the timeline, with the key gestures.
	 */
	void renderTimeline();

	/**
	 * @brief
	 *  The track of a row of the timeline.
	 * @param[in] iY Screen y.
	 * @return Whether it is a camera track and its index, nothing below the rows or over the ruler.
	 */
	[[nodiscard]] auto rowAt(float iY) const -> std::optional<std::pair<bool, size_t>>;

	/**
	 * @brief
	 *  The key of a track under a screen x.
	 * @param[in] iCamera Camera track (else event track).
	 * @param[in] iTrack Index of the track.
	 * @param[in] iX Screen x.
	 * @return Its index, nothing when no key is there.
	 */
	[[nodiscard]] auto keyAt(bool iCamera, size_t iTrack, float iX) const -> std::optional<size_t>;

	/**
	 * @brief
	 *  Select a clicked key (Ctrl toggles it) and start dragging the selection.
	 * @param[in] iCamera Camera track (else event track).
	 * @param[in] iTrack Index of the track.
	 * @param[in] iKey Index of the key.
	 */
	void pickKey(bool iCamera, size_t iTrack, size_t iKey);

	/**
	 * @brief
	 *  Clicks on the timeline: pick, scrub, add a key, open the track menu.
	 */
	void handleTimelineClicks();

	/**
	 * @brief
	 *  Follow the dragged keys; the release runs `sequence.move_keys`.
	 */
	void updateKeyDrag();

	/**
	 * @brief
	 *  Draw the context menu of a track.
	 */
	void renderTrackMenu();

	/**
	 * @brief
	 *  Draw one row of the timeline.
	 * @param[in] iCamera Camera track (else event track).
	 * @param[in] iTrack Index of the track.
	 * @param[in] iTop Top of the row on screen.
	 */
	void renderTimelineRow(bool iCamera, size_t iTrack, float iTop);

	/**
	 * @brief
	 *  Draw the top view of the camera path, the camera at the playhead.
	 */
	void renderPathView();

	/**
	 * @brief
	 *  Draw the fields of a camera key.
	 * @param[in] iKey The key.
	 */
	void renderCameraKey(const data::sequence::CameraKey& iKey);

	/**
	 * @brief
	 *  Draw the fields of an event key.
	 * @param[in] iKey The key.
	 */
	void renderEventKey(const data::sequence::EventKey& iKey);

	/**
	 * @brief
	 *  Run `sequence.set_key` on the selected key, merged while the field stays active.
	 * @param[in] iArgs The fields changed.
	 */
	void setSelectedKey(commands::CommandArgs iArgs);

	/**
	 * @brief
	 *  Screen x of a time on the timeline.
	 * @param[in] iTime Time in seconds.
	 * @return The x.
	 */
	[[nodiscard]] auto timeToX(float iTime) const -> float;

	/**
	 * @brief
	 *  Time of a screen x on the timeline.
	 * @param[in] iX The x.
	 * @return Time in seconds, not clamped.
	 */
	[[nodiscard]] auto xToTime(float iX) const -> float;

	/// File of the document.
	std::filesystem::path m_path;
	/// The sequence edited.
	data::sequence::Sequence m_sequence;
	/// Its history.
	SequenceUndoManager m_undo;
	/// Editor running the commands.
	EditorLayer* mp_editorLayer{nullptr};
	/// Picked track and keys.
	SequenceSelection m_selection;
	/// Playhead, in seconds.
	float m_time{0.f};
	/// Whether the preview plays.
	bool m_playing{false};
	/// Whether the playhead is being dragged.
	bool m_scrubbing{false};
	/// Whether the selected keys are being dragged.
	bool m_dragging{false};
	/// Time under the mouse when the key drag started.
	float m_dragStart{0.f};
	/// Shift of the dragged keys, drawn until the release runs `sequence.move_keys`.
	float m_dragOffset{0.f};
	/// Whether the keys snap to a tenth of a second.
	bool m_snap{true};
	/// Pixels per second of the timeline.
	float m_zoom{80.f};
	/// Screen x of time 0 on the timeline (set each frame).
	float m_originX{0.f};
	/// Screen x where the lanes start, after the track names (set each frame).
	float m_lanesX{0.f};
	/// Screen y of the first row, under the ruler (set each frame).
	float m_rowsTop{0.f};
	/// A key or track field is being typed into: the next edit merges.
	bool m_typing{false};
	/// The duration field is being dragged: the next edit merges.
	bool m_durationTyping{false};
	/// Whether the tab is open.
	bool m_pOpen{true};
	/// Whether the tab had the focus last frame.
	bool m_wasFocused{false};
	/// Empty scene history of the `Document` interface.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
