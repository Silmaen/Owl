/**
 * @file DrawData.cpp
 * @author Silmaen
 * @date 05/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "DrawData.h"
#include "renderer/Renderer.h"

namespace owl::renderer::gpu::opengl {

DrawData::~DrawData() = default;

void DrawData::init(const BufferLayout& iLayout, const std::string& iRenderer, std::vector<uint32_t>& iIndices,
					const std::string& iShaderName) {
	if (iLayout.getStride() > 0) {
		mp_vertexArray = mkShared<VertexArray>();
		mp_vertexBuffer = mkShared<VertexBuffer>(iLayout.getStride() * iIndices.size());
		mp_vertexBuffer->setLayout(iLayout);
		mp_vertexArray->addVertexBuffer(mp_vertexBuffer);
		mp_vertexArray->setIndexBuffer(mkShared<IndexBuffer>(iIndices.data(), iIndices.size()));
		setShader(iShaderName, iRenderer);
	}
}

void DrawData::initInstanced(const BufferLayout& iVertexLayout, const BufferLayout& iInstanceLayout,
							 const uint32_t iVertexCapacity, const uint32_t iInstanceCapacity,
							 const std::string& iRenderer, std::vector<uint32_t>& iIndices,
							 const std::string& iShaderName) {
	if (iVertexLayout.getStride() == 0 || iInstanceLayout.getStride() == 0)
		return;
	mp_vertexArray = mkShared<VertexArray>();
	mp_vertexBuffer = mkShared<VertexBuffer>(iVertexLayout.getStride() * iVertexCapacity);
	mp_vertexBuffer->setLayout(iVertexLayout);
	mp_vertexArray->addVertexBuffer(mp_vertexBuffer);
	mp_instanceBuffer = mkShared<VertexBuffer>(iInstanceLayout.getStride() * iInstanceCapacity);
	mp_instanceBuffer->setLayout(iInstanceLayout);
	mp_vertexArray->addInstanceBuffer(mp_instanceBuffer);
	mp_vertexArray->setIndexBuffer(mkShared<IndexBuffer>(iIndices.data(), iIndices.size()));
	setShader(iShaderName, iRenderer);
}

void DrawData::bind() const {
	if (mp_shader)
		mp_shader->bind();
	if (mp_vertexArray)
		mp_vertexArray->bind();
}

void DrawData::unbind() const {
	if (mp_vertexArray)
		VertexArray::unbind();
	if (mp_shader)
		mp_shader->unbind();
}

void DrawData::setVertexData(const void* iData, const uint32_t iSize) {
	if (mp_vertexBuffer)
		mp_vertexBuffer->setData(iData, iSize);
}

void DrawData::setInstanceData(const void* iData, const uint32_t iSize) {
	if (mp_instanceBuffer)
		mp_instanceBuffer->setData(iData, iSize);
}

auto DrawData::getIndexCount() const -> uint32_t {
	if (mp_vertexArray)
		return mp_vertexArray->getIndexBuffer()->getCount();
	return 0;
}

void DrawData::setShader(const std::string& iShaderName, const std::string& iRenderer) {
	auto& shLib = Renderer::getShaderLibrary();
	const auto baseName = Shader::composeName({.name = iShaderName, .renderer = iRenderer});
	if (!shLib.exists(baseName))
		shLib.load(baseName);
	mp_shader = static_pointer_cast<Shader>(shLib.get(baseName));
}

}// namespace owl::renderer::gpu::opengl
