/**
 * @file GraphContext.cpp
 * @author Silmaen
 * @date 07/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "null/GraphContext.h"
#if OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
#include "opengl/GraphContext.h"
#endif
#include "vulkan/GraphContext.h"
#endif
#if OWL_WITH_WEBGPU
#include "webgpu/GraphContext.h"
#endif
#include "renderer/Renderer.h"
#include "renderer/gpu/GraphContext.h"

namespace owl::renderer::gpu {

auto GraphContext::create(void* ioWindow) -> uniq<GraphContext> {
	const auto api = RenderCommand::getApi();
	switch (api) {
		case RenderAPI::Type::Null:
			return mkUniq<null::GraphContext>(ioWindow);
#if OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
		case RenderAPI::Type::OpenGL:
			return mkUniq<opengl::GraphContext>(static_cast<GLFWwindow*>(ioWindow));
#else
		case RenderAPI::Type::OpenGL:
			break;// No OpenGL backend on Android
#endif
		case RenderAPI::Type::Vulkan:
			return mkUniq<vulkan::GraphContext>(ioWindow);
		case RenderAPI::Type::WebGpu:
			break;// The backend of the Web build
#elif OWL_WITH_WEBGPU
		case RenderAPI::Type::WebGpu:
			return mkUniq<webgpu::GraphContext>(static_cast<GLFWwindow*>(ioWindow));
		case RenderAPI::Type::OpenGL:
		case RenderAPI::Type::Vulkan:
			break;// The Web build has WebGPU only
#else
		case RenderAPI::Type::OpenGL:
		case RenderAPI::Type::Vulkan:
		case RenderAPI::Type::WebGpu:
			break;// GPU backends not built (no Vulkan nor OpenGL in this build)
#endif
	}

	OWL_CORE_ERROR("GraphContext: Unknown render API ({}).", static_cast<int>(api))
	return nullptr;
}

GraphContext::~GraphContext() = default;

}// namespace owl::renderer::gpu
