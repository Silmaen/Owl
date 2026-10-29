/**
 * @file Framebuffer.cpp
 * @author Silmaen
 * @date 21/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "null/Framebuffer.h"
#if OWL_WITH_RENDER
#include "opengl/Framebuffer.h"
#include "vulkan/Framebuffer.h"
#endif
#include "renderer/Renderer.h"
#include "renderer/gpu/Framebuffer.h"
#include "renderer/gpu/RenderAPI.h"
#include "renderer/gpu/Texture.h"

#include <cstdint>

namespace owl::renderer::gpu {

auto Framebuffer::create(const FramebufferSpecification& iSpec) -> shared<Framebuffer> {
	const auto api = RenderCommand::getApi();
	switch (api) {
		case RenderAPI::Type::Null:
			return mkShared<null::Framebuffer>(iSpec);
#if OWL_WITH_RENDER
		case RenderAPI::Type::OpenGL:
			return mkShared<opengl::Framebuffer>(iSpec);
		case RenderAPI::Type::Vulkan:
			return mkShared<vulkan::Framebuffer>(iSpec);
#else
		case RenderAPI::Type::OpenGL:
		case RenderAPI::Type::Vulkan:
			break;// GPU backends not built (OWL_MODULE_RENDER=OFF)
#endif
	}
	OWL_CORE_ERROR("Unknown RendererAPI ({}).", static_cast<int>(api))
	return nullptr;
}

Framebuffer* Framebuffer::s_bound = nullptr;

Framebuffer::~Framebuffer() {
	if (s_bound == this)
		s_bound = nullptr;
}

auto Framebuffer::isLinearColor() const -> bool {
	const auto& attachments = getSpecification().attachments;
	return !attachments.empty() && attachments.front().format == AttachmentSpecification::Format::Rgba16F;
}

auto Framebuffer::canCopyToCube(const uint32_t iIndex, const TextureCube& iTarget, const uint32_t iFace,
								const uint32_t iLevel) const -> bool {
	const auto& specs = getSpecification();
	if (iIndex >= specs.attachments.size() || iFace >= TextureCube::g_faceCount || iLevel >= iTarget.getLevelCount())
		return false;
	const uint32_t face = iTarget.getFaceSize(iLevel);
	if (specs.size != math::vec2ui{face, face})
		return false;
	const auto cubeFormat = iTarget.getSpecification().format;
	switch (specs.attachments[iIndex].format) {
		case AttachmentSpecification::Format::RedFloat:
			return cubeFormat == ImageFormat::R32F;
		case AttachmentSpecification::Format::Rgba16F:
			return cubeFormat == ImageFormat::Rgba16F;
		case AttachmentSpecification::Format::Rgba8:
			return cubeFormat == ImageFormat::Rgba8;
		case AttachmentSpecification::Format::None:
		case AttachmentSpecification::Format::RedInteger:
		case AttachmentSpecification::Format::Depth32F:
		case AttachmentSpecification::Format::Surface:
			return false;
	}
	return false;
}

auto Framebuffer::copyAttachmentToCube(const uint32_t iIndex, TextureCube& ioTarget, const uint32_t iFace,
									   const uint32_t iLevel) -> bool {
	if (!canCopyToCube(iIndex, ioTarget, iFace, iLevel)) {
		OWL_CORE_WARN("Framebuffer ({}): Cannot copy attachment {} into face {} of level {} of the cube.",
					  getSpecification().debugName, iIndex, iFace, iLevel)
		return false;
	}
	return true;
}

}// namespace owl::renderer::gpu
