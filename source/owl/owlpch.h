/**
 * @file owlpch.h
 * @author Silmaen
 * @date 06/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Macros.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wundefined-func-template")

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <streambuf>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#if !defined(__clang__) or __clang_major__ > 15
#include <ranges>
#endif

#include "debug/Profiler.h"
#include "debug/Tracker.h"

// third party
#include <entt/entt.hpp>
#include <magic_enum.hpp>

OWL_DIAG_POP
