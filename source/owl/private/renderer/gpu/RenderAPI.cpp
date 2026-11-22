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
#if OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
#include "opengl/RenderAPI.h"
#endif
#include "vulkan/RenderAPI.h"
#endif
#if OWL_WITH_WEBGPU
#include "webgpu/RenderAPI.h"
#endif

#include <optional>

namespace owl::renderer::gpu {

auto RenderAPI::create(const Type& iType) -> uniq<RenderAPI> {
	switch (iType) {
		case Type::Null:
			return mkUniq<null::RenderAPI>();
#if OWL_WITH_WEBGPU
		case Type::OpenGL:
		case Type::Vulkan:
		case Type::WebGpu:
			// The browser has WebGPU only: a game asking for Vulkan or OpenGL draws through it.
			return mkUniq<webgpu::RenderAPI>();
#elif OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
		case Type::OpenGL:
			return mkUniq<opengl::RenderAPI>();
#else
		case Type::OpenGL:
			break;// No OpenGL backend on Android
#endif
		case Type::Vulkan:
			return mkUniq<vulkan::RenderAPI>();
		case Type::WebGpu:
			OWL_CORE_INFO("RenderAPI: WebGPU is the backend of the Web build, using the Null backend.")
			return mkUniq<null::RenderAPI>();
#else
		case Type::OpenGL:
		case Type::Vulkan:
		case Type::WebGpu:
			// GPU backends not built (no Vulkan nor OpenGL in this build): the engine runs headless.
			OWL_CORE_INFO("RenderAPI: {} not built in (no Vulkan nor OpenGL in this build), using the Null backend.",
						  magic_enum::enum_name(iType))
			return mkUniq<null::RenderAPI>();
#endif
	}

	OWL_CORE_ERROR("RenderAPI: Unknown render API type.")
	return nullptr;
}

auto RenderAPI::getFallback(const Type iType) -> std::optional<Type> {
	if (iType == Type::Vulkan)
		return Type::OpenGL;
	return std::nullopt;
}

RenderAPI::~RenderAPI() = default;

}// namespace owl::renderer::gpu
