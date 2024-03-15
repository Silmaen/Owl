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
#include "ShaderLibrary.h"
#include "TextureLibrary.h"

/**
 * @brief Namespace for the renderer elements.
 */
namespace owl::renderer {

/**
 * @brief Class Renderer.
 */
class OWL_API Renderer {
public:
	Renderer() = default;
	Renderer(const Renderer &) = delete;
	Renderer(Renderer &&) = delete;
	Renderer &operator=(const Renderer &) = delete;
	Renderer &operator=(Renderer &&) = delete;

	/**
	 * @brief Destructor.
	 */
	~Renderer() = default;

	/**
	 * @brief Initialize the renderer.
	 */
	static void init();

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
	enum struct State {
		Created,///< Renderer just created.
		Running,///< Renderer in stable state.
		Stopped,///< Renderer stopped.
		Error,///< Render has an error.
	};

	static State getState() { return m_internalState; }

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
	static void beginScene(const CameraOrtho &iCamera);

	/**
	 * @brief Ends a scene.
	 */
	static void endScene();

	/**
	 * @brief Access to the shader Library.
	 * @return The shader library.
	 */
	static ShaderLibrary &getShaderLibrary() { return *m_shaderLibrary; }

	/**
	 * @brief Access to the texture Library.
	 * @return The texture library.
	 */
	static TextureLibrary &getTextureLibrary() { return *m_textureLibrary; }

private:
	/// The state of the renderer.
	static State m_internalState;

	/**
	 * @brief Data for the current scene.
	 */
	struct SceneData {
		/// View projection Matrix.
		glm::mat4 viewProjectionMatrix;
	};

	/// The actual sceneData.
	static shared<SceneData> m_sceneData;
	/// Actual library of shaders.
	static shared<ShaderLibrary> m_shaderLibrary;
	/// Actual library of textures.
	static shared<TextureLibrary> m_textureLibrary;
};

}// namespace owl::renderer
