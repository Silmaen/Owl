/**
 * @file utils.h
 * @author Silmaen
 * @date 10/24/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
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
	// NOLINTBEGIN(performance-no-int-to-ptr)
	auto* texture = reinterpret_cast<ImTextureID>(iTexture->getRendererId());
	// NOLINTEND(performance-no-int-to-ptr)
	if (texture == nullptr)
		return std::nullopt;
	return texture;
}

/**
 * @brief Convert Texture to imgui texture.
 * @param iFrameBufer The texture to convert.
 * @return The Texture id for ImGui.
 */
inline auto imTexture(const shared<renderer::Framebuffer>& iFrameBufer, uint32_t iIndex) -> std::optional<ImTextureID> {
	if (iFrameBufer == nullptr)
		return std::nullopt;
	// NOLINTBEGIN(performance-no-int-to-ptr)
	auto texture = reinterpret_cast<ImTextureID>(iFrameBufer->getColorAttachmentRendererId(iIndex));
	// NOLINTEND(performance-no-int-to-ptr)
	if (texture == nullptr)
		return std::nullopt;
	return texture;
}

}// namespace owl::gui
