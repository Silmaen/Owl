/**
 * @file Assert.h
 * @author Silmaen
 * @date 07/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Log.h"

#if defined(OWL_DEBUG) && defined(OWL_ENGINE_BUILD)
#define OWL_ENABLE_ASSERTS
#endif

#ifdef OWL_ENABLE_ASSERTS
#if defined(_MSC_VER) || defined(__MINGW32__)
#include <intrin.h>
/// Stop in the attached debugger.
#define OWL_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__)
/// Stop in the attached debugger.
#define OWL_DEBUG_BREAK() __builtin_debugtrap()
#else
#include <csignal>
/// Stop in the attached debugger.
#define OWL_DEBUG_BREAK() std::raise(SIGTRAP)
#endif
#define OWL_ASSERT(x, ...)                                                                                             \
	{                                                                                                                  \
		if (!(x)) {                                                                                                    \
			OWL_ERROR("Assertion Failed: {}.", __VA_ARGS__)                                                            \
			OWL_DEBUG_BREAK();                                                                                         \
		}                                                                                                              \
	}
#define OWL_CORE_ASSERT(x, ...)                                                                                        \
	{                                                                                                                  \
		if (!(x)) {                                                                                                    \
			OWL_CORE_ERROR("Assertion Failed: {}.", __VA_ARGS__)                                                       \
			OWL_DEBUG_BREAK();                                                                                         \
		}                                                                                                              \
	}
#else
/// Check a condition in client code; a debug engine build logs the message and breaks when it fails.
#define OWL_ASSERT(x, ...)
/// Check a condition in engine code; a debug engine build logs the message and breaks when it fails.
#define OWL_CORE_ASSERT(x, ...)
#endif
