/**
 * @file RenderCommand.cpp
 * @author Silmaen
 * @date 09/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RenderCommand.h"

namespace owl::renderer {

uniq<RenderAPI> RenderCommand::mu_renderAPI = nullptr;

void RenderCommand::create(const RenderAPI::Type& iType) { mu_renderAPI = RenderAPI::create(iType); }

auto RenderCommand::getState() -> RenderAPI::State {
	if (mu_renderAPI)
		return mu_renderAPI->getState();
	return RenderAPI::State::Error;
}

}// namespace owl::renderer
