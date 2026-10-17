/**
 * @file PlayOverlay.h
 * @author Silmaen
 * @date 10/16/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace owl::nest::panel {

/**
 * @brief
 *  Simulation phase of the viewport, as the play overlay sees it.
 */
enum struct PlayPhase : uint8_t { Edit, Play, Pause };

/**
 * @brief
 *  Timer of the "Playing" / "Paused" message: it shows when a phase starts, stays a moment, fades, then is gone.
 */
class PlayBanner {
public:
	/// Seconds the message stays fully opaque.
	static constexpr float holdSeconds = 1.6f;
	/// Seconds of the fade-out that follows.
	static constexpr float fadeSeconds = 0.8f;

	/**
	 * @brief
	 *  Advance the timer; a new phase other than Edit restarts it, Edit hides the message.
	 * @param[in] iPhase Current phase.
	 * @param[in] iDeltaSeconds Time since the last call.
	 */
	void update(PlayPhase iPhase, float iDeltaSeconds);

	/**
	 * @brief
	 *  Opacity of the message, 0 when hidden, 1 while held.
	 * @return The alpha in [0, 1].
	 */
	[[nodiscard]] auto alpha() const -> float;

	/**
	 * @brief
	 *  Whether the message is still drawn.
	 * @return True when the alpha is above zero.
	 */
	[[nodiscard]] auto visible() const -> bool { return alpha() > 0.f; }

private:
	/// Phase of the last update.
	PlayPhase m_phase = PlayPhase::Edit;
	/// Seconds since the phase started.
	float m_elapsed = holdSeconds + fadeSeconds;
};

/**
 * @brief
 *  Decides whether the mouse button reaches the game: a press must start inside the viewport and outside the overlay
 *  controls, and then goes on until it is released, wherever the cursor goes.
 */
class PlayInputGate {
public:
	/**
	 * @brief
	 *  Filter the left button for this frame.
	 * @param[in] iDown Whether the button is down.
	 * @param[in] iInViewport Whether the cursor is over the viewport image.
	 * @param[in] iOverOverlay Whether the cursor is over an overlay control.
	 * @return True when the game sees the button as pressed.
	 */
	[[nodiscard]] auto filter(bool iDown, bool iInViewport, bool iOverOverlay) -> bool;

private:
	/// Whether the button is down.
	bool m_wasDown = false;
	/// Whether the current press started on the game.
	bool m_latched = false;
};

/**
 * @brief
 *  Screen rectangle of an overlay window.
 */
struct OverlayRect {
	float minX = 0.f;///< Left.
	float minY = 0.f;///< Top.
	float maxX = 0.f;///< Right.
	float maxY = 0.f;///< Bottom.

	/**
	 * @brief
	 *  Whether a point lies inside the rectangle.
	 * @param[in] iX Point abscissa.
	 * @param[in] iY Point ordinate.
	 * @return True when inside.
	 */
	[[nodiscard]] constexpr auto contains(const float iX, const float iY) const -> bool {
		return iX >= minX && iX < maxX && iY >= minY && iY < maxY;
	}
};

/**
 * @brief
 *  Whether a point lies in one of the given rectangles.
 * @tparam N Number of rectangles.
 * @param[in] iRects The rectangles.
 * @param[in] iCount How many of them are in use.
 * @param[in] iX Point abscissa.
 * @param[in] iY Point ordinate.
 * @return True when one contains the point.
 */
template<std::size_t N>
[[nodiscard]] constexpr auto anyContains(const std::array<OverlayRect, N>& iRects, const std::size_t iCount,
										 const float iX, const float iY) -> bool {
	for (std::size_t i = 0; i < iCount && i < N; ++i)
		if (iRects[i].contains(iX, iY))
			return true;
	return false;
}

}// namespace owl::nest::panel
