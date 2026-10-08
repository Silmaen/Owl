/**
 * @file ComputeShader.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer.h"
#include "renderer/gpu/ComputeShader.h"
#include "renderer/gpu/null/ComputeShader.h"
#if OWL_WITH_RENDER
#include "renderer/gpu/opengl/ComputeShader.h"
#include "renderer/gpu/vulkan/ComputeShader.h"
#endif

namespace owl::renderer::gpu {

auto ComputeShader::create([[maybe_unused]] const std::string& iShaderName,
						   [[maybe_unused]] const std::string& iRenderer) -> shared<ComputeShader> {
	const auto api = RenderCommand::getApi();
	switch (api) {
		case RenderAPI::Type::Null:
			return mkShared<null::ComputeShader>();
#if OWL_WITH_RENDER
		case RenderAPI::Type::OpenGL:
			return mkShared<opengl::ComputeShader>(iShaderName, iRenderer);
		case RenderAPI::Type::Vulkan:
			return mkShared<vulkan::ComputeShader>(iShaderName, iRenderer);
#else
		case RenderAPI::Type::OpenGL:
		case RenderAPI::Type::Vulkan:
			break;// GPU backends not built (OWL_MODULE_RENDER=OFF)
#endif
	}
	OWL_CORE_ERROR("Unknown RendererAPI ({}).", static_cast<int>(api))
	return nullptr;
}

ComputeShader::~ComputeShader() = default;

}// namespace owl::renderer::gpu
