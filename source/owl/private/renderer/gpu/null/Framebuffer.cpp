/**
 * @file Framebuffer.cpp
 * @author Silmaen
 * @date 30/07/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "Framebuffer.h"

namespace owl::renderer::gpu::null {

Framebuffer::Framebuffer(FramebufferSpecification iSpec) : m_specs{std::move(iSpec)} {}

Framebuffer::~Framebuffer() = default;

void Framebuffer::invalidate() {}

void Framebuffer::bind() { setBound(this); }

void Framebuffer::unbind() { setBound(nullptr); }

void Framebuffer::resize(const math::vec2ui iSize) { m_specs.size = iSize; }

auto Framebuffer::readPixel(uint32_t, int, int) -> int { return 0; }

void Framebuffer::clearAttachment(uint32_t, int) {}

auto Framebuffer::readColorAttachment(const uint32_t iAttachmentIndex) -> std::vector<uint8_t> {
	using Format = AttachmentSpecification::Format;
	if (iAttachmentIndex >= m_specs.attachments.size()) {
		OWL_CORE_WARN("Null Framebuffer: No attachment {} to read back.", iAttachmentIndex)
		return {};
	}
	if (const auto format = m_specs.attachments[iAttachmentIndex].format;
		format != Format::Surface && format != Format::Rgba8) {
		OWL_CORE_WARN("Null Framebuffer: Attachment {} is not an 8-bit colour attachment.", iAttachmentIndex)
		return {};
	}
	return std::vector<uint8_t>(static_cast<size_t>(m_specs.size.surface()) * 4, 0);
}

}// namespace owl::renderer::gpu::null
