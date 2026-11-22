/**
 * @file StorageBuffer.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/gpu/null/StorageBuffer.h"
#if OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
#include "renderer/gpu/opengl/StorageBuffer.h"
#endif
#include "renderer/gpu/vulkan/StorageBuffer.h"
#endif
#if OWL_WITH_WEBGPU
#include "webgpu/Buffer.h"
#endif

#include <cstdint>

namespace owl::renderer::gpu {

auto StorageBuffer::create(const uint32_t iSize, const uint32_t iBinding, [[maybe_unused]] const std::string& iRenderer)
		-> shared<StorageBuffer> {
	const auto api = RenderCommand::getApi();
	switch (api) {
#if OWL_WITH_WEBGPU
		case RenderAPI::Type::WebGpu:
			return mkShared<webgpu::StorageBuffer>(iSize, iBinding, iRenderer);
#else
		case RenderAPI::Type::WebGpu:
#endif
		case RenderAPI::Type::Null:
			return mkShared<null::StorageBuffer>(iSize, iBinding);
#if OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
		case RenderAPI::Type::OpenGL:
			return mkShared<opengl::StorageBuffer>(iSize, iBinding);
#else
		case RenderAPI::Type::OpenGL:
			break;// No OpenGL backend on Android
#endif
		case RenderAPI::Type::Vulkan:
			return mkShared<vulkan::StorageBuffer>(iSize, iBinding);
#else
		case RenderAPI::Type::OpenGL:
		case RenderAPI::Type::Vulkan:
			break;// GPU backends not built (no Vulkan nor OpenGL in this build)
#endif
	}
	OWL_CORE_ERROR("StorageBuffer: Unknown render API ({}).", static_cast<int>(api))
	return nullptr;
}

StorageBuffer::~StorageBuffer() = default;

}// namespace owl::renderer::gpu
