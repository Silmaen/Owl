/**
 * @file base2D.h
 * @author Silmaen
 * @date 18/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl {

/**
 * @brief Class base2D
 */
class Base2D final : public core::layer::Layer {
public:
	/**
	 * @brief Default constructor.
	 */
	Base2D();
	Base2D(const Base2D&) = delete;
	Base2D(Base2D&&) = delete;
	auto operator=(const Base2D&) -> Base2D& = delete;
	auto operator=(Base2D&&) -> Base2D& = delete;
	/**
	 * @brief Destructor.
	 */
	~Base2D() override = default;

	void onAttach() override;
	void onDetach() override;
	void onUpdate(const core::Timestep& iTs) override;
	void onEvent(event::Event& ioEvent) override;
	void onImGuiRender(const core::Timestep& iTs) override;

private:
	input::CameraOrthoController m_cameraController;

	math::vec2 m_viewportSize = {0.0f, 0.0f};

	shared<renderer::Texture> m_checkerboardTexture;
	math::vec4 m_squareColor = {0.2f, 0.3f, 0.8f, 1.0f};
	int32_t m_hoveredEntity = -1;

	shared<renderer::Texture> m_spriteTexture;
	math::vec3 m_spritePosition{0, 0, 0.5};
	float m_spriteRotation{0.f};
	shared<data::geometry::StaticMesh> m_mesh;
};

}// namespace owl
