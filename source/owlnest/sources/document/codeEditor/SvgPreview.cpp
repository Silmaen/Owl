/**
 * @file SvgPreview.cpp
 * @author Silmaen
 * @date 28/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SvgPreview.h"

#include "external/lunasvg_wrapper.h"

#include <core/Macros.h>
#include <gui/utils.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
OWL_DIAG_POP

namespace owl::nest::codeEditor {

SvgPreview::SvgPreview() = default;

SvgPreview::~SvgPreview() = default;

void SvgPreview::update(const core::Timestep& iTimeStep, const std::string& iText, const math::vec2ui iTargetSize) {
	const bool textChanged = iText != m_pendingText;
	const bool sizeChanged = iTargetSize != m_pendingSize;
	if (textChanged || sizeChanged) {
		m_pendingText = iText;
		m_pendingSize = iTargetSize;
		m_debounceLeft = kDebounceSeconds;
		m_dirty = true;
		return;
	}
	if (!m_dirty)
		return;
	m_debounceLeft -= iTimeStep.getSeconds();
	if (m_debounceLeft > 0.0f)
		return;
	rasterize();
	m_dirty = false;
}

void SvgPreview::rasterize() {
	if (m_pendingText.empty() || m_pendingSize.surface() == 0) {
		m_lastParseOk = m_pendingText.empty();
		m_texture.reset();
		m_textureSize = {0, 0};
		m_renderedText.clear();
		return;
	}
	const auto doc = lunasvg::Document::loadFromData(m_pendingText);
	if (!doc) {
		m_lastParseOk = false;
		return;
	}
	const uint32_t side = std::min(std::min(m_pendingSize.x(), m_pendingSize.y()), kMaxSide);
	if (side == 0) {
		m_lastParseOk = false;
		return;
	}
	const auto bitmap = doc->renderToBitmap(static_cast<int>(side), static_cast<int>(side));
	if (bitmap.isNull()) {
		m_lastParseOk = false;
		return;
	}
	// lunasvg outputs ARGB premultiplied bytes (memory order: B, G, R, A on
	// little-endian). Convert to RGBA straight alpha; do NOT flip vertically
	// (we use the default ImGui::Image uv mapping, matching the convention
	// used by `Texture2D::createFromSerialized` for PNG/JPG textures).
	const auto pixelCount = static_cast<size_t>(side) * side;
	std::vector<uint8_t> pixels(pixelCount * 4);
	const auto* src = bitmap.data();
	for (size_t i = 0; i < pixelCount; ++i) {
		const size_t idx = i * 4;
		const uint8_t a = src[idx + 3];
		if (a == 0) {
			pixels[idx + 0] = 0;
			pixels[idx + 1] = 0;
			pixels[idx + 2] = 0;
			pixels[idx + 3] = 0;
		} else {
			pixels[idx + 0] = static_cast<uint8_t>(static_cast<uint16_t>(src[idx + 2]) * 255 / a);
			pixels[idx + 1] = static_cast<uint8_t>(static_cast<uint16_t>(src[idx + 1]) * 255 / a);
			pixels[idx + 2] = static_cast<uint8_t>(static_cast<uint16_t>(src[idx + 0]) * 255 / a);
			pixels[idx + 3] = a;
		}
	}
	const math::vec2ui newSize{side, side};
	if (!m_texture || m_textureSize != newSize) {
		const renderer::gpu::Texture::Specification specs{.size = newSize,
													 .format = renderer::gpu::ImageFormat::Rgba8,
													 .generateMips = false};
		m_texture = renderer::gpu::Texture2D::create(specs);
		m_textureSize = newSize;
	}
	m_texture->setData(pixels.data(), static_cast<uint32_t>(pixels.size()));
	m_lastParseOk = true;
	m_renderedText = m_pendingText;
}

void SvgPreview::render(const ImVec2& iSize) const {
	ImGui::BeginChild("##svg_preview", iSize, ImGuiChildFlags_Borders);
	if (!m_lastParseOk) {
		ImGui::TextColored(ImVec4{0.95f, 0.4f, 0.4f, 1.f}, "Failed to parse SVG");
	} else if (auto im = gui::imTexture(m_texture); im.has_value()) {
		// Centre the image inside the available area, preserving aspect ratio.
		const auto avail = ImGui::GetContentRegionAvail();
		const float side = std::min(avail.x, avail.y);
		const auto cursor = ImGui::GetCursorPos();
		ImGui::SetCursorPos({cursor.x + (avail.x - side) * 0.5f, cursor.y + (avail.y - side) * 0.5f});
		ImGui::Image(im.value(), ImVec2{side, side});
	} else {
		ImGui::TextDisabled("(empty)");
	}
	ImGui::EndChild();
}

}// namespace owl::nest::codeEditor
