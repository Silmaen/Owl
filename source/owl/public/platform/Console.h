/**
 * @file Console.h
 * @author Silmaen
 * @date 19/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include <core/Core.h>

namespace owl::platform {

/**
 * @brief
 *  Give a windowed program the console of the shell that started it, for its command-line output.
 *
 * Owl Nest and the runner are GUI programs on Windows (no console window of their own): started from cmd or PowerShell,
 * `stdout` and `stderr` are attached to the parent's console; an output already redirected to a pipe or a file (CTest,
 * scripts, `> log.txt`) is left as it is. A double-click start has no parent console and changes nothing. No-op on
 * other platforms, where the terminal is always inherited.
 * @return True when the standard output now writes to the parent's console (always false on other platforms).
 */
auto OWL_API attachParentConsole() -> bool;

}// namespace owl::platform
