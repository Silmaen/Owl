/**
 * @file EntryPoint.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "app/Application.h"
#include "core/Log.h"
#include "core/utils/StringUtils.h"
#include "debug/Profiler.h"
#include "debug/TrackerClient.h"
#include "platform/Console.h"

#include <cstddef>
#include <string>
#include <unordered_map>

/**
 * @brief
 *  Main entry point for the program.
 * @param[in] iArgc Number of argument.
 * @param[in] iArgv List of arguments.
 * @return Execution code.
 */
// NOLINTBEGIN(misc-definitions-in-headers)
auto main(int iArgc, char* iArgv[]) -> int {
	// A GUI program on Windows: its command-line output goes to the console it was started from, if any.
	owl::platform::attachParentConsole();
	owl::core::Log::init();
	int exitCode = 0;
	{
		// Startup
		OWL_PROFILE_BEGIN_SESSION("Startup", "OwlProfile-startup.json")
		auto app = owl::app::createApplication(iArgc, iArgv);
		OWL_PROFILE_END_SESSION()
		// runtime
		OWL_PROFILE_BEGIN_SESSION("Runtime", "OwlProfile-runtime.json")
		OWL_CORE_DEBUG("Application: Running.")
		app->run();
		exitCode = app->getExitCode();
		OWL_PROFILE_END_SESSION()
		// Shutdown
		OWL_PROFILE_BEGIN_SESSION("Shutdown", "OwlProfile-shutdown.json")
		OWL_CORE_DEBUG("Application: Terminating.")
		app.reset();
#if OWL_TRACKER_VERBOSITY >= 1
		{
			OWL_SCOPE_UNTRACK
			const auto& memState = owl::debug::TrackerAPI::checkState();
			if (memState.allocationCalls > memState.deallocationCalls) {
				OWL_CORE_WARN("Memory: Leak at application release, {} in {} unreleased chunk(s).",
							  owl::core::utils::sizeToString(memState.allocatedMemory), memState.allocs.size())
#if OWL_TRACKER_VERBOSITY >= 2
				for (const auto& chunk: memState.allocs) { OWL_CORE_DEBUG("Memory: Leaked chunk {}.", chunk.toStr()) }
#endif
			}
		}
#endif
		OWL_PROFILE_END_SESSION()
	}
	{
		OWL_SCOPE_UNTRACK
		// ==================== Print Memory information ========================
#if OWL_TRACKER_VERBOSITY >= 1
		OWL_CORE_DEBUG("Memory: State at the end of execution.")
		const auto& memoryState = owl::debug::TrackerAPI::globals();
		OWL_CORE_DEBUG("Memory: Residual memory {}.", owl::core::utils::sizeToString(memoryState.allocatedMemory))
		OWL_CORE_DEBUG("Memory: Peak {}.", owl::core::utils::sizeToString(memoryState.memoryPeek))
		OWL_CORE_DEBUG("Memory: {} allocation calls.", memoryState.allocationCalls)
		OWL_CORE_DEBUG("Memory: {} deallocation calls.", memoryState.deallocationCalls)
#endif
#if OWL_TRACKER_VERBOSITY >= 2
		if (memoryState.allocationCalls > memoryState.deallocationCalls) {
			OWL_CORE_WARN("Memory: Chunks remain at the end of execution ({} allocation calls).",
						  memoryState.allocationCalls)
			std::unordered_map<std::string, size_t> remainingThirdSize;
			for (const auto& alloc: memoryState.allocs) {
				if (auto libname = alloc.getLibName(); libname.empty() || libname == "owl" || libname == "unknown") {
					OWL_CORE_DEBUG("Memory: Remaining chunk {}.", alloc.toStr(false, false))
				} else {
					remainingThirdSize[libname] += alloc.size;
				}
			}
			OWL_CORE_DEBUG("Memory: Remaining chunks of third-party libraries.")
			for (const auto& [libname, size]: remainingThirdSize) {
				OWL_CORE_DEBUG("Memory: Library '{}' keeps {}.", libname, owl::core::utils::sizeToString(size))
			}
		}
#endif
	}
	// Destroy the logger
	owl::core::Log::invalidate();
	return exitCode;
}
// NOLINTEND(misc-definitions-in-headers)
