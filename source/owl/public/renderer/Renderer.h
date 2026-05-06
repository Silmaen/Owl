/**
 * @file Renderer.h
 * @author Silmaen
 * @date 08/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/assets/AssetLibrary.h"
#include "gpu/RenderCommand.h"
#include "gpu/Shader.h"
#include "gpu/Texture.h"
#include "renderer/CameraOrtho.h"
#include "renderer/RenderStack.h"

#include <functional>

/**
 * @brief Top-level renderer namespace.
 *
 * Hosts the engine's high-level renderer facade (`Renderer`), the camera
 * abstractions (`Camera`, `CameraOrtho`, `CameraEditor`), the deferred
 * background renderer, and the texture decoder. Specialized renderers live
 * in nested namespaces:
 *   - `owl::renderer::gpu` — backend abstractions (`Texture`, `gpu::Shader`,
 *     `Buffer`, `gpu::RenderAPI`, `gpu::RenderCommand`, …) and their per-API impls.
 *   - `owl::renderer`   — render-stack orchestration (`RenderLayer`,
 *     `RenderLayerFactory`, `RenderStack`).
 *   - `owl::renderer` (formerly `owl::renderer::draw`)    — concrete draw-type renderers, split by
 *     family in sub-namespaces (`r2d` for the 2D batch renderer, `raycast`
 *     for the Wolfenstein-style raycaster).
 */
namespace owl::renderer {

/**
 * @brief Engine-wide renderer facade.
 *
 * Owns the shader and texture libraries and the active `RenderStack`,
 * orchestrates startup / shutdown, and forwards backend-level calls
 * (clear, viewport resize) to the active `gpu::RenderCommand`. There is exactly
 * one renderer per `Application`.
 */
class OWL_API Renderer {
public:
	/// Strongly-typed alias for the cached `gpu::Texture2D` asset library.
	using TextureLibrary = data::assets::AssetLibrary<gpu::Texture2D>;
	/// Strongly-typed alias for the cached `gpu::Shader` asset library.
	using ShaderLibrary = data::assets::AssetLibrary<gpu::Shader>;
	Renderer() = default;
	Renderer(const Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	auto operator=(const Renderer&) -> Renderer& = delete;
	auto operator=(Renderer&&) -> Renderer& = delete;

	/**
	 * @brief Destructor.
	 */
	~Renderer() = default;

	/**
	 * @brief Initialize the renderer (context + shaders in one call).
	 */
	static void init();

	/**
	 * @brief Initialize the rendering context only (no shader compilation).
	 *
	 * Call initShaders() separately to compile shaders. This allows showing a
	 * loading screen between context init and shader compilation.
	 */
	static void initContext();

	/// Callback type for shader compilation progress (current index, total count, shader name).
	using ShaderProgressCallback = std::function<void(uint32_t, uint32_t, const std::string&)>;

	/**
	 * @brief Compile all renderer shaders.
	 *
	 * Must be called after initContext(). If a progress callback is provided, it is
	 * called before each shader compilation with the shader index, total count, and name.
	 * @param[in] iProgress Optional progress callback.
	 */
	static void initShaders(const ShaderProgressCallback& iProgress = {});

	/**
	 * @brief Stops the renderer.
	 */
	static void shutdown();

	/**
	 * @brief Reset the renderer.
	 */
	static void reset();

	/**
	 * @brief The state of the renderer.
	 */
	enum struct State : uint8_t {
		Created,///< Renderer just created.
		Running,///< Renderer in stable state.
		Stopped,///< Renderer stopped.
		Error,///< Render has an error.
	};

	/**
	 * @brief Read the current lifecycle state.
	 * @return The state.
	 */
	[[nodiscard]] static auto getState() -> State { return m_internalState; }

	/**
	 * @brief Event on Window size change.
	 * @param[in] iWidth New width.
	 * @param[in] iHeight New height.
	 */
	static void onWindowResized(uint32_t iWidth, uint32_t iHeight);

	/**
	 * @brief Begins a scene.
	 * @param[in] iCamera The camera.
	 */
	static void beginScene(const Camera& iCamera);

	/**
	 * @brief Ends a scene.
	 */
	static void endScene();

	/**
	 * @brief Access to the shader Library.
	 * @return The shader library.
	 */
	static auto getShaderLibrary() -> ShaderLibrary&;

	/**
	 * @brief Access to the texture Library.
	 * @return The texture library.
	 */
	static auto getTextureLibrary() -> TextureLibrary&;

	/**
	 * @brief Install the active renderer stack (called when scenes activate).
	 *
	 * The stack is consulted by future per-layer dispatch code. Passing an empty
	 * stack clears the active configuration; callers fall back to the legacy
	 * direct `Renderer2D` path (current default for v0.2.0).
	 * @param[in] iStack The new active stack (moved in).
	 */
	static void setRenderStack(RenderStack iStack);

	/**
	 * @brief Access the active renderer stack (may be empty).
	 * @return The active stack.
	 */
	[[nodiscard]] static auto getRenderStack() -> const RenderStack&;

private:
	/// The state of the renderer.
	static State m_internalState;

	/**
	 * @brief Data for the current scene.
	 */
	struct SceneData {
		/// View projection Matrix.
		math::mat4 viewProjectionMatrix;
	};

	/// The actual sceneData.
	static shared<SceneData> m_sceneData;
	/// Actual library of shaders.
	static shared<ShaderLibrary> m_shaderLibrary;
	/// Actual library of textures.
	static shared<TextureLibrary> m_textureLibrary;
	/// Active renderer stack (may be empty — legacy `Renderer2D` path is then used).
	static RenderStack m_renderStack;
};

}// namespace owl::renderer
