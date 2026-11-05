/**
 * @file SceneReplay.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "app/Application.h"
#include "input/Input.h"
#include "scene/Scene.h"
#include "scene/WorldHash.h"
#include "window/Window.h"

#include <cstdint>
#include <utility>

namespace owl::scene {

namespace {

auto readCursorCaptured() -> bool {
	return app::Application::instanced() &&
		   app::Application::get().getWindow().getCursorMode() == window::CursorMode::Disabled;
}

}// namespace

auto Scene::setRecordingReplay(const bool iRecord) -> bool {
	if (iRecord && status != Status::Editing) {
		OWL_CORE_WARN("Scene: Recording refused, a replay starts with the runtime.")
		return false;
	}
	m_recordArmed = iRecord;
	if (!iRecord)
		m_recording.reset();
	return true;
}

auto Scene::takeRecordedReplay() -> std::optional<Replay> {
	m_recordArmed = false;
	return std::exchange(m_recording, std::nullopt);
}

auto Scene::setPlaybackReplay(const shared<const Replay>& iReplay) -> bool {
	if (iReplay && status != Status::Editing) {
		OWL_CORE_WARN("Scene: Replay refused, a replay starts with the runtime.")
		return false;
	}
	mp_playback = iReplay;
	m_replayProgress = {};
	m_nextCheckpoint = 0;
	if (iReplay)
		m_replayProgress.frameCount = iReplay->frames.size();
	return true;
}

auto Scene::routeUiPointer(const ReplayPointer& iLive) -> std::optional<ReplayPointer> {
	if (isPlayingReplay())
		return mp_playback->frames[m_replayProgress.frame].pointer;
	if (m_recording.has_value())
		m_framePointer = iLive;
	return iLive;
}

auto Scene::startReplay() -> input::InputState {
	m_framePointer.reset();
	if (mp_playback != nullptr) {
		const auto& replay = *mp_playback;
		m_replayProgress = {};
		m_replayProgress.frameCount = replay.frames.size();
		m_nextCheckpoint = 0;
		if (replay.seed != m_randomSeed)
			OWL_CORE_WARN("Scene: Replay seed {} differs from the scene seed {}, the replay's applies.", replay.seed,
						  m_randomSeed)
		m_randomSeed = replay.seed;
		m_simulationClock.setSettings({.tickRate = replay.tickRate, .maxTicksPerFrame = replay.maxTicksPerFrame});
		m_simulationClock.reset();
		m_cursorCaptured = false;
		return replay.start;
	}
	m_cursorCaptured = readCursorCaptured();
	const auto held = input::Input::readState();
	if (m_recordArmed) {
		const auto& settings = m_simulationClock.getSettings();
		m_recording.emplace();
		m_recording->seed = m_randomSeed;
		m_recording->tickRate = settings.tickRate;
		m_recording->maxTicksPerFrame = settings.maxTicksPerFrame;
		m_recording->start = held;
		m_recording->start.wheel = {0.f, 0.f};
		m_nextCheckpointTick = Replay::checkpointTicks;
	}
	return held;
}

void Scene::checkReplayStart() {
	if (m_recording.has_value())
		m_recording->startHash = computeWorldHash(*this);
	if (mp_playback == nullptr)
		return;
	if (const uint64_t hash = computeWorldHash(*this); hash != mp_playback->startHash) {
		OWL_CORE_WARN(
				"Scene: Replay start state differs from the recorded one (world hash {:#018x}, recorded {:#018x}).",
				hash, mp_playback->startHash)
		m_replayProgress.divergedFrame = 0;
	}
	if (mp_playback->frames.empty())
		m_replayProgress.finished = true;
}

auto Scene::beginReplayFrame(const core::Timestep& iTimeStep, input::InputState& oInput) -> bool {
	if (isPlayingReplay()) {
		const auto& frame = mp_playback->frames[m_replayProgress.frame];
		m_replayTimeStep.forceUpdate(frame.duration);
		oInput = frame.input;
		m_cursorCaptured = frame.cursorCaptured;
		return true;
	}
	oInput = input::Input::readState();
	m_cursorCaptured = readCursorCaptured();
	if (m_recording.has_value())
		m_recording->frames.push_back({.duration = iTimeStep.getDuration(),
									   .input = oInput,
									   .cursorCaptured = m_cursorCaptured,
									   .pointer = std::exchange(m_framePointer, std::nullopt)});
	return false;
}

void Scene::endReplayFrame() {
	const uint64_t ticks = m_simulationClock.getTotalTickCount();
	if (m_recording.has_value()) {
		if (ticks < m_nextCheckpointTick)
			return;
		m_recording->checkpoints.push_back({.frame = m_recording->frames.size(), .worldHash = computeWorldHash(*this)});
		while (m_nextCheckpointTick <= ticks) m_nextCheckpointTick += Replay::checkpointTicks;
		return;
	}
	if (!isPlayingReplay())
		return;
	auto& progress = m_replayProgress;
	++progress.frame;
	const auto& checkpoints = mp_playback->checkpoints;
	while (m_nextCheckpoint < checkpoints.size() && checkpoints[m_nextCheckpoint].frame < progress.frame)
		++m_nextCheckpoint;
	if (m_nextCheckpoint < checkpoints.size() && checkpoints[m_nextCheckpoint].frame == progress.frame) {
		const auto& checkpoint = checkpoints[m_nextCheckpoint++];
		++progress.checkedCount;
		if (const uint64_t hash = computeWorldHash(*this);
			hash != checkpoint.worldHash && !progress.divergedFrame.has_value()) {
			OWL_CORE_WARN("Scene: Replay diverges at frame {} (world hash {:#018x}, recorded {:#018x}).",
						  progress.frame, hash, checkpoint.worldHash)
			progress.divergedFrame = progress.frame;
		}
	}
	if (progress.frame >= progress.frameCount) {
		progress.finished = true;
		OWL_CORE_INFO("Scene: Replay finished after {} frames, {} checkpoints compared{}.", progress.frame,
					  progress.checkedCount, progress.divergedFrame.has_value() ? ", diverged" : "")
	}
}

}// namespace owl::scene
