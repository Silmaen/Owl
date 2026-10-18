/**
 * @file nestTestHelper.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "UndoManager.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/Transform.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace owl::nest::test {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
/**
 * @brief
 *  Fixture silencing the logger around every editor test.
 */
class NestTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }

	/// Scene under test.
	scene::Scene m_scene;
	/// Undo manager driving the commands under test.
	SceneUndoManager m_undo;
};
OWL_DIAG_POP

/**
 * @brief
 *  Local X translation of an entity.
 * @param[in] iEntity The entity.
 * @return The local X translation.
 */
inline auto localX(const scene::Entity& iEntity) -> float {
	return iEntity.getComponent<scene::component::Transform>().transform.translation().x();
}

/**
 * @brief
 *  Set the local X translation of an entity.
 * @param[in] iEntity The entity.
 * @param[in] iX The new local X translation.
 */
inline void setLocalX(const scene::Entity& iEntity, const float iX) {
	iEntity.getComponent<scene::component::Transform>().transform.translation().x() = iX;
}

/**
 * @brief
 *  World X translation of an entity.
 * @param[in] iScene The scene holding the entity.
 * @param[in] iEntity The entity.
 * @return The world X translation.
 */
inline auto worldX(const scene::Scene& iScene, const scene::Entity& iEntity) -> float {
	return iScene.getWorldTransform(iEntity).translation().x();
}

/**
 * @brief
 *  Parent UUID of an entity.
 * @param[in] iEntity The entity.
 * @return The parent UUID (0 for a root).
 */
inline auto parentOf(const scene::Entity& iEntity) -> core::UUID {
	return iEntity.getComponent<scene::component::Hierarchy>().parentId;
}

/**
 * @brief
 *  Children UUIDs of an entity, in sibling order.
 * @param[in] iEntity The entity.
 * @return The children UUIDs.
 */
inline auto childrenOf(const scene::Entity& iEntity) -> std::vector<core::UUID> {
	return iEntity.getComponent<scene::component::Hierarchy>().childrenIds;
}

/**
 * @brief
 *  Order-independent picture of a scene: per-entity YAML, keyed by UUID.
 *
 * Registry iteration order changes when an entity is recreated, so whole-scene YAML is not
 * comparable across an undo; this map is. With `iWithChildren`, each entry also records the
 * sibling order of the entity's children.
 * @param[in] iScene The scene.
 * @param[in] iWithChildren Append the ordered children UUIDs to each entry.
 * @return The UUID → state map.
 */
inline auto sceneState(const scene::Scene& iScene, const bool iWithChildren = true) -> std::map<uint64_t, std::string> {
	std::map<uint64_t, std::string> state;
	for (const auto& entity: iScene.getAllEntities()) {
		auto entry = scene::SceneSerializer::serializeEntityToString(entity);
		if (iWithChildren) {
			entry += "\nchildren:";
			for (const auto child: childrenOf(entity)) entry += std::format(" {}", static_cast<uint64_t>(child));
		}
		state.emplace(static_cast<uint64_t>(entity.getUUID()), std::move(entry));
	}
	return state;
}

}// namespace owl::nest::test
