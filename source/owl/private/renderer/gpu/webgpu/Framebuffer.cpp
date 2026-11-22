/**
 * @file Framebuffer.cpp
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Framebuffer.h"

#include "Texture.h"

#include <utility>

namespace owl::renderer::gpu::webgpu {

namespace {

using Format = AttachmentSpecification::Format;

auto toFormat(const Format iFormat) -> wgpu::TextureFormat {
	switch (iFormat) {
		case Format::Rgba8:
			return wgpu::TextureFormat::RGBA8Unorm;
		case Format::RedInteger:
			return wgpu::TextureFormat::R32Sint;
		case Format::Depth32F:
			return wgpu::TextureFormat::Depth32Float;
		case Format::Rgba16F:
			return wgpu::TextureFormat::RGBA16Float;
		case Format::RedFloat:
			return wgpu::TextureFormat::R32Float;
		case Format::Surface:
			return Context::get().format;
		case Format::None:
			break;
	}
	return wgpu::TextureFormat::Undefined;
}

// The texture format an attachment is sampled as (`getAttachmentTexture`).
auto toImageFormat(const Format iFormat) -> ImageFormat {
	switch (iFormat) {
		case Format::Rgba16F:
			return ImageFormat::Rgba16F;
		case Format::RedFloat:
		case Format::Depth32F:
			return ImageFormat::R32F;
		case Format::Rgba8:
		case Format::RedInteger:
		case Format::Surface:
		case Format::None:
			break;
	}
	return ImageFormat::Rgba8;
}

}// namespace

Framebuffer::Framebuffer(FramebufferSpecification iSpec) : m_spec{std::move(iSpec)} { create(); }

Framebuffer::~Framebuffer() {
	auto& context = Context::get();
	if (context.framebuffer == this)
		context.framebuffer = nullptr;
}

void Framebuffer::create() {
	auto& context = Context::get();
	m_textures.clear();
	m_views.clear();
	m_formats.clear();
	m_wrappers.clear();
	// The swapchain framebuffer draws on the canvas; a `Surface` attachment elsewhere is a texture of its format.
	m_surface = m_spec.swapChainTarget;
	if (m_spec.size.surface() == 0)
		return;
	for (const auto& attachment: m_spec.attachments) {
		if (m_surface && attachment.format == Format::Surface) {
			m_textures.emplace_back();
			m_views.emplace_back();
			m_formats.push_back(context.format);
			continue;
		}
		const auto format = toFormat(attachment.format);
		const bool depth = attachment.format == Format::Depth32F;
		const wgpu::TextureDescriptor desc{
				.label = m_spec.debugName.c_str(),
				.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding |
						 wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst,
				.size = {.width = m_spec.size.x(), .height = m_spec.size.y(), .depthOrArrayLayers = 1},
				.format = format};
		auto texture = context.device.CreateTexture(&desc);
		m_views.push_back(texture.CreateView());
		m_textures.push_back(std::move(texture));
		m_formats.push_back(depth ? wgpu::TextureFormat::Undefined : format);
	}
	m_wrappers.resize(m_textures.size());
}

auto Framebuffer::getTarget() const -> Target {
	auto& context = Context::get();
	Target target;
	for (size_t i = 0; i < m_spec.attachments.size() && i < m_views.size(); ++i) {
		const auto format = m_spec.attachments[i].format;
		if (format == Format::Depth32F) {
			target.depth = m_views[i];
			continue;
		}
		target.colors.push_back(m_surface && format == Format::Surface ? context.frameView : m_views[i]);
		target.formats.push_back(m_formats[i]);
	}
	// The canvas brings its own depth when the framebuffer has none.
	if (m_surface && target.depth == nullptr)
		target.depth = context.depthView;
	return target;
}

void Framebuffer::bind() {
	auto& context = Context::get();
	context.framebuffer = m_surface ? nullptr : this;
	setBound(m_surface ? nullptr : this);
}

void Framebuffer::unbind() {
	Context::get().framebuffer = nullptr;
	setBound(nullptr);
}

void Framebuffer::resize(const math::vec2ui iSize) {
	if (iSize == m_spec.size || iSize.surface() == 0)
		return;
	m_spec.size = iSize;
	const bool bound = Context::get().framebuffer == this;
	create();
	if (bound)
		Context::get().framebuffer = this;
}

auto Framebuffer::readPixel([[maybe_unused]] const uint32_t iAttachmentIndex, [[maybe_unused]] const int iX,
							[[maybe_unused]] const int iY) -> int {
	return -1;
}

void Framebuffer::clearAttachment(const uint32_t iAttachmentIndex, const int iValue) {
	if (iAttachmentIndex >= m_views.size() || m_spec.attachments[iAttachmentIndex].format != Format::RedInteger)
		return;
	Context::get().clearView(m_views[iAttachmentIndex],
							 {.r = static_cast<double>(iValue), .g = 0.0, .b = 0.0, .a = 0.0}, false);
}

void Framebuffer::clearAttachment(const uint32_t iAttachmentIndex, const math::vec4 iColorValue) {
	if (iAttachmentIndex >= m_views.size())
		return;
	const auto format = m_spec.attachments[iAttachmentIndex].format;
	if (format == Format::RedInteger || format == Format::Depth32F)
		return;
	const auto& view = m_surface && format == Format::Surface ? Context::get().frameView : m_views[iAttachmentIndex];
	Context::get().clearView(view,
							 {.r = static_cast<double>(iColorValue.x()),
							  .g = static_cast<double>(iColorValue.y()),
							  .b = static_cast<double>(iColorValue.z()),
							  .a = static_cast<double>(iColorValue.w())},
							 false);
}

auto Framebuffer::getColorAttachmentRendererId(const uint32_t iIndex) const -> uint64_t {
	if (iIndex >= m_wrappers.size() || !m_wrappers[iIndex])
		return 0;
	return m_wrappers[iIndex]->getRendererId();
}

auto Framebuffer::getAttachmentTexture(const uint32_t iIndex) -> shared<renderer::gpu::Texture2D> {
	if (iIndex >= m_textures.size() || m_textures[iIndex] == nullptr)
		return nullptr;
	if (!m_wrappers[iIndex]) {
		const auto format = m_spec.attachments[iIndex].format;
		m_wrappers[iIndex] = mkShared<Texture2D>(
				m_textures[iIndex], renderer::gpu::Texture::Specification{.size = m_spec.size,
																		  .format = toImageFormat(format),
																		  .filterMode = FilterMode::Nearest});
	}
	return m_wrappers[iIndex];
}

void Framebuffer::copyAttachment(const uint32_t iIndex, renderer::gpu::Framebuffer& ioTarget) {
	auto* target = dynamic_cast<Framebuffer*>(&ioTarget);
	if (target == nullptr || iIndex >= m_textures.size() || iIndex >= target->m_textures.size() ||
		m_textures[iIndex] == nullptr || target->m_textures[iIndex] == nullptr || target->m_spec.size != m_spec.size)
		return;
	auto& context = Context::get();
	const wgpu::TexelCopyTextureInfo source{.texture = m_textures[iIndex]};
	const wgpu::TexelCopyTextureInfo destination{.texture = target->m_textures[iIndex]};
	const wgpu::Extent3D extent{.width = m_spec.size.x(), .height = m_spec.size.y(), .depthOrArrayLayers = 1};
	const wgpu::CommandEncoder encoder = context.device.CreateCommandEncoder();
	encoder.CopyTextureToTexture(&source, &destination, &extent);
	const wgpu::CommandBuffer commands = encoder.Finish();
	context.queue.Submit(1, &commands);
}

auto Framebuffer::copyAttachmentToCube(const uint32_t iIndex, renderer::gpu::TextureCube& ioTarget,
									   const uint32_t iFace, const uint32_t iLevel) -> bool {
	auto* cube = dynamic_cast<TextureCube*>(&ioTarget);
	if (m_surface || cube == nullptr || cube->getTexture() == nullptr || iIndex >= m_textures.size() ||
		m_textures[iIndex] == nullptr ||
		!renderer::gpu::Framebuffer::copyAttachmentToCube(iIndex, ioTarget, iFace, iLevel))
		return false;
	auto& context = Context::get();
	const wgpu::TexelCopyTextureInfo source{.texture = m_textures[iIndex]};
	const wgpu::TexelCopyTextureInfo destination{.texture = cube->getTexture(),
												 .mipLevel = iLevel,
												 .origin = {.x = 0, .y = 0, .z = iFace}};
	const wgpu::Extent3D extent{.width = m_spec.size.x(), .height = m_spec.size.y(), .depthOrArrayLayers = 1};
	const wgpu::CommandEncoder encoder = context.device.CreateCommandEncoder();
	encoder.CopyTextureToTexture(&source, &destination, &extent);
	const wgpu::CommandBuffer commands = encoder.Finish();
	context.queue.Submit(1, &commands);
	return true;
}

}// namespace owl::renderer::gpu::webgpu
