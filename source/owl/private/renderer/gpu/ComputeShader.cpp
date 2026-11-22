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
#if OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
#include "renderer/gpu/opengl/ComputeShader.h"
#endif
#include "renderer/gpu/vulkan/ComputeShader.h"
#endif
#if OWL_WITH_WEBGPU
#include "renderer/gpu/webgpu/ComputeShader.h"
#endif

namespace owl::renderer::gpu {

auto ComputeShader::create([[maybe_unused]] const std::string& iShaderName,
						   [[maybe_unused]] const std::string& iRenderer) -> shared<ComputeShader> {
	const auto api = RenderCommand::getApi();
	switch (api) {
		case RenderAPI::Type::Null:
			return mkShared<null::ComputeShader>();
#if OWL_WITH_WEBGPU
		case RenderAPI::Type::WebGpu:
			return mkShared<webgpu::ComputeShader>(iShaderName, iRenderer);
		case RenderAPI::Type::OpenGL:
		case RenderAPI::Type::Vulkan:
			break;// The Web build has WebGPU only
#elif OWL_WITH_NATIVE_GPU
#if OWL_WITH_DESKTOP
		case RenderAPI::Type::OpenGL:
			return mkShared<opengl::ComputeShader>(iShaderName, iRenderer);
#else
		case RenderAPI::Type::OpenGL:
			break;// No OpenGL backend on Android
#endif
		case RenderAPI::Type::Vulkan:
			return mkShared<vulkan::ComputeShader>(iShaderName, iRenderer);
		case RenderAPI::Type::WebGpu:
			return mkShared<null::ComputeShader>();
#else
		case RenderAPI::Type::OpenGL:
		case RenderAPI::Type::Vulkan:
		case RenderAPI::Type::WebGpu:
			break;// GPU backends not built (no Vulkan nor OpenGL in this build)
#endif
	}
	OWL_CORE_ERROR("ComputeShader: Unknown render API ({}).", static_cast<int>(api))
	return nullptr;
}

ComputeShader::~ComputeShader() = default;

}// namespace owl::renderer::gpu
