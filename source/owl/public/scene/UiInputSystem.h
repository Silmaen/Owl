/**
 * @file UiInputSystem.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Entity.h"
#include "Scene.h"
#include "input/ActionFrame.h"

namespace owl::scene {
/**
 * @brief
 *  Manages UI input: hit-testing, hover/focus tracking, click routing.
 *
 * Called once per frame during runtime. Updates button states, handles clicks,
 * and invokes Lua callbacks. UI elements consume mouse events before the scene. The hover, press and focus state
 * lives in each scene (`Scene::getUiInputState()`).
 *
 * The keyboard and the gamepad drive a focus through the built-in `ui_*` actions of the scene (`Scene::getActions()`):
 * `ui_up` / `ui_down` / `ui_left` / `ui_right` move it to the nearest focusable widget (a `UiButton` not disabled or a
 * `UiSlider`, unless its `UiFocus` says otherwise) in that direction, from the layout shared with the HUD pass, or to
 * the neighbour a `UiFocus` names; held, they repeat. `ui_accept` clicks the focused button, `ui_left` / `ui_right`
 * adjust the focused slider, `ui_cancel` closes the top window when it is closable. The first press only shows the
 * focus. While a `UiWindow` is open the focus stays in the top one. The mouse takes the focus where it moves or
 * clicks and hides its highlight. A `UiList` is focusable too: `ui_up` / `ui_down` step its selection before the focus
 * leaves it; a widget scrolled out of a `UiScrollView` stays reachable and the view scrolls to show the focused one; the
 * `ui_scroll` action scrolls the view or list holding the focus.
 */
class OWL_API UiInputSystem final {
public:
	UiInputSystem() = delete;

	/**
	 * @brief
	 *  Process UI input for the current frame.
	 * @param[in] iScene The scene whose UI receives the mouse (nullptr: nothing happens).
	 * @param[in] iViewportSize The viewport dimensions.
	 * @param[in] iMousePos Mouse position in Y-down viewport pixels from the top left (the HUD layout space).
	 * @param[in] iMousePressed Whether the left mouse button is currently pressed.
	 * @param[in] iDeltaTime Time since the previous update, in seconds (drives the tooltip delay).
	 * @param[in] iWheel Mouse wheel offset of the frame (x horizontal, y vertical, positive up and right).
	 *
	 * The wheel scrolls the `UiScrollView` or `UiList` under the mouse (the nearest one above the widget under it that can
	 * move); a click on a bar grabs its thumb, a click on a list row selects it. A `UiWindow` under the mouse blocks every widget beneath it and counts as consuming the mouse. A click on a window
	 * raises it (last child of its parent, hence drawn last), the title bar drags it (kept inside the screen) and the
	 * close button hides it.
	 */
	static void update(Scene* iScene, const math::vec2ui& iViewportSize, const math::vec2& iMousePos,
					   bool iMousePressed, float iDeltaTime = 0.f, const math::vec2& iWheel = {0.f, 0.f});

	/**
	 * @brief
	 *  Move the keyboard and gamepad focus with the `ui_*` actions of one frame (`update` does it with the scene's own).
	 * @param[in,out] ioScene The scene whose UI is navigated.
	 * @param[in] iViewportSize The viewport dimensions.
	 * @param[in] iActions The actions of the frame: edges of `ui_accept` / `ui_cancel` / `ui_up` / `ui_down` / `ui_left`
	 *  / `ui_right`, and the held directions for the repeat.
	 * @param[in] iDeltaTime Time since the previous call, in seconds (drives the repeat).
	 */
	static void navigate(Scene& ioScene, const math::vec2ui& iViewportSize, const input::ActionFrame& iActions,
						 float iDeltaTime = 0.f);

	/**
	 * @brief
	 *  Give a widget the focus and show it, as a script does.
	 * @param[in,out] ioScene The active scene (the widget belongs to it or to its HUD asset).
	 * @param[in] iWidget The widget.
	 * @return False when the widget cannot take the focus: not a `UiButton` out of the disabled state or a `UiSlider`,
	 *  or a `UiFocus` that is not focusable.
	 */
	static auto setFocus(Scene& ioScene, const Entity& iWidget) -> bool;

	/**
	 * @brief
	 *  Get the widget holding the focus.
	 * @param[in,out] ioScene The active scene.
	 * @return The widget, or an invalid entity when none.
	 */
	[[nodiscard]] static auto getFocus(Scene& ioScene) -> Entity;

	/**
	 * @brief
	 *  Drop the focus: no widget holds it and nothing is highlighted until the next navigation.
	 * @param[in,out] ioScene The active scene.
	 */
	static void clearFocus(Scene& ioScene);

	/**
	 * @brief
	 *  Draw a window over its siblings: it becomes the last child of its parent.
	 * @param[in,out] ioScene The scene (or HUD asset) the window belongs to.
	 * @param[in] iWindow The window entity.
	 * @return True when the order changed.
	 */
	static auto raiseWindow(Scene& ioScene, entt::entity iWindow) -> bool;

	/**
	 * @brief
	 *  Check if the UI of a scene is currently consuming the mouse (hover over interactive element).
	 * @param[in] iScene The scene.
	 * @return True if a UI element is hovered or focused.
	 */
	[[nodiscard]] static auto isUIConsuming(const Scene& iScene) -> bool;

	/**
	 * @brief
	 *  Check whether the UI holds the world interactions of the frame back: a `UiWindow` of the scene or of its HUD
	 *  asset is shown (a pause menu, an inventory), or was when the frame's `update` started.
	 *
	 * While it is true the engine fires no world interaction: no `RaycastDoor` / `RaycastPushWall` built-in
	 * activation, no `Interaction` trigger callback. The click that closes the window (Resume) belongs to that frame,
	 * so it does not open the door in front of the player either.
	 * @param[in,out] ioScene The active scene.
	 * @return True when the world interactions are held back.
	 */
	[[nodiscard]] static auto isWorldInteractionBlocked(Scene& ioScene) -> bool;

	/**
	 * @brief
	 *  Reset the UI input state of a scene.
	 * @param[in,out] ioScene The scene.
	 */
	static void reset(Scene& ioScene);
};

}// namespace owl::scene
