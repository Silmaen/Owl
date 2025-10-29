/**
 * @file base2D.cpp
 * @author Silmaen
 * @date 18/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "base2D.h"

namespace owl {


Base2D::Base2D() : Layer("base2D"), m_cameraController{1280.0f / 720.0f, true} {}

void Base2D::onAttach() {
	OWL_PROFILE_FUNCTION()

	OWL_SCOPE_UNTRACK
	auto texLib = renderer::Renderer::getTextureLibrary();
	m_checkerboardTexture = texLib.load("CheckerBoard");
	m_spriteTexture = texLib.load("mario");
	m_mesh = data::MeshLoader::loadStaticMesh(core::Application::get().getAssetDirectories().front().assetsPath /
											  "meshs" / "wardrobe.glb");
	// first a simple test mesh as a quad
	// m_mesh = mkShared<data::geometry::StaticMesh>();
	// m_mesh->addVertex({-0.5f, -0.5f, 0.f});
	// m_mesh->addVertex({0.5f, -0.5f, 0.f});
	// m_mesh->addVertex({0.5f, 0.5f, 0.f});
	// m_mesh->addVertex({-0.5f, 0.5f, 0.f});
	// m_mesh->addTriangle({0, 1, 2});
	// m_mesh->addTriangle({2, 3, 0});
	// m_mesh->addTriangleExtraData<data::geometry::extradata::TriangleUVCoordinate>();
	// m_mesh->addTriangleExtraData<data::geometry::extradata::TriangleNormals>();
	// for (auto& [coord, uv, normals]:
	// 	 data::geometry::MeshTriangleRange(*m_mesh, data::component::TrianglesVertices, data::component::WriteUvCoords,
	// 									   data::component::WriteTriangleNormals)) {
	// 	uv.value()->setValue(
	// 			{math::vec2{coord.value()[0]->getPosition().x() + 0.5f, coord.value()[0]->getPosition().y() + 0.5f},
	// 			 {coord.value()[1]->getPosition().x() + 0.5f, coord.value()[1]->getPosition().y() + 0.5f},
	// 			 {coord.value()[2]->getPosition().x() + 0.5f, coord.value()[2]->getPosition().y() + 0.5f}});
	// 	normals.value()->setValue({math::vec3{0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}});
	// }
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
	if constexpr ((false)) {
		renderer::Renderer2D::beginScene(m_cameraController.getCamera());
		renderer::Renderer2D::drawQuad({.transform = math::Transform{{0.0f, 0.0f, -0.1f}, {}, {20.0f, 20.0f, 0.f}},
										.texture = m_checkerboardTexture,
										.tilingFactor = 10.f});
		renderer::Renderer2D::endScene();
	}
	// First part of the scene
	if constexpr ((false)) {
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
						{.transform = math::Transform{{x, y, -0.05f}, {}, {scalex * marg, scaley * marg, 0}},
						 .color = color,
						 .entityId = id});
				id++;
			}
		}
		renderer::Renderer2D::endScene();
	}
	// second part of the scene
	if constexpr ((false)) {
		OWL_PROFILE_SCOPE("Render Draws 1")
		renderer::Renderer2D::beginScene(m_cameraController.getCamera());
		renderer::Renderer2D::drawQuad({
				.transform = math::Transform{{1.0f, 0.0f, 0.0f}, {0, 0, -45.f}, {0.8f, 0.8f, 0}},
				.color = {0.8f, 0.2f, 0.3f, 1.0f},
		});
		renderer::Renderer2D::drawQuad({.transform = math::Transform{{-1.0f, 0.0f, 0.0f}, {}, {0.8f, 0.8f, 0}},
										.color = {0.8f, 0.2f, 0.3f, 1.0f}});
		renderer::Renderer2D::drawQuad(
				{.transform = math::Transform{{0.5f, -0.5f, 0.0f}, {}, {0.5f, 0.75f, 0}}, .color = m_squareColor});
		renderer::Renderer2D::drawQuad(
				{.transform = math::Transform{{-2.0f, 0.0f, 0.0f}, {0, 0, rotation}, {1.0f, 1.0f, 0}},
				 .texture = m_checkerboardTexture,
				 .tilingFactor = 20.f});
		renderer::Renderer2D::endScene();
	}
	// third part of the scene Movable sprite.
	if constexpr ((false)) {
		renderer::Renderer2D::beginScene(m_cameraController.getCamera());
		renderer::Renderer2D::drawQuad(
				{.transform = math::Transform{m_spritePosition, {0, 0, m_spriteRotation}, {1.0f, 1.0f, 0}},
				 .texture = m_spriteTexture,
				 .tilingFactor = 1.f});
		renderer::Renderer2D::endScene();
	}
	// final part of the scene Mesh rendering
	{
		OWL_PROFILE_SCOPE("Render Draws mesh")
		static float mesh_rotation = 0.f;
		mesh_rotation += iTs.getSeconds() * 0.5f;
		renderer::Renderer2D::beginScene(m_cameraController.getCamera());
		renderer::Renderer2D::drawMesh(
				{.transform = math::Transform{{0.0f, 0.0f, -0.05f}, {0.f, mesh_rotation, 0.f}, {0.1f, .1f, .1f}},
				 .color = {0.8f, 0.5f, 0.6f, 1.0f},
				 .texture = nullptr,
				 .mesh = m_mesh,
				 .entityId = 42});
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
		ImGui::Begin("Statistics");
		ImGui::Text("%s", std::format("FPS: {:.2f}", iTs.getFps()).c_str());
		ImGui::Text("%s", std::format("Current used memory: {}", debug::TrackerAPI::globals().allocatedMemory).c_str());
		ImGui::Text("%s", std::format("Max used memory: {}", debug::TrackerAPI::globals().memoryPeek).c_str());
		ImGui::Text("%s", std::format("Allocation calls: {}", debug::TrackerAPI::globals().allocationCalls).c_str());
		ImGui::Text("%s",
					std::format("Deallocation calls: {}", debug::TrackerAPI::globals().deallocationCalls).c_str());

		const auto stats = renderer::Renderer2D::getStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %u", stats.drawCalls);
		ImGui::Text("Quads: %u", stats.quadCount);
		ImGui::Text("Mesh Triangles: %u", stats.meshTriangleCount);
		ImGui::Text("Vertices: %u", stats.getTotalVertexCount());
		ImGui::Text("Indices: %u", stats.getTotalIndexCount());
		ImGui::Separator();
		ImGui::Text("Camera position: %f %f %f", static_cast<double>(m_cameraController.getCamera().getPosition().x()),
					static_cast<double>(m_cameraController.getCamera().getPosition().y()),
					static_cast<double>(m_cameraController.getCamera().getPosition().z()));
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
