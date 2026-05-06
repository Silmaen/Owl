/**
 * @file utils.h
 * @author Silmaen
 * @date 10/24/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "renderer/gpu/Framebuffer.h"
#include "renderer/gpu/Texture.h"
#include <imgui.h>

namespace owl::gui {
/**
 * @brief
 *  Convert Texture to imgui texture.
 * @param iTexture The texture to convert.
 * @return The Texture id for ImGui.
 */
inline auto imTexture(const shared<renderer::gpu::Texture>& iTexture) -> std::optional<ImTextureID> {
	if (iTexture == nullptr)
		return std::nullopt;
	auto texture = static_cast<ImTextureID>(iTexture->getRendererId());
	if (texture == 0)
		return std::nullopt;
	return texture;
}

/**
 * @brief
 *  Convert Texture to imgui texture.
 * @param iFrameBuffer The texture to convert.
 * @param iIndex The attachment index.
 * @return The Texture id for ImGui.
 */
inline auto imTexture(const shared<renderer::gpu::Framebuffer>& iFrameBuffer, const uint32_t iIndex)
		-> std::optional<ImTextureID> {
	if (iFrameBuffer == nullptr)
		return std::nullopt;
	auto texture = static_cast<ImTextureID>(iFrameBuffer->getColorAttachmentRendererId(iIndex));
	if (texture == 0)
		return std::nullopt;
	return texture;
}

/**
 * @brief
 *  Convert an owl `math::vec2` to ImGui's `ImVec2`.
 * @param[in] iVec The 2D vector to convert.
 * @return The matching `ImVec2`.
 */
constexpr auto vec(const math::vec2& iVec) -> ImVec2 { return {iVec.x(), iVec.y()}; }

/**
 * @brief
 *  Convert an unsigned 2D vector to ImGui's `ImVec2` (cast to float).
 * @param[in] iVec The unsigned 2D vector to convert.
 * @return The matching `ImVec2`.
 */
constexpr auto vec(const math::vec2ui& iVec) -> ImVec2 {
	return {static_cast<float>(iVec.x()), static_cast<float>(iVec.y())};
}

/**
 * @brief
 *  Convert an owl `math::vec4` to ImGui's `ImVec4`.
 * @param[in] iVec The 4D vector to convert.
 * @return The matching `ImVec4`.
 */
constexpr auto vec(const math::vec4& iVec) -> ImVec4 { return {iVec.x(), iVec.y(), iVec.z(), iVec.w()}; }

}// namespace owl::gui
