/**
 * @file Buffer.cpp
 * @author Silmaen
 * @date 08/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Buffer.h"
#include "core/external/opengl46.h"

namespace owl::renderer::opengl {

VertexBuffer::VertexBuffer(const uint32_t iSize) {
	OWL_PROFILE_FUNCTION()

	glCreateBuffers(1, &m_rendererId);
	glBindBuffer(GL_ARRAY_BUFFER, m_rendererId);
	glBufferData(GL_ARRAY_BUFFER, iSize, nullptr, GL_DYNAMIC_DRAW);
}

VertexBuffer::VertexBuffer(const float *iVertices, const uint32_t iSize) {
	OWL_PROFILE_FUNCTION()

	glCreateBuffers(1, &m_rendererId);
	glBindBuffer(GL_ARRAY_BUFFER, m_rendererId);
	glBufferData(GL_ARRAY_BUFFER, iSize, iVertices, GL_STATIC_DRAW);
}

VertexBuffer::~VertexBuffer() {
	OWL_PROFILE_FUNCTION()

	glDeleteBuffers(1, &m_rendererId);
}

void VertexBuffer::bind() const {
	OWL_PROFILE_FUNCTION()

	glBindBuffer(GL_ARRAY_BUFFER, m_rendererId);
}

void VertexBuffer::unbind() const {
	OWL_PROFILE_FUNCTION()

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VertexBuffer::setData(const void *iData, const uint32_t iSize) {
	glBindBuffer(GL_ARRAY_BUFFER, m_rendererId);
	glBufferSubData(GL_ARRAY_BUFFER, 0, iSize, iData);
}


IndexBuffer::IndexBuffer(const uint32_t *iIndices, const uint32_t iCount) : m_count(iCount) {
	OWL_PROFILE_FUNCTION()

	glCreateBuffers(1, &m_rendererId);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rendererId);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizei>(m_count * sizeof(uint32_t)), iIndices,
				 GL_STATIC_DRAW);
}

IndexBuffer::~IndexBuffer() {
	OWL_PROFILE_FUNCTION()

	glDeleteBuffers(1, &m_rendererId);
}

void IndexBuffer::bind() const {
	OWL_PROFILE_FUNCTION()

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rendererId);
}

void IndexBuffer::unbind() const {
	OWL_PROFILE_FUNCTION()

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

}// namespace owl::renderer::opengl
