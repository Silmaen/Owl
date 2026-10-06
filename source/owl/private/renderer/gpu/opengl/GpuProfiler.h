/**
 * @file GpuProfiler.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl::renderer::gpu::opengl {
/**
 * @brief
 *  GPU zones of the OpenGL backend on the Tracy timeline (`OWL_PROFILER=tracy`), no-ops otherwise.
 *
 * One zone per frame, from `RenderAPI::beginFrame` to `RenderAPI::endFrame`, timed with `GL_TIMESTAMP` queries
 * and collected after each buffer swap.
 */
class GpuProfiler final {
public:
	GpuProfiler() = delete;

	/**
	 * @brief
	 *  Create the Tracy GPU context; needs the current OpenGL context, called once after glad is loaded.
	 */
	static void init();

	/**
	 * @brief
	 *  Open the frame GPU zone.
	 */
	static void beginFrame();

	/**
	 * @brief
	 *  Close the frame GPU zone.
	 */
	static void endFrame();

	/**
	 * @brief
	 *  Read back the finished timestamp queries, after the buffer swap.
	 */
	static void collect();
};
}// namespace owl::renderer::gpu::opengl
