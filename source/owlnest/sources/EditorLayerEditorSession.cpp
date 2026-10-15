/**
 * @file EditorLayerEditorSession.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"

#include <renderer/gpu/RenderCommand.h>

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest {

namespace {

auto failure(const session::SessionError iError, std::string iMessage) -> unexpected<session::SessionFailure> {
	OWL_WARN("EditorSession: {}.", iMessage)
	return unexpected<session::SessionFailure>{
			session::SessionFailure{.error = iError, .message = std::move(iMessage)}};
}

}// namespace

auto getMcpIndicator(const mcp::HttpStatus iStatus) -> panel::StatusInfo::Mcp {
	switch (iStatus) {
		case mcp::HttpStatus::Off:
			return panel::StatusInfo::Mcp::Off;
		case mcp::HttpStatus::Listening:
			return panel::StatusInfo::Mcp::Listening;
		case mcp::HttpStatus::ClientActive:
			return panel::StatusInfo::Mcp::ClientActive;
	}
	return panel::StatusInfo::Mcp::Off;
}

void EditorLayer::updateMcp() {
	if (!mp_mcp)
		return;
	const bool busy = isMcpBusy();
	if (m_settingsPanel.consumeRegenerateTokenRequest())
		std::ignore = mp_mcp->regenerateToken();
	// A port being dragged is applied when the field is released, not at every step.
	if (!busy)
		mp_mcp->applySettings(m_settings.mcpEnabled, static_cast<uint16_t>(m_settings.mcpPort));
	std::ignore = mp_mcp->onUpdate(busy);
	panel::McpSettingsInfo info{.status = panel::StatusBar::getMcpLabel(getMcpIndicator(mp_mcp->getStatus())),
								.endpoint = mp_mcp->getEndpoint(),
								.tokenFile = mp_mcp->getTokenFile().string()};
	if (const auto failed = mp_mcp->getFailedPort(); failed.has_value() && m_settings.mcpEnabled)
		info.status = std::format("Cannot listen on 127.0.0.1:{} (port taken?)", *failed);
	m_settingsPanel.setMcpInfo(info);
}

auto EditorLayer::isMcpBusy() const -> bool {
	if (ImGui::GetCurrentContext() != nullptr &&
		(ImGui::IsAnyItemActive() ||
		 ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)))
		return true;
	if (m_sceneHierarchy.isEditing())
		return true;
	const auto* doc = activeSceneDocument();
	return doc != nullptr && doc->getViewport().isGizmoInUse();
}

auto EditorLayer::getSessionScenePath() const -> std::filesystem::path {
	const auto* doc = activeSceneDocument();
	return doc != nullptr ? doc->filePath() : std::filesystem::path{};
}

auto EditorLayer::makeCommandContext() -> commands::CommandContext {
	auto* doc = activeSceneDocument();
	return {.scene = doc != nullptr ? doc->getActiveScene() : nullptr,
			.undo = doc != nullptr ? &doc->undoManager() : nullptr,
			.playing = doc != nullptr && doc->state() != SceneDocument::State::Edit,
			.scenePath = doc != nullptr ? doc->filePath() : std::filesystem::path{},
			.onSaved = [this, doc](const std::filesystem::path& iPath) -> void {
				doc->setScenePath(iPath);
				refreshWindowTitle();
			}};
}

auto EditorLayer::startPlay(const session::PlayRequest& iRequest) -> expected<void, session::SessionFailure> {
	auto* doc = activeSceneDocument();
	if (doc == nullptr || !doc->getEditorScene())
		return failure(session::SessionError::NoScene, "no scene document to play");
	if (m_playDriver.isActive())
		return failure(session::SessionError::AlreadyPlaying, "a bounded play is running");
	if (const auto invalid = session::PlayDriver::validate(iRequest); invalid)
		return failure(invalid->error, invalid->message);
	if (doc->state() == SceneDocument::State::Edit)
		onScenePlay();
	else if (doc->state() == SceneDocument::State::Pause)
		onSceneResume();
	const auto& scene = doc->getActiveScene();
	scene->setRuntimeTimingsEnabled(true);
	m_playDriver.start(iRequest, scene->getSimulationClock().getStepTimestep(),
					   scene->getSimulationClock().getTotalTickCount());
	mp_playDocument = doc;
	m_playScene = scene;
	return {};
}

void EditorLayer::updateActiveDocument(SceneDocument& ioDocument, const core::Timestep& iTimeStep) {
	const bool bounded = m_playDriver.isActive() && &ioDocument == mp_playDocument &&
						 ioDocument.state() == SceneDocument::State::Play;
	if (bounded)
		m_playDriver.beginFrame();
	const auto drawsBefore = renderer::gpu::RenderCommand::getRenderCounters().drawCalls;
	const auto start = std::chrono::steady_clock::now();
	ioDocument.onUpdate(bounded ? m_playDriver.getTimestep() : iTimeStep);
	const double frameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	const uint64_t draws = renderer::gpu::RenderCommand::getRenderCounters().drawCalls - drawsBefore;
	m_sessionStatistics.addFrame(frameMs, draws);
	if (!bounded)
		return;
	m_playScene = ioDocument.getActiveScene();
	if (m_playScene && m_playDriver.endFrame(frameMs, draws, *m_playScene))
		finishBoundedPlay(/*iInterrupted=*/false);
}

auto EditorLayer::isPlayDocumentPlaying() const -> bool {
	return std::ranges::any_of(m_documents.list(), [this](const auto& iDocument) -> bool {
		return iDocument && iDocument.get() == mp_playDocument &&
			   static_cast<const SceneDocument*>(iDocument.get())->state() != SceneDocument::State::Edit;
	});
}

void EditorLayer::finishBoundedPlay(const bool iInterrupted) {
	const auto request = m_playDriver.getRequest();
	auto report = m_playDriver.finish(m_playScene.get(), iInterrupted);
	m_playScene.reset();
	mp_playDocument = nullptr;
	if (!iInterrupted && request.capture) {
		if (auto frame = captureFrame(); frame)
			report.capture = std::move(*frame);
	}
	m_playReport = std::move(report);
	if (iInterrupted)
		return;
	if (request.stopAtEnd)
		onSceneStop();
	else
		onScenePause();
}

auto EditorLayer::takePlayReport() -> std::optional<session::PlayReport> {
	return std::exchange(m_playReport, std::nullopt);
}

auto EditorLayer::stopPlay() -> expected<void, session::SessionFailure> {
	if (getState() == State::Edit)
		return failure(session::SessionError::NotPlaying, "the scene does not play");
	onSceneStop();
	return {};
}

auto EditorLayer::captureFrame() -> expected<session::FrameCapture, session::SessionFailure> {
	const auto* viewport = activeViewport();
	if (viewport == nullptr || !viewport->getFramebuffer())
		return failure(session::SessionError::NoScene, "no viewport to capture");
	const auto& framebuffer = viewport->getFramebuffer();
	auto pixels = framebuffer->readColorAttachment(0);
	if (pixels.empty())
		return failure(session::SessionError::CaptureUnavailable, "the renderer cannot read the viewport back");
	// Captures read the same in any viewer: opaque.
	for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
	return session::FrameCapture{.size = framebuffer->getSpecification().size, .rgba = std::move(pixels)};
}

auto EditorLayer::getStatistics() const -> session::SessionStatistics {
	auto statistics = m_sessionStatistics;
	if (const auto& scene = getActiveScene(); scene) {
		statistics.entityCount = scene->getAllEntities().size();
		statistics.lastPhases = scene->getLastRuntimeTimings();
		statistics.tickCount = getState() != State::Edit ? scene->getSimulationClock().getTotalTickCount() : 0;
	}
	return statistics;
}

}// namespace owl::nest
