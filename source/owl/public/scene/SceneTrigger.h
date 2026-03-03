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
	 * @brief The type of trigger
	 */
	enum struct TriggerType : uint8_t {
		Victory,/// Player win.
		Death,/// Player loose.
		Target,/// Passive position marker (no action on collision).
		Teleport,/// Teleport the player to a named target entity, optionally in another level.
	};
	/// The type of trigger.
	TriggerType type = TriggerType::Victory;

	/// Name of the level to load (Teleport type, empty = same level).
	std::string levelName;
	/// Name of the target entity to teleport to (Teleport type).
	std::string targetName;

	/**
	 * @brief Check if triggered.
	 * @return True if triggered.
	 */
	[[nodiscard]] auto isTriggered() const -> bool { return m_triggered; }

private:
	/// If triggered.
	bool m_triggered = false;
};

}// namespace owl::scene
