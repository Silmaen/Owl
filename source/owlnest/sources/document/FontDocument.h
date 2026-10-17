/**
 * @file FontDocument.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"

#include "viewer/FontCharset.h"

#include <data/fonts/Font.h>
#include <data/fonts/FontSettings.h>
#include <math/vectors.h>
#include <renderer/gpu/Framebuffer.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>

namespace owl::nest {

/**
 * @brief
 *  Undo manager typed for the import settings of a font.
 */
using FontUndoManager = UndoManager<data::fonts::FontSettings>;

/**
 * @brief
 *  A named Unicode block offered by the "Add range" menu of the font document.
 */
struct FontRangePreset {
	/// Block name shown in the menu and next to a matching range.
	std::string_view name;
	/// The block.
	data::fonts::CodePointRange range;
};

/**
 * @brief
 *  Document viewing a font asset (`.ttf` / `.otf`) and editing its import settings: the code-point ranges loaded on
 *  top of the default Latin charset, saved in the `.owlfont` sidecar next to the font.
 *
 * The "Samples" tab draws an editable text at several sizes with the engine's own text path (`Renderer2D`, outline
 * and box included), so it shows what a game draws; the "Atlas" tab shows the MSDF atlas with a glyph readout under
 * the cursor. The font is the one of the font library when loaded, else the document loads its own.
 *
 * Saving rebuilds the glyph atlas of the loaded font in place, so every text using it shows the new glyphs.
 */
class FontDocument final : public Document {
public:
	FontDocument(const FontDocument&) = delete;

	FontDocument(FontDocument&&) = delete;

	auto operator=(const FontDocument&) -> FontDocument& = delete;

	auto operator=(FontDocument&&) -> FontDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	FontDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~FontDocument() override;

	/**
	 * @brief
	 *  Document type.
	 * @return `DocumentType::Font`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Font; }

	/**
	 * @brief
	 *  Tab title: the font file stem.
	 * @return The title.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  Path of the font file (not of its sidecar).
	 * @return The path.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Check whether the settings differ from the saved ones.
	 * @return True when unsaved.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return m_settings != m_savedSettings; }

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
	 *  Per-frame update: draw the sample lines into their framebuffer while the tab is visible.
	 * @param[in] iTimeStep The frame time.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Event hook (ImGui handles the input).
	 * @param[in,out] ioEvent The event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Draw the document window: the samples and the glyph atlas of the font.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Write the sidecar (removed when no extra range is left) and rebuild the loaded font.
	 * @return True on success.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Save the settings as the sidecar of another font file.
	 * @param[in] iPath Path of the font file.
	 * @return True on success.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Unsaved settings for the autosave.
	 * @return The settings YAML.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Restore autosaved settings.
	 * @param[in] iSnapshot The settings YAML.
	 * @return True on success.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Unused scene undo manager, required by the interface.
	 * @return The placeholder manager.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Unused scene undo manager, required by the interface.
	 * @return The placeholder manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  The document draws its own Hierarchy and Properties panels.
	 * @return True.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Check whether an edit can be undone.
	 * @return True when the undo stack is not empty.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undoManager.canUndo(); }

	/**
	 * @brief
	 *  Check whether an edit can be redone.
	 * @return True when the redo stack is not empty.
	 */
	[[nodiscard]] auto canRedo() const -> bool override { return m_undoManager.canRedo(); }

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
	 *  Hierarchy panel: font file, glyph count, atlas.
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Properties panel: default charset and the editable extra ranges.
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title of the Hierarchy panel.
	 * @return The title.
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Font"; }

	/**
	 * @brief
	 *  Title of the Properties panel.
	 * @return The title.
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Font Charset"; }

	/**
	 * @brief
	 *  Open a font: its `.ttf` / `.otf`, or its `.owlfont` sidecar.
	 * @param[in] iPath The file.
	 * @return True on success.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Settings being edited.
	 * @return The settings.
	 */
	[[nodiscard]] auto settings() const -> const data::fonts::FontSettings& { return m_settings; }

	/**
	 * @brief
	 *  Undo manager of the document.
	 * @return The manager.
	 */
	[[nodiscard]] auto fontUndoManager() const -> const FontUndoManager& { return m_undoManager; }

	/**
	 * @brief
	 *  Add an extra range (undoable).
	 * @param[in] iRange The range.
	 */
	void addRange(const data::fonts::CodePointRange& iRange);

	/**
	 * @brief
	 *  Change the bounds of an extra range (undoable; consecutive edits of one range merge).
	 * @param[in] iIndex Index of the range.
	 * @param[in] iRange The new bounds, reordered when swapped.
	 */
	void setRange(size_t iIndex, const data::fonts::CodePointRange& iRange);

	/**
	 * @brief
	 *  Remove extra ranges in one undoable step.
	 * @param[in] iIndices Indices of the ranges to remove; out-of-range ones are ignored.
	 */
	void removeRanges(const std::set<size_t>& iIndices);

	/**
	 * @brief
	 *  Indices of the selected ranges (click, Ctrl+click to add).
	 * @return The selection.
	 */
	[[nodiscard]] auto selection() const -> const std::set<size_t>& { return m_selection; }

	/**
	 * @brief
	 *  The blocks offered by the "Add range" menu.
	 * @return The presets.
	 */
	[[nodiscard]] static auto rangePresets() -> std::span<const FontRangePreset>;

	/**
	 * @brief
	 *  Font sizes of the sample lines.
	 * @return The sizes in pixels.
	 */
	[[nodiscard]] static auto sampleSizes() -> std::span<const float>;

	/**
	 * @brief
	 *  Charset of the font told by Unicode block, with the settings being edited.
	 * @return The summary.
	 */
	[[nodiscard]] auto charsetSummary() const -> viewer::CharsetSummary { return viewer::summarizeCharset(m_settings); }

	/**
	 * @brief
	 *  Text drawn in the sample lines.
	 * @return The text.
	 */
	[[nodiscard]] auto sampleText() const -> const std::string& { return m_sampleText; }

	/**
	 * @brief
	 *  Change the text drawn in the sample lines (a view option, not an asset edit).
	 * @param[in] iText The text.
	 */
	void setSampleText(const std::string& iText) { m_sampleText = iText; }

	/**
	 * @brief
	 *  Check whether the sample lines are drawn with an outline.
	 * @return True when outlined.
	 */
	[[nodiscard]] auto isOutlineShown() const -> bool { return m_showOutline; }

	/**
	 * @brief
	 *  Draw the sample lines with an outline or not.
	 * @param[in] iShown The state.
	 */
	void setOutlineShown(const bool iShown) { m_showOutline = iShown; }

	/**
	 * @brief
	 *  Check whether the sample lines are drawn on a box.
	 * @return True when boxed.
	 */
	[[nodiscard]] auto isBoxShown() const -> bool { return m_showBox; }

	/**
	 * @brief
	 *  Draw the sample lines on a box or not.
	 * @param[in] iShown The state.
	 */
	void setBoxShown(const bool iShown) { m_showBox = iShown; }

	/**
	 * @brief
	 *  Check whether the document tab is open.
	 * @return True while open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Set the open state of the tab.
	 * @param[in] iOpen The state.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/**
	 * @brief
	 *  Apply new settings and push the matching undo command.
	 * @param[in] iDescription Undo menu label.
	 * @param[in] iSettings The new settings.
	 * @param[in] iMergeable True when consecutive edits with this label coalesce.
	 */
	void applyEdit(const std::string& iDescription, const data::fonts::FontSettings& iSettings, bool iMergeable);

	/**
	 * @brief
	 *  The font as loaded by the font library, or null.
	 * @return The font.
	 */
	[[nodiscard]] auto loadedFont() const -> shared<data::fonts::Font>;

	/**
	 * @brief
	 *  The font to show: the library one, else one loaded by the document (null when the file is not a font).
	 * @return The font.
	 */
	[[nodiscard]] auto viewedFont() -> shared<data::fonts::Font>;

	/**
	 * @brief
	 *  Draw the sample lines into the sample framebuffer (size from the last frame).
	 * @param[in] iFont The font.
	 */
	void renderSamples(const shared<data::fonts::Font>& iFont);

	/**
	 * @brief
	 *  Draw the "Samples" tab: options, then the sample picture.
	 * @param[in] iFont The font.
	 */
	void drawSamplesTab(const shared<data::fonts::Font>& iFont);

	/**
	 * @brief
	 *  Draw the "Atlas" tab: the MSDF atlas and the glyph under the cursor.
	 * @param[in] iFont The font.
	 */
	void drawAtlasTab(const shared<data::fonts::Font>& iFont);

	/// Path of the font file.
	std::filesystem::path m_path;
	/// Settings being edited.
	data::fonts::FontSettings m_settings;
	/// Settings on disk.
	data::fonts::FontSettings m_savedSettings;
	/// Undo stack of the settings.
	FontUndoManager m_undoManager;
	/// Selected range indices.
	std::set<size_t> m_selection;
	/// Font loaded by the document when the library does not hold it.
	shared<data::fonts::Font> m_ownFont;
	/// True when the document could not load the font itself.
	bool m_ownFontFailed{false};
	/// Picture of the sample lines.
	shared<renderer::gpu::Framebuffer> m_sampleFramebuffer;
	/// Size the sample picture should have (from the last drawn frame).
	math::vec2ui m_sampleSize{0, 0};
	/// Text of the sample lines.
	std::string m_sampleText{"Sphinx of black quartz, judge my vow 0123456789 \u00E9\u00E0\u00FC\u00C7 !?"};
	/// Draw an outline around the sample glyphs.
	bool m_showOutline{false};
	/// Draw a box behind the sample lines.
	bool m_showBox{false};
	/// True when the samples tab was drawn last frame, so it needs its picture.
	bool m_samplesVisible{false};
	/// Editor back-pointer.
	EditorLayer* mp_editorLayer{nullptr};
	/// ImGui open state of the tab.
	bool m_pOpen{true};
	/// Whether the window was focused last frame.
	bool m_wasFocused{false};
	/// Placeholder: `Document::undoManager()` returns a `SceneUndoManager&`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
