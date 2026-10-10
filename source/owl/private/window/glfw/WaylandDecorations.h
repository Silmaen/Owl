/**
 * @file WaylandDecorations.h
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::window::glfw {

/**
 * @brief
 *  Check whether the Wayland compositor draws the window decorations itself (`zxdg_decoration_manager_v1`).
 *
 * Connects to `WAYLAND_DISPLAY` through the system `libwayland-client` (loaded at run time, as GLFW does) and lists
 * the compositor globals. When it offers server-side decorations, libdecor would only load GTK for nothing: the
 * window creation then skips it.
 * @return True when the compositor offers `zxdg_decoration_manager_v1`; false without a compositor, without
 * `libwayland-client`, or off Linux.
 */
OWL_API auto compositorDrawsDecorations() -> bool;

}// namespace owl::window::glfw
