/**
 * @file Texture.cpp
 * @author Silmaen
 * @date 30/07/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Texture.h"

namespace owl::renderer::gpu::null {

Texture2D::Texture2D(std::filesystem::path iPath) : renderer::gpu::Texture2D{std::move(iPath)} {
	if (exists(m_path))
		m_specification.size = {1, 1};
}

Texture2D::Texture2D(const Specification& iSpecs) : renderer::gpu::Texture2D{iSpecs} {}

Texture2D::~Texture2D() = default;

void Texture2D::bind(uint32_t) const {}

void Texture2D::setData(void*, uint32_t) {}

auto Texture2D::setLevels(const TextureLevels& iLevels) -> bool {
	if (iLevels.levels.empty()) {
		OWL_CORE_WARN("Null Texture: No level to set.")
		return false;
	}
	m_specification.size = iLevels.size;
	m_specification.format = iLevels.format;
	m_specification.levelCount = static_cast<uint32_t>(iLevels.levels.size());
	return true;
}

}// namespace owl::renderer::gpu::null
