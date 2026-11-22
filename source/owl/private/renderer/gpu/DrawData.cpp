/**
 * @file DrawData.cpp
 * @author Silmaen
 * @date 04/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "null/DrawData.h"
#if OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
#include "opengl/DrawData.h"
#endif
#include "vulkan/DrawData.h"
#endif
#if OWL_WITH_WEBGPU
#include "webgpu/DrawData.h"
#endif
#include "renderer/Renderer.h"
#include "renderer/gpu/DrawData.h"

namespace owl::renderer::gpu {

DrawData::~DrawData() = default;

auto DrawData::create() -> shared<DrawData> {
	const auto api = RenderCommand::getApi();
	switch (api) {
#if OWL_WITH_WEBGPU
		case RenderAPI::Type::WebGpu:
			return mkShared<webgpu::DrawData>();
#else
		case RenderAPI::Type::WebGpu:
#endif
		case RenderAPI::Type::Null:
			return mkShared<null::DrawData>();
#if OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
		case RenderAPI::Type::OpenGL:
			return mkShared<opengl::DrawData>();
#else
		case RenderAPI::Type::OpenGL:
			break;// No OpenGL backend on Android
#endif
		case RenderAPI::Type::Vulkan:
			return mkShared<vulkan::DrawData>();
#else
		case RenderAPI::Type::OpenGL:
		case RenderAPI::Type::Vulkan:
			break;// GPU backends not built (no Vulkan nor OpenGL in this build)
#endif
	}
	OWL_CORE_ERROR("DrawData: Unknown render API ({}).", static_cast<int>(api))
	return nullptr;
}

}// namespace owl::renderer::gpu
