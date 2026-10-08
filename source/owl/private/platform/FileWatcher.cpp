/**
 * @file FileWatcher.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "platform/FileWatcher.h"

#include "debug/Profiler.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <stop_token>
#include <system_error>
#include <thread>
#include <utility>

namespace owl::platform {

auto isSameFile(const std::filesystem::path& iLhs, const std::filesystem::path& iRhs) -> bool {
	std::error_code error;
	if (std::filesystem::equivalent(iLhs, iRhs, error))
		return true;
	const auto lhs = std::filesystem::weakly_canonical(iLhs, error);
	if (error)
		return false;
	const auto rhs = std::filesystem::weakly_canonical(iRhs, error);
	return !error && lhs == rhs;
}

FileWatcher::FileWatcher(const std::chrono::milliseconds iPeriod) : m_period{iPeriod} {}

FileWatcher::~FileWatcher() { stop(); }

void FileWatcher::collect(const std::filesystem::path& iDirectory, std::unordered_map<std::string, Stamp>& ioStamps) {
	std::error_code error;
	auto iterator = std::filesystem::recursive_directory_iterator(
			iDirectory, std::filesystem::directory_options::skip_permission_denied, error);
	if (error)
		return;
	for (const auto end = std::filesystem::recursive_directory_iterator{}; iterator != end; iterator.increment(error)) {
		if (error)
			break;
		if (!iterator->is_regular_file(error) || error)
			continue;
		const auto time = iterator->last_write_time(error);
		if (error)
			continue;
		const auto size = iterator->file_size(error);
		if (error)
			continue;
		ioStamps.insert_or_assign(iterator->path().generic_string(), Stamp{.time = time, .size = size});
	}
}

void FileWatcher::addDirectory(const std::filesystem::path& iDirectory) {
	const auto directory = iDirectory.lexically_normal();
	{
		const std::scoped_lock<std::mutex> lock(m_mutex);
		if (std::ranges::find(m_directories, directory) != m_directories.end())
			return;
		m_directories.push_back(directory);
	}
	std::unordered_map<std::string, Stamp> stamps;
	collect(directory, stamps);
	const std::scoped_lock<std::mutex> lock(m_scanMutex);
	m_known.merge(stamps);
}

void FileWatcher::removeDirectory(const std::filesystem::path& iDirectory) {
	const std::scoped_lock<std::mutex> lock(m_mutex);
	std::erase(m_directories, iDirectory.lexically_normal());
}

auto FileWatcher::getDirectories() const -> std::vector<std::filesystem::path> {
	const std::scoped_lock<std::mutex> lock(m_mutex);
	return m_directories;
}

void FileWatcher::start() {
	if (m_thread.joinable())
		return;
	m_thread = std::jthread([this](const std::stop_token& iStop) -> void {
		OWL_PROFILE_THREAD_NAME("FileWatcher")

		while (!iStop.stop_requested()) {
			scan();
			std::unique_lock<std::mutex> lock(m_wakeMutex);
			m_wake.wait_for(lock, iStop, m_period, []() -> bool { return false; });
		}
	});
}

void FileWatcher::stop() {
	if (!m_thread.joinable())
		return;
	m_thread.request_stop();
	m_wake.notify_all();
	m_thread.join();
	m_thread = {};
}

auto FileWatcher::takeChanges() -> std::vector<std::filesystem::path> {
	const std::scoped_lock<std::mutex> lock(m_mutex);
	m_hasChanges.store(false, std::memory_order_release);
	return std::exchange(m_pending, {});
}

void FileWatcher::scan() {
	const auto directories = getDirectories();
	std::unordered_map<std::string, Stamp> current;
	for (const auto& directory: directories) collect(directory, current);

	std::vector<std::filesystem::path> reported;
	{
		const std::scoped_lock<std::mutex> scanLock(m_scanMutex);
		std::erase_if(m_known, [&current](const auto& iEntry) -> bool { return !current.contains(iEntry.first); });
		std::erase_if(m_settling, [&current](const auto& iEntry) -> bool { return !current.contains(iEntry.first); });
		for (const auto& [path, stamp]: current) {
			if (const auto known = m_known.find(path); known != m_known.end() && known->second == stamp) {
				m_settling.erase(path);
				continue;
			}
			if (const auto settling = m_settling.find(path);
				settling != m_settling.end() && settling->second == stamp) {
				m_settling.erase(settling);
				m_known.insert_or_assign(path, stamp);
				reported.emplace_back(path);
				continue;
			}
			m_settling.insert_or_assign(path, stamp);
		}
	}
	if (reported.empty())
		return;
	const std::scoped_lock<std::mutex> lock(m_mutex);
	for (auto& path: reported)
		if (std::ranges::find(m_pending, path) == m_pending.end())
			m_pending.push_back(std::move(path));
	m_hasChanges.store(true, std::memory_order_release);
}

}// namespace owl::platform
