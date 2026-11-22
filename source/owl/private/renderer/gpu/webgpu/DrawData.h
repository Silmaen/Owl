/**
 * @file DrawData.h
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Buffer.h"
#include "Shader.h"
#include "core/external/webgpu.h"
#include "renderer/gpu/DrawData.h"

#include <cstdint>
#include <string>
#include <vector>

namespace owl::renderer::gpu::webgpu {

struct Block;

/**
 * @brief
 *  WebGPU draw: its buffers, its baked shader and the render pipeline made from them for the canvas.
 */
class OWL_API DrawData final : public renderer::gpu::DrawData {
public:
	DrawData() = default;
	DrawData(const DrawData&) = delete;
	DrawData(DrawData&&) = delete;
	auto operator=(const DrawData&) -> DrawData& = delete;
	auto operator=(DrawData&&) -> DrawData& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~DrawData() override = default;

	/**
	 * @brief
	 *  Initialises a draw of vertices.
	 * @param[in] iLayout Layout of the vertices.
	 * @param[in] iRenderer Renderer of the shader.
	 * @param[in] iIndices The indices.
	 * @param[in] iShaderName Name of the shader.
	 * @param[in] iState Pipeline state.
	 */
	void init(const BufferLayout& iLayout, const std::string& iRenderer, std::vector<uint32_t>& iIndices,
			  const std::string& iShaderName, const PipelineState& iState) override;

	/**
	 * @brief
	 *  Initialises a draw of instanced vertices.
	 * @param[in] iVertexLayout Layout of the vertices.
	 * @param[in] iInstanceLayout Layout of the instances.
	 * @param[in] iVertexCapacity Vertex capacity.
	 * @param[in] iInstanceCapacity Instance capacity.
	 * @param[in] iRenderer Renderer of the shader.
	 * @param[in] iIndices The indices.
	 * @param[in] iShaderName Name of the shader.
	 * @param[in] iState Pipeline state.
	 */
	void initInstanced(const BufferLayout& iVertexLayout, const BufferLayout& iInstanceLayout, uint32_t iVertexCapacity,
					   uint32_t iInstanceCapacity, const std::string& iRenderer, std::vector<uint32_t>& iIndices,
					   const std::string& iShaderName, const PipelineState& iState) override;

	/**
	 * @brief
	 *  Nothing to do: the draw binds its pipeline.
	 */
	void bind() const override {}

	/**
	 * @brief
	 *  Nothing to do: the draw binds its pipeline.
	 */
	void unbind() const override {}

	/**
	 * @brief
	 *  Writes the vertices.
	 * @param[in] iData The vertices.
	 * @param[in] iSize Their size in bytes.
	 */
	void setVertexData(const void* iData, uint32_t iSize) override;

	/**
	 * @brief
	 *  Writes the instances.
	 * @param[in] iData The instances.
	 * @param[in] iSize Their size in bytes.
	 */
	void setInstanceData(const void* iData, uint32_t iSize) override;

	/**
	 * @brief
	 *  Number of indices.
	 * @return The count.
	 */
	[[nodiscard]] auto getIndexCount() const -> uint32_t override;

	/**
	 * @brief
	 *  Takes the shader of the draw from the shader library.
	 * @param[in] iShaderName Name of the shader.
	 * @param[in] iRenderer Its renderer.
	 */
	void setShader(const std::string& iShaderName, const std::string& iRenderer) override;

	/**
	 * @brief
	 *  The render pipeline for the formats of a target, made at their first use.
	 * @param[in] iFormats Formats of the colour attachments.
	 * @param[in] iDepth True when the target has a depth attachment.
	 * @return The pipeline, null when the shader has no baked stage.
	 */
	[[nodiscard]] auto getPipeline(const std::vector<wgpu::TextureFormat>& iFormats, bool iDepth) const
			-> wgpu::RenderPipeline;

	/**
	 * @brief
	 *  The block of bindings of the draw.
	 * @return The block, or nullptr.
	 */
	[[nodiscard]] auto getBlock() const -> const Block* { return mp_block; }

	/**
	 * @brief
	 *  The vertex buffer.
	 * @return The buffer, or nullptr.
	 */
	[[nodiscard]] auto getVertexBuffer() const -> const shared<VertexBuffer>& { return mp_vertexBuffer; }

	/**
	 * @brief
	 *  The instance buffer.
	 * @return The buffer, or nullptr.
	 */
	[[nodiscard]] auto getInstanceBuffer() const -> const shared<VertexBuffer>& { return mp_instanceBuffer; }

	/**
	 * @brief
	 *  The index buffer.
	 * @return The buffer, or nullptr.
	 */
	[[nodiscard]] auto getIndexBuffer() const -> const shared<IndexBuffer>& { return mp_indexBuffer; }

private:
	/// Vertices.
	shared<VertexBuffer> mp_vertexBuffer;
	/// Instances.
	shared<VertexBuffer> mp_instanceBuffer;
	/// Indices.
	shared<IndexBuffer> mp_indexBuffer;
	/// The shader.
	shared<Shader> mp_shader;
	/// The block of bindings active at the initialisation.
	const Block* mp_block = nullptr;
	/**
	 * @brief
	 *  A pipeline and the target it was made for.
	 */
	struct Variant {
		/// Formats of the colour attachments.
		std::vector<wgpu::TextureFormat> formats;
		/// Depth attachment.
		bool depth = false;
		/// The pipeline.
		wgpu::RenderPipeline pipeline;
	};
	/// The pipelines made, one per target layout.
	mutable std::vector<Variant> m_variants;
};

}// namespace owl::renderer::gpu::webgpu
