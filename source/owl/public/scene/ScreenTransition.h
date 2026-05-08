/**
 * @file ScreenTransition.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <optional>
#include <string>

namespace owl::scene {
/**
 * @brief
 *  Screen-space transition effects (fade + wipes) and orchestrated
 * scene-to-scene handoffs.
 *
 * Acts as both:
 *
 * - A primitive overlay renderer — call `play(type, duration, colour)` (or
 *   the legacy `start(type, duration)`) to draw a one-shot fade or wipe over
 *   the current frame; `update(dt)` advances it.
 * - A full **scene-load orchestrator** — call `requestSceneLoad(...)` to
 *   chain *out-anim → loading screen (spinner + "Loading" text) → scene
 *   load → in-anim* in a single call. The host (`EditorLayer` / `RunnerLayer`)
 *   polls `pendingLoadPath` once the out-anim ends, performs the actual load,
 *   and the orchestrator advances to the in-anim once a configurable
 *   `minHoldDuration` has elapsed (so the loading screen stays visible long
 *   enough to register, even when sync loads complete in one frame).
 *
 * Also exposed through Lua:
 *
 * - `ui.transition_play(type, duration, [r, g, b, a])` → `play`.
 * - `scene.transition_to(path, type, duration)` → `requestSceneLoad`.
 *
 * Replaces the manual `pending_scene + transition_fade_out + load_scene`
 * pump scattered across the sample-project Lua scripts.
 */
class OWL_API ScreenTransition final {
public:
	ScreenTransition() = delete;

	/**
	 * @brief
	 *  Transition kind shared between the primitive overlay and the
	 * scene-load orchestrator's out / in animations.
	 *
	 * `Fade*` cross-fade the configured colour with the underlying scene.
	 * `Wipe*` slide a colour bar across the viewport in the named direction;
	 * the bar covers the full viewport at progress 0 and reaches a
	 * fully-cleared frame at progress 1.
	 */
	enum struct Type : uint8_t {
		None,///< No transition active.
		FadeIn,///< Cross-fade from opaque colour to transparent.
		FadeOut,///< Cross-fade from transparent to opaque colour.
		WipeLeft,///< Bar slides off-screen towards the left.
		WipeRight,///< Bar slides off-screen towards the right.
		WipeUp,///< Bar slides off-screen towards the top.
		WipeDown,///< Bar slides off-screen towards the bottom.
	};

	/**
	 * @brief
	 *  Phase of the scene-load orchestrator.
	 *
	 * `Idle` — no request active; nothing to render.
	 * `OutAnim` — playing the configured out animation; original scene
	 *             still rendered underneath.
	 * `Loading` — out-anim done, scene swap pending / in flight; the
	 *             loading screen (spinner + label) covers the viewport.
	 * `InAnim` — new scene loaded, playing the in animation to reveal it.
	 */
	enum struct Phase : uint8_t {
		Idle,
		OutAnim,
		Loading,
		InAnim,
	};

	/**
	 * @brief
	 *  Request describing a chained out-anim → load → in-anim flow.
	 */
	struct SceneLoadRequest {
		/// Target scene path (engine-resolved through asset directories).
		std::string scenePath;
		/// Animation kind to play before the load.
		Type outType = Type::FadeOut;
		/// Out-anim duration in seconds.
		float outDuration = 0.3f;
		/// Animation kind to play after the load completes.
		Type inType = Type::FadeIn;
		/// In-anim duration in seconds.
		float inDuration = 0.3f;
		/**
		 * Minimum time the loading screen stays visible (seconds). Sync
		 * loads typically complete in one frame; this floor ensures the
		 * spinner is actually seen by the player.
		 */
		float minHoldDuration = 0.4f;
		/// Cover colour for the out-anim, loading screen, and in-anim.
		math::vec4 color{0.f, 0.f, 0.f, 1.f};
	};

	/**
	 * @brief
	 *  Start a primitive overlay with an explicit colour (no scene load).
	 * @param[in] iType The overlay kind.
	 * @param[in] iDuration Duration in seconds (clamped above 1ms so renders
	 * never divide by zero).
	 * @param[in] iColor RGBA tint (alpha is multiplied with the animated
	 * factor — pass `1` for fully solid).
	 */
	static void play(Type iType, float iDuration, const math::vec4& iColor);

	/**
	 * @brief
	 *  Start a primitive overlay with the default opaque-black colour.
	 * @param[in] iType Overlay kind.
	 * @param[in] iDuration Duration in seconds.
	 */
	static void start(Type iType, float iDuration);

	/**
	 * @brief
	 *  Start a chained out-anim → load → in-anim flow.
	 *
	 * Phase becomes `OutAnim` immediately; once the out-anim completes the
	 * orchestrator transitions to `Loading`, at which point
	 * `pendingLoadPath` returns the configured path exactly once for the
	 * host to handle. After the host loads, the loading screen stays up for
	 * at least `minHoldDuration` seconds before the in-anim plays.
	 * @param[in] iRequest The load parameters.
	 */
	static void requestSceneLoad(const SceneLoadRequest& iRequest);

	/**
	 * @brief
	 *  Advance the active overlay / orchestrator (call once per frame).
	 * @param[in] iDeltaTime Frame delta time in seconds.
	 */
	static void update(float iDeltaTime);

	/**
	 * @brief
	 *  Render whatever the current state needs (overlay, loading
	 * screen, …). Safe to call when idle (no draw issued).
	 * @param[in] iViewportWidth Viewport width in pixels.
	 * @param[in] iViewportHeight Viewport height in pixels.
	 */
	static void render(float iViewportWidth, float iViewportHeight);

	/**
	 * @brief
	 *  Check if a transition is currently active.
	 * @return True when a transition is in progress.
	 */
	[[nodiscard]] static auto isActive() -> bool;

	/**
	 * @brief
	 *  Get the current transition progress (0..1).
	 * @return Progress in `[0, 1]`; 0 when no transition is active.
	 */
	[[nodiscard]] static auto getProgress() -> float;

	/**
	 * @brief
	 *  Active overlay kind — only meaningful while `OutAnim` /
	 * `InAnim` (and for legacy primitive overlays). `Idle` and `Loading`
	 *  both report `Type::None`.
	 * @return The active transition type.
	 */
	[[nodiscard]] static auto getType() -> Type;

	/**
	 * @brief
	 *  Configured cover colour (undefined while `Idle`).
	 * @return The configured cover colour.
	 */
	[[nodiscard]] static auto getColor() -> const math::vec4&;

	/**
	 * @brief
	 *  Current orchestrator phase.
	 * @return The current phase.
	 */
	[[nodiscard]] static auto getPhase() -> Phase;

	/**
	 * @brief
	 *  Pop the pending scene-load path the host is expected to swap to.
	 *
	 * Returns the configured path exactly once after the out-anim completes.
	 * Subsequent calls during the same orchestrator run return `nullopt`.
	 * @return The scene path if a swap is due, otherwise `nullopt`.
	 */
	[[nodiscard]] static auto pendingLoadPath() -> std::optional<std::string>;

	/**
	 * @brief
	 *  Drop any in-flight overlay or orchestrator state.
	 */
	static void reset();

private:
	/// Active overlay kind (also used during the OutAnim / InAnim phases).
	static Type s_type;
	/// Configured duration of the active overlay (seconds).
	static float s_duration;
	/// Elapsed time since the active overlay started (seconds).
	static float s_elapsed;
	/// Active tint colour.
	static math::vec4 s_color;
	/// Orchestrator phase (`Idle` for direct `play` / `start` calls).
	static Phase s_phase;
	/// Pending request (consumed once during the `Loading` phase).
	static std::optional<SceneLoadRequest> s_request;
	/// Whether the host has been handed the load path yet for the current run.
	static bool s_loadDispatched;
	/// Time held in the `Loading` phase (seconds).
	static float s_loadingHeld;
};

}// namespace owl::scene
