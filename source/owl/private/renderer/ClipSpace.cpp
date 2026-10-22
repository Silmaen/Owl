/**
 * @file ClipSpace.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/ClipSpace.h"
#include "renderer/gpu/RenderCommand.h"

#include <cstddef>

namespace owl::renderer {

auto getActiveClipSpace() -> ClipSpace {
	return gpu::RenderCommand::getApi() == gpu::RenderAPI::Type::Vulkan ? ClipSpace::Vulkan : ClipSpace::OpenGl;
}

auto toClipSpace(const math::mat4& iProjection, const ClipSpace iClipSpace) -> math::mat4 {
	math::mat4 projection = iProjection;
	if (iClipSpace == ClipSpace::Vulkan) {
		for (size_t column = 0; column < 4; ++column) projection(1, column) = -projection(1, column);
	}
	return projection;
}

auto toActiveClipSpace(const math::mat4& iProjection) -> math::mat4 {
	return toClipSpace(iProjection, getActiveClipSpace());
}

auto cameraRelativeView(const math::mat4& iView) -> math::mat4 {
	math::mat4 view = iView;
	view(0, 3) = 0.f;
	view(1, 3) = 0.f;
	view(2, 3) = 0.f;
	return view;
}

}// namespace owl::renderer
