/**
 * @file Renderer.h
 * @author Silmaen
 * @date 08/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "CameraOrtho.h"
#include "RenderCommand.h"
#include "RenderStack.h"
#include "Shader.h"
#include "data/assets/AssetLibrary.h"

#include <functional>

/**
 * @brief Namespace for the renderer elements.
 */
namespace owl::renderer {

/**
 * @brief Base renderer class.
 */
class OWL_API Renderer {
public:
	using TextureLibrary = data::assets::AssetLibrary<Texture2D>;
	using ShaderLibrary = data::assets::AssetLibrary<Shader>;
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

	static auto getState() -> State { return m_internalState; }

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
