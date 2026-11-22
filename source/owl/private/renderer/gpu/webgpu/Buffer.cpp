/**
 * @file Buffer.cpp
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Buffer.h"

#include "Context.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <utility>

namespace owl::renderer::gpu::webgpu {

namespace {
constexpr uint64_t g_minBufferSize = 16;

auto alignUp(const uint64_t iSize) -> uint64_t { return (iSize + 3) & ~uint64_t{3}; }
}// namespace

auto makeBuffer(const uint64_t iSize, const wgpu::BufferUsage iUsage, const char* iLabel) -> wgpu::Buffer {
	const wgpu::BufferDescriptor desc{.label = iLabel,
									  .usage = iUsage | wgpu::BufferUsage::CopyDst,
									  .size = std::max(alignUp(iSize), g_minBufferSize)};
	return Context::get().device.CreateBuffer(&desc);
}

void writeBuffer(const wgpu::Buffer& iBuffer, const uint64_t iOffset, const void* iData, const uint64_t iSize) {
	if (iBuffer == nullptr || iData == nullptr || iSize == 0)
		return;
	// A write takes a multiple of 4 bytes: the last bytes go through a padded word.
	const auto size = static_cast<size_t>(iSize);
	const size_t whole = size & ~size_t{3};
	auto& queue = Context::get().queue;
	if (whole > 0)
		queue.WriteBuffer(iBuffer, iOffset, iData, whole);
	if (const size_t rest = size - whole; rest > 0) {
		std::array<uint8_t, 4> tail{};
		std::memcpy(tail.data(), static_cast<const uint8_t*>(iData) + whole, rest);
		queue.WriteBuffer(iBuffer, iOffset + whole, tail.data(), tail.size());
	}
}

VertexBuffer::VertexBuffer(const uint32_t iSize) : m_buffer{makeBuffer(iSize, wgpu::BufferUsage::Vertex, "Vertices")} {}

void VertexBuffer::setData(const void* iData, const uint32_t iSize) { writeBuffer(m_buffer, 0, iData, iSize); }

IndexBuffer::IndexBuffer(const uint32_t* iIndices, const uint32_t iCount)
	: m_buffer{makeBuffer(uint64_t{iCount} * sizeof(uint32_t), wgpu::BufferUsage::Index, "Indices")}, m_count{iCount} {
	writeBuffer(m_buffer, 0, iIndices, uint64_t{iCount} * sizeof(uint32_t));
}

IndexBuffer::~IndexBuffer() = default;

UniformBuffer::UniformBuffer(const uint32_t iSize, const uint32_t iBinding, std::string iRenderer)
	: m_buffer{makeBuffer(iSize, wgpu::BufferUsage::Uniform, "Uniforms")}, m_renderer{std::move(iRenderer)},
	  m_binding{iBinding} {}

void UniformBuffer::bind() { Context::get().bindBuffer(m_renderer, m_binding, m_buffer); }

void UniformBuffer::setData(const void* iData, const uint32_t iSize, const uint32_t iOffset) {
	writeBuffer(m_buffer, iOffset, iData, iSize);
}

StorageBuffer::StorageBuffer(const uint32_t iSize, const uint32_t iBinding, std::string iRenderer)
	: m_buffer{makeBuffer(iSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::Indirect | wgpu::BufferUsage::CopySrc,
						  "Storage")},
	  m_shadow(iSize, 0), m_renderer{std::move(iRenderer)}, m_binding{iBinding} {}

void StorageBuffer::setData(const void* iData, const uint32_t iSize, const uint32_t iOffset) {
	writeBuffer(m_buffer, iOffset, iData, iSize);
	if (iData != nullptr && uint64_t{iOffset} + iSize <= m_shadow.size())
		std::memcpy(m_shadow.data() + iOffset, iData, iSize);
}

void StorageBuffer::getData(void* oData, const uint32_t iSize, const uint32_t iOffset) {
	if (oData == nullptr || uint64_t{iOffset} + iSize > m_shadow.size())
		return;
	std::memcpy(oData, m_shadow.data() + iOffset, iSize);
}

void StorageBuffer::bind() { Context::get().bindBuffer(m_renderer, m_binding, m_buffer); }

void StorageBuffer::bind(const uint32_t iBinding) { Context::get().bindBuffer(m_renderer, iBinding, m_buffer); }

}// namespace owl::renderer::gpu::webgpu
