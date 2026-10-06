/**
 * @file RenderCommand.cpp
 * @author Silmaen
 * @date 09/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/gpu/RenderCommand.h"

#include <cstdint>

namespace owl::renderer::gpu {

uniq<RenderAPI> RenderCommand::m_renderAPI = nullptr;
uint64_t RenderCommand::m_drawCallCount = 0;

void RenderCommand::create(const RenderAPI::Type& iType) { m_renderAPI = RenderAPI::create(iType); }

auto RenderCommand::getState() -> RenderAPI::State {
	if (m_renderAPI)
		return m_renderAPI->getState();
	return RenderAPI::State::Error;
}

}// namespace owl::renderer::gpu
