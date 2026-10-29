/**
 * @file RendererParticlesLayer.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "renderer/RenderLayer.h"

#include <string>

namespace owl::renderer {

/**
 * @brief
 *  Render-stack layer drawing particle emitters and trails through `RendererParticles`.
 *
 * Entities of this layer (a `RendererTag` matching the layer name, or untagged when it is the first layer) with a
 * `scene::component::ParticleEmitter` or a `scene::component::Trail` are drawn with the scene camera, 2D or 3D, depth
 * tested against the layers drawn before it: place it after the world layers it decorates, before the HUD.
 */
class OWL_API RendererParticlesLayer final : public RenderLayer {
public:
	/**
	 * @brief
	 *  The factory type key for this layer.
	 * @return The type key string.
	 */
	static constexpr auto typeKey() -> const char* { return "RendererParticles"; }
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
	explicit RendererParticlesLayer(std::string iName);
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
	 *  Open a frame: bind the camera.
	 * @param[in] iCamera The active camera.
	 */
	void onBeginFrame(const Camera& iCamera) override;
	/**
	 * @brief
	 *  Render the layer (no-op; `Scene::render` draws the emitters and trails).
	 * @param[in,out] ioScene The scene being rendered.
	 */
	void onRender(scene::Scene& ioScene) override;
	/**
	 * @brief
	 *  Close the frame.
	 */
	void onEndFrame() override;
	/**
	 * @brief
	 *  Apply the merged YAML configuration (the layer has no key).
	 * @param[in] iConfig The merged config node.
	 */
	void applyConfig(const std::string& iConfig) override;

private:
	/// Instance name from the project stack.
	std::string m_name;
};

}// namespace owl::renderer
