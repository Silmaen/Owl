/**
 * @file Buffer.cpp
 * @author Silmen
 * @date 08/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Buffer.h"
#include "Renderer.h"

#include "opengl/Buffer.h"

namespace owl::renderer {

shrd<VertexBuffer> VertexBuffer::create(float *vertices, uint32_t size) {
	switch (Renderer::getAPI()) {
		case APIType::None:
			OWL_CORE_ASSERT(false, "APIType::None is currently not supported!");
			return nullptr;
		case APIType::OpenGL:
			return mk_shrd<opengl::VertexBuffer>(vertices, size);
	}

	OWL_CORE_ASSERT(false, "Unknown API Type!");
	return nullptr;
}

shrd<IndexBuffer> IndexBuffer::create(uint32_t *indices, uint32_t size) {
	switch (Renderer::getAPI()) {
		case APIType::None:
			OWL_CORE_ASSERT(false, "APIType::None is currently not supported!");
			return nullptr;
		case APIType::OpenGL:
			return mk_shrd<opengl::IndexBuffer>(indices, size);
	}

	OWL_CORE_ASSERT(false, "Unknown API Type!");
	return nullptr;
}

}// namespace owl::renderer