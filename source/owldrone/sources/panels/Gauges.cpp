/**
 * @file Gauges.cpp
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "Gauges.h"

#include "gauge/AirSpeed.h"
#include "gauge/Altitude.h"
#include "gauge/ArtificialHorizon.h"
#include "gauge/Compas.h"
#include "gauge/MotorRate.h"
#include "gauge/VerticalSpeed.h"

using namespace owl;

namespace drone::panels {

namespace {
constexpr auto vec(math::vec2 iVec) -> ImVec2 { return {iVec.x(), iVec.y()}; }
}// namespace

Gauges::Gauges() {

	const renderer::FramebufferSpecification specs{
			.size = {1280, 720},
			.attachments =
					{
							{.format = renderer::AttachmentSpecification::Format::Surface,
							 .tiling = renderer::AttachmentSpecification::Tiling::Optimal},
							{.format = renderer::AttachmentSpecification::Format::RedInteger,
							 .tiling = renderer::AttachmentSpecification::Tiling::Optimal},
							//{renderer::AttachmentSpecification::Format::Depth24Stencil8,
							// renderer::AttachmentSpecification::Tiling::Optimal}
					},
			.samples = 1,
			.swapChainTarget = false,
			.debugName = "gauges"};
	m_framebuffer = renderer::Framebuffer::create(specs);

	// camera
	m_camera = mkShared<renderer::CameraOrtho>(0, 1280, 0, 720);

	// gauge for speed
	m_gauges.emplace_back(mkShared<gauge::AirSpeed>());
	// gauge for vertical speed
	m_gauges.emplace_back(mkShared<gauge::VerticalSpeed>());
	// gauge for m_altitude
	m_gauges.emplace_back(mkShared<gauge::Altitude>());
	// gauge for compass
	m_gauges.emplace_back(mkShared<gauge::Compas>());
	// gauge for motor rates
	m_gauges.emplace_back(mkShared<gauge::MotorRate>());
	// gauge for artificial horizon
	m_gauges.emplace_back(mkShared<gauge::ArtificialHorizon>());
}

Gauges::~Gauges() = default;

void Gauges::onUpdate(const core::Timestep&) {
	OWL_PROFILE_FUNCTION()

	// Updatte data from flight controller
	if (const auto& rc = getRemoteController(); rc) {
		static_pointer_cast<gauge::AirSpeed>(m_gauges[0])->setVelocity(rc->getHorizontalVelocity());
		static_pointer_cast<gauge::VerticalSpeed>(m_gauges[1])->setVerticalVelocity(rc->getVerticalVelocity());
		static_pointer_cast<gauge::Altitude>(m_gauges[2])->setAltitude(rc->getAltitude());
		const auto angles = rc->getRotations();
		static_pointer_cast<gauge::Compas>(m_gauges[3])->setHeading(angles.z());
		static_pointer_cast<gauge::MotorRate>(m_gauges[4])->setMotorRates(rc->getMotorRates());
		static_pointer_cast<gauge::ArtificialHorizon>(m_gauges[5])->setPitchRoll(angles.y(), angles.x());
	}

	// Draw into the frame buffer
	const auto spec = m_framebuffer->getSpecification();
	const float aspectRatio = m_viewportSize.ratio();
	const float scaling = std::min(aspectRatio, 0.666f);

	if (m_viewportSize.surface() > 0 && m_viewportSize != spec.size) {
		m_framebuffer->resize(m_viewportSize);
		m_camera->setProjection(-aspectRatio, aspectRatio, -1, 1);
	}
	// Render
	m_framebuffer->bind();
	renderer::RenderCommand::setClearColor({0.0f, 0.0f, 0.0f, 1});
	renderer::RenderCommand::clear();

	// Clear our entity ID attachment to -1
	m_framebuffer->clearAttachment(1, -1);

	// defines pos an scale
	math::vec3 pos = {-0.5f * scaling, 1.f * scaling, 0};
	for (const auto& gauge: m_gauges) {
		gauge->setScale({scaling, scaling, 0});
		gauge->setPosition(pos);
		pos.x() *= -1;
		if (pos.x() < 0)
			pos.y() -= scaling;
	}
	// Do the drawings!
	// ===============================================================
	renderer::Renderer2D::beginScene(*m_camera);

	// draw all backgrounds
	for (const auto& gauge: m_gauges) gauge->drawBack();
	// draw all cursors

	for (const auto& gauge: m_gauges) gauge->drawCursors();
	// draw all covers
	for (const auto& gauge: m_gauges) gauge->drawCover();

	renderer::Renderer2D::endScene();
	// ===============================================================
	// free the frame buffer.
	m_framebuffer->unbind();
}

void Gauges::onRender() {
	OWL_PROFILE_FUNCTION()

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	ImGui::Begin("Drone Gauges");
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
	if (const auto tex = gui::imTexture(m_framebuffer, 0); tex.has_value())
		ImGui::Image(tex.value(), viewportPanelSize, vec(m_framebuffer->getLowerData()),
					 vec(m_framebuffer->getUpperData()));

	else
		OWL_WARN("No frameBuffer to render...")

	ImGui::End();
	ImGui::PopStyleVar();
}

}// namespace drone::panels
