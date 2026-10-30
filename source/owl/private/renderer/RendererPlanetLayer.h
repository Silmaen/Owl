/**
 * @file RendererPlanetLayer.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "renderer/RenderLayer.h"
#include "renderer/RendererMesh.h"

#include <string>

namespace owl::renderer {

/**
 * @brief
 *  Render-stack layer drawing the planets (`scene::component::Planet`) through `RendererPlanet`.
 *
 * The terrain chunks are meshes of `Renderer3D`, lit by the scene lights (the layer sun stands in for a scene without
 * light, `SunDirection` / `Ambient` like `RendererMesh`), then the atmospheres are blended over them. It composes with
 * the other 3D layers through the depth buffer.
 */
class OWL_API RendererPlanetLayer final : public RenderLayer {
public:
	/**
	 * @brief
	 *  The factory type key for this layer.
	 * @return The type key string.
	 */
	static constexpr auto typeKey() -> const char* { return "RendererPlanet"; }
	/**
	 * @brief
	 *  Register this layer type with the `RenderLayerFactory`.
	 */
	static void registerWithFactory();
	/**
	 * @brief
	 *  Construct the layer with its scene-unique instance name.
	 * @param[in] iName The instance name from the project stack.
	 */
	explicit RendererPlanetLayer(std::string iName);
	/**
	 * @brief
	 *  Get the instance name.
	 * @return The instance name.
	 */
	[[nodiscard]] auto getName() const -> const std::string& override { return m_name; }
	/**
	 * @brief
	 *  Get the factory type key.
	 * @return The type key.
	 */
	[[nodiscard]] auto getTypeKey() const -> const char* override { return typeKey(); }
	/**
	 * @brief
	 *  Open a frame: open `RendererMesh` and `RendererPlanet` for the camera.
	 * @param[in] iCamera The active camera.
	 */
	void onBeginFrame(const Camera& iCamera) override;
	/**
	 * @brief
	 *  Render the layer (no-op; `Scene::render` draws the planets).
	 * @param[in,out] ioScene The scene being rendered.
	 */
	void onRender(scene::Scene& ioScene) override;
	/**
	 * @brief
	 *  Close the frame: flush the meshes, draw the atmospheres.
	 */
	void onEndFrame() override;
	/**
	 * @brief
	 *  Apply the merged YAML configuration (`SunDirection`, `Ambient`, `SunIlluminance`, `AmbientLuminance`).
	 * @param[in] iConfig The merged config node.
	 */
	void applyConfig(const std::string& iConfig) override;

	/**
	 * @brief
	 *  The lighting configuration of the layer.
	 * @return The configuration.
	 */
	[[nodiscard]] auto getConfig() const -> const MeshLayerConfig& { return m_config; }

private:
	/// Lighting of the layer (the sun when the scene has no light).
	MeshLayerConfig m_config;
	/// Instance name from the project stack.
	std::string m_name;
};

}// namespace owl::renderer
