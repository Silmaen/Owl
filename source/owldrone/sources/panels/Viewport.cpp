/**
 * @file Viewport.cpp
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "Viewport.h"

#include "IO/CameraSystem.h"

using namespace owl;

namespace drone::panels {

constexpr ImVec2 vec(math::vec2 iVec) { return {iVec.x(), iVec.y()}; }

Viewport::Viewport() {
	const renderer::FramebufferSpecification specs{
			.size = {1280, 720},
			.attachments =
					{
							{renderer::AttachmentSpecification::Format::Surface,
							 renderer::AttachmentSpecification::Tiling::Optimal},
							{renderer::AttachmentSpecification::Format::RedInteger,
							 renderer::AttachmentSpecification::Tiling::Optimal},
							//{renderer::AttachmentSpecification::Format::Depth24Stencil8,
							//renderer::AttachmentSpecification::Tiling::Optimal}
					},
			.samples = 1,
			.swapChainTarget = false,
			.debugName = "viewport"};
	mp_framebuffer = renderer::Framebuffer::create(specs);
	// camera
	m_camera = mkShared<renderer::CameraOrtho>(0, 1280, 0, 720);
}

Viewport::~Viewport() = default;

void Viewport::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	auto& cam = IO::CameraSystem::get();
	cam.onUpdate(iTimeStep);

	const auto spec = mp_framebuffer->getSpecification();

	const float aspectRatio = m_viewportSize.ratio();
	const float scaling = std::min(aspectRatio, 2.f);

	if (m_viewportSize.surface() > 0 && m_viewportSize != spec.size) {
		mp_framebuffer->resize(m_viewportSize);
		m_camera->setProjection(-aspectRatio, aspectRatio, -1, 1);
	}
	// Render
	mp_framebuffer->bind();
	renderer::RenderCommand::setClearColor({0.2f, 0.2f, 0.2f, 1});
	renderer::RenderCommand::clear();

	// Clear our entity ID attachment to -1
	mp_framebuffer->clearAttachment(1, -1);

	// Do the drawings!
	// ===============================================================
	renderer::Renderer2D::beginScene(*m_camera);
	const float ratio = cam.getFrame()->getSize().ratio();
	const renderer::utils::PRS tran{.position = {0, 0, 0}, .rotation = 180, .size = {ratio * scaling, 1 * scaling}};
	renderer::Renderer2D::drawQuad({.transform = tran, .texture = cam.getFrame()});
	renderer::Renderer2D::endScene();
	// ===============================================================
	// free the frame buffer.
	mp_framebuffer->unbind();
}

// NOLINTBEGIN(performance-no-int-to-ptr)
void Viewport::onRender() {
	OWL_PROFILE_FUNCTION()

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	ImGui::Begin("Drone Viewport");
	const auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
	const auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
	const auto viewportOffset = ImGui::GetWindowPos();
	m_viewportLower = {viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y};
	m_viewportUpper = {viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y};

	m_viewportFocused = ImGui::IsWindowFocused();
	m_viewportHovered = ImGui::IsWindowHovered();
	core::Application::get().getImGuiLayer()->blockEvents(!m_viewportFocused && !m_viewportHovered);

	const ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	m_viewportSize = {static_cast<uint32_t>(viewportPanelSize.x), static_cast<uint32_t>(viewportPanelSize.y)};
	if (const uint64_t textureId = mp_framebuffer->getColorAttachmentRendererId(0); textureId != 0)
		ImGui::Image(reinterpret_cast<void*>(textureId), viewportPanelSize, vec(mp_framebuffer->getLowerData()),
					 vec(mp_framebuffer->getUpperData()));

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const auto* const path = static_cast<const char*>(payload->Data);
			const std::filesystem::path scenePath = core::Application::get().getAssetDirectory() / path;
			OWL_CORE_WARN("Could not load file {}: unsupported format.", scenePath.string())
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::End();
	ImGui::PopStyleVar();
}
// NOLINTEND(performance-no-int-to-ptr)

}// namespace drone::panels
