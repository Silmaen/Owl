/**
 * @file Profiler.cpp
 * @author Silmaen
 * @date 07/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "debug/Profiler.h"

namespace owl::debug {

Profiler::Profiler() = default;

Profiler::~Profiler() { endSession(); }

void Profiler::beginSession(const std::string& iName, const std::string& iFilepath) {
	const std::lock_guard<std::mutex> lock(m_profilerMutex);
	if (m_currentSession) {
		// If there is already a current session, then close it before beginning
		// new one. Subsequent profiling output meant for the original session
		// will end up in the newly opened session instead.	That's better than
		// having badly formatted profiling output.

		// Edge case: BeginSession() might be
		// before Log::Init()
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
			OWL_CORE_ERROR("Instrumentor could not open results file '{}'.", iFilepath)
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
	const auto highResStart = floatingPointMicroseconds{m_startTimePoint.time_since_epoch()};
	const auto elapsedTime =
			std::chrono::time_point_cast<std::chrono::microseconds>(endTimePoint).time_since_epoch() -
			std::chrono::time_point_cast<std::chrono::microseconds>(m_startTimePoint).time_since_epoch();

	Profiler::get().writeProfile({m_name, highResStart, elapsedTime, std::this_thread::get_id()});

	m_stopped = true;
}

}// namespace owl::debug
