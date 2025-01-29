/**
 * @file RenderAPI.cpp
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "RenderAPI.h"

namespace owl::renderer::null {

void RenderAPI::init() {
	OWL_PROFILE_FUNCTION()

	if (getState() != State::Created)
		return;

	// renderer is now ready
	setState(State::Ready);
}

void RenderAPI::setViewport([[maybe_unused]] const uint32_t iX, [[maybe_unused]] const uint32_t iY,
							[[maybe_unused]] const uint32_t iWidth, [[maybe_unused]] const uint32_t iHeight) {}

void RenderAPI::setClearColor([[maybe_unused]] const math::vec4& iColor) {}

void RenderAPI::clear() {}

void RenderAPI::drawData([[maybe_unused]] const shared<DrawData>& iData, [[maybe_unused]] uint32_t iIndexCount) {}

void RenderAPI::drawLine([[maybe_unused]] const shared<DrawData>& iData, [[maybe_unused]] uint32_t iIndexCount) {}

}// namespace owl::renderer::null
