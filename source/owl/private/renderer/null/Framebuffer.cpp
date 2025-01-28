/**
 * @file Framebuffer.cpp
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "Framebuffer.h"

namespace owl::renderer::null {

Framebuffer::Framebuffer(FramebufferSpecification iSpec) : m_specs{std::move(iSpec)} {}

Framebuffer::~Framebuffer() = default;

void Framebuffer::invalidate() {}

void Framebuffer::bind() {}

void Framebuffer::unbind() {}

void Framebuffer::resize(const math::vec2ui iSize) { m_specs.size = iSize; }

auto Framebuffer::readPixel(uint32_t, int, int) -> int { return 0; }

void Framebuffer::clearAttachment(uint32_t, int) {}

}// namespace owl::renderer::null
