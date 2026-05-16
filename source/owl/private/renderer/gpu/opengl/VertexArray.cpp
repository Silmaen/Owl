/**
 * @file VertexArray.cpp
 * @author Silmaen
 * @date 08/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "VertexArray.h"
#include "core/external/opengl46.h"

namespace owl::renderer::gpu::opengl {

namespace utils {

namespace {
auto toGlBaseType(const ShaderDataType& iType) -> GLenum {
	switch (iType) {
		case ShaderDataType::Float:
		case ShaderDataType::Float2:
		case ShaderDataType::Float3:
		case ShaderDataType::Float4:
		case ShaderDataType::Mat3:
		case ShaderDataType::Mat4:
			return GL_FLOAT;
		case ShaderDataType::Int:
		case ShaderDataType::Int2:
		case ShaderDataType::Int3:
		case ShaderDataType::Int4:
			return GL_INT;
		case ShaderDataType::Bool:
			return GL_BOOL;
		case ShaderDataType::None:
			break;
	}

	OWL_CORE_ASSERT(false, "Unknown ShaderDataType!")
	return 0;
}
}// namespace
}// namespace utils

VertexArray::VertexArray() {
	OWL_PROFILE_FUNCTION()

	glCreateVertexArrays(1, &m_rendererId);
}

VertexArray::~VertexArray() {
	OWL_PROFILE_FUNCTION()

	glDeleteVertexArrays(1, &m_rendererId);
}

void VertexArray::bind() const {
	OWL_PROFILE_FUNCTION()

	glBindVertexArray(m_rendererId);
}

void VertexArray::unbind() {
	OWL_PROFILE_FUNCTION()

	glBindVertexArray(0);
}

namespace {
void wireBufferAttributes(uint32_t& ioVertexBufferIndex, const BufferLayout& iLayout, bool iPerInstance) {
	// NOLINTBEGIN(performance-no-int-to-ptr)
	for (const auto& element: iLayout) {
		const auto count = static_cast<int32_t>(element.getComponentCount());
		const auto type = utils::toGlBaseType(element.type);
		const auto stride = static_cast<int>(iLayout.getStride());
		switch (element.type) {
			case ShaderDataType::Float:
			case ShaderDataType::Float2:
			case ShaderDataType::Float3:
			case ShaderDataType::Float4:
				{
					glEnableVertexAttribArray(ioVertexBufferIndex);
					glVertexAttribPointer(ioVertexBufferIndex, count, type, element.normalized ? GL_TRUE : GL_FALSE,
										  stride, reinterpret_cast<const void*>(element.offset));
					if (iPerInstance)
						glVertexAttribDivisor(ioVertexBufferIndex, 1);
					ioVertexBufferIndex++;
					break;
				}
			case ShaderDataType::Int:
			case ShaderDataType::Int2:
			case ShaderDataType::Int3:
			case ShaderDataType::Int4:
			case ShaderDataType::Bool:
				{
					glEnableVertexAttribArray(ioVertexBufferIndex);
					glVertexAttribIPointer(ioVertexBufferIndex, count, type, stride,
										   reinterpret_cast<const void*>(element.offset));
					if (iPerInstance)
						glVertexAttribDivisor(ioVertexBufferIndex, 1);
					ioVertexBufferIndex++;
					break;
				}
			case ShaderDataType::Mat3:
			case ShaderDataType::Mat4:
				{
					for (int32_t i = 0; i < count; i++) {
						glEnableVertexAttribArray(ioVertexBufferIndex);
						glVertexAttribPointer(
								ioVertexBufferIndex, count, type, element.normalized ? GL_TRUE : GL_FALSE, stride,
								reinterpret_cast<const void*>(element.offset +
															  (sizeof(float) * static_cast<uint32_t>(count * i))));
						glVertexAttribDivisor(ioVertexBufferIndex, 1);
						ioVertexBufferIndex++;
					}
					break;
				}
			case ShaderDataType::None:
				OWL_CORE_ASSERT(false, "Unknown ShaderDataType!")
				break;
		}
	}
	// NOLINTEND(performance-no-int-to-ptr)
}
}// namespace

void VertexArray::addVertexBuffer(const VertexBuf& iVertexBuffer) {
	OWL_PROFILE_FUNCTION()

	OWL_CORE_ASSERT(!iVertexBuffer->getLayout().getElements().empty(), "Vertex Buffer has no layout!")

	glBindVertexArray(m_rendererId);
	iVertexBuffer->bind();
	wireBufferAttributes(m_vertexBufferIndex, iVertexBuffer->getLayout(), /*iPerInstance=*/false);
	m_vertexBuffers.push_back(iVertexBuffer);
}

void VertexArray::addInstanceBuffer(const VertexBuf& iVertexBuffer) {
	OWL_PROFILE_FUNCTION()

	OWL_CORE_ASSERT(!iVertexBuffer->getLayout().getElements().empty(), "Instance Buffer has no layout!")

	glBindVertexArray(m_rendererId);
	iVertexBuffer->bind();
	wireBufferAttributes(m_vertexBufferIndex, iVertexBuffer->getLayout(), /*iPerInstance=*/true);
	m_vertexBuffers.push_back(iVertexBuffer);
}

void VertexArray::setIndexBuffer(const IndexBuf& iIndexBuffer) {
	OWL_PROFILE_FUNCTION()

	glBindVertexArray(m_rendererId);
	iIndexBuffer->bind();

	m_indexBuffer = iIndexBuffer;
}


}// namespace owl::renderer::gpu::opengl
