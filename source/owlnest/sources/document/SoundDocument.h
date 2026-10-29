/**
 * @file SoundDocument.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "commands/DocumentCommands.h"

#include <sound/SoundCue.h>
#include <sound/SoundData.h>
#include <sound/SoundHandle.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest {

/// Undo stack of a sound cue.
using SoundUndoManager = UndoManager<sound::SoundCue>;

/**
 * @brief
 *  Marker of a take drawn on its waveform and dragged with the mouse.
 */
enum struct SoundMarker : uint8_t {
	TrimStart,///< First played second.
	TrimEnd,///< Last played second.
	LoopStart,///< Start of the looped part.
	LoopEnd,///< End of the looped part.
};

/**
 * @brief
 *  Sound cue document (`.owlsound`): its takes with their trim and loop points on the waveform, the volume and pitch
 *  with their random change, and a preview of one take or of the cue.
 */
class SoundDocument final : public Document, public commands::SoundCueEditor {
public:
	SoundDocument(const SoundDocument&) = delete;

	SoundDocument(SoundDocument&&) = delete;

	auto operator=(const SoundDocument&) -> SoundDocument& = delete;

	auto operator=(SoundDocument&&) -> SoundDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	SoundDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~SoundDocument() override;

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Document type.
	 * @return `DocumentType::Sound`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Sound; }

	/**
	 * @brief
	 *  Title of the tab.
	 * @return The file name, or "Untitled".
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  File of the document.
	 * @return The path of the cue on disk.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Whether the cue changed since the last save.
	 * @return True when unsaved changes exist.
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
	 *  Detach the document from the editor; stops the preview.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Per-frame update: forgets the finished preview.
	 * @param[in] iTimeStep Frame time.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Event handler (no event used).
	 * @param[in,out] ioEvent The event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Draw the document window.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Save to the document's file.
	 * @return True on success.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Save to a file, which becomes the document's file.
	 * @param[in] iPath The file.
	 * @return True on success.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Unsaved state of the cue, for the autosave.
	 * @return The cue as YAML.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Restore an autosave.
	 * @param[in] iSnapshot A `recoverySnapshot()` text.
	 * @return True when it was a cue.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Scene undo stack, unused by this document.
	 * @return An empty stack.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Scene undo stack, unused by this document.
	 * @return An empty stack.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Whether an edit can be undone.
	 * @return True when the stack holds one.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether an edit can be redone.
	 * @return True when the stack holds one.
	 */
	[[nodiscard]] auto canRedo() const -> bool override { return m_undo.canRedo(); }

	/**
	 * @brief
	 *  The Takes and Sound cue panels replace the Scene Hierarchy and the Properties while this cue is active.
	 * @return Always true.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Draw the list of the takes, in place of the Scene Hierarchy.
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Draw the properties of the cue, in place of the Properties.
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title of the panel holding the takes.
	 * @return "Takes".
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Takes"; }

	/**
	 * @brief
	 *  Title of the panel holding the cue properties.
	 * @return "Sound Cue".
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Sound Cue"; }

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

	// --- Cue API -------------------------------------------------------------
	/**
	 * @brief
	 *  The cue being edited.
	 * @return The cue.
	 */
	[[nodiscard]] auto cue() const -> const sound::SoundCue& override { return m_cue; }

	/**
	 * @brief
	 *  Undo stack of the cue.
	 * @return The stack.
	 */
	[[nodiscard]] auto soundUndoManager() const -> const SoundUndoManager& { return m_undo; }

	/**
	 * @brief
	 *  Load a cue file.
	 * @param[in] iPath The file.
	 * @return False when it is not a sound cue.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Start a cue not yet on disk, of one take playing an audio file; its file is the audio file with the
	 *  `.owlsound` extension, written by the first save.
	 * @param[in] iAudioFile The audio file on disk.
	 */
	void startFromAudio(const std::filesystem::path& iAudioFile);

	/**
	 * @brief
	 *  Set the volume, the pitch, their random change and the loop flag.
	 * @param[in] iCue A cue whose takes are ignored.
	 * @param[in] iMerge Whether the edit merges with the previous one (a drag).
	 * @return True when something changed.
	 */
	auto setProperties(const sound::SoundCue& iCue, bool iMerge = false) -> bool override;

	/**
	 * @brief
	 *  Append a take playing a whole audio file.
	 * @param[in] iFile Audio file, relative to the asset folders; a sound cue is refused.
	 * @return True when it was added.
	 */
	auto addVariant(const std::string& iFile) -> bool override;

	/**
	 * @brief
	 *  Replace a take.
	 * @param[in] iIndex Index of the take.
	 * @param[in] iVariant Its new values; the trim and loop points are clamped into order.
	 * @param[in] iMerge Whether the edit merges with the previous one (a drag).
	 * @return True when something changed.
	 */
	auto setVariant(size_t iIndex, const sound::SoundVariant& iVariant, bool iMerge = false) -> bool override;

	/**
	 * @brief
	 *  Remove takes.
	 * @param[in] iIndices Indices of the takes.
	 * @return True when one was removed.
	 */
	auto removeVariants(std::span<const size_t> iIndices) -> bool override;

	/**
	 * @brief
	 *  Copy takes after the last of them.
	 * @param[in] iIndices Indices of the takes.
	 * @return True when one was copied.
	 */
	auto duplicateVariants(std::span<const size_t> iIndices) -> bool override;

	/**
	 * @brief
	 *  Move takes one row up or down, as a block.
	 * @param[in] iIndices Indices of the takes.
	 * @param[in] iDown True to move them down.
	 * @return True when they moved.
	 */
	auto moveVariants(std::span<const size_t> iIndices, bool iDown) -> bool override;

	// --- Selection -----------------------------------------------------------
	/**
	 * @brief
	 *  Selected takes, in increasing order.
	 * @return Their indices.
	 */
	[[nodiscard]] auto getSelection() const -> const std::vector<size_t>& { return m_selection; }

	/**
	 * @brief
	 *  Replace the selection.
	 * @param[in] iIndices Indices of the takes; out of range ones are dropped.
	 */
	void setSelection(std::vector<size_t> iIndices);

	// --- Preview -------------------------------------------------------------
	/**
	 * @brief
	 *  Play the cue as a game would: a random take with a random volume and pitch.
	 */
	void playCue();

	/**
	 * @brief
	 *  Play one take with the base volume and pitch.
	 * @param[in] iIndex Index of the take.
	 */
	void playVariant(size_t iIndex);

	/**
	 * @brief
	 *  Stop the preview.
	 */
	void stop();

	/**
	 * @brief
	 *  Whether the preview plays.
	 * @return True while a sound of the document plays.
	 */
	[[nodiscard]] auto isPlaying() const -> bool;

	// --- Tab -----------------------------------------------------------------
	/**
	 * @brief
	 *  Whether the tab is open.
	 * @return False once the user closed it.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Set the open state of the tab.
	 * @param[in] iOpen The state.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/// Waveform and duration of an audio file.
	struct WaveformView {
		/// Peak of each column, in [0, 1].
		std::vector<float> peaks;
		/// Length of the file in seconds, 0 when it cannot be read.
		float duration{0.f};
	};

	/**
	 * @brief
	 *  Apply a change of the cue as one undo step.
	 * @param[in] iNext The cue after the change.
	 * @param[in] iDescription Text of the undo entry.
	 * @param[in] iMerge Whether it merges with the previous edit of the same text.
	 * @return True when the cue changed.
	 */
	auto commit(sound::SoundCue iNext, const std::string& iDescription, bool iMerge) -> bool;

	/**
	 * @brief
	 *  Run an edit gesture as a registry command on this cue (journaled, reachable from the console and MCP).
	 * @param[in] iName The `sound.*` command.
	 * @param[in] iArgs Its arguments, without `path`.
	 */
	void runCommand(std::string_view iName, commands::CommandArgs iArgs);

	/**
	 * @brief
	 *  Run a command on this cue as it is, a pending preview left as it is.
	 * @param[in] iName The `sound.*` command.
	 * @param[in] iArgs Its arguments, without `path`.
	 */
	void sendCommand(std::string_view iName, commands::CommandArgs iArgs) const;

	/**
	 * @brief
	 *  Show an edit in progress (a drag, typing) without recording it: the commands run once it ends.
	 * @param[in] iNext The cue as edited.
	 */
	void preview(sound::SoundCue iNext);

	/**
	 * @brief
	 *  Turn a finished preview into `sound.set` / `sound.set_take` commands, once no widget is active.
	 * @param[in] iForce Commit even while a widget is active (another edit comes).
	 */
	void commitPreview(bool iForce = false);

	/**
	 * @brief
	 *  Drop the preview, back to the cue before it.
	 */
	void cancelPreview();

	/**
	 * @brief
	 *  Play a take of the cue.
	 * @param[in] iVariant The take.
	 * @param[in] iVolume Volume multiplier.
	 * @param[in] iPitch Pitch multiplier.
	 */
	void play(const sound::SoundVariant& iVariant, float iVolume, float iPitch);

	/**
	 * @brief
	 *  Waveform of a take's file, read once.
	 * @param[in] iFile The audio file, relative to the asset folders.
	 * @return The view.
	 */
	auto getWaveform(const std::string& iFile) -> const WaveformView&;

	/**
	 * @brief
	 *  Draw the buttons acting on the selected takes.
	 */
	void renderVariantToolbar();

	/**
	 * @brief
	 *  Handle a click on a take of the list: select it (Ctrl and Shift for many), play it on a double-click.
	 * @param[in] iIndex Index of the take.
	 */
	void clickVariant(size_t iIndex);

	/**
	 * @brief
	 *  Draw the selected take: its waveform with the markers and its values.
	 */
	void renderVariantEditor();

	/**
	 * @brief
	 *  Draw a waveform with the trim and loop markers and handle their drags.
	 * @param[in] iIndex Index of the take.
	 * @param[in] iView Its waveform.
	 */
	void renderWaveform(size_t iIndex, const WaveformView& iView);

	/// Disk path of the cue.
	std::filesystem::path m_path;
	/// Cue being edited.
	sound::SoundCue m_cue;
	/// Undo stack of the cue.
	SoundUndoManager m_undo;
	/// Editor back-pointer.
	EditorLayer* mp_editorLayer{nullptr};
	/// Selected takes, in increasing order.
	std::vector<size_t> m_selection;
	/// Waveforms by audio file.
	std::map<std::string, WaveformView> m_waveforms;
	/// Sound data of the preview, kept while it plays.
	shared<sound::SoundData> m_previewData;
	/// Sound data of the takes already played, by `getVariantKey`.
	std::map<std::string, shared<sound::SoundData>> m_previewCache;
	/// Handle of the preview.
	sound::SoundHandle m_previewHandle{sound::invalidSoundHandle};
	/// Marker being dragged on the waveform.
	std::optional<SoundMarker> m_draggedMarker;
	/// The cue before the edit being previewed, while one is.
	std::optional<sound::SoundCue> m_previewBefore;
	/// Tracks the ImGui "Open" state of the tab.
	bool m_pOpen{true};
	/// Whether the tab was focused last frame.
	bool m_wasFocused{false};
	/// Placeholder required by `Document::undoManager()`.
	SceneUndoManager m_emptySceneUndo;
};

/**
 * @brief
 *  Put the trim and loop points of a take into order: start before end, loop inside the played part.
 * @param[in] iVariant The take.
 * @param[in] iDuration Length of its file in seconds, 0 when unknown.
 * @return The take with ordered points.
 */
[[nodiscard]] auto orderMarkers(sound::SoundVariant iVariant, float iDuration) -> sound::SoundVariant;

}// namespace owl::nest
