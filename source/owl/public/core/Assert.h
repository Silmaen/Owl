/**
 * @file Assert.h
 * @author Silmaen
 * @date 07/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Log.h"

#if defined(OWL_DEBUG) && defined(OWL_ENGINE_BUILD)
#define OWL_ENABLE_ASSERTS
#endif

#ifdef OWL_ENABLE_ASSERTS
#include <debugbreak.h>
#define OWL_ASSERT(x, ...)                                                                                             \
	{                                                                                                                  \
		if (!(x)) {                                                                                                    \
			OWL_ERROR("Assertion Failed: {}", __VA_ARGS__)                                                             \
			debug_break();                                                                                             \
		}                                                                                                              \
	}
#define OWL_CORE_ASSERT(x, ...)                                                                                        \
	{                                                                                                                  \
		if (!(x)) {                                                                                                    \
			OWL_CORE_ERROR("Assertion Failed: {}", __VA_ARGS__)                                                        \
			debug_break();                                                                                             \
		}                                                                                                              \
	}
#else
#define OWL_ASSERT(x, ...)
#define OWL_CORE_ASSERT(x, ...)
#endif
