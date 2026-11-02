/**
 * @file ViewNavigation.h
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <renderer/CameraEditor.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace owl::nest::panel {

/**
 * @brief
 *  What the left drag does in the viewport when it does not start on the gizmo.
 *
 * Every mode keeps the other gestures: right drag looks around and flies with W A S D, middle drag pans, Ctrl and Alt
 * drags orbit, pan and dolly, the wheel zooms.
 */
enum struct ViewNavigationMode : uint8_t {
	Select,///< The left button selects and drives the gizmo, the view never moves with it.
	Orbit,///< The left drag orbits around the reference point.
	Fly,///< The left drag looks around from the eye; W A S D, Q and E fly the camera while it is held.
	Pan,///< The left drag pans, as a hand grabbing the scene.
};

/// Every navigation mode, in the order of the viewport buttons.
constexpr std::array g_viewNavigationModes{ViewNavigationMode::Select, ViewNavigationMode::Orbit,
										   ViewNavigationMode::Fly, ViewNavigationMode::Pan};

/**
 * @brief
 *  Name of a navigation mode, as stored in the editor settings and shown on its button.
 * @param[in] iMode The mode.
 * @return Its name.
 */
[[nodiscard]] auto getName(ViewNavigationMode iMode) -> std::string_view;

/**
 * @brief
 *  Navigation mode of a name, ignoring the case.
 * @param[in] iName The name.
 * @return The mode, or nothing for an unknown name.
 */
[[nodiscard]] auto parseViewNavigationMode(std::string_view iName) -> std::optional<ViewNavigationMode>;

/**
 * @brief
 *  One sentence telling what the mouse and the keys do in a mode, for the tooltip of its button.
 * @param[in] iMode The mode.
 * @return The help text.
 */
[[nodiscard]] auto getHelp(ViewNavigationMode iMode) -> const char*;

/**
 * @brief
 *  Identifier of the editor action that selects a mode (`view.navigateOrbit`, ...).
 * @param[in] iMode The mode.
 * @return The action identifier.
 */
[[nodiscard]] auto getActionId(ViewNavigationMode iMode) -> const char*;

/**
 * @brief
 *  Left drag of the editor camera for a mode.
 * @param[in] iMode The mode.
 * @return What the left drag does to the camera.
 */
[[nodiscard]] auto getLeftDrag(ViewNavigationMode iMode) -> renderer::CameraEditor::LeftDrag;

/**
 * @brief
 *  State of the left button of the viewport under a navigation mode.
 *
 * In the Select mode a press picks at once, as it always did. In the other modes a press off the gizmo moves the
 * view while it drags; it picks on release when the cursor barely moved, so a click still selects.
 */
struct ViewNavigation {
	/// Distance in pixels under which a press and release is a click, not a drag.
	static constexpr float clickTravel = 4.f;

	/// The mode chosen with the viewport buttons.
	ViewNavigationMode mode = ViewNavigationMode::Select;
	/// Whether the drag of the left button held now moves the view.
	bool leftDragNavigates = false;
	/// Whether the release of the left button held now picks.
	bool pickOnRelease = false;

	/**
	 * @brief
	 *  The left button goes down.
	 * @param[in] iFree Whether the press may pick: over the viewport, off the gizmo, with no tool owning the clicks.
	 * @return True when the press picks now (the Select mode).
	 */
	auto pressLeft(bool iFree) -> bool;

	/**
	 * @brief
	 *  The left button goes up.
	 * @param[in] iTravel Longest distance the cursor went from the press, in pixels.
	 * @return True when the release picks: a click without drag in a navigation mode.
	 */
	auto releaseLeft(float iTravel) -> bool;

	/**
	 * @brief
	 *  Left drag the editor camera does now.
	 * @return The left drag of the mode while the held press navigates, else none.
	 */
	[[nodiscard]] auto getActiveLeftDrag() const -> renderer::CameraEditor::LeftDrag;
};

}// namespace owl::nest::panel
