/**
 * @file NavigationToolState.h
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl::nest::panel {

/**
 * @brief
 *  State of the isometric Navigation tool and its link to the graph overlay switch.
 *
 * Selecting the tool turns the overlay on and remembers whether it was; leaving the tool puts the overlay back. The
 * tool needs the overlay: switching the overlay off (Show menu, eye button) leaves the tool, so the tool button and
 * the switch never disagree.
 */
struct NavigationToolState {
	/// Whether the tool is on.
	bool active = false;
	/// State of the overlay switch when the tool was selected.
	bool overlayBefore = false;

	/**
	 * @brief
	 *  Select the tool: the overlay is turned on.
	 * @param[in,out] ioOverlay The overlay switch.
	 */
	void select(bool& ioOverlay) {
		if (active)
			return;
		active = true;
		overlayBefore = ioOverlay;
		ioOverlay = true;
	}

	/**
	 * @brief
	 *  Leave the tool: the overlay goes back to what it was when the tool was selected.
	 * @param[in,out] ioOverlay The overlay switch.
	 */
	void leave(bool& ioOverlay) {
		if (!active)
			return;
		active = false;
		ioOverlay = overlayBefore;
	}

	/**
	 * @brief
	 *  Toggle the tool, as its button does.
	 * @param[in,out] ioOverlay The overlay switch.
	 */
	void toggle(bool& ioOverlay) {
		if (active)
			leave(ioOverlay);
		else
			select(ioOverlay);
	}

	/**
	 * @brief
	 *  Keep the tool consistent with the rest of the editor, once per frame.
	 *
	 * The tool exists in the Edit state of an isometric view only: elsewhere it is left and the overlay restored.
	 * An overlay switched off while the tool is on leaves the tool and stays off.
	 * @param[in,out] ioOverlay The overlay switch.
	 * @param[in] iAvailable True in the Edit state of an isometric view.
	 */
	void sync(bool& ioOverlay, const bool iAvailable) {
		if (!active)
			return;
		if (!iAvailable)
			leave(ioOverlay);
		else if (!ioOverlay)
			active = false;
	}
};

}// namespace owl::nest::panel
