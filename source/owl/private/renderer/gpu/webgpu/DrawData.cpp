/**
 * @file DrawData.cpp
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "DrawData.h"

#include "Context.h"
#include "renderer/Renderer.h"

#include <array>
#include <cstddef>

namespace owl::renderer::gpu::webgpu {

namespace {

// The attributes of a layout at the locations the shader gives their names (else in order), a column per matrix location.
void addAttributes(const BufferLayout& iLayout, const Shader& iShader,
				   std::vector<wgpu::VertexAttribute>& ioAttributes) {
	for (const auto& element: iLayout) {
		const auto location =
				iShader.getInputLocation(element.name).value_or(static_cast<uint32_t>(ioAttributes.size()));
		switch (element.type) {
			case ShaderDataType::Float:
				ioAttributes.push_back(
						{.format = wgpu::VertexFormat::Float32, .offset = element.offset, .shaderLocation = location});
				break;
			case ShaderDataType::Float2:
				ioAttributes.push_back({.format = wgpu::VertexFormat::Float32x2,
										.offset = element.offset,
										.shaderLocation = location});
				break;
			case ShaderDataType::Float3:
				ioAttributes.push_back({.format = wgpu::VertexFormat::Float32x3,
										.offset = element.offset,
										.shaderLocation = location});
				break;
			case ShaderDataType::Float4:
				ioAttributes.push_back({.format = wgpu::VertexFormat::Float32x4,
										.offset = element.offset,
										.shaderLocation = location});
				break;
			case ShaderDataType::Int:
			case ShaderDataType::Bool:
				ioAttributes.push_back(
						{.format = wgpu::VertexFormat::Sint32, .offset = element.offset, .shaderLocation = location});
				break;
			case ShaderDataType::Int2:
				ioAttributes.push_back(
						{.format = wgpu::VertexFormat::Sint32x2, .offset = element.offset, .shaderLocation = location});
				break;
			case ShaderDataType::Int3:
				ioAttributes.push_back(
						{.format = wgpu::VertexFormat::Sint32x3, .offset = element.offset, .shaderLocation = location});
				break;
			case ShaderDataType::Int4:
				ioAttributes.push_back(
						{.format = wgpu::VertexFormat::Sint32x4, .offset = element.offset, .shaderLocation = location});
				break;
			case ShaderDataType::Mat3:
				for (uint32_t col = 0; col < 3; ++col)
					ioAttributes.push_back({.format = wgpu::VertexFormat::Float32x3,
											.offset = element.offset + col * 12,
											.shaderLocation = location + col});
				break;
			case ShaderDataType::Mat4:
				for (uint32_t col = 0; col < 4; ++col)
					ioAttributes.push_back({.format = wgpu::VertexFormat::Float32x4,
											.offset = element.offset + col * 16,
											.shaderLocation = location + col});
				break;
			case ShaderDataType::None:
				break;
		}
	}
}

auto toBlend(const BlendMode iMode) -> wgpu::BlendState {
	switch (iMode) {
		case BlendMode::Opaque:
			return {.color = {.operation = wgpu::BlendOperation::Add,
							  .srcFactor = wgpu::BlendFactor::One,
							  .dstFactor = wgpu::BlendFactor::Zero},
					.alpha = {.operation = wgpu::BlendOperation::Add,
							  .srcFactor = wgpu::BlendFactor::One,
							  .dstFactor = wgpu::BlendFactor::Zero}};
		case BlendMode::Additive:
			return {.color = {.operation = wgpu::BlendOperation::Add,
							  .srcFactor = wgpu::BlendFactor::SrcAlpha,
							  .dstFactor = wgpu::BlendFactor::One},
					.alpha = {.operation = wgpu::BlendOperation::Add,
							  .srcFactor = wgpu::BlendFactor::Zero,
							  .dstFactor = wgpu::BlendFactor::One}};
		case BlendMode::Alpha:
			break;
	}
	return {.color = {.operation = wgpu::BlendOperation::Add,
					  .srcFactor = wgpu::BlendFactor::SrcAlpha,
					  .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha},
			.alpha = {.operation = wgpu::BlendOperation::Add,
					  .srcFactor = wgpu::BlendFactor::One,
					  .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha}};
}

auto toCull(const CullMode iMode) -> wgpu::CullMode {
	switch (iMode) {
		case CullMode::Back:
			return wgpu::CullMode::Back;
		case CullMode::Front:
			return wgpu::CullMode::Front;
		case CullMode::None:
			break;
	}
	return wgpu::CullMode::None;
}

}// namespace

void DrawData::init(const BufferLayout& iLayout, const std::string& iRenderer, std::vector<uint32_t>& iIndices,
					const std::string& iShaderName, const PipelineState& iState) {
	m_pipelineState = iState;
	mp_block = Context::get().activeBlock;
	setShader(iShaderName, iRenderer);
	if (iLayout.getStride() != 0) {
		mp_vertexBuffer = mkShared<VertexBuffer>(static_cast<uint32_t>(iLayout.getStride() * iIndices.size()));
		mp_vertexBuffer->setLayout(iLayout);
	}
	mp_indexBuffer = mkShared<IndexBuffer>(iIndices.data(), static_cast<uint32_t>(iIndices.size()));
}

void DrawData::initInstanced(const BufferLayout& iVertexLayout, const BufferLayout& iInstanceLayout,
							 const uint32_t iVertexCapacity, const uint32_t iInstanceCapacity,
							 const std::string& iRenderer, std::vector<uint32_t>& iIndices,
							 const std::string& iShaderName, const PipelineState& iState) {
	m_pipelineState = iState;
	mp_block = Context::get().activeBlock;
	setShader(iShaderName, iRenderer);
	mp_vertexBuffer = mkShared<VertexBuffer>(iVertexLayout.getStride() * iVertexCapacity);
	mp_vertexBuffer->setLayout(iVertexLayout);
	mp_instanceBuffer = mkShared<VertexBuffer>(iInstanceLayout.getStride() * iInstanceCapacity);
	mp_instanceBuffer->setLayout(iInstanceLayout);
	mp_indexBuffer = mkShared<IndexBuffer>(iIndices.data(), static_cast<uint32_t>(iIndices.size()));
}

void DrawData::setVertexData(const void* iData, const uint32_t iSize) {
	if (mp_vertexBuffer)
		mp_vertexBuffer->setData(iData, iSize);
}

void DrawData::setInstanceData(const void* iData, const uint32_t iSize) {
	if (mp_instanceBuffer)
		mp_instanceBuffer->setData(iData, iSize);
}

auto DrawData::getIndexCount() const -> uint32_t { return mp_indexBuffer ? mp_indexBuffer->getCount() : 0; }

void DrawData::setShader(const std::string& iShaderName, const std::string& iRenderer) {
	m_shaderBaseName = iShaderName;
	auto& library = Renderer::getShaderLibrary();
	const auto name = renderer::gpu::Shader::composeName({.name = iShaderName, .renderer = iRenderer});
	if (!library.exists(name))
		library.load(name);
	mp_shader = std::dynamic_pointer_cast<Shader>(library.get(name));
	m_variants.clear();
}

auto DrawData::getPipeline(const std::vector<wgpu::TextureFormat>& iFormats, const bool iDepth) const
		-> wgpu::RenderPipeline {
	for (const auto& variant: m_variants)
		if (variant.depth == iDepth && variant.formats == iFormats)
			return variant.pipeline;
	if (!mp_shader || mp_shader->getVertex() == nullptr || mp_shader->getFragment() == nullptr || mp_block == nullptr)
		return nullptr;
	std::vector<wgpu::VertexAttribute> vertexAttributes;
	std::vector<wgpu::VertexAttribute> instanceAttributes;
	std::vector<wgpu::VertexBufferLayout> layouts;
	if (mp_vertexBuffer) {
		addAttributes(mp_vertexBuffer->getLayout(), *mp_shader, vertexAttributes);
		if (mp_instanceBuffer) {
			instanceAttributes = vertexAttributes;
			addAttributes(mp_instanceBuffer->getLayout(), *mp_shader, instanceAttributes);
			instanceAttributes.erase(instanceAttributes.begin(),
									 instanceAttributes.begin() + static_cast<std::ptrdiff_t>(vertexAttributes.size()));
		}
		layouts.push_back({.stepMode = wgpu::VertexStepMode::Vertex,
						   .arrayStride = mp_vertexBuffer->getLayout().getStride(),
						   .attributeCount = vertexAttributes.size(),
						   .attributes = vertexAttributes.data()});
		if (mp_instanceBuffer)
			layouts.push_back({.stepMode = wgpu::VertexStepMode::Instance,
							   .arrayStride = mp_instanceBuffer->getLayout().getStride(),
							   .attributeCount = instanceAttributes.size(),
							   .attributes = instanceAttributes.data()});
	}
	const auto blend = toBlend(m_pipelineState.blendMode);
	std::vector<wgpu::ColorTargetState> targets;
	targets.reserve(iFormats.size());
	// An integer attachment (entity identifiers) is written as is: no blending.
	for (const auto format: iFormats)
		targets.push_back({.format = format, .blend = isBlendableFormat(format) ? &blend : nullptr});
	const wgpu::FragmentState fragment{.module = mp_shader->getFragment(),
									   .entryPoint = "fragmentMain",
									   .targetCount = targets.size(),
									   .targets = targets.data()};
	const wgpu::DepthStencilState depth{
			.format = Context::depthFormat,
			.depthWriteEnabled = m_pipelineState.depthTest && m_pipelineState.depthWrite ? wgpu::OptionalBool::True
																						 : wgpu::OptionalBool::False,
			.depthCompare = m_pipelineState.depthTest ? wgpu::CompareFunction::Less : wgpu::CompareFunction::Always};
	const auto label = mp_shader->getFullName();
	const wgpu::RenderPipelineDescriptor desc{
			.label = label.c_str(),
			.layout = mp_block->pipelineLayout,
			.vertex = {.module = mp_shader->getVertex(),
					   .entryPoint = "vertexMain",
					   .bufferCount = layouts.size(),
					   .buffers = layouts.data()},
			.primitive = {.topology = m_pipelineState.topology == PrimitiveTopology::Lines
											  ? wgpu::PrimitiveTopology::LineList
											  : wgpu::PrimitiveTopology::TriangleList,
						  .frontFace = wgpu::FrontFace::CCW,
						  .cullMode = toCull(m_pipelineState.cullMode)},
			.depthStencil = iDepth ? &depth : nullptr,
			.fragment = &fragment};
	auto pipeline = Context::get().device.CreateRenderPipeline(&desc);
	m_variants.push_back({.formats = iFormats, .depth = iDepth, .pipeline = pipeline});
	return pipeline;
}

}// namespace owl::renderer::gpu::webgpu
