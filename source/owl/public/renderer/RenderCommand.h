/**
 * @file RenderCommand.h
 * @author Silmaen
 * @date 09/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "RenderAPI.h"
#include "core/Core.h"

namespace owl::renderer {
/**
 * @brief Class gathering all render commands.
 */
class OWL_API RenderCommand final {
public:
	RenderCommand() = default;
	RenderCommand(const RenderCommand&) = delete;
	RenderCommand(RenderCommand&&) = delete;
	auto operator=(const RenderCommand&) -> RenderCommand& = delete;
	auto operator=(RenderCommand&&) -> RenderCommand& = delete;

	/**
	 * @brief Destructor.
	 */
	~RenderCommand() = default;

	/**
	 * @brief Initialize the renderer.
	 */
	static void init() {
		if (mu_renderAPI)
			mu_renderAPI->init();
	}

	/**
	 * @brief Reset RenderAPI.
	 */
	static void invalidate() { mu_renderAPI.reset(); }

	/**
	 * @brief Get the state of the API.
	 * @return API state.
	 */
	static auto getState() -> RenderAPI::State;

	/**
	 * @brief Define the view port for this API.
	 * @param[in] iX Starting X coordinate.
	 * @param[in] iY Starting Y coordinate.
	 * @param[in] iWidth Viewport's width.
	 * @param[in] iHeight Viewport Height.
	 */
	static void setViewport(const uint32_t iX, const uint32_t iY, const uint32_t iWidth, const uint32_t iHeight) {
		mu_renderAPI->setViewport(iX, iY, iWidth, iHeight);
	}

	/**
	 * @brief Binding to the definition of background color.
	 * @param[in] iColor The new background color.
	 */
	static void setClearColor(const math::vec4& iColor) { mu_renderAPI->setClearColor(iColor); }

	/**
	 * @brief Binding to clear screen.
	 */
	static void clear() { mu_renderAPI->clear(); }

	/**
	 * @brief Binding the draw of vertex array.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	static void drawData(const shared<DrawData>& iData, const uint32_t iIndexCount = 0) {
		mu_renderAPI->drawData(iData, iIndexCount);
	}
	/**
		 * @brief Binding the draw of vertex array as lines.
		 * @param[in] iData Draw data to render.
		 * @param[in] iIndexCount Number of vertex to draw (=0 all).
		 */
	static void drawLine(const shared<DrawData>& iData, const uint32_t iIndexCount = 0) {
		mu_renderAPI->drawLine(iData, iIndexCount);
	}

	/**
	 * @brief Create or replace the API base on it type.
	 * @param[in] iType The type of the new render API.
	 */
	static void create(const RenderAPI::Type& iType);

	/**
	 * @brief Get the actual API type.
	 * @return API Type.
	 */
	static auto getApi() -> RenderAPI::Type {
		if (mu_renderAPI)
			return mu_renderAPI->getApi();
		return static_cast<RenderAPI::Type>(-1);// NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
	}

	/**
	 * @brief Get the maximum number of texture slots.
	 * @return Number of texture slots.
	 */
	static auto getMaxTextureSlots() -> uint32_t {
		if (mu_renderAPI)
			return mu_renderAPI->getMaxTextureSlots();
		return 0;
	}

	/**
	 * @brief Reset value for the frame to render.
	 */
	static void beginFrame() {
		if (mu_renderAPI)
			mu_renderAPI->beginFrame();
	}

	/**
	 * @brief Reset value for the batch to render.
	 */
	static void beginBatch() {
		if (mu_renderAPI)
			mu_renderAPI->beginBatch();
	}

	/**
	 * @brief Reset value for the teture load.
	 */
	static void beginTextureLoad() {
		if (mu_renderAPI)
			mu_renderAPI->beginTextureLoad();
	}

	/**
	 * @brief Ends draw call for the current batch.
	 */
	static void endBatch() {
		if (mu_renderAPI)
			mu_renderAPI->endBatch();
	}

	/**
	 * @brief Ends draw call for the current frame.
	 */
	static void endFrame() {
		if (mu_renderAPI)
			mu_renderAPI->endFrame();
	}

	/**
	 * @brief Ends texture load.
	 */
	static void endTextureLoad() {
		if (mu_renderAPI)
			mu_renderAPI->endTextureLoad();
	}

	/**
	 * @brief Check if the API type require initializations.
	 * @return True if initialization required.
	 */
	static auto requireInit() -> bool {
		if (mu_renderAPI)
			return mu_renderAPI->requireInit();
		return false;
	}

private:
	/// Pointer to the render API
	static uniq<RenderAPI> mu_renderAPI;
};
}// namespace owl::renderer
