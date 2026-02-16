/**
 * @file LogSink.cpp
 * @author Silmaen
 * @date 16/02/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "debug/LogSink.h"

namespace owl::debug {

void LogBuffer::push(LogEntry iEntry) {
	const std::lock_guard<std::mutex> lock(m_mutex);
	m_entries.emplace_back(std::move(iEntry));
	while (m_entries.size() > maxEntries)
		m_entries.pop_front();
}

auto LogBuffer::getEntries() const -> std::deque<LogEntry> {
	const std::lock_guard<std::mutex> lock(m_mutex);
	return m_entries;
}

void LogBuffer::clear() {
	const std::lock_guard<std::mutex> lock(m_mutex);
	m_entries.clear();
}

}// namespace owl::debug
