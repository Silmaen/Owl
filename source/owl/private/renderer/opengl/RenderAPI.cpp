/**
 * @file RenderAPI.cpp
 * @author Silmaen
 * @date 09/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/Application.h"
#include "RenderAPI.h"
#include "core/external/opengl46.h"

namespace owl::renderer::opengl {
namespace {
[[maybe_unused]] void messageCallback(unsigned iSource, unsigned iType, unsigned iId, const unsigned iSeverity,
									  [[maybe_unused]] int iLength, const char* iMessage, const void* iUserParam) {
	switch (iSeverity) {
		case GL_DEBUG_SEVERITY_HIGH:
			OWL_CORE_CRITICAL("OpenGL: {}({})-{} : {} / {}", iSource, iType, iId, iMessage, iUserParam)
			return;
		case GL_DEBUG_SEVERITY_MEDIUM:
			OWL_CORE_ERROR("OpenGL: {}({})-{} : {} / {}", iSource, iType, iId, iMessage, iUserParam)
			return;
		case GL_DEBUG_SEVERITY_LOW:
			OWL_CORE_WARN("OpenGL: {}({})-{} : {} / {}", iSource, iType, iId, iMessage, iUserParam)
			return;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			OWL_CORE_INFO("OpenGL: {}({})-{} : {} / {}", iSource, iType, iId, iMessage, iUserParam)
			return;
		default:
			OWL_CORE_TRACE("OpenGL: {}({})-{} : {} / {}", iSource, iType, iId, iMessage, iUserParam)
	}

	OWL_CORE_ASSERT(false, "Unknown severity level!")
}
}// namespace

void RenderAPI::init() {
	OWL_PROFILE_FUNCTION()

	auto [major, minor] = core::Application::get().getWindow().getGraphContext()->getVersion();
	if (const bool goodVersion = major > 4 || (major == 4 && minor >= 5); !goodVersion) {
		setState(State::Error);
		OWL_CORE_ERROR("Owl Engine OpenGL Renderer requires at least OpenGL version 4.5 but version {}.{} found", major,
					   minor)
	}

	if (getState() != State::Created)
		return;
#ifdef OWL_DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(messageCallback, nullptr);

	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
#endif

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LINE_SMOOTH);

	// renderer is now ready
	setState(State::Ready);
}

void RenderAPI::setViewport(const uint32_t iX, const uint32_t iY, const uint32_t iWidth, const uint32_t iHeight) {
	glViewport(static_cast<int32_t>(iX), static_cast<int32_t>(iY), static_cast<int32_t>(iWidth),
			   static_cast<int32_t>(iHeight));
}

void RenderAPI::setClearColor(const math::vec4& iColor) {
	glClearColor(iColor.r(), iColor.g(), iColor.b(), iColor.a());
}

void RenderAPI::clear() { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }

void RenderAPI::drawData(const shared<DrawData>& iData, const uint32_t iIndexCount) {
	iData->bind();
	const uint32_t count = (iIndexCount != 0u) ? iIndexCount : iData->getIndexCount();
	glDrawElements(GL_TRIANGLES, static_cast<int32_t>(count), GL_UNSIGNED_INT, nullptr);
}

void RenderAPI::drawLine(const shared<DrawData>& iData, const uint32_t iIndexCount) {
	iData->bind();
	const uint32_t count = (iIndexCount != 0u) ? iIndexCount : iData->getIndexCount();
	glLineWidth(2.0f);
	glDrawArrays(GL_LINES, 0, static_cast<int32_t>(count));
}

auto RenderAPI::getMaxTextureSlots() const -> uint32_t {
	int32_t textureUnits = 0;
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &textureUnits);
	return std::min(32u, static_cast<uint32_t>(textureUnits));
}

void RenderAPI::setDepthMask(const bool iEnabled) { glDepthMask(iEnabled ? GL_TRUE : GL_FALSE); }

}// namespace owl::renderer::opengl
