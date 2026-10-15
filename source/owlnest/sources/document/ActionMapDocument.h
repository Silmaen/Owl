/**
 * @file ActionMapDocument.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "BindingCapture.h"
#include "Document.h"
#include "UndoManager.h"

#include <input/ActionFrame.h>
#include <input/ActionMap.h>
#include <input/InputFrame.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  Undo manager typed for `input::ActionMap` edits.
 *
 * Each command keeps the whole map before and after the edit: a map is a few dozen small records.
 */
using ActionMapUndoManager = UndoManager<input::ActionMap>;

/**
 * @brief
 *  The `input_actions.yml` of a project, edited in a tab: actions, their kind and their bindings, with its own undo
 *  stack, a "press a key" capture of bindings and a live monitor of the actions.
 *
 * Every edit is a method returning whether it changed the map, so the tab and the tests drive the same code. Saving
 * writes the file atomically and hands the map to the editor, which reloads it in the engine context (a running Play
 * session sees it on its next frame).
 */
class ActionMapDocument final : public Document {
public:
	ActionMapDocument(const ActionMapDocument&) = delete;

	ActionMapDocument(ActionMapDocument&&) = delete;

	auto operator=(const ActionMapDocument&) -> ActionMapDocument& = delete;

	auto operator=(ActionMapDocument&&) -> ActionMapDocument& = delete;

	/**
	 * @brief
	 *  Create an empty map document.
	 */
	ActionMapDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~ActionMapDocument() override;

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Kind of document.
	 * @return `DocumentType::ActionMap`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::ActionMap; }

	/**
	 * @brief
	 *  Title of the tab.
	 * @return The file name without its extension, "Untitled" without a path.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  Path of the file.
	 * @return The path, empty when none was set.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Whether the map differs from the saved one.
	 * @return True when there are unsaved edits.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return m_map != m_savedMap; }

	/**
	 * @brief
	 *  Attach the document to the editor.
	 * @param[in] iEditor The editor.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Detach the document from the editor.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Feed the capture and the live monitor with the devices.
	 * @param[in] iTimeStep Time since the last frame.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Event hook (nothing to do: ImGui consumes its own events).
	 * @param[in,out] ioEvent The event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Draw the tab.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Write the map to its file.
	 * @return True when written; false without a path or when the write failed.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Write the map to another file, which becomes the document's.
	 * @param[in] iPath The file.
	 * @return True when written.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  The map as YAML, for the autosave.
	 * @return The text.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Put back the map of an autosave.
	 * @param[in] iSnapshot The YAML text.
	 * @return True when it parsed.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Placeholder of the `Document` base (the typed manager is `actionMapUndoManager`).
	 * @return The empty scene undo manager.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Placeholder of the `Document` base.
	 * @return The empty scene undo manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Whether an edit can be undone.
	 * @return True when the stack is not empty.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether an undone edit can be redone.
	 * @return True when the redo stack is not empty.
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

	// --- Action map API ------------------------------------------------------
	/**
	 * @brief
	 *  The map being edited.
	 * @return The map.
	 */
	[[nodiscard]] auto map() const -> const input::ActionMap& { return m_map; }

	/**
	 * @brief
	 *  The typed undo stack.
	 * @return The manager.
	 */
	[[nodiscard]] auto actionMapUndoManager() const -> const ActionMapUndoManager& { return m_undo; }

	/**
	 * @brief
	 *  Read a map file into the document.
	 * @param[in] iPath The `input_actions.yml` file.
	 * @return True when it was read.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Start from the built-in actions, for a project without map file; the document is dirty until saved.
	 * @param[in] iPath The file the map will be saved to.
	 */
	void startFromDefaults(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Whether a name can be given to an action.
	 * @param[in] iName The name.
	 * @param[in] iIgnored Action of the name being replaced (a rename keeps its own), or nothing.
	 * @return True for a non-empty name no other action carries.
	 */
	[[nodiscard]] auto isNameAvailable(std::string_view iName, std::optional<std::size_t> iIgnored = std::nullopt) const
			-> bool;

	/**
	 * @brief
	 *  A name no action carries yet.
	 * @param[in] iBase The wanted name; a number is appended when taken.
	 * @return The free name.
	 */
	[[nodiscard]] auto uniqueName(std::string_view iBase) const -> std::string;

	/**
	 * @brief
	 *  Add an action without bindings.
	 * @param[in] iName The name, free.
	 * @param[in] iKind The kind.
	 * @return True when added (false for a taken or empty name).
	 */
	auto addAction(const std::string& iName, input::ActionKind iKind) -> bool;

	/**
	 * @brief
	 *  Remove an action.
	 * @param[in] iIndex Position of the action.
	 * @return True when removed.
	 */
	auto removeAction(std::size_t iIndex) -> bool;

	/**
	 * @brief
	 *  Rename an action, keeping its place.
	 * @param[in] iIndex Position of the action.
	 * @param[in] iName The new name, free.
	 * @return True when renamed (false for a taken or empty name).
	 */
	auto renameAction(std::size_t iIndex, const std::string& iName) -> bool;

	/**
	 * @brief
	 *  Change what an action reports.
	 * @param[in] iIndex Position of the action.
	 * @param[in] iKind The new kind.
	 * @return True when it changed.
	 */
	auto setActionKind(std::size_t iIndex, input::ActionKind iKind) -> bool;

	/**
	 * @brief
	 *  Add a binding at the end of an action.
	 * @param[in] iAction Position of the action.
	 * @param[in] iBinding The binding.
	 * @return True when added.
	 */
	auto addBinding(std::size_t iAction, const input::ActionBinding& iBinding) -> bool;

	/**
	 * @brief
	 *  Remove a binding.
	 * @param[in] iAction Position of the action.
	 * @param[in] iBinding Position of the binding.
	 * @return True when removed.
	 */
	auto removeBinding(std::size_t iAction, std::size_t iBinding) -> bool;

	/**
	 * @brief
	 *  Replace a binding.
	 * @param[in] iAction Position of the action.
	 * @param[in] iBindingIndex Position of the binding.
	 * @param[in] iBinding The new binding.
	 * @param[in] iMerge True for a drag of a slider: the edits of the next second join one undo step.
	 * @return True when it changed.
	 */
	auto setBinding(std::size_t iAction, std::size_t iBindingIndex, const input::ActionBinding& iBinding,
					bool iMerge = false) -> bool;

	/**
	 * @brief
	 *  Define a built-in action in the map, with its default bindings, so it can be edited.
	 * @param[in] iName Name of a built-in action the map does not define.
	 * @return True when added.
	 */
	auto overrideBuiltIn(std::string_view iName) -> bool;

	/**
	 * @brief
	 *  Replace the whole map by the built-in actions.
	 * @return True when the map changed.
	 */
	auto resetToDefaults() -> bool;

	/**
	 * @brief
	 *  Built-in actions the map does not define (the engine completes the map with them).
	 * @return Their names.
	 */
	[[nodiscard]] auto missingBuiltIns() const -> std::vector<std::string>;

	// --- Selection -----------------------------------------------------------
	/**
	 * @brief
	 *  Action shown in the details.
	 * @return Its position, nothing when none.
	 */
	[[nodiscard]] auto selectedAction() const -> std::optional<std::size_t> {
		return m_selected < m_map.size() ? std::optional{m_selected} : std::nullopt;
	}

	/**
	 * @brief
	 *  Select an action.
	 * @param[in] iIndex Position of the action.
	 */
	void selectAction(const std::size_t iIndex) { m_selected = iIndex; }

	// --- Capture -------------------------------------------------------------
	/**
	 * @brief
	 *  Wait for the next input of the user and bind it to an action.
	 * @param[in] iAction Position of the action.
	 * @param[in] iReplaced Binding that takes the input (its scale and component are kept), or nothing to add one.
	 * @param[in] iBaseline Device state now: what is held is ignored until released.
	 */
	void startCapture(std::size_t iAction, std::optional<std::size_t> iReplaced, const input::InputState& iBaseline);

	/**
	 * @brief
	 *  Stop waiting for an input.
	 */
	void cancelCapture();

	/**
	 * @brief
	 *  Whether an input is awaited.
	 * @return True while capturing.
	 */
	[[nodiscard]] auto isCapturing() const -> bool { return m_capture.isArmed(); }

	/**
	 * @brief
	 *  Action and binding the capture fills.
	 * @return The action position, nothing when not capturing.
	 */
	[[nodiscard]] auto captureTarget() const -> std::optional<std::size_t> {
		return isCapturing() ? std::optional{m_captureAction} : std::nullopt;
	}

	/**
	 * @brief
	 *  Binding replaced by the capture.
	 * @return Its position, nothing when the capture adds one.
	 */
	[[nodiscard]] auto captureReplaced() const -> std::optional<std::size_t> { return m_captureReplaced; }

	// --- Live monitor --------------------------------------------------------
	/**
	 * @brief
	 *  Advance the capture and the monitor with a new device state.
	 * @param[in] iState Device state of this frame.
	 * @return True when the capture ended on a binding.
	 */
	auto updateInput(const input::InputState& iState) -> bool;

	/**
	 * @brief
	 *  Reading of every action of the map under the last device state.
	 * @return One state per action, in map order.
	 */
	[[nodiscard]] auto monitor() const -> const std::vector<input::ActionState>& { return m_monitor; }

	// --- Tab -----------------------------------------------------------------
	/**
	 * @brief
	 *  True while the user has not clicked the close button of the tab.
	 * @return True until ImGui flips the close flag.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Set the open flag.
	 * @param[in] iOpen The new value.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/**
	 * @brief
	 *  Make a map the current one and record the step.
	 * @param[in] iNext The map after the edit.
	 * @param[in] iDescription Description of the step.
	 * @param[in] iMerge Whether the step joins the previous one of the same description.
	 * @return True when the map changed.
	 */
	auto commit(input::ActionMap iNext, const std::string& iDescription, bool iMerge = false) -> bool;

	/**
	 * @brief
	 *  Apply a change to one action and commit it.
	 * @tparam Mutator Callable taking an `ActionDefinition&`.
	 * @param[in] iIndex Position of the action.
	 * @param[in] iDescription Description of the step.
	 * @param[in] iMutator The change.
	 * @param[in] iMerge Whether the step joins the previous one of the same description.
	 * @return True when the map changed.
	 */
	template<typename Mutator>
	auto modifyAction(std::size_t iIndex, const std::string& iDescription, Mutator&& iMutator, bool iMerge = false)
			-> bool;

	/**
	 * @brief
	 *  Keep the selection inside the map after an undo, a redo or a removal.
	 */
	void clampSelection();

	/**
	 * @brief
	 *  Recompute the readings of the monitor.
	 */
	void refreshMonitor();

	/**
	 * @brief
	 *  Draw the list of actions and the built-in ones the map leaves out.
	 */
	void renderActionList();

	/**
	 * @brief
	 *  Draw the details of the selected action: name, kind, bindings, live reading.
	 */
	void renderActionDetails();

	/**
	 * @brief
	 *  Draw one binding and its properties.
	 * @param[in] iAction Position of the action.
	 * @param[in] iBindingIndex Position of the binding.
	 */
	void renderBinding(std::size_t iAction, std::size_t iBindingIndex);

	/**
	 * @brief
	 *  Draw the live reading of an action: flags, value, vector.
	 * @param[in] iAction Position of the action.
	 */
	void renderReading(std::size_t iAction);

	/// Disk path of the map.
	std::filesystem::path m_path;
	/// Map being edited.
	input::ActionMap m_map;
	/// Map of the last save, to tell whether the document is dirty.
	input::ActionMap m_savedMap;
	/// Undo stack of the map.
	ActionMapUndoManager m_undo;
	/// Editor back-pointer.
	EditorLayer* mp_editorLayer{nullptr};
	/// Position of the selected action (past the end when none).
	std::size_t m_selected{0};
	/// Capture of the next input.
	BindingCapture m_capture;
	/// Action the capture fills.
	std::size_t m_captureAction{0};
	/// Binding the capture replaces.
	std::optional<std::size_t> m_captureReplaced;
	/// Device frames the monitor reads actions from.
	input::InputFrame m_inputFrame;
	/// Reading of each action.
	std::vector<input::ActionState> m_monitor;
	/// Text field of the name of the selected action.
	std::string m_nameBuffer;
	/// Action whose name the text field holds.
	std::optional<std::size_t> m_nameBufferOf;
	/// Whether Escape was down at the previous device state (it cancels a capture on its press).
	bool m_escapeHeld{false};
	/// Tracks the ImGui "Open" state of the tab.
	bool m_pOpen{true};
	/// Whether the tab was focused last frame.
	bool m_wasFocused{false};
	/// Placeholder required by `Document::undoManager()`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
