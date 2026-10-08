/**
 * @file ComponentRegistry.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "core/UUID.h"
#include "scene/Entity.h"
#include "scene/component/ID.h"

#include <entt/entt.hpp>

#include <concepts>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::scene {

/**
 * @brief
 *  A component type the registry can handle: a YAML key, serialization both ways, and copies (a `name()` is
 *  optional, the key stands for it otherwise).
 */
template<typename T>
concept isRegistrableComponent = std::is_copy_constructible_v<T> && std::is_default_constructible_v<T> &&
								 requires(const T& iComponent, T& ioComponent, const core::Serializer& iSerializer) {
									 { T::key() } -> std::convertible_to<const char*>;
									 { iComponent.serialize(iSerializer) } -> std::same_as<void>;
									 { ioComponent.deserialize(iSerializer) } -> std::same_as<void>;
								 };

/// Map from entity UUID to the entity of the destination registry, used to copy whole storages.
using EntityRemap = std::unordered_map<core::UUID, entt::entity>;

/**
 * @brief
 *  Everything the engine needs to handle one component type without knowing it at compile time.
 */
struct OWL_API ComponentDescriptor {
	/// YAML key of the component (unique in the registry).
	std::string key;
	/// Display name (inspector, editor commands).
	std::string name;
	/// True when users add and remove it (false for the mandatory ID, Tag, Transform, Visibility, Hierarchy).
	bool optional = true;
	/// True when duplication and the Play copy copy it (false for ID and Tag).
	bool copiable = true;
	/// True for the components of the engine.
	bool builtin = false;
	/// Whether an entity has the component.
	std::function<bool(const Entity& iEntity)> has;
	/// Add a default component (replacing an existing one).
	std::function<void(Entity& ioEntity)> add;
	/// Remove the component (no-op when absent).
	std::function<void(Entity& ioEntity)> remove;
	/// Write `key: {...}` into the current YAML map when the entity has the component.
	std::function<void(const Entity& iEntity, const core::Serializer& iOut)> serialize;
	/// Add or replace the component from its YAML node (the value under `key`).
	std::function<void(Entity& ioEntity, const core::Serializer& iNode)> deserialize;
	/// Copy the component of `iSrc` onto `oDst` when `iSrc` has it (entity duplication).
	std::function<void(Entity& oDst, const Entity& iSrc)> copy;
	/// Copy every component of a registry into another one, entities matched by UUID (Play copy).
	std::function<void(entt::registry& oDst, const entt::registry& iSrc, const EntityRemap& iRemap)> copyAll;
	/// Optional inspector body drawn by the editor for a game component; returns true when it edited something.
	std::function<bool(Entity& ioEntity)> inspect;
};

/**
 * @brief
 *  Registry of every component type the scene serializes, copies and shows in the editor.
 *
 * The engine components are registered first, in their serialization order. A game registers its own components
 * once at startup, before loading any scene: they are then saved and loaded with the scene, kept by prefabs,
 * duplication and the Play copy, and offered by the editor ("Add Component", `component.add`).
 */
class OWL_API ComponentRegistry final {
public:
	ComponentRegistry() = delete;

	~ComponentRegistry() = delete;

	ComponentRegistry(const ComponentRegistry&) = delete;

	ComponentRegistry(ComponentRegistry&&) = delete;

	auto operator=(const ComponentRegistry&) -> ComponentRegistry& = delete;

	auto operator=(ComponentRegistry&&) -> ComponentRegistry& = delete;

	/**
	 * @brief
	 *  Build the descriptor of a component type.
	 * @tparam T The component type.
	 * @param[in] iOptional Whether users add and remove it.
	 * @param[in] iCopiable Whether duplication and the Play copy copy it.
	 * @return The descriptor (not registered).
	 */
	template<isRegistrableComponent T>
	[[nodiscard]] static auto describe(const bool iOptional = true, const bool iCopiable = true)
			-> ComponentDescriptor {
		ComponentDescriptor desc;
		desc.key = T::key();
		if constexpr (requires { T::name(); })
			desc.name = T::name();
		else
			desc.name = T::key();
		desc.optional = iOptional;
		desc.copiable = iCopiable;
		desc.has = [](const Entity& iEntity) -> bool { return iEntity.hasComponent<T>(); };
		desc.add = [](Entity& ioEntity) -> void { ioEntity.addOrReplaceComponent<T>(); };
		desc.remove = [](Entity& ioEntity) -> void {
			if (ioEntity.hasComponent<T>())
				ioEntity.removeComponent<T>();
		};
		desc.serialize = [](const Entity& iEntity, const core::Serializer& iOut) -> void {
			if (iEntity.hasComponent<T>())
				iEntity.getComponent<T>().serialize(iOut);
		};
		desc.deserialize = [](Entity& ioEntity, const core::Serializer& iNode) -> void {
			ioEntity.addOrReplaceComponent<T>().deserialize(iNode);
		};
		desc.copy = [](Entity& oDst, const Entity& iSrc) -> void {
			if (iSrc.hasComponent<T>())
				oDst.addOrReplaceComponent<T>(iSrc.getComponent<T>());
		};
		desc.copyAll = [](entt::registry& oDst, const entt::registry& iSrc, const EntityRemap& iRemap) -> void {
			for (const auto view = iSrc.view<component::ID, T>(); const auto entity: view) {
				if (const auto target = iRemap.find(view.template get<component::ID>(entity).id);
					target != iRemap.end())
					oDst.emplace_or_replace<T>(target->second, view.template get<T>(entity));
			}
		};
		return desc;
	}

	/**
	 * @brief
	 *  Register a game component type.
	 * @tparam T The component type.
	 * @param[in] iInspect Optional inspector body for the editor (returns true when it edited something).
	 * @return False when its key or name is already registered.
	 */
	template<isRegistrableComponent T>
	static auto registerComponent(std::function<bool(Entity& ioEntity)> iInspect = {}) -> bool {
		auto desc = describe<T>();
		desc.inspect = std::move(iInspect);
		return registerDescriptor(std::move(desc));
	}

	/**
	 * @brief
	 *  Register a component from its descriptor.
	 * @param[in] iDescriptor The descriptor (every function but `inspect` must be set).
	 * @return False when the descriptor is incomplete or its key or name is already registered.
	 */
	static auto registerDescriptor(ComponentDescriptor iDescriptor) -> bool;

	/**
	 * @brief
	 *  Remove a game component type (engine components stay).
	 * @param[in] iKey The YAML key.
	 * @return False when the key is unknown or names an engine component.
	 */
	static auto unregisterComponent(const std::string& iKey) -> bool;

	/**
	 * @brief
	 *  Every registered component: the engine ones in serialization order, then the game ones.
	 * @return The descriptors.
	 */
	[[nodiscard]] static auto getAll() -> const std::vector<ComponentDescriptor>&;

	/**
	 * @brief
	 *  Find a component by YAML key or display name.
	 * @param[in] iKeyOrName The key or the name.
	 * @return The descriptor, or nullptr.
	 */
	[[nodiscard]] static auto find(const std::string& iKeyOrName) -> const ComponentDescriptor*;
};

}// namespace owl::scene
