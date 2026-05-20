/**
 * @file Tracker.cpp
 * @author Silmaen
 * @date 17/08/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"
#include <cstdio>
#include <stack>

#include "debug/Tracker.h"

#include "core/utils/StringUtils.h"

#ifdef OWL_STACKTRACE
#include <cpptrace/cpptrace.hpp>
#endif

// NOLINTBEGIN(misc-no-recursion)

#ifdef OWL_TRACKER_ACTIVE
#define OWL_DEALLOC_EXCEPT noexcept
#endif
namespace {
thread_local bool g_AntiLoop = false;
std::mutex g_TrackerMutex;

class AntiLoopScope {
public:
	AntiLoopScope() : m_previous{g_AntiLoop} { g_AntiLoop = true; }

	~AntiLoopScope() { g_AntiLoop = m_previous; }

	AntiLoopScope(const AntiLoopScope&) = delete;

	AntiLoopScope(AntiLoopScope&&) = delete;

	auto operator=(const AntiLoopScope&) -> AntiLoopScope& = delete;

	auto operator=(AntiLoopScope&&) -> AntiLoopScope& = delete;

private:
	bool m_previous;
};

class TrackerState {
public:
	~TrackerState() = default;

	TrackerState(const TrackerState&) = delete;

	TrackerState(TrackerState&&) = delete;

	auto operator=(const TrackerState&) -> TrackerState& = delete;

	auto operator=(TrackerState&&) -> TrackerState& = delete;

	static auto get() -> TrackerState& {
		static TrackerState instance;
		return instance;
	}

	void pushTrack(const bool iState) { m_doTrack.push(iState); }

	void popTrack() {
		if (m_doTrack.size() > 1)
			m_doTrack.pop();
	}

	[[nodiscard]] auto canTrack() const -> bool {
		if (m_doTrack.empty())
			return true;
		return m_doTrack.top();
	}

private:
	// states of tracking
	std::stack<bool> m_doTrack;

	TrackerState() { m_doTrack.push(true); }
};

}// namespace
#if defined(OWL_TRACKER_ACTIVE) && (!defined(__cpp_sized_deallocation) || __cpp_sized_deallocation == 0)

void operator delete(void* iMemory, size_t iSize) OWL_DEALLOC_EXCEPT;
#endif

// NOLINTBEGIN(*-no-malloc,cppcoreguidelines-owning-memory)
#ifdef OWL_TRACKER_ACTIVE
auto operator new(const size_t iSize) -> void* {
	void* mem = malloc(iSize);
	owl::debug::TrackerAPI::allocate(mem, iSize);
	return mem;
}

void operator delete(void* iMemory, size_t iSize) OWL_DEALLOC_EXCEPT {
	owl::debug::TrackerAPI::deallocate(iMemory, iSize);
	free(iMemory);
}

void operator delete(void* iMemory) OWL_DEALLOC_EXCEPT {
	owl::debug::TrackerAPI::deallocate(iMemory);
	free(iMemory);
}
#endif
// NOLINTEND(*-no-malloc,cppcoreguidelines-owning-memory)

namespace owl::debug {

namespace {
class StateManager {
public:
	static void allocate() {
		s_globalAllocationState = mkShared<AllocationState>();
		s_currentAllocationState = mkShared<AllocationState>();
		s_lastAllocationState = mkShared<AllocationState>();
	}

	static void pushMemory(void* iLocation, const size_t iSize) {
		const AntiLoopScope antiLoop;
		const std::lock_guard<std::mutex> lock{g_TrackerMutex};
		if (!s_globalAllocationState)
			allocate();
		s_currentAllocationState->pushMemory(iLocation, iSize);
		s_globalAllocationState->pushMemory(iLocation, iSize);
	}

	static void freeMemory(void* iLocation, const size_t iSize) {
		const AntiLoopScope antiLoop;
		const std::lock_guard<std::mutex> lock{g_TrackerMutex};
		if (!s_currentAllocationState)
			return;
		s_currentAllocationState->freeMemory(iLocation, iSize);
		s_globalAllocationState->freeMemory(iLocation, iSize);
	}

	static void swapCurrent() {
		const AntiLoopScope antiLoop;
		const std::lock_guard<std::mutex> lock{g_TrackerMutex};
		if (!s_lastAllocationState)
			allocate();
		s_lastAllocationState->resetState();
		std::swap(s_currentAllocationState, s_lastAllocationState);
	}

	[[nodiscard]] static auto getLastAllocationState() -> const AllocationState& {
		const AntiLoopScope antiLoop;
		const std::lock_guard<std::mutex> lock{g_TrackerMutex};
		if (!s_lastAllocationState)
			allocate();
		return *s_lastAllocationState;
	}

	[[nodiscard]] static auto getGlobalAllocationState() -> const AllocationState& {
		const AntiLoopScope antiLoop;
		const std::lock_guard<std::mutex> lock{g_TrackerMutex};
		if (!s_globalAllocationState)
			allocate();
		return *s_globalAllocationState;
	}

private:
	// Global Memory allocation state's info.
	static shared<AllocationState> s_globalAllocationState;
	// Current Memory allocation state's info.
	static shared<AllocationState> s_currentAllocationState;
	// Last Memory allocation state's info.
	static shared<AllocationState> s_lastAllocationState;
};
shared<AllocationState> StateManager::s_globalAllocationState;
shared<AllocationState> StateManager::s_currentAllocationState;
shared<AllocationState> StateManager::s_lastAllocationState;
}// namespace
#ifdef OWL_STACKTRACE
struct TraceInternal {
	// Stack trace of the allocation.
	cpptrace::stacktrace fullTrace;

	[[nodiscard]] auto getCallerInfo() const -> const cpptrace::stacktrace_frame& {
		for (const auto& frame: fullTrace) {
			if ((!frame.filename.ends_with(".cpp") && !frame.filename.ends_with(".h")) ||
				frame.filename.ends_with("Tracker.cpp") || frame.filename.ends_with("Core.h") ||
				frame.filename.contains("bits"))
				continue;
			return frame;
		}
		return fullTrace.frames.front();
	}
};
#endif

// =========================== TrackerAPI =================================
void TrackerAPI::allocate(void* iMemoryPtr, const size_t iSize) {
	if (g_AntiLoop)
		return;
	g_AntiLoop = true;
	if (!TrackerState::get().canTrack()) {
		g_AntiLoop = false;
		return;
	}
	StateManager::pushMemory(iMemoryPtr, iSize);
	g_AntiLoop = false;
}

void TrackerAPI::deallocate(void* iMemoryPtr, const size_t iSize) {
	if (g_AntiLoop)
		return;
	g_AntiLoop = true;
	if (!TrackerState::get().canTrack()) {
		g_AntiLoop = false;
		return;
	}
	StateManager::freeMemory(iMemoryPtr, iSize);
	g_AntiLoop = false;
}

auto TrackerAPI::checkState() -> const AllocationState& {
	StateManager::swapCurrent();
	return StateManager::getLastAllocationState();
}

auto TrackerAPI::globals() -> const AllocationState& { return StateManager::getGlobalAllocationState(); }

// =========================== Allocation Info =================================
AllocationInfo::AllocationInfo(void* iLocation, const size_t iSize) : location{iLocation}, size{iSize} {
#ifdef OWL_STACKTRACE
	traceInternal = mkShared<TraceInternal>();
	traceInternal->fullTrace = cpptrace::generate_trace();
#endif
}

auto AllocationInfo::getLibName() const -> std::string {// NOLINT(readability-convert-member-functions-to-static)
#ifdef OWL_STACKTRACE
	if (traceInternal && !traceInternal->fullTrace.empty()) {
		auto last = traceInternal->getCallerInfo();
		if (last.symbol.contains("owl::"))
			return "owl";
		// special case for openAL: looked into the file path
		if (last.filename.contains("openal"))
			return "openal";
		if (last.symbol.contains("::"))
			// the first level of namespace is the library name
			return last.symbol.substr(0, last.symbol.find("::"));
		if (last.filename.contains("yaml-cpp"))
			return "YAML";
	}
	return "unknown";
#else
	return "";
#endif
}

auto AllocationInfo::toStr([[maybe_unused]] const bool iTracePrint, [[maybe_unused]] const bool iFullTrace) const
		-> std::string {
	std::string result = std::format("memory chunk at {} size ({})", location, core::utils::sizeToString(size));

#ifdef OWL_STACKTRACE
	if (!traceInternal || traceInternal->fullTrace.empty()) {
		result += std::format(" <empty Trace>");
	} else {
		auto last = traceInternal->getCallerInfo();
		result += std::format(" allocated {} ({}:{}:{})", last.symbol, last.filename, last.line.value_or(0),
							  last.column.value_or(0));
		if (iTracePrint) {
			result += std::format("\n *** {} StackTrace (most recent call first) *** \n",
								  iFullTrace ? "Full" : "Simplified");
			uint32_t frameId = 0;
			for (auto frame: traceInternal->fullTrace) {
				if (!iFullTrace && !frame.symbol.starts_with("owl::"))
					continue;
				result += std::format("#{} at {}:{}:{} symbol {}{} ({:#x})\n", frameId, frame.filename,
									  frame.line.value_or(0), frame.column.value_or(0),
									  frame.is_inline ? "(inline) " : "", frame.symbol, frame.raw_address);
				++frameId;
			}
		}
	}
#endif
	return result;
}

// =========================== Allocation state =================================
AllocationState::~AllocationState() { g_AntiLoop = true; }

void AllocationState::pushMemory(void* iLocation, size_t iSize) {
	allocationCalls++;
	allocatedMemory += iSize;
	memoryPeek = std::max(memoryPeek, allocatedMemory);
	if (!g_AntiLoop)
		std::fputs("Tracker: anti-loop guard tripped during pushMemory\n", stderr);
	allocs.emplace_back(iLocation, iSize);
	index.insert_or_assign(iLocation, std::prev(allocs.end()));
}

void AllocationState::freeMemory(void* iLocation, size_t iSize) {
	const auto it = index.find(iLocation);
	if (it == index.end())
		return;
	const auto listIt = it->second;
	if (iSize == 0)
		iSize = listIt->size;
	allocs.erase(listIt);
	index.erase(it);
	deallocationCalls++;
	allocatedMemory -= iSize;
}

void AllocationState::resetState() {
	allocs.clear();
	index.clear();
	allocatedMemory = 0;
	allocationCalls = 0;
	deallocationCalls = 0;
	memoryPeek = 0;
}

// =========================== scopes ==============================
ScopeUntrack::ScopeUntrack() {
	g_AntiLoop = true;
	TrackerState::get().pushTrack(false);
	g_AntiLoop = false;
}

ScopeUntrack::~ScopeUntrack() {
	g_AntiLoop = true;
	TrackerState::get().popTrack();
	g_AntiLoop = false;
}

ScopeTrack::ScopeTrack() {
	g_AntiLoop = true;
	TrackerState::get().pushTrack(true);
	g_AntiLoop = false;
}

ScopeTrack::~ScopeTrack() {
	g_AntiLoop = true;
	TrackerState::get().popTrack();
	g_AntiLoop = false;
}

}// namespace owl::debug

// NOLINTEND(misc-no-recursion)
