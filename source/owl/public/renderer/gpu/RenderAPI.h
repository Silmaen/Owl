/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 09/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "DrawData.h"
#include "math/vectors.h"

namespace owl::renderer::gpu {
class StorageBuffer;
}// namespace owl::renderer::gpu

/**
 * @brief
 *  Backend abstractions for the renderer module.
 *
 * Hosts the GPU-API-agnostic interfaces (`RenderAPI`, `RenderCommand`,
 * `Texture`, `Shader`, `Buffer`, `DrawData`, `Framebuffer`, `GraphContext`,
 * `UniformBuffer`) plus the data enums they share (`ImageFormat`,
 * `FilterMode`, `LoadState`, `ShaderType`, `ShaderDataType`). Concrete
 * implementations live under sub-namespaces — `owl::renderer::gpu::null`
 * for the headless backend, `owl::renderer::gpu::opengl` for OpenGL,
 * `owl::renderer::gpu::vulkan` for Vulkan. The selected backend is
 * chosen at runtime by `RenderCommand::create(Type)` and carried as a
 * `uniq<RenderAPI>` for the engine's lifetime.
 */
namespace owl::renderer::gpu {
/**
 * @brief
 *  Abstract class to manage rendering API.
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
	 * @brief
	 *  Destructor.
	 */
	virtual ~RenderAPI();

	/**
	 * @brief
	 *  Initialize the renderer.
	 */
	virtual void init() = 0;

	/**
	 * @brief
	 *  Define the view port for this API.
	 * @param[in] iX Starting X coordinate.
	 * @param[in] iY Starting Y coordinate.
	 * @param[in] iWidth Viewport's width.
	 * @param[in] iHeight Viewport Height.
	 */
	virtual void setViewport(uint32_t iX, uint32_t iY, uint32_t iWidth, uint32_t iHeight) = 0;

	/**
	 * @brief
	 *  Define the background colour.
	 * @param[in] iColor The background colour.
	 */
	virtual void setClearColor(const math::vec4& iColor) = 0;

	/**
	 * @brief
	 *  Clear the screen.
	 */
	virtual void clear() = 0;

	/**
	 * @brief
	 *  Binding the draw of vertex array.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	virtual void drawData(const shared<DrawData>& iData, uint32_t iIndexCount) = 0;

	/**
	 * @brief
	 *  Issue an instanced draw using a `DrawData` initialised via
	 *  `initInstanced`. Maps to `vkCmdDrawIndexed(..., instanceCount, ...)`
	 *  on Vulkan and `glDrawElementsInstanced` on OpenGL.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of indices per instance.
	 * @param[in] iInstanceCount Number of instances to draw.
	 */
	virtual void drawDataInstanced(const shared<DrawData>& iData, uint32_t iIndexCount, uint32_t iInstanceCount) = 0;

	/**
	 * @brief
	 *  Binding the draw of vertex array as line.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	virtual void drawLine(const shared<DrawData>& iData, uint32_t iIndexCount = 0) = 0;

	/**
	 * @brief
	 *  Get the maximum number of texture slots.
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
	 * @brief
	 *  Get the actual API type.
	 * @return API Type.
	 */
	[[nodiscard]] auto getApi() const -> Type { return m_type; }

	/**
	 * @brief
	 *  Static method to create a Render API.
	 * @param[in] iType Type of API.
	 * @return Render.
	 */
	static auto create(const Type& iType) -> uniq<RenderAPI>;

	/**
	 * @brief
	 *  Get the actual API state.
	 * @return API State.
	 */
	[[nodiscard]] auto getState() const -> State { return m_state; }

	/**
	 * @brief
	 *  Check if the API type require initializations.
	 * @return tRue if initialization required.
	 */
	[[nodiscard]] auto requireInit() const -> bool { return m_type == Type::OpenGL || m_type == Type::Vulkan; }

	/**
	 * @brief
	 *  Reset value for the frame to render.
	 */
	virtual void beginFrame() {}

	/**
	 * @brief
	 *  Reset value for the batch to render.
	 */
	virtual void beginBatch() {}

	/**
	 * @brief
	 *  Reset value for the texture load.
	 */
	virtual void beginTextureLoad() {}

	/**
	 * @brief
	 *  Ends texture load.
	 */
	virtual void endTextureLoad() {}

	/**
	 * @brief
	 *  Ends draw call for the current batch.
	 */
	virtual void endBatch() {}

	/**
	 * @brief
	 *  Change the subpass.
	 */
	virtual void nextSubpass() {}

	/**
	 * @brief
	 *  Ends draw call for the current frame.
	 */
	virtual void endFrame() {}

	/**
	 * @brief
	 *  Enable or disable depth buffer writing.
	 * @param[in] iEnabled True to enable depth writing, false to disable.
	 */
	virtual void setDepthMask([[maybe_unused]] bool iEnabled) {}

	/**
	 * @brief
	 *  Enable or disable depth testing.
	 * @param[in] iEnabled True to enable depth testing, false to disable.
	 */
	virtual void setDepthTest([[maybe_unused]] bool iEnabled) {}

	/**
	 * @brief
	 *  Fence between compute writes to SSBOs and downstream graphics reads
	 *  (vertex-attribute pulls, vertex/fragment shader SSBO/UBO loads).
	 *  Must be issued by any caller that runs a `ComputeShader::dispatch()`
	 *  before consuming the output buffer in the draw pass. No-op on the
	 *  null backend.
	 */
	virtual void storageBufferMemoryBarrier() {}

	/**
	 * @brief
	 *  GPU-driven indexed indirect draw. Reads `iMaxDrawCount`
	 *  `DrawIndexedIndirectCommand` records from `iCommandBuffer`
	 *  (starting at offset 0) and `uint32_t` drawCount from
	 *  `iCountBuffer` at offset 0. The actual emitted draw count is
	 *  clamped to `min(*iCountBuffer, iMaxDrawCount)`. Vulkan: maps to
	 *  `vkCmdDrawIndexedIndirectCount`. OpenGL: maps to
	 *  `glMultiDrawElementsIndirectCount`. Null: no-op.
	 * @param[in] iData Draw data with bound vertex / index buffers.
	 * @param[in] iCommandBuffer SSBO carrying the indirect commands
	 *  (also usable as VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT /
	 *  GL_DRAW_INDIRECT_BUFFER).
	 * @param[in] iCountBuffer Single-uint SSBO carrying the actual
	 *  command count.
	 * @param[in] iMaxDrawCount Upper bound on the emitted draw count.
	 */
	virtual void drawIndexedIndirect([[maybe_unused]] const shared<DrawData>& iData,
									 [[maybe_unused]] const shared<StorageBuffer>& iCommandBuffer,
									 [[maybe_unused]] const shared<StorageBuffer>& iCountBuffer,
									 [[maybe_unused]] uint32_t iMaxDrawCount) {}

protected:
	/**
	 * @brief
	 *  Define the API State.
	 * @param[in] iState The new API State.
	 */
	void setState(const State& iState) { m_state = iState; }

private:
	/// Type of Renderer API.
	Type m_type = Type::Null;
	/// The current state of the API.
	State m_state = State::Created;
};

}// namespace owl::renderer::gpu
