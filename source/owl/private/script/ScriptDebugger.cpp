/**
 * @file ScriptDebugger.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "script/ScriptDebugger.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <ranges>
#include <set>
#include <utility>

namespace owl::script {

namespace {

struct DebuggerState {
	std::mutex mutex;
	std::atomic<bool> enabled{false};
	std::atomic<size_t> breakpointCount{0};
	std::map<std::string, std::set<int>, std::less<>> breakpoints;
	std::optional<BreakpointHit> hit;
};

auto state() -> DebuggerState& {
	static DebuggerState s_state;
	return s_state;
}

auto normalized(std::string_view iPath) -> std::string {
	if (!iPath.empty() && (iPath.front() == '@' || iPath.front() == '='))
		iPath.remove_prefix(1);
	std::string out{iPath};
	std::ranges::replace(out, '\\', '/');
	return out;
}

auto endsOnFolder(const std::string_view iLong, const std::string_view iShort) -> bool {
	return iLong.size() > iShort.size() && iLong.ends_with(iShort) && iLong[iLong.size() - iShort.size() - 1] == '/';
}

void recount(DebuggerState& ioState) {
	size_t count = 0;
	for (const auto& lines: ioState.breakpoints | std::views::values) count += lines.size();
	ioState.breakpointCount.store(count, std::memory_order_relaxed);
}

}// namespace

void ScriptDebugger::setEnabled(const bool iEnabled) { state().enabled.store(iEnabled, std::memory_order_relaxed); }

auto ScriptDebugger::isEnabled() -> bool { return state().enabled.load(std::memory_order_relaxed); }

void ScriptDebugger::setBreakpoint(const std::string& iScript, const int iLine, const bool iActive) {
	auto& debugger = state();
	const std::scoped_lock<std::mutex> lock{debugger.mutex};
	auto& lines = debugger.breakpoints[normalized(iScript)];
	if (iActive)
		lines.insert(iLine);
	else
		lines.erase(iLine);
	std::erase_if(debugger.breakpoints, [](const auto& iEntry) -> bool { return iEntry.second.empty(); });
	recount(debugger);
}

void ScriptDebugger::setBreakpoints(const std::string& iScript, const std::vector<int>& iLines) {
	auto& debugger = state();
	const std::scoped_lock<std::mutex> lock{debugger.mutex};
	const auto key = normalized(iScript);
	if (iLines.empty())
		debugger.breakpoints.erase(key);
	else
		debugger.breakpoints[key] = {iLines.begin(), iLines.end()};
	recount(debugger);
}

auto ScriptDebugger::getBreakpoints(const std::string& iScript) -> std::vector<int> {
	auto& debugger = state();
	const std::scoped_lock<std::mutex> lock{debugger.mutex};
	const auto found = debugger.breakpoints.find(normalized(iScript));
	if (found == debugger.breakpoints.end())
		return {};
	return {found->second.begin(), found->second.end()};
}

auto ScriptDebugger::isBreakpoint(const std::string_view iScript, const int iLine) -> bool {
	if (!hasBreakpoints())
		return false;
	auto& debugger = state();
	const auto script = normalized(iScript);
	const std::scoped_lock<std::mutex> lock{debugger.mutex};
	return std::ranges::any_of(debugger.breakpoints, [&](const auto& iEntry) -> bool {
		return iEntry.second.contains(iLine) && isSameScript(iEntry.first, script);
	});
}

auto ScriptDebugger::hasBreakpoints() -> bool {
	const auto& debugger = state();
	return debugger.enabled.load(std::memory_order_relaxed) &&
		   debugger.breakpointCount.load(std::memory_order_relaxed) != 0;
}

void ScriptDebugger::clear() {
	auto& debugger = state();
	const std::scoped_lock<std::mutex> lock{debugger.mutex};
	debugger.breakpoints.clear();
	debugger.hit.reset();
	recount(debugger);
}

void ScriptDebugger::reportHit(BreakpointHit iHit) {
	auto& debugger = state();
	const std::scoped_lock<std::mutex> lock{debugger.mutex};
	if (!debugger.hit.has_value())
		debugger.hit = std::move(iHit);
}

auto ScriptDebugger::takeHit() -> std::optional<BreakpointHit> {
	auto& debugger = state();
	const std::scoped_lock<std::mutex> lock{debugger.mutex};
	return std::exchange(debugger.hit, std::nullopt);
}

auto ScriptDebugger::isSameScript(const std::string_view iFirst, const std::string_view iSecond) -> bool {
	const auto first = normalized(iFirst);
	const auto second = normalized(iSecond);
	if (first.empty() || second.empty())
		return false;
	return first == second || endsOnFolder(first, second) || endsOnFolder(second, first);
}

}// namespace owl::script
