/**
 * @file ComputeShader.cpp
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "ComputeShader.h"

#include "Buffer.h"
#include "Context.h"
#include "Texture.h"
#include "data/assets/AssetSearchPaths.h"
#include "renderer/utils/shaderFileUtils.h"

#include <format>
#include <regex>
#include <string>
#include <vector>

namespace owl::renderer::gpu::webgpu {

ComputeShader::ComputeShader(const std::string& iShaderName, const std::string& iRenderer)
	: m_name{std::format("{}/{}", iRenderer, iShaderName)} {
	const auto bytes =
			data::assets::readAssetBytes(utils::getPrecompiledWgslPath(iShaderName, iRenderer, ShaderType::Compute));
	if (!bytes.has_value()) {
		OWL_CORE_ERROR("WebGPU: No baked WGSL for compute shader '{}'; export the game again.", m_name)
		return;
	}
	const std::string code(bytes->begin(), bytes->end());
	// `@binding(N) @group(0) var<storage, ...> name : type` or `var name : texture_2d<f32>` or `var name : sampler`.
	static const std::regex binding{R"(@binding\((\d+)\)\s*@group\(0\)\s*var(<[^>]*>)?\s+\w+\s*:\s*(\w+))"};
	for (std::sregex_iterator it{code.begin(), code.end(), binding}; it != std::sregex_iterator{}; ++it) {
		const auto index = static_cast<uint32_t>(std::stoul((*it)[1].str()));
		const auto type = (*it)[3].str();
		m_bindings[index] = (*it)[2].matched              ? Kind::Buffer
							: type.starts_with("sampler") ? Kind::Sampler
														  : Kind::Texture;
	}
	wgpu::ShaderSourceWGSL wgsl{};
	wgsl.code = code.c_str();
	const wgpu::ShaderModuleDescriptor moduleDesc{.nextInChain = &wgsl, .label = m_name.c_str()};
	auto& context = Context::get();
	const wgpu::ShaderModule module = context.device.CreateShaderModule(&moduleDesc);
	// No layout: the one WebGPU derives from the shader, its group 0 read back at each dispatch.
	const wgpu::ComputePipelineDescriptor pipelineDesc{.label = m_name.c_str(), .compute = {.module = module}};
	m_pipeline = context.device.CreateComputePipeline(&pipelineDesc);
}

void ComputeShader::bindStorageBuffer(const uint32_t iBinding, const shared<renderer::gpu::StorageBuffer>& iBuffer) {
	const auto* buffer = dynamic_cast<const StorageBuffer*>(iBuffer.get());
	if (buffer == nullptr) {
		OWL_CORE_WARN_ONCE("WebGPU: bindStorageBuffer called with a non-WebGPU buffer on compute shader '{}'.", m_name)
		return;
	}
	m_buffers[iBinding] = buffer->getBuffer();
}

void ComputeShader::bindTexture(const uint32_t iBinding, const shared<renderer::gpu::Texture2D>& iTexture) {
	const auto* texture = dynamic_cast<const Texture2D*>(iTexture.get());
	if (texture == nullptr) {
		OWL_CORE_WARN_ONCE("WebGPU: bindTexture called with a non-WebGPU texture on compute shader '{}'.", m_name)
		return;
	}
	m_views[iBinding] = texture->getView();
	m_samplers[iBinding + 1] = texture->getSampler();
}

void ComputeShader::dispatch(const uint32_t iGroupsX, const uint32_t iGroupsY, const uint32_t iGroupsZ) {
	if (m_pipeline == nullptr || iGroupsX == 0 || iGroupsY == 0 || iGroupsZ == 0)
		return;
	auto& context = Context::get();
	std::vector<wgpu::BindGroupEntry> entries;
	entries.reserve(m_bindings.size());
	for (const auto& [index, kind]: m_bindings) {
		switch (kind) {
			case Kind::Buffer:
				{
					const auto found = m_buffers.find(index);
					entries.push_back({.binding = index,
									   .buffer = found != m_buffers.end() ? found->second : context.emptyBuffer});
					break;
				}
			case Kind::Texture:
				{
					const auto found = m_views.find(index);
					entries.push_back({.binding = index,
									   .textureView = found != m_views.end() ? found->second : context.whiteView});
					break;
				}
			case Kind::Sampler:
				{
					const auto found = m_samplers.find(index);
					entries.push_back({.binding = index,
									   .sampler = found != m_samplers.end() ? found->second : context.defaultSampler});
					break;
				}
		}
	}
	const wgpu::CommandEncoder encoder = context.device.CreateCommandEncoder();
	const wgpu::ComputePassDescriptor passDesc{.label = m_name.c_str()};
	const wgpu::ComputePassEncoder pass = encoder.BeginComputePass(&passDesc);
	pass.SetPipeline(m_pipeline);
	if (!entries.empty()) {
		const wgpu::BindGroupDescriptor groupDesc{.layout = m_pipeline.GetBindGroupLayout(0),
												  .entryCount = entries.size(),
												  .entries = entries.data()};
		pass.SetBindGroup(0, context.device.CreateBindGroup(&groupDesc));
	}
	pass.DispatchWorkgroups(iGroupsX, iGroupsY, iGroupsZ);
	pass.End();
	const wgpu::CommandBuffer commands = encoder.Finish();
	context.queue.Submit(1, &commands);
}

}// namespace owl::renderer::gpu::webgpu
