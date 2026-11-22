/**
 * @file Context.cpp
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Context.h"

#include "Buffer.h"
#include "DrawData.h"
#include "Framebuffer.h"

#include <array>
#include <vector>

namespace owl::renderer::gpu::webgpu {

namespace {

// Large enough for any uniform block or storage array a shader reads when a draw binds nothing there.
constexpr uint64_t g_emptyBufferSize = 65536;

auto toVisibility(const uint32_t iStages) -> wgpu::ShaderStage {
	wgpu::ShaderStage visibility = wgpu::ShaderStage::None;
	if ((iStages & ShaderStage::Vertex) != 0U)
		visibility |= wgpu::ShaderStage::Vertex;
	if ((iStages & ShaderStage::Fragment) != 0U)
		visibility |= wgpu::ShaderStage::Fragment;
	if ((iStages & ShaderStage::Compute) != 0U)
		visibility |= wgpu::ShaderStage::Compute;
	return visibility;
}

}// namespace

auto Context::get() -> Context& {
	static Context context;
	return context;
}

void Context::initDefaults() {
	emptyBuffer = makeBuffer(g_emptyBufferSize, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage, "Empty");
	const wgpu::TextureDescriptor desc{.label = "White",
									   .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
									   .size = {.width = 1, .height = 1, .depthOrArrayLayers = 1},
									   .format = wgpu::TextureFormat::RGBA8Unorm};
	whiteTexture = device.CreateTexture(&desc);
	constexpr uint32_t white = 0xffffffff;
	const wgpu::TexelCopyTextureInfo destination{.texture = whiteTexture};
	const wgpu::TexelCopyBufferLayout layout{.bytesPerRow = 4, .rowsPerImage = 1};
	const wgpu::Extent3D extent{.width = 1, .height = 1, .depthOrArrayLayers = 1};
	queue.WriteTexture(&destination, &white, sizeof(white), &layout, &extent);
	whiteView = whiteTexture.CreateView();
	const wgpu::SamplerDescriptor samplerDesc{.label = "Default"};
	defaultSampler = device.CreateSampler(&samplerDesc);
	textureViews.assign(g_textureSlots, whiteView);
	samplers.assign(g_textureSlots, defaultSampler);
	const wgpu::TextureDescriptor cubeDesc{.label = "White cube",
										   .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
										   .size = {.width = 1, .height = 1, .depthOrArrayLayers = 6},
										   .format = wgpu::TextureFormat::RGBA8Unorm};
	whiteCube = device.CreateTexture(&cubeDesc);
	const std::array<uint32_t, 6> whiteFaces{white, white, white, white, white, white};
	const wgpu::TexelCopyTextureInfo cubeDestination{.texture = whiteCube};
	const wgpu::Extent3D cubeExtent{.width = 1, .height = 1, .depthOrArrayLayers = 6};
	queue.WriteTexture(&cubeDestination, whiteFaces.data(), sizeof(whiteFaces), &layout, &cubeExtent);
	const wgpu::TextureViewDescriptor cubeViewDesc{.dimension = wgpu::TextureViewDimension::Cube};
	whiteCubeView = whiteCube.CreateView(&cubeViewDesc);
	cubeViews.assign(g_cubeSlots, whiteCubeView);
	const wgpu::SamplerDescriptor cubeSamplerDesc{.label = "Cubes",
												  .magFilter = wgpu::FilterMode::Linear,
												  .minFilter = wgpu::FilterMode::Linear,
												  .mipmapFilter = wgpu::MipmapFilterMode::Linear};
	cubeSampler = device.CreateSampler(&cubeSamplerDesc);
}

void Context::release() { *this = Context{}; }

void Context::configure(const uint32_t iWidth, const uint32_t iHeight) {
	if (surface == nullptr || iWidth == 0 || iHeight == 0 || (iWidth == width && iHeight == height))
		return;
	const wgpu::SurfaceConfiguration config{.device = device,
											.format = format,
											.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
											.width = iWidth,
											.height = iHeight,
											.alphaMode = wgpu::CompositeAlphaMode::Opaque,
											.presentMode = wgpu::PresentMode::Fifo};
	surface.Configure(&config);
	const wgpu::TextureDescriptor depthDesc{.label = "Canvas depth",
											.usage = wgpu::TextureUsage::RenderAttachment,
											.size = {.width = iWidth, .height = iHeight, .depthOrArrayLayers = 1},
											.format = depthFormat};
	depthTexture = device.CreateTexture(&depthDesc);
	depthView = depthTexture.CreateView();
	width = iWidth;
	height = iHeight;
}

void Context::declare(const std::string& iRenderer, const std::span<const BindingDecl> iBindings) {
	Block block;
	block.name = iRenderer;
	std::vector<wgpu::BindGroupLayoutEntry> bufferEntries;
	uint32_t textureStages = 0;
	bool textures2D = false;
	bool cubes = false;
	for (const auto& binding: iBindings) {
		if (binding.type == BindingType::CombinedImageSampler ||
			binding.type == BindingType::CombinedImageSamplerCube) {
			textureStages |= binding.stages;
			textures2D = textures2D || binding.type == BindingType::CombinedImageSampler;
			cubes = cubes || binding.type == BindingType::CombinedImageSamplerCube;
			continue;
		}
		block.buffers.push_back(binding);
		wgpu::BindGroupLayoutEntry entry{.binding = binding.binding, .visibility = toVisibility(binding.stages)};
		entry.buffer.type = binding.type == BindingType::UniformBuffer ? wgpu::BufferBindingType::Uniform
																	   : wgpu::BufferBindingType::ReadOnlyStorage;
		bufferEntries.push_back(entry);
	}
	const wgpu::BindGroupLayoutDescriptor bufferDesc{.label = "Buffers",
													 .entryCount = bufferEntries.size(),
													 .entries = bufferEntries.data()};
	block.bufferLayout = device.CreateBindGroupLayout(&bufferDesc);
	block.textures = cubes ? (textures2D ? Block::Textures::Pbr : Block::Textures::Cubes) : Block::Textures::Table;
	const auto textureVisibility = toVisibility(textureStages == 0 ? ShaderStage::Fragment : textureStages);
	std::vector<wgpu::BindGroupLayoutEntry> textureEntries;
	const auto addTexture = [&](const uint32_t iBinding, const wgpu::TextureSampleType iType,
								const wgpu::TextureViewDimension iDimension) -> void {
		wgpu::BindGroupLayoutEntry entry{.binding = iBinding, .visibility = textureVisibility};
		entry.texture.sampleType = iType;
		entry.texture.viewDimension = iDimension;
		textureEntries.push_back(entry);
	};
	const auto addSampler = [&](const uint32_t iBinding) -> void {
		wgpu::BindGroupLayoutEntry entry{.binding = iBinding, .visibility = textureVisibility};
		entry.sampler.type = wgpu::SamplerBindingType::Filtering;
		textureEntries.push_back(entry);
	};
	// The texture tables of the shaders: the textures, then their samplers.
	const uint32_t tableSlots = block.textures == Block::Textures::Table ? g_textureSlots
								: block.textures == Block::Textures::Pbr ? PbrTextures::g_tableSlots
																		 : 0;
	for (uint32_t slot = 0; slot < tableSlots; ++slot)
		addTexture(slot, wgpu::TextureSampleType::Float, wgpu::TextureViewDimension::e2D);
	for (uint32_t slot = 0; slot < tableSlots; ++slot) addSampler(g_textureSlots + slot);
	if (block.textures == Block::Textures::Pbr) {
		// The shadow maps are loaded texel by texel: no filtering, so no float32-filterable needed.
		addTexture(PbrTextures::g_cascadeBinding, wgpu::TextureSampleType::UnfilterableFloat,
				   wgpu::TextureViewDimension::e2DArray);
		addTexture(PbrTextures::g_localBinding, wgpu::TextureSampleType::UnfilterableFloat,
				   wgpu::TextureViewDimension::e2DArray);
		addTexture(PbrTextures::g_probeBinding, wgpu::TextureSampleType::Float, wgpu::TextureViewDimension::e2DArray);
		addSampler(PbrTextures::g_probeSamplerBinding);
		wgpu::BindGroupLayoutEntry entry{.binding = PbrTextures::g_cubeLayersBinding, .visibility = textureVisibility};
		entry.buffer.type = wgpu::BufferBindingType::Uniform;
		textureEntries.push_back(entry);
	}
	if (block.textures == Block::Textures::Cubes) {
		for (uint32_t slot = 0; slot < g_cubeSlots; ++slot) {
			wgpu::BindGroupLayoutEntry entry{.binding = g_firstCubeBinding + slot, .visibility = textureVisibility};
			entry.texture.sampleType = wgpu::TextureSampleType::Float;
			entry.texture.viewDimension = wgpu::TextureViewDimension::Cube;
			textureEntries.push_back(entry);
		}
		wgpu::BindGroupLayoutEntry entry{.binding = g_cubeSamplerBinding, .visibility = textureVisibility};
		entry.sampler.type = wgpu::SamplerBindingType::Filtering;
		textureEntries.push_back(entry);
	}
	const wgpu::BindGroupLayoutDescriptor textureDesc{.label = "Textures",
													  .entryCount = textureEntries.size(),
													  .entries = textureEntries.data()};
	block.textureLayout = device.CreateBindGroupLayout(&textureDesc);
	const std::array groups{block.bufferLayout, block.textureLayout};
	const wgpu::PipelineLayoutDescriptor layoutDesc{.label = "Pipeline",
													.bindGroupLayoutCount = groups.size(),
													.bindGroupLayouts = groups.data()};
	block.pipelineLayout = device.CreatePipelineLayout(&layoutDesc);
	// The draws keep a pointer to their block: a declared block is updated in place, never moved.
	auto [found, inserted] = blocks.try_emplace(iRenderer, std::move(block));
	if (!inserted)
		found->second = std::move(block);
}

void Context::bindBuffer(const std::string& iRenderer, const uint32_t iBinding, const wgpu::Buffer& iBuffer) {
	// A buffer belongs to the block of its renderer, as a Vulkan descriptor does: two blocks share binding numbers.
	const std::string& block = iRenderer.empty() && activeBlock != nullptr ? activeBlock->name : iRenderer;
	buffers[block][iBinding] = iBuffer;
}

auto Context::getTarget() const -> Target {
	if (framebuffer != nullptr)
		return framebuffer->getTarget();
	return {.colors = {frameView}, .formats = {format}, .depth = depthView};
}

void Context::clearView(const wgpu::TextureView& iView, const wgpu::Color& iColor, const bool iDepth) const {
	if (iView == nullptr)
		return;
	const wgpu::RenderPassColorAttachment color{.view = iView,
												.loadOp = wgpu::LoadOp::Clear,
												.storeOp = wgpu::StoreOp::Store,
												.clearValue = iColor};
	const wgpu::RenderPassDepthStencilAttachment depth{.view = iView,
													   .depthLoadOp = wgpu::LoadOp::Clear,
													   .depthStoreOp = wgpu::StoreOp::Store,
													   .depthClearValue = static_cast<float>(iColor.r)};
	const wgpu::RenderPassDescriptor pass{.label = "Clear",
										  .colorAttachmentCount = iDepth ? 0U : 1U,
										  .colorAttachments = iDepth ? nullptr : &color,
										  .depthStencilAttachment = iDepth ? &depth : nullptr};
	const wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
	encoder.BeginRenderPass(&pass).End();
	const wgpu::CommandBuffer commands = encoder.Finish();
	queue.Submit(1, &commands);
}

void Context::clear(const math::vec4& iColor) {
	const auto target = getTarget();
	const wgpu::Color color{.r = static_cast<double>(iColor.x()),
							.g = static_cast<double>(iColor.y()),
							.b = static_cast<double>(iColor.z()),
							.a = static_cast<double>(iColor.w())};
	// An integer attachment (entity identifiers) keeps its own clear: `Framebuffer::clearAttachment`.
	for (size_t i = 0; i < target.colors.size(); ++i)
		if (!isIntegerFormat(target.formats[i]))
			clearView(target.colors[i], color, false);
	clearView(target.depth, {.r = 1.0, .g = 0.0, .b = 0.0, .a = 0.0}, true);
}

void Context::draw(const DrawData& iData, const uint32_t iIndexCount, const uint32_t iInstanceCount,
				   const wgpu::Buffer& iIndirect, const uint64_t iIndirectOffset) {
	const auto target = getTarget();
	if (target.colors.empty() || target.colors.front() == nullptr)
		return;
	if (iIndirect == nullptr && (iIndexCount == 0 || iInstanceCount == 0))
		return;
	const Block* block = iData.getBlock();
	const auto pipeline = iData.getPipeline(target.formats, target.depth != nullptr);
	if (block == nullptr || pipeline == nullptr || !iData.getIndexBuffer())
		return;
	std::vector<wgpu::BindGroupEntry> bufferEntries;
	bufferEntries.reserve(block->buffers.size());
	const auto blockBuffers = buffers.find(block->name);
	for (const auto& binding: block->buffers) {
		wgpu::Buffer buffer = emptyBuffer;
		if (blockBuffers != buffers.end())
			if (const auto found = blockBuffers->second.find(binding.binding);
				found != blockBuffers->second.end() && found->second != nullptr)
				buffer = found->second;
		bufferEntries.push_back({.binding = binding.binding, .buffer = buffer});
	}
	const wgpu::BindGroupDescriptor bufferGroupDesc{.layout = block->bufferLayout,
													.entryCount = bufferEntries.size(),
													.entries = bufferEntries.data()};
	const wgpu::BindGroup bufferGroup = device.CreateBindGroup(&bufferGroupDesc);
	std::vector<wgpu::BindGroupEntry> textureEntries;
	textureEntries.reserve(size_t{2} * g_textureSlots + g_cubeSlots + 1);
	if (block->textures == Block::Textures::Table) {
		for (uint32_t slot = 0; slot < g_textureSlots; ++slot)
			textureEntries.push_back({.binding = slot, .textureView = textureViews[slot]});
		for (uint32_t slot = 0; slot < g_textureSlots; ++slot)
			textureEntries.push_back({.binding = g_textureSlots + slot, .sampler = samplers[slot]});
	}
	if (block->textures == Block::Textures::Pbr)
		pbr.appendEntries(boundTextures, boundCubes, frame, textureEntries);
	if (block->textures == Block::Textures::Cubes) {
		for (uint32_t slot = 0; slot < g_cubeSlots; ++slot)
			textureEntries.push_back({.binding = g_firstCubeBinding + slot, .textureView = cubeViews[slot]});
		textureEntries.push_back({.binding = g_cubeSamplerBinding, .sampler = cubeSampler});
	}
	const wgpu::BindGroupDescriptor textureGroupDesc{.layout = block->textureLayout,
													 .entryCount = textureEntries.size(),
													 .entries = textureEntries.data()};
	const wgpu::BindGroup textureGroup = device.CreateBindGroup(&textureGroupDesc);

	std::vector<wgpu::RenderPassColorAttachment> colors;
	colors.reserve(target.colors.size());
	for (const auto& view: target.colors)
		colors.push_back({.view = view, .loadOp = wgpu::LoadOp::Load, .storeOp = wgpu::StoreOp::Store});
	// The clear value is read even when unused: the browser refuses the NaN that marks it undefined.
	const wgpu::RenderPassDepthStencilAttachment depth{.view = target.depth,
													   .depthLoadOp = wgpu::LoadOp::Load,
													   .depthStoreOp = wgpu::StoreOp::Store,
													   .depthClearValue = 1.f};
	const wgpu::RenderPassDescriptor passDesc{.label = "Draw",
											  .colorAttachmentCount = colors.size(),
											  .colorAttachments = colors.data(),
											  .depthStencilAttachment = target.depth != nullptr ? &depth : nullptr};
	const wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
	const wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDesc);
	pass.SetPipeline(pipeline);
	pass.SetBindGroup(0, bufferGroup);
	pass.SetBindGroup(1, textureGroup);
	if (const auto& vertices = iData.getVertexBuffer(); vertices)
		pass.SetVertexBuffer(0, vertices->getBuffer());
	if (const auto& instances = iData.getInstanceBuffer(); instances)
		pass.SetVertexBuffer(1, instances->getBuffer());
	pass.SetIndexBuffer(iData.getIndexBuffer()->getBuffer(), wgpu::IndexFormat::Uint32);
	if (iIndirect != nullptr)
		pass.DrawIndexedIndirect(iIndirect, iIndirectOffset);
	else
		pass.DrawIndexed(iIndexCount, iInstanceCount);
	pass.End();
	const wgpu::CommandBuffer commands = encoder.Finish();
	queue.Submit(1, &commands);
}

auto isBlendableFormat(const wgpu::TextureFormat iFormat) -> bool {
	return !isIntegerFormat(iFormat) && iFormat != wgpu::TextureFormat::R32Float &&
		   iFormat != wgpu::TextureFormat::RG32Float && iFormat != wgpu::TextureFormat::RGBA32Float;
}

auto isIntegerFormat(const wgpu::TextureFormat iFormat) -> bool {
	return iFormat == wgpu::TextureFormat::R32Sint || iFormat == wgpu::TextureFormat::R32Uint;
}

}// namespace owl::renderer::gpu::webgpu
