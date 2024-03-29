/**
 * @file base2D.cpp
 * @author Silmaen
 * @date 18/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "base2D.h"

#include <glm/gtc/type_ptr.hpp>

namespace owl {


base2D::base2D() : core::layer::Layer("base2D"), cameraController{1280.0f / 720.0f, true} {}

void base2D::onAttach() {
	OWL_PROFILE_FUNCTION()

	const auto texturePath = core::Application::get().getAssetDirectory() / "textures";
	checkerboardTexture = renderer::Texture2D::create(texturePath / "CheckerBoard.png");
}

void base2D::onDetach() {
	OWL_PROFILE_FUNCTION()
}

void base2D::onUpdate(const core::Timestep &ts) {
	OWL_PROFILE_FUNCTION()

	// Update
	cameraController.onUpdate(ts);

	// Render stats
	renderer::Renderer2D::resetStats();

	// Clear screen
	{
		OWL_PROFILE_SCOPE("Render Preparation")
		renderer::RenderCommand::setClearColor({0.1f, 0.1f, 0.1f, 1});
		renderer::RenderCommand::clear();
	}

	static float rotation = 0.f;
	rotation += ts.getSeconds() * 50.f;
	// Background
	{
		renderer::Renderer2D::beginScene(cameraController.getCamera());
		renderer::Renderer2D::drawQuad({.transform = renderer::utils::PRS{
												.position = {0.0f, 0.0f, -0.1f},
												.size = {20.0f, 20.0f}},
										.texture = checkerboardTexture,
										.tilingFactor = 10.f});
		renderer::Renderer2D::endScene();
	}
	// First part of the scene
	{
		OWL_PROFILE_SCOPE("Render Draws 2")
		renderer::Renderer2D::beginScene(cameraController.getCamera());
		int32_t id = 0;
		for (uint8_t idy = 0; idy < 11; ++idy) {
			for (uint8_t idx = 0; idx < 11; ++idx) {
				constexpr float marg = 0.9f;
				constexpr float scaley = 1.f;
				constexpr float scalex = 1.f;
				const float x = -5.0f + static_cast<float>(idx) * scalex;
				const float y = -5.0f + static_cast<float>(idy) * scaley;
				const glm::vec4 color = {(x + 5.0f) / 10.0f, 0.4f, (y + 5.0f) / 10.0f, 0.7f};
				renderer::Renderer2D::drawQuad({.transform = renderer::utils::PRS{
														.position = {x, y, -0.05},
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
		renderer::Renderer2D::beginScene(cameraController.getCamera());
		renderer::Renderer2D::drawQuad({
				.transform = renderer::utils::PRS{
						.position = {1.0f, 0.0f, 0.0f},
						.rotation = -45.f,
						.size = {0.8f, 0.8f}},
				.color = {0.8f, 0.2f, 0.3f, 1.0f},
		});
		renderer::Renderer2D::drawQuad({.transform = renderer::utils::PRS{
												.position = {-1.0f, 0.0f, 0.0f},
												.size = {0.8f, 0.8f}},
										.color = {0.8f, 0.2f, 0.3f, 1.0f}});
		renderer::Renderer2D::drawQuad({.transform = renderer::utils::PRS{
												.position = {0.5f, -0.5f, 0.0f},
												.size = {0.5f, 0.75f}},
										.color = squareColor});
		renderer::Renderer2D::drawQuad({.transform = renderer::utils::PRS{
												.position = {-2.0f, 0.0f, 0.0f},
												.rotation = rotation,
												.size = {1.0f, 1.0f}},
										.texture = checkerboardTexture,
										.tilingFactor = 20.f});
		renderer::Renderer2D::endScene();
	}
}

void base2D::onEvent(event::Event &event) { cameraController.onEvent(event); }

void base2D::onImGuiRender(const core::Timestep &ts) {
	{
		ImGui::ShowDemoWindow();
	}
	// ==================================================================
	{
		ImGui::Begin("Settings");
		ImGui::ColorEdit4("Square Color", glm::value_ptr(squareColor));
		ImGui::End();
	}
	// ==================================================================
	{
		const auto &tracker = debug::Tracker::get();
		ImGui::Begin("Statistics");
		ImGui::Text("%s", fmt::format("FPS: {:.2f}", ts.getFps()).c_str());
		ImGui::Text("%s", fmt::format("Current used memory: {}",
									  tracker.globals().allocatedMemory)
					.c_str());
		ImGui::Text("%s", fmt::format("Max used memory: {}", tracker.globals().memoryPeek)
					.c_str());
		ImGui::Text(
				"%s", fmt::format("Allocation calls: {}", tracker.globals().allocationCalls)
				.c_str());
		ImGui::Text("%s", fmt::format("Deallocation calls: {}",
									  tracker.globals().deallocationCalls)
					.c_str());

		const auto stats = renderer::Renderer2D::getStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats.drawCalls);
		ImGui::Text("Quads: %d", stats.quadCount);
		ImGui::Text("Vertices: %d", stats.getTotalVertexCount());
		ImGui::Text("Indices: %d", stats.getTotalIndexCount());
		ImGui::Text("Viewport size: %f %f", static_cast<double>(viewportSize.x), static_cast<double>(viewportSize.y));
		ImGui::Text("Aspect ratio: %f", static_cast<double>(viewportSize.x / viewportSize.y));
		ImGui::End();
	}
	viewportSize = {core::Application::get().getWindow().getWidth(), core::Application::get().getWindow().getHeight()};
}

}// namespace owl
