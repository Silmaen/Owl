/**
 * @file RenderAPI.cpp
 * @author Silmaen
 * @date 09/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "null/RenderAPI.h"
#include "renderer/gpu/RenderAPI.h"
#if OWL_WITH_RENDER
#include "opengl/RenderAPI.h"
#include "vulkan/RenderAPI.h"
#endif

namespace owl::renderer::gpu {

auto RenderAPI::create(const Type& iType) -> uniq<RenderAPI> {
	switch (iType) {
		case Type::Null:
			return mkUniq<null::RenderAPI>();
#if OWL_WITH_RENDER
		case Type::OpenGL:
			return mkUniq<opengl::RenderAPI>();
		case Type::Vulkan:
			return mkUniq<vulkan::RenderAPI>();
#else
		case Type::OpenGL:
		case Type::Vulkan:
			// GPU backends not built (OWL_MODULE_RENDER=OFF): the engine runs headless.
			OWL_CORE_WARN("RenderAPI: {} not built in (OWL_MODULE_RENDER=OFF), using the Null backend.",
						  iType == Type::OpenGL ? "OpenGL" : "Vulkan")
			return mkUniq<null::RenderAPI>();
#endif
	}

	OWL_CORE_ERROR("Unknown RendererAPI!")
	return nullptr;
}

RenderAPI::~RenderAPI() = default;

}// namespace owl::renderer::gpu
