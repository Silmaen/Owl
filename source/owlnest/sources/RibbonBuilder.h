/**
 * @file RibbonBuilder.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "document/Document.h"

#include <gui/widgets/Ribbon.h>

#include <optional>

namespace owl::nest {

class EditorLayer;

/**
 * @brief
 *  Builds the editor ribbon: the File and Edit tabs plus the contextual tab of the active document.
 *
 * The buttons only call editor actions: the builder holds no editor state besides the ribbon itself.
 */
class RibbonBuilder final {
public:
	RibbonBuilder(const RibbonBuilder&) = delete;

	RibbonBuilder(RibbonBuilder&&) = delete;

	auto operator=(const RibbonBuilder&) -> RibbonBuilder& = delete;

	auto operator=(RibbonBuilder&&) -> RibbonBuilder& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in,out] ioEditor The editor whose actions the buttons call; it outlives the builder.
	 */
	explicit RibbonBuilder(EditorLayer& ioEditor) : mp_editor{&ioEditor} {}

	~RibbonBuilder() = default;

	/**
	 * @brief
	 *  Populate the ribbon with the File, Edit and contextual tabs.
	 */
	void build();

	/**
	 * @brief
	 *  Rebuild the ribbon when the active document type changed since the last build.
	 */
	void refresh();

	/**
	 * @brief
	 *  Remove every tab.
	 */
	void clear() { m_ribbon.clear(); }

	/**
	 * @brief
	 *  Draw the ribbon.
	 */
	void onRender() { m_ribbon.onRender(); }

private:
	/**
	 * @brief
	 *  Contextual tab for scene documents (Playback, Gizmo, Scene file ops, Package).
	 */
	void buildSceneTab();

	/**
	 * @brief
	 *  Contextual tab for node graph documents.
	 */
	void buildNodeGraphTab();

	/**
	 * @brief
	 *  Contextual tab for animation-clip documents (Playback, Frame, File).
	 */
	void buildAnimationTab();

	/**
	 * @brief
	 *  Contextual tab for code / text documents (Save, Save As, Close, language…).
	 */
	void buildCodeTab();

	/**
	 * @brief
	 *  Contextual tab for tilemap documents (Save / Save As / Close + undo helpers).
	 */
	void buildTilemapTab();

	/**
	 * @brief
	 *  Contextual tab for tileset documents (Save / Save As / Close + grid helpers).
	 */
	void buildTilesetTab();

	/// The editor whose actions the buttons call.
	EditorLayer* mp_editor;
	/// Top-of-window action ribbon (File / Edit / contextual tabs).
	gui::widgets::Ribbon m_ribbon;
	/// Document type of the last contextual tab built; a change of active type rebuilds the ribbon.
	std::optional<DocumentType> m_lastDocType;
};

}// namespace owl::nest
