/**
 * @file EntryPoint.h
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "core/Application.h"
#include "core/Log.h"
#include "debug/Profiler.h"
#include "debug/TrackerClient.h"

/**
 * @brief Main entry point for the program.
 * @param[in] iArgc Number of argument.
 * @param[in] iArgv List of arguments.
 * @return Execution code.
 */
// NOLINTBEGIN(misc-definitions-in-headers)
auto main(int iArgc, char* iArgv[]) -> int {
	owl::core::Log::init();
	{
		// Startup
		OWL_PROFILE_BEGIN_SESSION("Startup", "OwlProfile-startup.json")
		auto app = owl::core::createApplication(iArgc, iArgv);
		OWL_PROFILE_END_SESSION()
		// runtime
		OWL_PROFILE_BEGIN_SESSION("Runtime", "OwlProfile-runtime.json")
		OWL_CORE_TRACE("run!")
		app->run();
		OWL_PROFILE_END_SESSION()
		// Shutdown
		OWL_PROFILE_BEGIN_SESSION("Shutdown", "OwlProfile-shutdown.json")
		OWL_CORE_TRACE("Terminate application.")
		app.reset();
#if OWL_TRACKER_VERBOSITY >= 1
		{
			OWL_SCOPE_UNTRACK
			const auto& memState = owl::debug::TrackerAPI::checkState();
			if (memState.allocationCalls > memState.deallocationCalls) {
				OWL_CORE_TRACE("----------------------------------")
				OWL_CORE_TRACE("Leak Detected during App release")
				OWL_CORE_TRACE("-----------------------------------")
				OWL_CORE_TRACE("")
				OWL_CORE_TRACE(" LEAK Amount: {} in {} Unallocated chunks", memState.allocatedMemory.str(),
							   memState.allocs.size())
#if OWL_TRACKER_VERBOSITY >= 2
				for (const auto& chunk: memState.allocs) { OWL_CORE_TRACE(" ** {}", chunk.toStr()) }
#endif
				OWL_CORE_TRACE("----------------------------------")
				OWL_CORE_TRACE("")
			}
		}
#endif
		OWL_PROFILE_END_SESSION()
	}
	{
		OWL_SCOPE_UNTRACK
		// ==================== Print Memory informations ========================
#if OWL_TRACKER_VERBOSITY >= 1
		OWL_CORE_INFO("Memory State at the end of execution:")
		const auto& memoryState = owl::debug::TrackerAPI::globals();
		OWL_CORE_INFO("Residual memory          : {}", memoryState.allocatedMemory.str())
		OWL_CORE_INFO("Memory peek              : {}", memoryState.memoryPeek.str())
		OWL_CORE_INFO("Total Allocation calls   : {}", memoryState.allocationCalls)
		OWL_CORE_INFO("Total Deallocation calls : {}", memoryState.deallocationCalls)
#endif
#if OWL_TRACKER_VERBOSITY >= 2
		if (memoryState.allocationCalls > memoryState.deallocationCalls) {
			OWL_CORE_INFO("Remaining memory chunks  :", memoryState.allocationCalls)
			for (const auto& alloc: memoryState.allocs) { OWL_CORE_INFO("* {}", alloc.toStr(false, false)) }
		}
#endif
	}
	// Destroy the logger
	owl::core::Log::invalidate();
	return 0;
}
// NOLINTEND(misc-definitions-in-headers)
