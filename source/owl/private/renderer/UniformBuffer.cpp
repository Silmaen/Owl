/**
 * @file UniformBuffer.cpp
 * @author Silmaen
 * @date 02/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/UniformBuffer.h"
#include "null/UniformBuffer.h"
#include "opengl/UniformBuffer.h"
#include "renderer/Renderer.h"
#include "vulkan/UniformBuffer.h"

namespace owl::renderer {

auto UniformBuffer::create(uint32_t iSize, uint32_t iBinding, const std::string& iRenderer) -> shared<UniformBuffer> {
	switch (RenderCommand::getApi()) {
		case RenderAPI::Type::Null:
			return mkShared<null::UniformBuffer>(iSize, iBinding);
		case RenderAPI::Type::OpenGL:
			return mkShared<opengl::UniformBuffer>(iSize, iBinding);
		case RenderAPI::Type::Vulkan:
			return mkShared<vulkan::UniformBuffer>(iSize, iBinding, iRenderer);
	}
	OWL_CORE_ERROR("Unknown Graphic API Type!")
	return nullptr;
}

UniformBuffer::~UniformBuffer() = default;

}// namespace owl::renderer
