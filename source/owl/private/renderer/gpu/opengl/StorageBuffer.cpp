/**
 * @file StorageBuffer.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "StorageBuffer.h"

#include "core/external/opengl46.h"

namespace owl::renderer::gpu::opengl {

StorageBuffer::StorageBuffer(const uint32_t iSize, const uint32_t iBinding) : m_binding{iBinding} {
	glCreateBuffers(1, &m_rendererId);
	glNamedBufferData(m_rendererId, iSize, nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_binding, m_rendererId);
}

StorageBuffer::~StorageBuffer() {
	if (m_rendererId != 0)
		glDeleteBuffers(1, &m_rendererId);
	m_rendererId = 0;
}

void StorageBuffer::setData(const void* iData, const uint32_t iSize, const uint32_t iOffset) {
	if (m_rendererId == 0 || iData == nullptr || iSize == 0)
		return;
	glNamedBufferSubData(m_rendererId, iOffset, iSize, iData);
}

void StorageBuffer::bind() { glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_binding, m_rendererId); }

}// namespace owl::renderer::gpu::opengl
