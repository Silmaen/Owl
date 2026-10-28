/**
 * @file ScriptDebugger.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::script {

/**
 * @brief
 *  A value read from a running script: a global, a local at a breakpoint, a visual-script wire.
 */
struct WatchValue {
	/// Name of the variable, or key of the wire.
	std::string name;
	/// Lua type name (`number`, `string`, `boolean`, `table`, `nil`, …).
	std::string type;
	/// Value as text: a number, a quoted string, `{x=1, y=2}` for a short table, `table (12 entries)` beyond.
	std::string value;
};

/**
 * @brief
 *  A breakpoint reached by a script.
 */
struct BreakpointHit {
	/// Script that reached it, as its chunk is named (path of the `.lua` or `.owlvs`).
	std::string script;
	/// Line reached, from 1 (a line of the compiled Lua for a visual script).
	int line{0};
	/// Entity running the script, as the error messages name it.
	std::string context;
	/// Locals of the function at that line.
	std::vector<WatchValue> locals;
};

/**
 * @brief
 *  Breakpoints and value probes of the running scripts, set by Owl Nest during Play.
 *
 * A breakpoint does not stop the script: the script runs on to the end of its call, the hit is kept with the
 * locals at that line and the host pauses the game when it takes it (`takeHit`). Until it is taken, later hits are
 * dropped. Disabled, the debugger arms nothing and a script runs as in a game. Enabled, visual scripts also compile
 * with value probes: every wire keeps its last value, read by `ScriptInstance::getProbeValues`.
 *
 * A script is named by its path; two paths name the same script when one ends with the other on a folder boundary
 * (`scripts/a.lua` and `/project/scripts/a.lua`). Thread-safe.
 */
class OWL_API ScriptDebugger final {
public:
	ScriptDebugger() = delete;

	/**
	 * @brief
	 *  Arm or disarm the breakpoints and the probes, for the scripts loaded from then on.
	 * @param[in] iEnabled True while the editor plays a scene.
	 */
	static void setEnabled(bool iEnabled);

	/**
	 * @brief
	 *  Whether the breakpoints and the probes are armed.
	 * @return True when enabled.
	 */
	[[nodiscard]] static auto isEnabled() -> bool;

	/**
	 * @brief
	 *  Set or remove a breakpoint.
	 * @param[in] iScript Path of the script.
	 * @param[in] iLine Line, from 1.
	 * @param[in] iActive True to set it, false to remove it.
	 */
	static void setBreakpoint(const std::string& iScript, int iLine, bool iActive);

	/**
	 * @brief
	 *  Replace every breakpoint of a script.
	 * @param[in] iScript Path of the script.
	 * @param[in] iLines Lines, from 1; empty removes them all.
	 */
	static void setBreakpoints(const std::string& iScript, const std::vector<int>& iLines);

	/**
	 * @brief
	 *  Breakpoints of a script.
	 * @param[in] iScript Path of the script.
	 * @return Its lines, sorted.
	 */
	[[nodiscard]] static auto getBreakpoints(const std::string& iScript) -> std::vector<int>;

	/**
	 * @brief
	 *  Whether a line of a script holds a breakpoint, checked by the line hook of the scripts.
	 * @param[in] iScript Chunk name, with or without its `@` prefix.
	 * @param[in] iLine Line, from 1.
	 * @return True when a breakpoint is set there and the debugger is enabled.
	 */
	[[nodiscard]] static auto isBreakpoint(std::string_view iScript, int iLine) -> bool;

	/**
	 * @brief
	 *  Whether any breakpoint is set and the debugger is enabled: the scripts arm their line hook only then.
	 * @return True when a script may stop.
	 */
	[[nodiscard]] static auto hasBreakpoints() -> bool;

	/**
	 * @brief
	 *  Remove every breakpoint and the hit not taken yet.
	 */
	static void clear();

	/**
	 * @brief
	 *  Keep a hit, unless one is waiting already.
	 * @param[in] iHit The breakpoint reached.
	 */
	static void reportHit(BreakpointHit iHit);

	/**
	 * @brief
	 *  Take the hit waiting, so that the next one is kept.
	 * @return The hit, or nothing.
	 */
	[[nodiscard]] static auto takeHit() -> std::optional<BreakpointHit>;

	/**
	 * @brief
	 *  Whether two paths name the same script.
	 * @param[in] iFirst A path or chunk name.
	 * @param[in] iSecond Another one.
	 * @return True when equal, or when one ends with the other on a folder boundary.
	 */
	[[nodiscard]] static auto isSameScript(std::string_view iFirst, std::string_view iSecond) -> bool;
};

}// namespace owl::script
