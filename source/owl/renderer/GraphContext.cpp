/**
 * @file GraphContext.cpp
 * @author Silmaen
 * @date 07/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "GraphContext.h"
#include "Renderer.h"
#include "null/GraphContext.h"
#include "opengl/GraphContext.h"
#include "opengl_legacy/GraphContext.h"
#include "vulkan/GraphContext.h"

namespace owl::renderer {

uniq<GraphContext> GraphContext::create(void *ioWindow) {
	switch (RenderCommand::getAPI()) {
		case RenderAPI::Type::Null:
			return mkUniq<null::GraphContext>(static_cast<GLFWwindow *>(ioWindow));
		case RenderAPI::Type::OpenGL:
			return mkUniq<opengl::GraphContext>(static_cast<GLFWwindow *>(ioWindow));
		case RenderAPI::Type::OpenglLegacy:
			return mkUniq<opengl_legacy::GraphContext>(static_cast<GLFWwindow *>(ioWindow));
		case RenderAPI::Type::Vulkan:
			return mkUniq<vulkan::GraphContext>(static_cast<GLFWwindow *>(ioWindow));
	}

	OWL_CORE_ERROR("Unknown RendererAPI!")
	return nullptr;
}

GraphContext::~GraphContext() = default;

}// namespace owl::renderer
