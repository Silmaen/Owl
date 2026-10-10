/**
 * @file Toast.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

namespace owl::gui::widgets {

/**
 * @brief
 *  Meaning of a toast, which picks its colour and glyph.
 */
enum struct ToastKind : uint8_t {
	Success,///< An action completed ("Saved main_menu.owl").
	Info,///< Something worth knowing.
	Warning,///< Completed with a problem.
	Error,///< Failed.
};

/**
 * @brief
 *  One short notification.
 */
struct Toast {
	/// Meaning.
	ToastKind kind = ToastKind::Info;
	/// One line.
	std::string title;
	/// Optional second line, muted.
	std::string detail;
	/// Seconds the toast has been shown.
	float age = 0.f;
	/// How many identical toasts were folded into this one.
	uint32_t count = 1;
};

/**
 * @brief
 *  Stack of short notifications shown in the bottom-right corner of the main viewport, newest at the bottom.
 *
 * A toast pushed while an identical one is still shown folds into it (with a counter) instead of stacking.
 */
class OWL_API ToastQueue {
public:
	/// Seconds a toast stays fully visible.
	static constexpr float lifetime = 3.5f;
	/// Seconds of the fade-out that follows.
	static constexpr float fadeTime = 0.4f;
	/// Toasts shown at once; older ones are dropped.
	static constexpr size_t maxToasts = 4;

	/**
	 * @brief
	 *  Show a toast.
	 * @param[in] iKind Its meaning.
	 * @param[in] iTitle One line.
	 * @param[in] iDetail Optional second line.
	 */
	void push(ToastKind iKind, std::string iTitle, std::string iDetail = {});

	/**
	 * @brief
	 *  Age the toasts and drop the expired ones.
	 * @param[in] iSeconds Elapsed time since the last call.
	 */
	void update(float iSeconds);

	/**
	 * @brief
	 *  Draw the toasts over every window of the main viewport.
	 */
	void onImGuiRender() const;

	/**
	 * @brief
	 *  The toasts being shown, oldest first.
	 * @return The toasts.
	 */
	[[nodiscard]] auto getToasts() const -> const std::deque<Toast>& { return m_toasts; }

	/**
	 * @brief
	 *  Remove every toast.
	 */
	void clear() { m_toasts.clear(); }

	/**
	 * @brief
	 *  Opacity of a toast of a given age.
	 * @param[in] iAge Seconds shown.
	 * @return 1 while visible, falling to 0 over `fadeTime`.
	 */
	[[nodiscard]] static auto getOpacity(float iAge) -> float;

private:
	/// The toasts being shown, oldest first.
	std::deque<Toast> m_toasts;
};

}// namespace owl::gui::widgets
