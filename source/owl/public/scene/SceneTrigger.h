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
class OWL_API SceneTrigger {
public:
	/**
	 * @brief Default constructor.
	 */
	SceneTrigger();
	/**
	 * @brief Default destructor.
	 */
	virtual ~SceneTrigger();
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
	 * @param ioEntity The entity that have trigger.
	 */
	void onTriggered(Entity& ioEntity);

	/**
	 * @brief The type of trigger
	 */
	enum struct TriggerType : uint8_t {
		Victory,/// Player win.
		Death,/// Player loose.
	};
	/// The type of trigger.
	TriggerType type;

	/**
	 * @brief Check if triggered.
	 * @return True if triggered.
	 */
	auto isTriggered() const -> bool { return m_triggered; }

private:
	/// If triggered.
	bool m_triggered = false;
};

}// namespace owl::scene
