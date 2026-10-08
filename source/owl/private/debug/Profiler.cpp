/**
 * @file Profiler.cpp
 * @author Silmaen
 * @date 07/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "debug/Profiler.h"

#include <iomanip>
#ifdef OWL_PROFILER_TRACY
#include "core/external/tracy.h"
#endif
#include <cstddef>
#include <mutex>
#include <sstream>
#include <thread>

namespace owl::debug {

#ifdef OWL_PROFILER_TRACY
static_assert(sizeof(ProfileSourceLocation) == sizeof(___tracy_source_location_data));
static_assert(offsetof(ProfileSourceLocation, name) == offsetof(___tracy_source_location_data, name));
static_assert(offsetof(ProfileSourceLocation, function) == offsetof(___tracy_source_location_data, function));
static_assert(offsetof(ProfileSourceLocation, file) == offsetof(___tracy_source_location_data, file));
static_assert(offsetof(ProfileSourceLocation, line) == offsetof(___tracy_source_location_data, line));
static_assert(offsetof(ProfileSourceLocation, color) == offsetof(___tracy_source_location_data, color));
#endif

auto getProfilerBackend() noexcept -> ProfilerBackend {
#if defined(OWL_PROFILER_TRACY)
	return ProfilerBackend::Tracy;
#elif defined(OWL_PROFILER_CHROME)
	return ProfilerBackend::Chrome;
#else
	return ProfilerBackend::None;
#endif
}

ProfileZone::ProfileZone([[maybe_unused]] const ProfileSourceLocation* iLocation) noexcept {
#ifdef OWL_PROFILER_TRACY
	const auto ctx = ___tracy_emit_zone_begin(reinterpret_cast<const ___tracy_source_location_data*>(iLocation), 1);
	m_id = ctx.id;
	m_active = ctx.active;
#endif
}

ProfileZone::~ProfileZone() {
#ifdef OWL_PROFILER_TRACY
	if (m_active != 0)
		___tracy_emit_zone_end(TracyCZoneCtx{.id = m_id, .active = m_active});
#endif
}

void markProfilerFrame() noexcept {
#ifdef OWL_PROFILER_TRACY
	___tracy_emit_frame_mark(nullptr);
#endif
}

void setProfilerThreadName([[maybe_unused]] const char* iName) noexcept {
#ifdef OWL_PROFILER_TRACY
	___tracy_set_thread_name(iName);
#endif
}

auto isProfilerConnected() noexcept -> bool {
#ifdef OWL_PROFILER_TRACY
	return ___tracy_connected() != 0;
#else
	return false;
#endif
}

Profiler::Profiler() = default;

Profiler::~Profiler() { endSession(); }

void Profiler::beginSession(const std::string& iName, const std::string& iFilepath) {
	const std::lock_guard<std::mutex> lock(m_profilerMutex);
	if (m_currentSession) {

		if (core::Log::initiated()) {
			OWL_CORE_ERROR("Profiler::BeginSession('{}') when session '{}' already open.", iName,
						   m_currentSession->name)
		}
		internalEndSession();
	}
	m_outputStream.open(iFilepath);

	if (m_outputStream.is_open()) {
		m_currentSession = mkUniq<ProfileSession>(iName);
		writeHeader();
	} else {
		if (core::Log::initiated()) {// Edge case: BeginSession() might be  before Log::Init()
			OWL_CORE_ERROR("Profiler: Could not open results file '{}'.", iFilepath)
		}
	}
}

void Profiler::endSession() {
	const std::lock_guard<std::mutex> lock(m_profilerMutex);
	internalEndSession();
}

void Profiler::writeProfile(const ProfileResult& iResult) {
	std::stringstream json;

	json << std::setprecision(3) << std::fixed;
	json << ",{";
	json << R"("cat":"function",)";
	json << "\"dur\":" << (iResult.elapsedTime.count()) << ',';
	json << R"("name":")" << iResult.name << "\",";
	json << R"("ph":"X",)";
	json << "\"pid\":0,";
	json << "\"tid\":" << iResult.threadId << ",";
	json << "\"ts\":" << iResult.start.count();
	json << "}";

	const std::lock_guard<std::mutex> lock(m_profilerMutex);
	if (m_currentSession) {
		m_outputStream << json.str();
		m_outputStream.flush();
	}
}

void Profiler::writeHeader() {
	m_outputStream << R"({"otherData": {},"traceEvents":[{})";
	m_outputStream.flush();
}

void Profiler::writeFooter() {
	m_outputStream << "]}";
	m_outputStream.flush();
}

void Profiler::internalEndSession() {
	if (m_currentSession) {
		writeFooter();
		m_outputStream.close();
		m_currentSession.reset();
		m_currentSession = nullptr;
	}
}

ProfileTimer::ProfileTimer(const char* iName) : m_name(iName), m_startTimePoint{std::chrono::steady_clock::now()} {}

ProfileTimer::~ProfileTimer() {
	if (!m_stopped)
		stop();
}

void ProfileTimer::stop() {
	const auto endTimePoint = std::chrono::steady_clock::now();
	const auto highResStart = FloatingPointMicroseconds{m_startTimePoint.time_since_epoch()};
	const auto elapsedTime =
			std::chrono::time_point_cast<std::chrono::microseconds>(endTimePoint).time_since_epoch() -
			std::chrono::time_point_cast<std::chrono::microseconds>(m_startTimePoint).time_since_epoch();

	Profiler::get().writeProfile({.name = m_name,
								  .start = highResStart,
								  .elapsedTime = elapsedTime,
								  .threadId = std::this_thread::get_id()});

	m_stopped = true;
}

}// namespace owl::debug
