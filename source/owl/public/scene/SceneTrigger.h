/**
 * @file SceneTrigger.h
 * @author Silmaen
 * @date 12/30/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Scene.h"
#include "core/Core.h"

namespace owl::scene {

/**
 * @brief Class describing a trigger in the scene.
 */
class OWL_API SceneTrigger final {
public:
	/**
	 * @brief Default constructor.
	 */
	SceneTrigger();
	/**
	 * @brief Default destructor.
	 */
	~SceneTrigger();
	/**
	 * @brief Default copy constructor.
	 */
	SceneTrigger(const SceneTrigger&) = default;
	/**
	 * @brief Default move constructor.
	 */
	SceneTrigger(SceneTrigger&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const SceneTrigger&) -> SceneTrigger& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(SceneTrigger&&) -> SceneTrigger& = default;

	/**
	 * @brief Action on a new trigger.
	 * @param ioPlayer The player entity that triggered.
	 * @param iTriggerEntity The entity carrying the trigger component.
	 */
	void onTriggered(Entity& ioPlayer, const Entity& iTriggerEntity);

	/**
	 * @brief Called when the player enters the trigger volume (edge).
	 * @param[in,out] ioPlayer The player entity.
	 * @param[in] iTriggerEntity The trigger entity.
	 */
	void onTriggerEnter(const Entity& ioPlayer, const Entity& iTriggerEntity);

	/**
	 * @brief Called when the player exits the trigger volume (edge).
	 * @param[in,out] ioPlayer The player entity.
	 * @param[in] iTriggerEntity The trigger entity.
	 */
	void onTriggerExit(const Entity& ioPlayer, const Entity& iTriggerEntity);

	/**
	 * @brief Called each frame for Timer triggers.
	 * @param[in] iDeltaTime Frame delta in seconds.
	 * @param[in] iTriggerEntity The trigger entity.
	 */
	void updateTimer(float iDeltaTime, const Entity& iTriggerEntity);

	/**
	 * @brief The type of trigger.
	 */
	enum struct TriggerType : uint8_t {
		Victory,/// Player win.
		Death,/// Player loose.
		Target,/// Passive position marker (no action on collision).
		Teleport,/// Teleport the player to a named target entity, optionally in another level.
		Timer,/// Fires after a duration, optionally repeating.
		Interaction,/// Fires when the player presses interact key within range.
		LuaCallback,/// Generic: calls on_triggered in Lua on overlap.
	};
	/// The type of trigger.
	TriggerType type = TriggerType::Victory;

	/// Name of the level to load (Teleport type, empty = same level).
	std::string levelName;
	/// Name of the target entity to teleport to (Teleport type).
	std::string targetName;

	/// Timer duration in seconds (Timer type).
	float timerDuration = 1.0f;
	/// Whether the timer repeats (Timer type).
	bool timerRepeating = false;

	/// Interaction range multiplier (Interaction type).
	float interactionRange = 1.5f;

	/// Name of the Lua function to call (Timer, Interaction, LuaCallback types).
	/// If empty, defaults to: on_timer, on_interact, on_triggered respectively.
	std::string callbackName;

	/**
	 * @brief Check if triggered.
	 * @return True if triggered.
	 */
	[[nodiscard]] auto isTriggered() const -> bool { return m_triggered; }

	/// Check if the player was overlapping last frame.
	[[nodiscard]] auto wasOverlapping() const -> bool { return m_playerOverlapping; }
	/// Set overlap state for the current frame.
	void setOverlapping(const bool iOverlapping) { m_playerOverlapping = iOverlapping; }

	/// Start/restart the timer.
	void startTimer();
	/// Stop the timer.
	void stopTimer();
	/// Reset the timer elapsed time.
	void resetTimer();
	/// Check if the timer is running.
	[[nodiscard]] auto isTimerRunning() const -> bool { return m_timerRunning; }

private:
	/// If triggered.
	bool m_triggered = false;
	/// Whether the player was overlapping last frame (for enter/exit detection).
	bool m_playerOverlapping = false;
	/// Timer elapsed accumulator.
	float m_timerElapsed = 0.0f;
	/// Whether the timer is currently running.
	bool m_timerRunning = false;
	/// Cooldown for interaction key (prevents repeat while held).
	bool m_interactKeyWasDown = false;
};

}// namespace owl::scene
