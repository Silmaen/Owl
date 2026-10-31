/**
 * @file Subtitles.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::scene {

/**
 * @brief
 *  The subtitles a scene shows at the bottom of the screen: the lines of the audible sounds with a subtitle, renewed
 *  every frame by `owl.subtitles`, and the ones of `ui.show_subtitle`; the newest at the bottom.
 */
class OWL_API Subtitles final {
public:
	/**
	 * @brief
	 *  One line on screen.
	 */
	struct Line {
		/// Text shown, already translated.
		std::string text;
		/// Seconds left on screen.
		float remaining = 0.f;
	};

	/// Lines shown at once: a new line pushes the oldest out.
	static constexpr size_t g_maxLines = 3;
	/// Time the line of a sound stays once the sound stopped or went out of hearing, to be read to the end.
	static constexpr float g_lingerSeconds = 1.5f;

	/**
	 * @brief
	 *  Default constructor.
	 */
	Subtitles() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~Subtitles() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	Subtitles(const Subtitles&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	Subtitles(Subtitles&&) = default;

	/**
	 * @brief
	 *  Copy assignment.
	 * @return This queue.
	 */
	auto operator=(const Subtitles&) -> Subtitles& = default;

	/**
	 * @brief
	 *  Move assignment.
	 * @return This queue.
	 */
	auto operator=(Subtitles&&) -> Subtitles& = default;

	/**
	 * @brief
	 *  Show a line for a time; the same text already on screen is renewed (its time left is never shortened) instead
	 *  of repeated.
	 * @param[in] iText The text; an empty text shows nothing.
	 * @param[in] iSeconds Seconds on screen, at least 0.1.
	 */
	void show(std::string_view iText, float iSeconds);

	/**
	 * @brief
	 *  Count the time down and drop the lines whose time ran out.
	 * @param[in] iDeltaSeconds Seconds since the previous update.
	 */
	void update(float iDeltaSeconds);

	/**
	 * @brief
	 *  Remove every line.
	 */
	void clear() { m_lines.clear(); }

	/**
	 * @brief
	 *  The lines on screen, the oldest first.
	 * @return The lines.
	 */
	[[nodiscard]] auto getLines() const -> std::span<const Line> { return m_lines; }

private:
	/// Lines on screen, the oldest first.
	std::vector<Line> m_lines;
};

}// namespace owl::scene
