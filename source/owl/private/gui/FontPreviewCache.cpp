/**
 * @file FontPreviewCache.cpp
 * @author Silmaen
 * @date 26/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/FontPreviewCache.h"

#include "math/matrices.h"
#include "math/vectors.h"
#include "renderer/CameraOrtho.h"
#include "renderer/Renderer2D.h"
#include "renderer/gpu/RenderCommand.h"

namespace owl::gui {

namespace {
/**
 * Sample string covering lower/upper case, digits, punctuation, and accented Latin-1 codepoints.
 * Stored as Latin-1 single-byte sequence so each glyph maps 1:1 to a font atlas codepoint
 * (the MSDF atlas covers 0x20-0xFF). The accented bytes are e9 (small e acute), e0 (small a
 * grave), fc (small u diaeresis), and c7 (capital C cedilla).
 */
constexpr const char* kSample = "Aa Bb 1!? \xE9\xE0\xFC\xC7";

constexpr math::vec2ui kPreviewSize{256u, 64u};

auto makePreviewFramebuffer() -> shared<renderer::gpu::Framebuffer> {
	const renderer::gpu::FramebufferSpecification specs{
			.size = kPreviewSize,
			.attachments = {{.format = renderer::gpu::AttachmentSpecification::Format::Surface,
							 .tiling = renderer::gpu::AttachmentSpecification::Tiling::Optimal}},
			.samples = 1,
			.swapChainTarget = false,
			.debugName = "fontPreview"};
	return renderer::gpu::Framebuffer::create(specs);
}

void renderPreviewInto(renderer::gpu::Framebuffer& ioFramebuffer, const shared<data::fonts::Font>& iFont) {
	ioFramebuffer.bind();
	renderer::gpu::RenderCommand::setClearColor({0.f, 0.f, 0.f, 0.f});
	renderer::gpu::RenderCommand::clear();
	// Camera maps [-1,1] to the full framebuffer; drawString fits text into a unit quad,
	// so a transform with x-scale matching the aspect ratio fills horizontally with margin.
	const renderer::CameraOrtho camera{-1.f, 1.f, -1.f, 1.f};
	renderer::Renderer2D::beginScene(camera);
	renderer::StringData drawData;
	const auto aspect = static_cast<float>(kPreviewSize.x()) / static_cast<float>(std::max(kPreviewSize.y(), 1u));
	// Text fits into a unit-square quad locally; scale it so it spans most of the framebuffer.
	const math::Transform xform{math::vec3{0.f, 0.f, 0.f}, math::vec3{0.f, 0.f, 0.f},
								math::vec3{0.9f * aspect, 0.9f, 1.f}};
	drawData.transform = xform;
	drawData.text = kSample;
	drawData.font = iFont;
	drawData.color = math::vec4{1.f, 1.f, 1.f, 1.f};
	renderer::Renderer2D::drawString(drawData);
	renderer::Renderer2D::endScene();
	ioFramebuffer.unbind();
}

}// namespace

auto FontPreviewCache::get() -> FontPreviewCache& {
	static FontPreviewCache s_instance;
	return s_instance;
}

auto FontPreviewCache::request(const shared<data::fonts::Font>& iFont) -> shared<renderer::gpu::Framebuffer> {
	if (iFont == nullptr)
		return nullptr;
	const std::string& key = iFont->getName();
	auto it = m_cache.find(key);
	if (it == m_cache.end()) {
		Entry entry;
		entry.fb = makePreviewFramebuffer();
		m_cache.try_emplace(key, std::move(entry));
		m_pending.push_back(iFont);
		return nullptr;
	}
	return it->second.ready ? it->second.fb : nullptr;
}

void FontPreviewCache::pumpPending() {
	if (m_pending.empty())
		return;
	for (const auto& font: m_pending) {
		auto it = m_cache.find(font->getName());
		if (it == m_cache.end() || !it->second.fb)
			continue;
		renderPreviewInto(*it->second.fb, font);
		it->second.ready = true;
	}
	m_pending.clear();
}

void FontPreviewCache::clear() {
	m_cache.clear();
	m_pending.clear();
}

}// namespace owl::gui
