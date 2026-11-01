/**
 * @file RendererParticlesLayer.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "renderer/Camera.h"
#include "renderer/RenderLayer.h"

#include <cstdint>
#include <string>

namespace owl::renderer {

/**
 * @brief
 *  Render-stack layer drawing particle emitters and trails through `RendererParticles`.
 *
 * Entities of this layer (a `RendererTag` matching the layer name, or untagged when it is the first layer) with a
 * `scene::component::ParticleEmitter` or a `scene::component::Trail` are drawn with the scene camera, 2D or 3D, depth
 * tested against the layers drawn before it: place it after the world layers it decorates, before the HUD.
 *
 * Its configuration has two keys: `Projection` (`Scene`, the default, or `Raycast`) and `Fov` (degrees, 75 by
 * default). `Raycast` draws the particles in the first-person view of a `RendererRaycast` layer of the same `Fov`: the
 * eye on the camera position of the map plane, looking along its local +Y, the world Z the height above the eye (the
 * floor at -0.5, the ceiling at 0.5), as the raycaster places its sprites and the HUD markers.
 */
class OWL_API RendererParticlesLayer final : public RenderLayer {
public:
	/**
	 * @brief
	 *  The camera the particles are drawn with.
	 */
	enum struct Projection : uint8_t {
		/// The scene camera, perspective or orthographic.
		Scene,
		/// The first-person view of a raycast layer, built from the scene camera.
		Raycast,
	};

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
	 *  Apply the merged YAML configuration: `Projection` (`Scene` or `Raycast`, case-insensitive) and `Fov`.
	 * @param[in] iConfig The merged config node.
	 */
	void applyConfig(const std::string& iConfig) override;

	/**
	 * @brief
	 *  View-projection the particles are drawn with: the scene camera's, or the raycast view built from it.
	 * @param[in] iCamera The active scene camera.
	 * @return The view-projection matrix bound by this layer.
	 */
	[[nodiscard]] auto getEffectiveViewProjection(const Camera& iCamera) const -> math::mat4 override;

	/**
	 * @brief
	 *  Get the camera the particles are drawn with.
	 * @return The configured projection, `Scene` by default.
	 */
	[[nodiscard]] auto getProjection() const -> Projection { return m_projection; }

	/**
	 * @brief
	 *  Get the horizontal field of view of the raycast projection.
	 * @return The field of view, in degrees.
	 */
	[[nodiscard]] auto getFovDegrees() const -> float { return m_fovDegrees; }

	/**
	 * @brief
	 *  Build the perspective camera of a raycast view.
	 *
	 * The eye stands at the camera position on the map plane (Z = 0), looks along the camera's local +Y projected on
	 * the plane, with the world Z up. The horizontal field of view is `iFovDegrees` and a height of one unit at a
	 * distance of one fills the screen height, as the raycaster draws its walls.
	 * @param[in] iCamera The scene camera of the raycast view.
	 * @param[in] iFovDegrees The horizontal field of view of the raycast layer, in degrees.
	 * @return The camera, for the clip space of the active backend.
	 */
	[[nodiscard]] static auto makeRaycastCamera(const Camera& iCamera, float iFovDegrees) -> Camera;

private:
	/// Instance name from the project stack.
	std::string m_name;
	/// Camera the particles are drawn with.
	Projection m_projection = Projection::Scene;
	/// Horizontal field of view of the raycast projection, in degrees.
	float m_fovDegrees = 75.f;
};

}// namespace owl::renderer
