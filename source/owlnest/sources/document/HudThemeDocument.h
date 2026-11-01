/**
 * @file HudThemeDocument.h
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "commands/HudThemeCommands.h"

#include <scene/HudTheme.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace owl::nest {

/// Undo stack of a HUD theme.
using HudThemeUndoManager = UndoManager<scene::HudTheme>;

/**
 * @brief
 *  Editor tab of a `.owlhudtheme`: the list of its styles and the properties of the selected one.
 *
 * Every edit is a `theme.*` command; a dragged value is previewed and committed once the field is released. The theme
 * as edited replaces the one of the engine context's `HudThemeLibrary`, so the HUDs and menus following it (an open
 * HUD editor, the viewport) show the edits live.
 */
class HudThemeDocument final : public Document, public commands::HudThemeEditor {
public:
	HudThemeDocument(const HudThemeDocument&) = delete;

	HudThemeDocument(HudThemeDocument&&) = delete;

	auto operator=(const HudThemeDocument&) -> HudThemeDocument& = delete;

	auto operator=(HudThemeDocument&&) -> HudThemeDocument& = delete;

	/**
	 * @brief
	 *  Constructor.
	 */
	HudThemeDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~HudThemeDocument() override;

	/**
	 * @brief
	 *  Type of the document.
	 * @return `DocumentType::HudTheme`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::HudTheme; }

	/**
	 * @brief
	 *  Name of the tab.
	 * @return The file stem.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  File of the theme.
	 * @return The path.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Whether there are unsaved edits.
	 * @return True when the history moved since the last save.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return m_undo.isDirty(); }

	/**
	 * @brief
	 *  Attach to the editor.
	 * @param[in] iEditor The editor layer.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Detach from the editor.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Commit a previewed edit once no field is active.
	 * @param[in] iTimeStep Frame time.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Events (none handled).
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
	 *  Write the file.
	 * @return True on success.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Write the theme to another file.
	 * @param[in] iPath The file.
	 * @return True on success.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Text kept by the crash recovery.
	 * @return The theme as YAML.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Restore a recovery snapshot.
	 * @param[in] iSnapshot Text of `recoverySnapshot`.
	 * @return True when it was read.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Placeholder scene history (the theme has its own).
	 * @return An empty history.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Placeholder scene history (the theme has its own).
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

	/**
	 * @brief
	 *  The theme as it is.
	 * @return The theme.
	 */
	[[nodiscard]] auto theme() const -> const scene::HudTheme& override { return m_theme; }

	/**
	 * @brief
	 *  Replace the theme as one undoable step.
	 * @param[in] iTheme The theme after the edit.
	 * @param[in] iDescription Name of the step.
	 * @return False when nothing changes.
	 */
	auto setTheme(const scene::HudTheme& iTheme, const std::string& iDescription) -> bool override;

	/**
	 * @brief
	 *  Read a theme file.
	 * @param[in] iPath The file.
	 * @return True when it was read.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Index of the selected style.
	 * @return The index, or none.
	 */
	[[nodiscard]] auto getSelection() const -> std::optional<size_t> { return m_selection; }

	/**
	 * @brief
	 *  Select a style.
	 * @param[in] iIndex Its index (none clears the selection).
	 */
	void setSelection(std::optional<size_t> iIndex);

	/**
	 * @brief
	 *  Asset path the scenes give for this theme (relative to the project folder).
	 * @return The path.
	 */
	[[nodiscard]] auto getAssetPath() const -> std::string;

	/**
	 * @brief
	 *  Whether the tab is open.
	 * @return The flag.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Open or close the tab.
	 * @param[in] iOpen The flag.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/**
	 * @brief
	 *  Run a `theme.*` command on this file.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Its arguments (`path` is added).
	 * @return True when it ran.
	 */
	auto runCommand(std::string_view iName, commands::CommandArgs iArgs) -> bool;

	/**
	 * @brief
	 *  Turn the previewed style into `theme.rename_style` / `theme.set_style` once no field is active.
	 * @param[in] iForce Commit even while a field is active.
	 */
	void commitPreview(bool iForce = false);

	/**
	 * @brief
	 *  Give the theme as shown to the engine context, for the HUDs following it.
	 */
	void publish() const;

	/**
	 * @brief
	 *  Draw the list of styles and its buttons.
	 */
	void renderStyleList();

	/**
	 * @brief
	 *  Draw the properties of the selected style.
	 */
	void renderStyleProperties();

	/// Disk path of the `.owlhudtheme`.
	std::filesystem::path m_path;
	/// Theme being edited.
	scene::HudTheme m_theme;
	/// Its history.
	HudThemeUndoManager m_undo;
	/// Placeholder scene history.
	SceneUndoManager m_emptySceneUndo;
	/// Selected style.
	std::optional<size_t> m_selection;
	/// Style edited in the properties and not committed yet.
	std::optional<scene::HudStyle> m_preview;
	/// Editor layer (non-owning).
	EditorLayer* mp_editorLayer = nullptr;
	/// Whether the tab is open.
	bool m_pOpen = true;
	/// Whether the window had the focus on the last frame.
	bool m_wasFocused = false;
};

}// namespace owl::nest
