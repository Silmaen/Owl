/**
 * @file RenderAPI.cpp
 * @author Silmaen
 * @date 09/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "GpuProfiler.h"
#include "RenderAPI.h"
#include "Shader.h"
#include "StorageBuffer.h"
#include "app/Application.h"
#include "core/external/opengl46.h"

namespace owl::renderer::gpu::opengl {

namespace {
[[maybe_unused]] void messageCallback(unsigned iSource, unsigned iType, unsigned iId, const unsigned iSeverity,
									  [[maybe_unused]] int iLength, const char* iMessage, const void* iUserParam) {
	switch (iSeverity) {
		case GL_DEBUG_SEVERITY_HIGH:
			OWL_CORE_CRITICAL("OpenGL: {}({})-{} : {} / {}.", iSource, iType, iId, iMessage, iUserParam)
			return;
		case GL_DEBUG_SEVERITY_MEDIUM:
			OWL_CORE_ERROR("OpenGL: {}({})-{} : {} / {}.", iSource, iType, iId, iMessage, iUserParam)
			return;
		case GL_DEBUG_SEVERITY_LOW:
			OWL_CORE_WARN("OpenGL: {}({})-{} : {} / {}.", iSource, iType, iId, iMessage, iUserParam)
			return;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			OWL_CORE_INFO("OpenGL: {}({})-{} : {} / {}.", iSource, iType, iId, iMessage, iUserParam)
			return;
		default:
			OWL_CORE_TRACE("OpenGL: {}({})-{} : {} / {}.", iSource, iType, iId, iMessage, iUserParam)
	}

	OWL_CORE_ASSERT(false, "Unknown severity level!")
}
}// namespace

void RenderAPI::init() {
	OWL_PROFILE_FUNCTION()

	auto [major, minor] = app::Application::get().getWindow().getGraphContext()->getVersion();
	if (const bool goodVersion = major > 4 || (major == 4 && minor >= 5); !goodVersion) {
		setState(State::Error);
		OWL_CORE_ERROR("Owl Engine OpenGL Renderer requires at least OpenGL version 4.5 but version {}.{} found.",
					   major, minor)
	}

	if (getState() != State::Created)
		return;
#ifdef OWL_DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(messageCallback, nullptr);

	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
#endif

	OWL_CORE_INFO("OpenGL: Shaders loaded as {}.",
				  getShaderFormat() == ShaderFormat::Spirv ? "SPIR-V" : "GLSL 4.50 (no GL_ARB_gl_spirv)")

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Depth test stays off by default (2D is painter-ordered); Renderer3D enables it around 3D mesh draws.
	glDisable(GL_DEPTH_TEST);
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

void RenderAPI::drawDataInstanced(const shared<DrawData>& iData, const uint32_t iIndexCount,
								  const uint32_t iInstanceCount) {
	if (iInstanceCount == 0)
		return;
	iData->bind();
	const uint32_t count = (iIndexCount != 0u) ? iIndexCount : iData->getIndexCount();
	glDrawElementsInstanced(GL_TRIANGLES, static_cast<int32_t>(count), GL_UNSIGNED_INT, nullptr,
							static_cast<int32_t>(iInstanceCount));
}

void RenderAPI::drawLine(const shared<DrawData>& iData, const uint32_t iIndexCount) {
	iData->bind();
	const uint32_t count = (iIndexCount != 0u) ? iIndexCount : iData->getIndexCount();
	glLineWidth(2.0f);
	glDrawArrays(GL_LINES, 0, static_cast<int32_t>(count));
}

void RenderAPI::drawLineInstanced(const shared<DrawData>& iData, const uint32_t iIndexCount,
								  const uint32_t iInstanceCount) {
	if (iInstanceCount == 0)
		return;
	iData->bind();
	const uint32_t count = (iIndexCount != 0u) ? iIndexCount : iData->getIndexCount();
	glLineWidth(2.0f);
	glDrawElementsInstanced(GL_LINES, static_cast<int32_t>(count), GL_UNSIGNED_INT, nullptr,
							static_cast<int32_t>(iInstanceCount));
}

auto RenderAPI::getMaxTextureSlots() const -> uint32_t {
	int32_t textureUnits = 0;
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &textureUnits);
	return std::min(32u, static_cast<uint32_t>(textureUnits));
}

void RenderAPI::setDepthMask(const bool iEnabled) { glDepthMask(iEnabled ? GL_TRUE : GL_FALSE); }

void RenderAPI::setDepthTest(const bool iEnabled) {
	if (iEnabled)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
}

void RenderAPI::storageBufferMemoryBarrier() {
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT |
					GL_COMMAND_BARRIER_BIT);
}

void RenderAPI::drawIndexedIndirect(const shared<DrawData>& iData,
									const shared<renderer::gpu::StorageBuffer>& iCommandBuffer,
									const shared<renderer::gpu::StorageBuffer>& iCountBuffer,
									const uint32_t iMaxDrawCount) {
	if (!iData || !iCommandBuffer || !iCountBuffer || iMaxDrawCount == 0)
		return;
	iData->bind();
	const auto* cmdSsbo = dynamic_cast<const StorageBuffer*>(iCommandBuffer.get());
	const auto* countSsbo = dynamic_cast<const StorageBuffer*>(iCountBuffer.get());
	if (cmdSsbo == nullptr || countSsbo == nullptr || cmdSsbo->getHandle() == 0 || countSsbo->getHandle() == 0) {
		OWL_CORE_WARN("OpenGL: drawIndexedIndirect with non-OpenGL or empty SSBOs.")
		return;
	}
	if (glad_glMultiDrawElementsIndirectCount == nullptr) {
		OWL_CORE_WARN("OpenGL: drawIndexedIndirect needs OpenGL 4.6 (glMultiDrawElementsIndirectCount), skipped.")
		return;
	}
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, cmdSsbo->getHandle());
	glBindBuffer(GL_PARAMETER_BUFFER, countSsbo->getHandle());
	glMultiDrawElementsIndirectCount(GL_TRIANGLES, GL_UNSIGNED_INT, /*indirect=*/nullptr,
									 /*drawcount=*/0, static_cast<GLsizei>(iMaxDrawCount),
									 /*stride=*/static_cast<GLsizei>(sizeof(uint32_t) * 5));
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindBuffer(GL_PARAMETER_BUFFER, 0);
}

RenderAPI::~RenderAPI() {
	if (m_queriesCreated)
		glDeleteQueries(static_cast<GLsizei>(g_slotCount * 2), m_queries.front().data());
}

auto RenderAPI::hasGpuTimestamps() const -> bool {
	if (getState() != State::Ready)
		return false;
	GLint bits = 0;
	glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &bits);
	return bits > 0;
}

void RenderAPI::setGpuTimestampsEnabled(const bool iEnabled) {
	if (iEnabled == m_timingEnabled)
		return;
	if (!iEnabled) {
		m_timingEnabled = false;
		m_frameOpen = false;
		return;
	}
	if (!hasGpuTimestamps()) {
		OWL_CORE_WARN("OpenGL: GPU timestamps not supported by the context.")
		return;
	}
	if (!m_queriesCreated) {
		glGenQueries(static_cast<GLsizei>(g_slotCount * 2), m_queries.front().data());
		m_queriesCreated = true;
	}
	m_slotFrame.fill(0);
	m_completed.clear();
	m_frameId = 0;
	m_frameOpen = false;
	m_timingEnabled = true;
}

void RenderAPI::beginFrame() {
	GpuProfiler::beginFrame();
	if (!m_timingEnabled)
		return;
	++m_frameId;
	const auto slot = static_cast<size_t>(m_frameId % g_slotCount);
	if (m_slotFrame[slot] != 0)
		harvest(slot);
	m_slotFrame[slot] = m_frameId;
	glQueryCounter(m_queries[slot][0], GL_TIMESTAMP);
	m_frameOpen = true;
}

void RenderAPI::endFrame() {
	GpuProfiler::endFrame();
	if (!m_timingEnabled || !m_frameOpen)
		return;
	glQueryCounter(m_queries[static_cast<size_t>(m_frameId % g_slotCount)][1], GL_TIMESTAMP);
	m_frameOpen = false;
}

void RenderAPI::harvest(const size_t iSlot) {
	GpuFrameTiming timing{.frameId = m_slotFrame[iSlot], .busyMs = 0.0, .spanMs = 0.0, .intervalCount = 0};
	GLint available = 0;
	glGetQueryObjectiv(m_queries[iSlot][1], GL_QUERY_RESULT_AVAILABLE, &available);
	if (available != 0) {
		GLuint64 begin = 0;
		GLuint64 end = 0;
		glGetQueryObjectui64v(m_queries[iSlot][0], GL_QUERY_RESULT, &begin);
		glGetQueryObjectui64v(m_queries[iSlot][1], GL_QUERY_RESULT, &end);
		if (end >= begin) {
			timing.spanMs = static_cast<double>(end - begin) * 1e-6;
			timing.busyMs = timing.spanMs;
			timing.intervalCount = 1;
		}
	}
	m_completed.push_back(timing);
	m_slotFrame[iSlot] = 0;
}

auto RenderAPI::popGpuFrameTimings() -> std::vector<GpuFrameTiming> {
	std::vector<GpuFrameTiming> out;
	out.swap(m_completed);
	return out;
}

auto RenderAPI::getDeviceName() const -> std::string {
	if (getState() != State::Ready)
		return "none";
	const auto* const name = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
	return name == nullptr ? std::string{"unknown"} : std::string{name};
}

}// namespace owl::renderer::gpu::opengl
