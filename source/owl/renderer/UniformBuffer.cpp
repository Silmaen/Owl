/**
 * @file UniformBuffer.cpp
 * @author Silmaen
 * @date 02/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include <magic_enum.hpp>

#include "Renderer.h"
#include "UniformBuffer.h"
#include "renderer/opengl/UniformBuffer.h"

namespace owl::renderer {

shrd<UniformBuffer> UniformBuffer::create(uint32_t size, uint32_t binding) {
	auto type = Renderer::getAPI();
	switch (type) {
		case RenderAPI::Type::None:
		case RenderAPI::Type::Vulkan:
			OWL_CORE_ASSERT(false, "Render API {} is not yet supported", magic_enum::enum_name(type))
			return nullptr;
		case RenderAPI::Type::OpenGL:
			return mk_shrd<opengl::UniformBuffer>(size, binding);
	}
	OWL_CORE_ASSERT(false, "Unknown API Type!")
	return nullptr;
}

}// namespace owl::renderer
