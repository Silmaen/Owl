/**
 * @file GpuProfiler.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "GpuProfiler.h"

#ifdef OWL_PROFILER_TRACY
#include "core/external/opengl46.h"
#include "core/external/tracy.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wcast-align")
OWL_DIAG_DISABLE_CLANG("-Wsign-conversion")
OWL_DIAG_DISABLE_CLANG("-Wshorten-64-to-32")
OWL_DIAG_DISABLE_CLANG("-Wimplicit-int-conversion")
OWL_DIAG_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wreserved-macro-identifier")
OWL_DIAG_DISABLE_CLANG("-Wundef")
#include <tracy/TracyOpenGL.hpp>
OWL_DIAG_POP

#include <optional>
#endif

namespace owl::renderer::gpu::opengl {

#ifdef OWL_PROFILER_TRACY
namespace {
constexpr tracy::SourceLocationData g_frameLocation{"OpenGL frame", "GpuProfiler::beginFrame", __FILE__, __LINE__, 0};
bool g_contextCreated = false;
std::optional<tracy::GpuCtxScope> g_frameZone;
}// namespace

void GpuProfiler::init() {
	if (g_contextCreated)
		return;
	auto& wrapper = tracy::GetGpuCtx();
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory) Tracy owns its GPU context, as TracyGpuContext does
	wrapper.ptr = new (tracy::tracy_malloc(sizeof(tracy::GpuCtx))) tracy::GpuCtx;
	g_contextCreated = true;
}

void GpuProfiler::beginFrame() {
	if (!g_contextCreated || g_frameZone.has_value())
		return;
	g_frameZone.emplace(&g_frameLocation, true);
}

void GpuProfiler::endFrame() { g_frameZone.reset(); }

void GpuProfiler::collect() {
	if (!g_contextCreated)
		return;
	tracy::GetGpuCtx().ptr->Collect();
}
#else
void GpuProfiler::init() {}

void GpuProfiler::beginFrame() {}

void GpuProfiler::endFrame() {}

void GpuProfiler::collect() {}
#endif

}// namespace owl::renderer::gpu::opengl
