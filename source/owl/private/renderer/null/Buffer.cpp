/**
 * @file Buffer.cpp
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Buffer.h"

namespace owl::renderer::null {

VertexBuffer::VertexBuffer([[maybe_unused]] uint32_t iSize) {}

VertexBuffer::VertexBuffer([[maybe_unused]] float *iVertices, [[maybe_unused]] uint32_t iSize) {}

VertexBuffer::~VertexBuffer() = default;

void VertexBuffer::bind() const {}

void VertexBuffer::unbind() const {}

void VertexBuffer::setData([[maybe_unused]] const void *iData, [[maybe_unused]] uint32_t iSize) {}

IndexBuffer::IndexBuffer([[maybe_unused]] uint32_t *iIndices, const uint32_t iCount) : m_count(iCount) {}

IndexBuffer::~IndexBuffer() = default;

void IndexBuffer::bind() const {}

void IndexBuffer::unbind() const {}

}// namespace owl::renderer::null
