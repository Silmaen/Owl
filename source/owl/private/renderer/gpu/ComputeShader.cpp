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
#include "renderer/gpu/opengl/ComputeShader.h"
#include "renderer/gpu/vulkan/ComputeShader.h"

namespace owl::renderer::gpu {

auto ComputeShader::create(const std::string& iShaderName, const std::string& iRenderer) -> shared<ComputeShader> {
	const auto api = RenderCommand::getApi();
	switch (api) {
		case RenderAPI::Type::Null:
			return mkShared<null::ComputeShader>();
		case RenderAPI::Type::OpenGL:
			return mkShared<opengl::ComputeShader>(iShaderName, iRenderer);
		case RenderAPI::Type::Vulkan:
			return mkShared<vulkan::ComputeShader>(iShaderName, iRenderer);
	}
	OWL_CORE_ERROR("Unknown RendererAPI ({}).", static_cast<int>(api))
	return nullptr;
}

ComputeShader::~ComputeShader() = default;

}// namespace owl::renderer::gpu
