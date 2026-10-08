/**
 * @file GraphContext.cpp
 * @author Silmaen
 * @date 07/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "null/GraphContext.h"
#if OWL_WITH_RENDER
#include "opengl/GraphContext.h"
#include "vulkan/GraphContext.h"
#endif
#include "renderer/Renderer.h"
#include "renderer/gpu/GraphContext.h"

namespace owl::renderer::gpu {

auto GraphContext::create(void* ioWindow) -> uniq<GraphContext> {
	const auto api = RenderCommand::getApi();
	switch (api) {
		case RenderAPI::Type::Null:
			return mkUniq<null::GraphContext>(ioWindow);
#if OWL_WITH_RENDER
		case RenderAPI::Type::OpenGL:
			return mkUniq<opengl::GraphContext>(static_cast<GLFWwindow*>(ioWindow));
		case RenderAPI::Type::Vulkan:
			return mkUniq<vulkan::GraphContext>(static_cast<GLFWwindow*>(ioWindow));
#else
		case RenderAPI::Type::OpenGL:
		case RenderAPI::Type::Vulkan:
			break;// GPU backends not built (OWL_MODULE_RENDER=OFF)
#endif
	}

	OWL_CORE_ERROR("Unknown RendererAPI ({}).", static_cast<int>(api))
	return nullptr;
}

GraphContext::~GraphContext() = default;

}// namespace owl::renderer::gpu
