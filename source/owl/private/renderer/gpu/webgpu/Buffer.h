/**
 * @file Buffer.h
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/webgpu.h"
#include "renderer/gpu/Buffer.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/gpu/UniformBuffer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace owl::renderer::gpu::webgpu {

/**
 * @brief
 *  Makes a buffer whose size WebGPU accepts (a multiple of 4, at least 16 bytes).
 * @param[in] iSize Size in bytes.
 * @param[in] iUsage Usage of the buffer (CopyDst is added).
 * @param[in] iLabel Label for the messages of the browser.
 * @return The buffer.
 */
auto makeBuffer(uint64_t iSize, wgpu::BufferUsage iUsage, const char* iLabel) -> wgpu::Buffer;

/**
 * @brief
 *  Writes data into a buffer through the queue, padded to the 4 bytes a write needs.
 * @param[in] iBuffer The buffer.
 * @param[in] iOffset Offset in bytes.
 * @param[in] iData The data.
 * @param[in] iSize Size of the data in bytes.
 */
void writeBuffer(const wgpu::Buffer& iBuffer, uint64_t iOffset, const void* iData, uint64_t iSize);

/**
 * @brief
 *  WebGPU vertex buffer.
 */
class OWL_API VertexBuffer final : public renderer::gpu::VertexBuffer {
public:
	VertexBuffer(const VertexBuffer&) = delete;
	VertexBuffer(VertexBuffer&&) = delete;
	auto operator=(const VertexBuffer&) -> VertexBuffer& = delete;
	auto operator=(VertexBuffer&&) -> VertexBuffer& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iSize Size in bytes.
	 */
	explicit VertexBuffer(uint32_t iSize);

	/**
	 * @brief
	 *  Destructor.
	 */
	~VertexBuffer() override = default;

	/**
	 * @brief
	 *  Nothing to do: the draw binds its buffers.
	 */
	void bind() const override {}

	/**
	 * @brief
	 *  Nothing to do: the draw binds its buffers.
	 */
	void unbind() const override {}

	/**
	 * @brief
	 *  Writes the data at the start of the buffer.
	 * @param[in] iData The data.
	 * @param[in] iSize Its size in bytes.
	 */
	void setData(const void* iData, uint32_t iSize) override;

	/**
	 * @brief
	 *  The WebGPU buffer.
	 * @return The buffer.
	 */
	[[nodiscard]] auto getBuffer() const -> const wgpu::Buffer& { return m_buffer; }

private:
	/// The WebGPU buffer.
	wgpu::Buffer m_buffer;
};

/**
 * @brief
 *  WebGPU index buffer, 32-bit indices.
 */
class OWL_API IndexBuffer final : public renderer::gpu::IndexBuffer {
public:
	IndexBuffer(const IndexBuffer&) = delete;
	IndexBuffer(IndexBuffer&&) = delete;
	auto operator=(const IndexBuffer&) -> IndexBuffer& = delete;
	auto operator=(IndexBuffer&&) -> IndexBuffer& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iIndices The indices.
	 * @param[in] iCount Their number.
	 */
	IndexBuffer(const uint32_t* iIndices, uint32_t iCount);

	/**
	 * @brief
	 *  Destructor.
	 */
	~IndexBuffer() override;

	/**
	 * @brief
	 *  Nothing to do: the draw binds its buffers.
	 */
	void bind() const override {}

	/**
	 * @brief
	 *  Nothing to do: the draw binds its buffers.
	 */
	void unbind() const override {}

	/**
	 * @brief
	 *  Number of indices.
	 * @return The count.
	 */
	[[nodiscard]] auto getCount() const -> uint32_t override { return m_count; }

	/**
	 * @brief
	 *  The WebGPU buffer.
	 * @return The buffer.
	 */
	[[nodiscard]] auto getBuffer() const -> const wgpu::Buffer& { return m_buffer; }

private:
	/// The WebGPU buffer.
	wgpu::Buffer m_buffer;
	/// Number of indices.
	uint32_t m_count = 0;
};

/**
 * @brief
 *  WebGPU uniform buffer, bound at its binding of group 0.
 */
class UniformBuffer final : public renderer::gpu::UniformBuffer {
public:
	UniformBuffer(const UniformBuffer&) = delete;
	UniformBuffer(UniformBuffer&&) = delete;
	auto operator=(const UniformBuffer&) -> UniformBuffer& = delete;
	auto operator=(UniformBuffer&&) -> UniformBuffer& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iSize Size in bytes.
	 * @param[in] iBinding Binding of group 0.
	 * @param[in] iRenderer Renderer block of the buffer, empty for the active one.
	 */
	UniformBuffer(uint32_t iSize, uint32_t iBinding, std::string iRenderer);

	/**
	 * @brief
	 *  Destructor.
	 */
	~UniformBuffer() override = default;

	/**
	 * @brief
	 *  Binds the buffer for the next draws.
	 */
	void bind() override;

	/**
	 * @brief
	 *  Writes data into the buffer.
	 * @param[in] iData The data.
	 * @param[in] iSize Its size in bytes.
	 * @param[in] iOffset Offset in bytes.
	 */
	void setData(const void* iData, uint32_t iSize, uint32_t iOffset) override;

private:
	/// The WebGPU buffer.
	wgpu::Buffer m_buffer;
	/// Renderer block of the buffer.
	std::string m_renderer;
	/// Binding of group 0.
	uint32_t m_binding = 0;
};

/**
 * @brief
 *  WebGPU storage buffer, bound at a binding of group 0.
 */
class StorageBuffer final : public renderer::gpu::StorageBuffer {
public:
	StorageBuffer(const StorageBuffer&) = delete;
	StorageBuffer(StorageBuffer&&) = delete;
	auto operator=(const StorageBuffer&) -> StorageBuffer& = delete;
	auto operator=(StorageBuffer&&) -> StorageBuffer& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iSize Size in bytes.
	 * @param[in] iBinding Default binding of group 0.
	 * @param[in] iRenderer Renderer block of the buffer, empty for the active one.
	 */
	StorageBuffer(uint32_t iSize, uint32_t iBinding, std::string iRenderer);

	/**
	 * @brief
	 *  Destructor.
	 */
	~StorageBuffer() override = default;

	/**
	 * @brief
	 *  Writes data into the buffer.
	 * @param[in] iData The data.
	 * @param[in] iSize Its size in bytes.
	 * @param[in] iOffset Offset in bytes.
	 */
	void setData(const void* iData, uint32_t iSize, uint32_t iOffset) override;

	/**
	 * @brief
	 *  Reads the buffer back: a page maps a buffer asynchronously only, so it gives the last data written here.
	 * @param[out] oData Destination.
	 * @param[in] iSize Size in bytes.
	 * @param[in] iOffset Offset in bytes.
	 */
	void getData(void* oData, uint32_t iSize, uint32_t iOffset) override;

	/**
	 * @brief
	 *  Binds the buffer at its default binding.
	 */
	void bind() override;

	/**
	 * @brief
	 *  Binds the buffer at a binding.
	 * @param[in] iBinding Binding of group 0.
	 */
	void bind(uint32_t iBinding) override;

	/**
	 * @brief
	 *  The WebGPU buffer.
	 * @return The buffer.
	 */
	[[nodiscard]] auto getBuffer() const -> const wgpu::Buffer& { return m_buffer; }

private:
	/// The WebGPU buffer.
	wgpu::Buffer m_buffer;
	/// Copy of the data written, read back by getData.
	std::vector<uint8_t> m_shadow;
	/// Renderer block of the buffer.
	std::string m_renderer;
	/// Default binding of group 0.
	uint32_t m_binding = 0;
};

}// namespace owl::renderer::gpu::webgpu
