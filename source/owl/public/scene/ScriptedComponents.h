/**
 * @file ScriptedComponents.h
 * @author Silmaen
 * @date 16/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"
#include "script/ScriptEngine.h"

#include <cstddef>
#include <string>
#include <vector>

namespace owl::data::assets::pack {
class ExtensionRegistry;
}// namespace owl::data::assets::pack

namespace owl::scene {

class Entity;

namespace component {

/**
 * @brief
 *  Values of one component declared by a script, on one entity: the fields of its type, each with its value.
 */
struct OWL_API ScriptedComponent {
	/// Fields in the order of the declaration.
	std::vector<script::ScriptProperty> fields;

	/**
	 * @brief
	 *  Find a field by name.
	 * @param[in] iName The field name.
	 * @return The field, or null when the type has none of that name.
	 */
	[[nodiscard]] auto find(const std::string& iName) -> script::ScriptProperty*;

	/**
	 * @brief
	 *  Find a field by name, read only.
	 * @param[in] iName The field name.
	 * @return The field, or null when the type has none of that name.
	 */
	[[nodiscard]] auto find(const std::string& iName) const -> const script::ScriptProperty*;
};

}// namespace component

/**
 * @brief
 *  Type of a component declared by the entry script of an extension pack.
 */
struct OWL_API ScriptedComponentType {
	/// Key of the component, starting with the id of its pack and a dot (`demo.rules.Health`).
	std::string key;
	/// Id of the extension pack declaring it.
	std::string extensionId;
	/// Fields with their default value.
	std::vector<script::ScriptProperty> fields;
};

/**
 * @brief
 *  Components declared in Lua by the mounted extension packs, registered on the open `ComponentRegistry`.
 *
 * The entry script of a `Script` pack declares its components in a global `components` table, each one a list of
 * fields written like script properties. They are saved with the scene, copied by prefabs, duplication and Play,
 * shown in the inspector and read and written by scripts (`entity:get_field`, `entity:set_field`). Each one lives
 * in an EnTT storage of its own, named by its key.
 */
class OWL_API ScriptedComponents final {
public:
	/**
	 * @brief
	 *  Register a component type, or replace the fields of one already registered by the same pack.
	 * @param[in] iType The type.
	 * @return False when the key does not start with the id of its pack and a dot, or is taken by another component.
	 */
	static auto registerType(const ScriptedComponentType& iType) -> bool;

	/**
	 * @brief
	 *  Unregister a component type; the values already on entities are no longer saved.
	 * @param[in] iKey Key of the type.
	 * @return False when no scripted type has that key.
	 */
	static auto unregisterType(const std::string& iKey) -> bool;

	/**
	 * @brief
	 *  Register the components of every mounted `Script` pack with an entry script, and unregister those of the packs
	 *  no longer mounted.
	 * @param[in] iExtensions The mounted packs.
	 * @return Number of scripted component types registered afterwards.
	 */
	static auto sync(const data::assets::pack::ExtensionRegistry& iExtensions) -> size_t;

	/**
	 * @brief
	 *  Unregister every scripted component type.
	 */
	static void clear();

	/**
	 * @brief
	 *  The registered types.
	 * @return The types, in registration order.
	 */
	[[nodiscard]] static auto getTypes() -> const std::vector<ScriptedComponentType>&;

	/**
	 * @brief
	 *  Find a registered type.
	 * @param[in] iKey Key of the type.
	 * @return The type, or null.
	 */
	[[nodiscard]] static auto findType(const std::string& iKey) -> const ScriptedComponentType*;

	/**
	 * @brief
	 *  Values of a scripted component on an entity.
	 * @param[in] iEntity The entity.
	 * @param[in] iKey Key of the component.
	 * @return The values, or null when the entity has no such component.
	 */
	[[nodiscard]] static auto get(const Entity& iEntity, const std::string& iKey) -> component::ScriptedComponent*;

	/**
	 * @brief
	 *  Add a scripted component with the default values of its type, or keep the one the entity has.
	 * @param[in] iEntity The entity.
	 * @param[in] iKey Key of the component.
	 * @return The values, or null when no type has that key.
	 */
	static auto add(const Entity& iEntity, const std::string& iKey) -> component::ScriptedComponent*;
};

}// namespace owl::scene
