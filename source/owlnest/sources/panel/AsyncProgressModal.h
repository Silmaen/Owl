/**
 * @file AsyncProgressModal.h
 * @author Silmaen
 * @date 16/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include <atomic>
#include <mutex>

namespace owl::nest {
/**
 * @brief
 *  Thread-safe progress state shared between a background task and the main thread.
 */
struct AsyncProgressState {
	/// Current progress fraction [0.0, 1.0]. Written by worker, read by main thread.
	std::atomic<float> progress{0.0f};
	/// Cancel flag. Written by main thread, read by worker.
	std::atomic<bool> cancelRequested{false};
	/// Error flag. Written by worker, read by main thread.
	std::atomic<bool> hasError{false};
	/// Whether the operation completed successfully. Set in termination callback.
	std::atomic<bool> completed{false};

	/**
	 * @brief
	 *  Set the status message (thread-safe).
	 */
	void setMessage(const std::string& iMessage) {
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_statusMessage = iMessage;
	}

	/**
	 * @brief
	 *  Get the current status message (thread-safe).
	 */
	[[nodiscard]] auto getMessage() const -> std::string {
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_statusMessage;
	}

	/**
	 * @brief
	 *  Set the error message and flag (thread-safe).
	 */
	void setError(const std::string& iMessage) {
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			m_errorMessage = iMessage;
		}
		hasError.store(true);
	}

	/**
	 * @brief
	 *  Get the error message (thread-safe).
	 */
	[[nodiscard]] auto getError() const -> std::string {
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_errorMessage;
	}

private:
	/// Guards `m_statusMessage` / `m_errorMessage` against concurrent worker writes.
	mutable std::mutex m_mutex;
	/// Latest status string surfaced to the UI thread.
	std::string m_statusMessage;
	/// Error string set by the worker on failure (empty when no error).
	std::string m_errorMessage;
};
namespace panel {
/**
 * @brief
 *  Modal ImGui panel displaying async operation progress.
 */
class AsyncProgressModal final {
public:
	AsyncProgressModal(const AsyncProgressModal&) = delete;

	AsyncProgressModal(AsyncProgressModal&&) = delete;

	auto operator=(const AsyncProgressModal&) -> AsyncProgressModal& = delete;

	auto operator=(AsyncProgressModal&&) -> AsyncProgressModal& = delete;

	AsyncProgressModal() = default;

	~AsyncProgressModal() = default;

	/**
	 * @brief
	 *  Start showing the progress modal.
	 * @param[in] iTitle The modal window title.
	 * @param[in] iState Shared progress state for the operation.
	 * @param[in] iCancellable Whether to show a Cancel button.
	 */
	void open(const std::string& iTitle, shared<AsyncProgressState> iState, bool iCancellable = true);

	/**
	 * @brief
	 *  Render the modal. Call every frame from onImGuiRender().
	 */
	void onImGuiRender();

	/**
	 * @brief
	 *  Check if the modal is currently active.
	 */
	[[nodiscard]] auto isActive() const -> bool { return m_active; }

	/**
	 * @brief
	 *  Check if the operation has finished (success or error).
	 */
	[[nodiscard]] auto isFinished() const -> bool;

	/**
	 * @brief
	 *  Close and reset the modal state.
	 */
	void close();

private:
	/// Title of the modal window.
	std::string m_title;
	/// Shared progress state.
	shared<AsyncProgressState> m_state;
	/// Whether to show a cancel button.
	bool m_cancellable = true;
	/// Whether the popup needs to be opened on next render.
	bool m_pendingOpen = false;
	/// Whether the modal is currently active.
	bool m_active = false;
};

}// namespace panel
}// namespace owl::nest
