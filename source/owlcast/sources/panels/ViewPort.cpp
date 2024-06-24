/**
 * @file ViewPort.cpp
 * @author Silmaen
 * @date 23/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ViewPort.h"

namespace owl::raycaster::panel {

ViewPort::ViewPort() : BaseDrawPanel{"viewport"} {
	const renderer::FramebufferSpecification specs{
			.size = {1280, 720},
			.attachments = {{.format=renderer::AttachmentSpecification::Format::Surface,
							 .tiling=renderer::AttachmentSpecification::Tiling::Optimal},
							{.format=renderer::AttachmentSpecification::Format::RedInteger,
							 .tiling=renderer::AttachmentSpecification::Tiling::Optimal}},
			.samples = 1,
			.swapChainTarget = false,
			.debugName = "viewport"};
	mp_framebuffer = renderer::Framebuffer::create(specs);
	// camera
	m_camera = mkShared<renderer::CameraOrtho>(0, 1280, 0, 720);
}

ViewPort::~ViewPort() = default;

void ViewPort::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) { OWL_PROFILE_FUNCTION() }

}// namespace owl::raycaster::panel
