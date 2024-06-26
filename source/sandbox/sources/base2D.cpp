/**
 * @file base2D.cpp
 * @author Silmaen
 * @date 18/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "base2D.h"

namespace owl {


Base2D::Base2D() : core::layer::Layer("base2D"), m_cameraController{1280.0f / 720.0f, true} {}

void Base2D::onAttach() {
	OWL_PROFILE_FUNCTION()

	const auto texturePath = core::Application::get().getAssetDirectory() / "textures";
	m_checkerboardTexture = renderer::Texture2D::create(texturePath / "CheckerBoard.png");
	m_spriteTexture = renderer::Texture2D::create(texturePath / "mario.png");
}

void Base2D::onDetach() { OWL_PROFILE_FUNCTION() }

void Base2D::onUpdate(const core::Timestep& iTs) {
	OWL_PROFILE_FUNCTION()

	// Update
	m_cameraController.onUpdate(iTs);
	// sprite movement
	if (input::Input::isKeyPressed(input::key::Right)) {
		m_spritePosition.x() += 0.05f;
	}
	if (input::Input::isKeyPressed(input::key::Left)) {
		m_spritePosition.x() -= 0.05f;
	}
	if (input::Input::isKeyPressed(input::key::Up)) {
		m_spriteRotation += 0.5f;
	}
	if (input::Input::isKeyPressed(input::key::Down)) {
		m_spriteRotation -= 0.5f;
	}

	// Render stats
	renderer::Renderer2D::resetStats();

	// Clear screen
	{
		OWL_PROFILE_SCOPE("Render Preparation")
		renderer::RenderCommand::setClearColor({0.1f, 0.1f, 0.1f, 1});
		renderer::RenderCommand::clear();
	}

	static float rotation = 0.f;
	rotation += iTs.getSeconds() * 50.f;
	// Background
	{
		renderer::Renderer2D::beginScene(m_cameraController.getCamera());
		renderer::Renderer2D::drawQuad(
				{.transform = renderer::utils::PRS{.position = {0.0f, 0.0f, -0.1f}, .size = {20.0f, 20.0f}},
				 .texture = m_checkerboardTexture,
				 .tilingFactor = 10.f});
		renderer::Renderer2D::endScene();
	}
	// First part of the scene
	{
		OWL_PROFILE_SCOPE("Render Draws 2")
		renderer::Renderer2D::beginScene(m_cameraController.getCamera());
		int32_t id = 0;
		for (uint8_t idy = 0; idy < 11; ++idy) {
			for (uint8_t idx = 0; idx < 11; ++idx) {
				constexpr float marg = 0.9f;
				constexpr float scaley = 1.f;
				constexpr float scalex = 1.f;
				const float x = -5.0f + static_cast<float>(idx) * scalex;
				const float y = -5.0f + static_cast<float>(idy) * scaley;
				const math::vec4 color = {(x + 5.0f) / 10.0f, 0.4f, (y + 5.0f) / 10.0f, 0.7f};
				renderer::Renderer2D::drawQuad(
						{.transform = renderer::utils::PRS{.position = {x, y, -0.05f},
														   .size = {scalex * marg, scaley * marg}},
						 .color = color,
						 .entityID = id});
				id++;
			}
		}
		renderer::Renderer2D::endScene();
	}
	// second part of the scene
	{
		OWL_PROFILE_SCOPE("Render Draws 1")
		renderer::Renderer2D::beginScene(m_cameraController.getCamera());
		renderer::Renderer2D::drawQuad({
				.transform =
						renderer::utils::PRS{.position = {1.0f, 0.0f, 0.0f}, .rotation = -45.f, .size = {0.8f, 0.8f}},
				.color = {0.8f, 0.2f, 0.3f, 1.0f},
		});
		renderer::Renderer2D::drawQuad(
				{.transform = renderer::utils::PRS{.position = {-1.0f, 0.0f, 0.0f}, .size = {0.8f, 0.8f}},
				 .color = {0.8f, 0.2f, 0.3f, 1.0f}});
		renderer::Renderer2D::drawQuad(
				{.transform = renderer::utils::PRS{.position = {0.5f, -0.5f, 0.0f}, .size = {0.5f, 0.75f}},
				 .color = m_squareColor});
		renderer::Renderer2D::drawQuad({.transform = renderer::utils::PRS{.position = {-2.0f, 0.0f, 0.0f},
																		  .rotation = rotation,
																		  .size = {1.0f, 1.0f}},
										.texture = m_checkerboardTexture,
										.tilingFactor = 20.f});
		renderer::Renderer2D::endScene();
	}
	// third part of the scene Movable sprite.
	{
		renderer::Renderer2D::beginScene(m_cameraController.getCamera());
		renderer::Renderer2D::drawQuad({.transform = renderer::utils::PRS{.position = m_spritePosition,
																		  .rotation = m_spriteRotation,
																		  .size = {1.0f, 1.0f}},
										.texture = m_spriteTexture,
										.tilingFactor = 1.f});
		renderer::Renderer2D::endScene();
	}
}

void Base2D::onEvent(event::Event& ioEvent) { m_cameraController.onEvent(ioEvent); }


void Base2D::onImGuiRender(const core::Timestep& iTs) {
	{
		//ImGui::ShowDemoWindow();
	}
	// ==================================================================
	{
		ImGui::Begin("Settings");
		ImGui::ColorEdit4("Square Color", m_squareColor.data());
		ImGui::End();
	}
	// ==================================================================
	{
		const auto& tracker = debug::Tracker::get();
		ImGui::Begin("Statistics");
		ImGui::Text("%s", fmt::format("FPS: {:.2f}", iTs.getFps()).c_str());
		ImGui::Text("%s", fmt::format("Current used memory: {}", tracker.globals().allocatedMemory).c_str());
		ImGui::Text("%s", fmt::format("Max used memory: {}", tracker.globals().memoryPeek).c_str());
		ImGui::Text("%s", fmt::format("Allocation calls: {}", tracker.globals().allocationCalls).c_str());
		ImGui::Text("%s", fmt::format("Deallocation calls: {}", tracker.globals().deallocationCalls).c_str());

		const auto stats = renderer::Renderer2D::getStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats.drawCalls);
		ImGui::Text("Quads: %d", stats.quadCount);
		ImGui::Text("Vertices: %d", stats.getTotalVertexCount());
		ImGui::Text("Indices: %d", stats.getTotalIndexCount());
		ImGui::Text("Viewport size: %f %f", static_cast<double>(m_viewportSize.x()),
					static_cast<double>(m_viewportSize.y()));
		ImGui::Text("Aspect ratio: %f", static_cast<double>(m_viewportSize.x() / m_viewportSize.y()));
		ImGui::Text("Entity Id: %d", m_hoveredEntity);
		ImGui::End();
	}
	m_viewportSize = {static_cast<float>(core::Application::get().getWindow().getWidth()),
					  static_cast<float>(core::Application::get().getWindow().getHeight())};
}

}// namespace owl
