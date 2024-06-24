/**
 * @file MapWindow.cpp
 * @author Silmaen
 * @date 23/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "MapWindow.h"

namespace owl::raycaster::panel {

MapWindow::MapWindow() : BaseDrawPanel{"mapWindow"} {
	const renderer::FramebufferSpecification specs{
			.size = {1280, 720},
			.attachments = {{.format=renderer::AttachmentSpecification::Format::Surface,
							 .tiling=renderer::AttachmentSpecification::Tiling::Optimal},
							{.format=renderer::AttachmentSpecification::Format::RedInteger,
							 .tiling=renderer::AttachmentSpecification::Tiling::Optimal}},
			.samples = 1,
			.swapChainTarget = false,
			.debugName = "mapWindow"};
	mp_framebuffer = renderer::Framebuffer::create(specs);
	// camera
	m_camera = mkShared<renderer::CameraOrtho>(0, 1280, 0, 720);
}

MapWindow::~MapWindow() = default;

void MapWindow::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) { OWL_PROFILE_FUNCTION() }

}// namespace owl::raycaster::panel
