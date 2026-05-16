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
#include "renderer/gpu/opengl/StorageBuffer.h"
#include "renderer/gpu/vulkan/StorageBuffer.h"

namespace owl::renderer::gpu {

auto StorageBuffer::create(const uint32_t iSize, const uint32_t iBinding, [[maybe_unused]] const std::string& iRenderer)
		-> shared<StorageBuffer> {
	const auto api = RenderCommand::getApi();
	switch (api) {
		case RenderAPI::Type::Null:
			return mkShared<null::StorageBuffer>(iSize, iBinding);
		case RenderAPI::Type::OpenGL:
			return mkShared<opengl::StorageBuffer>(iSize, iBinding);
		case RenderAPI::Type::Vulkan:
			return mkShared<vulkan::StorageBuffer>(iSize, iBinding);
	}
	OWL_CORE_ERROR("Unknown RendererAPI ({}).", static_cast<int>(api))
	return nullptr;
}

StorageBuffer::~StorageBuffer() = default;

}// namespace owl::renderer::gpu
