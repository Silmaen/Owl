/**
 * @file Sequencer.h
 * @author Silmaen
 * @date 27/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>
#include <string>
#include <vector>

namespace owl::gui::widgets {

/**
 * @brief One row of a `sequencer()` widget — a labelled frame range.
 */
struct SequencerEntry {
	/// Row label displayed in the legend column.
	std::string label;
	/// Inclusive start frame.
	int32_t startFrame{0};
	/// Inclusive end frame (`endFrame >= startFrame`).
	int32_t endFrame{0};
	/// RGBA fill colour for the bar (each channel in `[0, 1]`). Defaults to a soft blue.
	math::vec4 color{0.4f, 0.6f, 1.0f, 1.0f};
};

/**
 * @brief Options controlling a `sequencer()` instance.
 */
struct SequencerOptions {
	/// Lowest frame number the timeline displays.
	int32_t frameMin{0};
	/// Highest frame number the timeline displays (inclusive).
	int32_t frameMax{100};
	/// When true, the user can drag the bar handles to change `startFrame` / `endFrame`.
	bool allowEditStartEnd{true};
	/// When true, the playhead is shown and is draggable.
	bool showCurrentFrame{true};
};

/**
 * @brief Owl wrapper around the ImSequencer widget from the ImGuizmo bundle.
 *
 * Renders a horizontal multi-track timeline with a legend column on the left and a frame
 * ruler on top. Each `SequencerEntry` becomes a coloured bar with draggable start/end
 * handles (when `allowEditStartEnd` is set). A vertical playhead bound to `ioCurrentFrame`
 * scrubs through the timeline; clicking the ruler moves it.
 *
 * The widget is a thin pimpl over `ImSequencer::Sequencer` — no ImSequencer types are
 * exposed in this header, so callers do not have to depend on the ImGuizmo bundle.
 *
 * @param[in] iLabel Stable id used to scope ImGui state.
 * @param[in,out] ioEntries Tracks rendered top-to-bottom; modified in place when the user
 *                drags a bar's handles.
 * @param[in,out] ioCurrentFrame Playhead position, modified when the user clicks the ruler.
 * @param[in,out] ioFirstVisibleFrame Left-most visible frame (set by the user via the
 *                horizontal scrollbar). Pass `0` for a fresh widget.
 * @param[in] iOpts Display and behaviour options.
 * @param[in] iSize Pixel size of the canvas (width, height). `{0, h}` spans the available
 *            content width with a `h`-pixel-tall canvas.
 * @return `true` when any entry was modified or the playhead moved during this call.
 */
OWL_API auto sequencer(const char* iLabel, std::vector<SequencerEntry>& ioEntries, int32_t& ioCurrentFrame,
					   int32_t& ioFirstVisibleFrame, const SequencerOptions& iOpts = {},
					   const math::vec2& iSize = {0.f, 200.f}) -> bool;

}// namespace owl::gui::widgets
