/**
 * @file UniformBuffer.cpp
 * @author Silmaen
 * @date 02/01/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer.h"
#include "renderer/gpu/UniformBuffer.h"
#include "renderer/gpu/null/UniformBuffer.h"
#if OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
#include "renderer/gpu/opengl/UniformBuffer.h"
#endif
#include "renderer/gpu/vulkan/UniformBuffer.h"
#endif
#if OWL_WITH_WEBGPU
#include "webgpu/Buffer.h"
#endif

#include <cstdint>

namespace owl::renderer::gpu {

auto UniformBuffer::create(uint32_t iSize, uint32_t iBinding, [[maybe_unused]] const std::string& iRenderer)
		-> shared<UniformBuffer> {
	const auto api = RenderCommand::getApi();
	switch (api) {
#if OWL_WITH_WEBGPU
		case RenderAPI::Type::WebGpu:
			return mkShared<webgpu::UniformBuffer>(iSize, iBinding, iRenderer);
#else
		case RenderAPI::Type::WebGpu:
#endif
		case RenderAPI::Type::Null:
			return mkShared<null::UniformBuffer>(iSize, iBinding);
#if OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
		case RenderAPI::Type::OpenGL:
			return mkShared<opengl::UniformBuffer>(iSize, iBinding);
#else
		case RenderAPI::Type::OpenGL:
			break;// No OpenGL backend on Android
#endif
		case RenderAPI::Type::Vulkan:
			return mkShared<vulkan::UniformBuffer>(iSize, iBinding, iRenderer);
#else
		case RenderAPI::Type::OpenGL:
		case RenderAPI::Type::Vulkan:
			break;// GPU backends not built (no Vulkan nor OpenGL in this build)
#endif
	}
	OWL_CORE_ERROR("UniformBuffer: Unknown render API ({}).", static_cast<int>(api))
	return nullptr;
}

UniformBuffer::~UniformBuffer() = default;

}// namespace owl::renderer::gpu
