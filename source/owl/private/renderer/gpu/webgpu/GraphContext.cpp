/**
 * @file GraphContext.cpp
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "GraphContext.h"

#include "Context.h"
#include "core/external/glfw3.h"

#include <webgpu/webgpu.h>

#include <cstdint>

namespace owl::renderer::gpu::webgpu {

GraphContext::GraphContext(GLFWwindow* ioWindow) : mp_window{ioWindow} {}

void GraphContext::init() {
	OWL_PROFILE_FUNCTION()

	auto& context = Context::get();
	// The page requested the adapter and the device before main: a browser gives them asynchronously only.
	context.device = wgpu::Device::Acquire(emscripten_webgpu_get_device());
	if (context.device == nullptr) {
		OWL_CORE_ERROR("WebGPU: The page made no device.")
		return;
	}
	context.queue = context.device.GetQueue();
	wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvas{};
	canvas.selector = "#canvas";
	const wgpu::SurfaceDescriptor surfaceDesc{.nextInChain = &canvas, .label = "Canvas"};
	context.instance = wgpu::CreateInstance();
	context.surface = context.instance.CreateSurface(&surfaceDesc);
	wgpu::SurfaceCapabilities capabilities;
	context.surface.GetCapabilities(nullptr, &capabilities);
	context.format = capabilities.formatCount > 0 ? capabilities.formats[0] : wgpu::TextureFormat::BGRA8Unorm;
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(mp_window, &width, &height);
	context.configure(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
	OWL_CORE_INFO("WebGPU: Canvas {}x{} ready.", width, height)
}

void GraphContext::swapBuffers() {}

}// namespace owl::renderer::gpu::webgpu
