/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 09/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "DrawData.h"
#include "math/vectors.h"

namespace owl::renderer {

/**
 * @brief Abstract class to manage rendering API.
 */
class OWL_API RenderAPI {
public:
	/// Render API types.
	enum struct Type : uint8_t {
		Null = 0,///< Null Renderer.
		OpenGL = 1,///< OpenGL Renderer.
		Vulkan = 2,///< Vulkan renderer API.
	};

	explicit RenderAPI(const Type& iType) : m_type{iType} {}
	RenderAPI(const RenderAPI&) = delete;
	RenderAPI(RenderAPI&&) = delete;
	auto operator=(const RenderAPI&) -> RenderAPI& = delete;
	auto operator=(RenderAPI&&) -> RenderAPI& = delete;

	/**
	 * @brief Destructor.
	 */
	virtual ~RenderAPI();

	/**
	 * @brief Initialize the renderer.
	 */
	virtual void init() = 0;

	/**
	 * @brief Define the view port for this API.
	 * @param[in] iX Starting X coordinate.
	 * @param[in] iY Starting Y coordinate.
	 * @param[in] iWidth Viewport's width.
	 * @param[in] iHeight Viewport Height.
	 */
	virtual void setViewport(uint32_t iX, uint32_t iY, uint32_t iWidth, uint32_t iHeight) = 0;

	/**
	 * @brief Define the background color.
	 * @param[in] iColor The background color.
	 */
	virtual void setClearColor(const math::vec4& iColor) = 0;

	/**
	 * @brief Clear the screen.
	 */
	virtual void clear() = 0;

	/**
	 * @brief Binding the draw of vertex array.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	virtual void drawData(const shared<DrawData>& iData, uint32_t iIndexCount) = 0;

	/**
	 * @brief Binding the draw of vertex array as line.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	virtual void drawLine(const shared<DrawData>& iData, uint32_t iIndexCount = 0) = 0;

	/**
	 * @brief Get the maximum number of texture slots.
	 * @return Number of texture slots.
	 */
	[[nodiscard]] virtual auto getMaxTextureSlots() const -> uint32_t = 0;


	/// Render API states.
	enum struct State : uint8_t {
		Created,///< Just created.
		Ready,///< Ready to work.
		Error///< in error.
	};

	/**
	 * @brief Get the actual API type.
	 * @return API Type.
	 */
	[[nodiscard]] auto getApi() const -> Type { return m_type; }

	/**
	 * @brief Static method to create a Render API.
	 * @param[in] iType Type of API.
	 * @return Render.
	 */
	static auto create(const Type& iType) -> uniq<RenderAPI>;

	/**
	 * @brief Get the actual API state.
	 * @return API State.
	 */
	[[nodiscard]] auto getState() const -> State { return m_state; }

	/**
	 * @brief Check if the API type require initializations.
	 * @return tRue if initialization required.
	 */
	[[nodiscard]] auto requireInit() const -> bool { return m_type == Type::OpenGL || m_type == Type::Vulkan; }

	/**
	 * @brief Reset value for the frame to render.
	 */
	virtual void beginFrame() {}

	/**
	 * @brief Reset value for the batch to render.
	 */
	virtual void beginBatch() {}

	/**
	 * @brief Reset value for the texture load.
	 */
	virtual void beginTextureLoad() {}

	/**
	 * @brief Ends texture load.
	 */
	virtual void endTextureLoad() {}

	/**
	 * @brief Ends draw call for the current batch.
	 */
	virtual void endBatch() {}

	/**
	 * @brief Change the subpass.
	 */
	virtual void nextSubpass() {}

	/**
	 * @brief Ends draw call for the current frame.
	 */
	virtual void endFrame() {}

	/**
	 * @brief Enable or disable depth buffer writing.
	 * @param[in] iEnabled True to enable depth writing, false to disable.
	 */
	virtual void setDepthMask([[maybe_unused]] bool iEnabled) {}

protected:
	/**
	 * @brief Define the API State.
	 * @param[in] iState The new API State.
	 */
	void setState(const State& iState) { m_state = iState; }

private:
	/// Type of Renderer API.
	Type m_type = Type::Null;
	/// The current state of the API.
	State m_state = State::Created;
};

}// namespace owl::renderer
