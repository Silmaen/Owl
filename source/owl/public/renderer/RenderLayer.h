/**
 * @file RenderLayer.h
 * @author Silmaen
 * @date 30/04/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Camera.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

#include <string>

namespace owl::scene {
class Scene;
}// namespace owl::scene

namespace owl::renderer {

/**
 * @brief Abstract interface for a single renderer in a composable render stack.
 *
 * A `RenderLayer` represents one rendering pass in the project-level renderer stack
 * (e.g. `Renderer2D` for HUD, raycasting world, voxel terrain). Layers are ordered
 * back-to-front by the owning `RenderStack` and share the active framebuffer.
 *
 * Implementations are constructed by the `RenderLayerFactory` via a string type key
 * (e.g. `"Renderer2D"`) and given a runtime `name` that scenes / entities reference.
 *
 * Thread-safety: layers are always accessed from the main thread.
 */
class OWL_API RenderLayer {
public:
	RenderLayer(const RenderLayer&) = delete;
	RenderLayer(RenderLayer&&) = delete;
	auto operator=(const RenderLayer&) -> RenderLayer& = delete;
	auto operator=(RenderLayer&&) -> RenderLayer& = delete;

	/**
	 * @brief Default constructor.
	 */
	RenderLayer() = default;

	/**
	 * @brief Virtual destructor (out-of-line to anchor the vtable).
	 */
	virtual ~RenderLayer();

	/**
	 * @brief Get the runtime instance name (e.g. `"world"`, `"hud"`).
	 *
	 * The name is the identifier scenes use in `EnabledRenderers` and entities use
	 * in `RendererTag`. It is unique within a stack.
	 * @return The layer instance name.
	 */
	[[nodiscard]] virtual auto getName() const -> const std::string& = 0;

	/**
	 * @brief Get the static type key (e.g. `"Renderer2D"`).
	 *
	 * The type key is the factory registration key shared by every instance of this
	 * concrete layer class.
	 * @return The type key.
	 */
	[[nodiscard]] virtual auto getTypeKey() const -> const char* = 0;

	/**
	 * @brief Open a frame for this layer.
	 * @param[in] iCamera The active camera for this layer.
	 */
	virtual void onBeginFrame(const Camera& iCamera) = 0;

	/**
	 * @brief Render the contents of the scene that belong to this layer.
	 *
	 * Implementations iterate over entities tagged for this layer (or untagged
	 * entities if this is the first layer in the stack) and emit draw calls.
	 * @param[in,out] ioScene The scene to render.
	 */
	virtual void onRender(scene::Scene& ioScene) = 0;

	/**
	 * @brief Close the frame for this layer (flush, present).
	 */
	virtual void onEndFrame() = 0;

	/**
	 * @brief Apply a configuration block (project default + scene override merged).
	 *
	 * Called once per scene activation, before the first `onBeginFrame`. The YAML node
	 * is the merged result of the project's `DefaultConfig` and the scene's
	 * `Overrides` for this layer instance. May be a null/empty node when neither
	 * provides anything.
	 * @param[in] iConfig YAML node holding the merged config (may be Null).
	 */
	virtual void applyConfig(const YAML::Node& iConfig) = 0;
};

}// namespace owl::renderer
