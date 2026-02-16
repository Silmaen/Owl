/**
 * @file LogSink.h
 * @author Silmaen
 * @date 16/02/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Log.h"

#include <chrono>
#include <deque>
#include <mutex>
#include <string>

namespace owl::debug {

/**
 * @brief A single log entry captured from the engine.
 */
struct OWL_API LogEntry {
	/// The severity level of the log.
	core::Log::Level level = core::Log::Level::Info;
	/// The logger name ("OWL" or "APP").
	std::string loggerName;
	/// The log message text.
	std::string message;
	/// The timestamp when the entry was logged.
	std::chrono::system_clock::time_point timestamp{};
};

/**
 * @brief Thread-safe circular buffer storing recent log entries.
 */
class OWL_API LogBuffer {
public:
	/// Maximum number of entries stored.
	static constexpr size_t maxEntries = 1000;

	/**
	 * @brief Push a new log entry into the buffer.
	 * @param[in] iEntry The entry to store.
	 */
	void push(LogEntry iEntry);

	/**
	 * @brief Get a thread-safe copy of all stored entries.
	 * @return A deque of log entries.
	 */
	auto getEntries() const -> std::deque<LogEntry>;

	/**
	 * @brief Clear all stored entries.
	 */
	void clear();

private:
	mutable std::mutex m_mutex;
	std::deque<LogEntry> m_entries;
};

}// namespace owl::debug
