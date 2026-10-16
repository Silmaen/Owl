/**
 * @file EditorSession.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorSession.h"

#include <input/Input.h>
#include <renderer/TextureDecoder.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>

namespace owl::nest::session {

auto FrameCapture::encodePng() const -> std::vector<uint8_t> {
	if (!isValid()) {
		OWL_WARN("EditorSession: No valid image to encode ({}x{}, {} bytes).", size.x(), size.y(), rgba.size())
		return {};
	}
	return renderer::encodeImagePng(size, rgba);
}

void SessionStatistics::addFrame(const double iFrameMs, const uint64_t iDrawCalls) {
	++frameCount;
	lastFrameMs = iFrameMs;
	lastDrawCalls = iDrawCalls;
	meanFrameMs += (iFrameMs - meanFrameMs) / static_cast<double>(frameCount);
}

auto PlayDriver::validate(const PlayRequest& iRequest) -> std::optional<SessionFailure> {
	if (iRequest.frames == 0 && iRequest.ticks == 0)
		return SessionFailure{.error = SessionError::InvalidRequest, .message = "give `frames` or `ticks`"};
	if (iRequest.frames > PlayRequest::maxCount || iRequest.ticks > PlayRequest::maxCount)
		return SessionFailure{.error = SessionError::InvalidRequest,
							  .message = std::format("at most {} frames or ticks", PlayRequest::maxCount)};
	if (!std::isfinite(iRequest.timestepMs) || iRequest.timestepMs < 0.0 || iRequest.timestepMs > 1000.0)
		return SessionFailure{.error = SessionError::InvalidRequest,
							  .message = "`timestep_ms` is between 0 (one tick per frame) and 1000"};
	return std::nullopt;
}

void PlayDriver::start(const PlayRequest& iRequest, const core::Timestep& iTickStep, const uint64_t iTicksBefore) {
	m_request = iRequest;
	m_step = iTickStep;
	if (iRequest.timestepMs > 0.0)
		m_step.forceUpdate(std::chrono::duration_cast<core::Timestep::duration>(
				std::chrono::duration<double, std::milli>{iRequest.timestepMs}));
	m_lastTickTotal = iTicksBefore;
	m_report = PlayReport{};
	m_report.timestepMs = static_cast<double>(m_step.getMilliseconds());
	m_active = true;
}

void PlayDriver::beginFrame() const {
	input::Input::resetInjection();
	injectHeldInput({.keys = m_request.keys, .mouseButtons = m_request.mouseButtons, .gamepad = m_request.gamepad});
}

auto PlayDriver::endFrame(const double iFrameMs, const uint64_t iDrawCalls, const scene::Scene& iScene) -> bool {
	++m_report.frames;
	m_report.totalMs += iFrameMs;
	m_report.maxFrameMs = std::max(m_report.maxFrameMs, iFrameMs);
	m_report.drawCalls += iDrawCalls;
	// A scene swapped by a transition restarts its clock: its ticks count from zero.
	const auto total = iScene.getSimulationClock().getTotalTickCount();
	m_report.ticks += total >= m_lastTickTotal ? total - m_lastTickTotal : total;
	m_lastTickTotal = total;
	return (m_request.frames > 0 && m_report.frames >= m_request.frames) ||
		   (m_request.ticks > 0 && m_report.ticks >= m_request.ticks) || m_report.frames >= PlayRequest::maxCount;
}

auto PlayDriver::finish(const scene::Scene* iScene, const bool iInterrupted) -> PlayReport {
	input::Input::resetInjection();
	m_active = false;
	if (iScene != nullptr) {
		m_report.lastPhases = iScene->getLastRuntimeTimings();
		m_report.entityCount = iScene->getAllEntities().size();
	}
	m_report.interrupted = iInterrupted;
	return m_report;
}

EditorSession::~EditorSession() = default;

auto EditorSession::execute(const std::string_view iName, const commands::CommandArgs& iArgs,
							const commands::CommandSource iSource)
		-> expected<commands::CommandResult, commands::CommandFailure> {
	auto context = makeCommandContext();
	context.source = iSource;
	return getRegistry().execute(iName, iArgs, context);
}

auto EditorSession::replay(const commands::Macro& iMacro, const std::string& iDescription)
		-> expected<commands::MacroReport, commands::MacroFailure> {
	if (isScenePlaying()) {
		OWL_WARN("EditorSession: Macro refused, the scene is playing.")
		return unexpected<commands::MacroFailure>{
				commands::MacroFailure{.error = commands::MacroError::NoScene,
									   .line = 0,
									   .message = "the scene is playing, stop it to edit"}};
	}
	return iMacro.replay(getRegistry(), makeCommandContext(), iDescription);
}

}// namespace owl::nest::session
