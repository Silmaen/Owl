/**
 * @file PluginUiRenderer.h
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../session/PluginHost.h"
#include "../session/PluginUi.h"

#include <script/ScriptConsole.h>

#include <cstdint>
#include <string_view>

namespace owl::nest::panel {

/**
 * @brief
 *  Draws the widgets of an Owl Nest plugin with the panel style of the editor: section headers, the property grid
 *  (label on the left, value on the right) and the X / Y / Z fields of the inspector.
 *
 * A dragged or typed value is kept here while its field is active, whatever the script gives on the next frames, and
 * reported as changed once, when the field is released: an edit is one command, hence one undo step.
 */
class PluginUiRenderer final : public session::PluginUi {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	PluginUiRenderer() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~PluginUiRenderer() override;

	PluginUiRenderer(const PluginUiRenderer&) = default;

	PluginUiRenderer(PluginUiRenderer&&) = default;

	auto operator=(const PluginUiRenderer&) -> PluginUiRenderer& = default;

	auto operator=(PluginUiRenderer&&) -> PluginUiRenderer& = default;

	/**
	 * @brief
	 *  Start the widgets of one handler, in the current window.
	 * @param[in] iId Identifier of what is drawn (a panel, a component, a document): its widgets get ids of their own.
	 * @param[in] iNested True inside a section already open (a component of the inspector): the sections the plugin
	 *  asks for are sub-headers.
	 */
	void begin(std::string_view iId, bool iNested);

	/**
	 * @brief
	 *  End the widgets started by `begin`: close the grid and the section left open.
	 */
	void end();

	/**
	 * @brief
	 *  Draw one widget.
	 * @param[in] iWidget The widget.
	 * @return Its value and whether the user acted on it.
	 */
	auto draw(const session::PluginWidget& iWidget) -> session::PluginWidgetResult override;

private:
	/**
	 * @brief
	 *  Open the property grid when it is not.
	 * @return True when rows can be drawn.
	 */
	auto openGrid() -> bool;

	/**
	 * @brief
	 *  Close the property grid when it is open.
	 */
	void closeGrid();

	/**
	 * @brief
	 *  The value a field shows: the one being edited when the field is the active one, else the one given.
	 * @param[in] iField ImGui identifier of the field.
	 * @param[in] iGiven The value the script gave.
	 * @return The value shown.
	 */
	[[nodiscard]] auto shownValue(uint32_t iField, const script::HostValue& iGiven) const -> script::HostValue;

	/**
	 * @brief
	 *  Account for the field just drawn: keep its value while it is edited, report it once when it is released.
	 * @param[in] iField ImGui identifier of the field.
	 * @param[in] iEdited Whether the field changed its value on this frame.
	 * @param[in] iValue The value the field shows.
	 * @return The value and whether the edit ended.
	 */
	auto commit(uint32_t iField, bool iEdited, script::HostValue iValue) -> session::PluginWidgetResult;

	/**
	 * @brief
	 *  Draw the row of a value: its label, its field, the empty reset cell.
	 * @param[in] iWidget The widget.
	 * @return Its value and whether the user acted on it.
	 */
	auto drawRow(const session::PluginWidget& iWidget) -> session::PluginWidgetResult;

	/// Field being edited (0 for none).
	uint32_t m_activeField{0};
	/// Its value.
	script::HostValue m_activeValue;
	/// Widgets drawn since `begin`, the ImGui identifier of the next one.
	int m_count{0};
	/// Whether the property grid is open.
	bool m_gridOpen{false};
	/// Whether a section opened by the plugin has to be closed.
	bool m_sectionOpen{false};
	/// Whether the sections are sub-headers.
	bool m_nested{false};
};

/**
 * @brief
 *  Draw the window of every plugin panel shown; closing a window hides its panel.
 * @param[in,out] ioHost The plugins.
 * @param[in,out] ioUi What draws their widgets.
 */
void renderPluginPanels(session::PluginHost& ioHost, PluginUiRenderer& ioUi);

}// namespace owl::nest::panel
