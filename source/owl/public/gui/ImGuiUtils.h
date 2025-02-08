/**
 * @file ImGuiUtils.h
 * @author Silmaen
 * @date 10/24/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "renderer/Framebuffer.h"
#include "renderer/Texture.h"
#include <imgui.h>

namespace owl::gui {

/**
 * @brief Convert Texture to imgui texture.
 * @param iTexture The texture to convert.
 * @return The Texture id for ImGui.
 */
inline auto imTexture(const shared<renderer::Texture>& iTexture) -> std::optional<ImTextureID> {
	if (iTexture == nullptr)
		return std::nullopt;
	auto texture = static_cast<ImTextureID>(iTexture->getRendererId());
	if (texture == 0)
		return std::nullopt;
	return texture;
}

/**
 * @brief Convert Texture to imgui texture.
 * @param iFrameBuffer The texture to convert.
 * @param iIndex The attachment index.
 * @return The Texture id for ImGui.
 */
inline auto imTexture(const shared<renderer::Framebuffer>& iFrameBuffer, const uint32_t iIndex)
		-> std::optional<ImTextureID> {
	if (iFrameBuffer == nullptr)
		return std::nullopt;
	auto texture = static_cast<ImTextureID>(iFrameBuffer->getColorAttachmentRendererId(iIndex));
	if (texture == 0)
		return std::nullopt;
	return texture;
}

constexpr auto vec(const math::vec2& iVec) -> ImVec2 { return {iVec.x(), iVec.y()}; }
constexpr auto vec(const math::vec2ui& iVec) -> ImVec2 {
	return {static_cast<float>(iVec.x()), static_cast<float>(iVec.y())};
}
constexpr auto vec(const math::vec4& iVec) -> ImVec4 { return {iVec.x(), iVec.y(), iVec.z(), iVec.w()}; }

}// namespace owl::gui
