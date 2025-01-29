/**
 * @file DrawData.cpp
 * @author Silmaen
 * @date 04/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/DrawData.h"
#include "null/DrawData.h"
#include "opengl/DrawData.h"
#include "renderer/Renderer.h"
#include "vulkan/DrawData.h"

namespace owl::renderer {

DrawData::~DrawData() = default;

auto DrawData::create() -> shared<DrawData> {
	switch (RenderCommand::getApi()) {
		case RenderAPI::Type::Null:
			return mkShared<null::DrawData>();
		case RenderAPI::Type::OpenGL:
			return mkShared<opengl::DrawData>();
		case RenderAPI::Type::Vulkan:
			return mkShared<vulkan::DrawData>();
	}
	OWL_CORE_ERROR("Unknown Graphic API Type!")
	return nullptr;
}

}// namespace owl::renderer
