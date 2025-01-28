/**
 * @file UniformBuffer.cpp
 * @author Silmaen
 * @date 02/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "UniformBuffer.h"
#include "core/external/opengl46.h"

namespace owl::renderer::opengl {

UniformBuffer::UniformBuffer(const uint32_t iSize, const uint32_t iBinding) {
	glCreateBuffers(1, &m_rendererId);
	glNamedBufferData(m_rendererId, iSize, nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, iBinding, m_rendererId);
}

UniformBuffer::~UniformBuffer() {
	glDeleteBuffers(1, &m_rendererId);
}

void UniformBuffer::setData(const void *iData, const uint32_t iSize, const uint32_t iOffset) {
	glNamedBufferSubData(m_rendererId, iOffset, iSize, iData);
}


}// namespace owl::renderer::opengl
