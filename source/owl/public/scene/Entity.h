/**
 * @file Entity.h
 * @author Silmaen
 * @date 22/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Scene.h"
#include "component/ID.h"
#include "component/Tag.h"

#include <entt/entity/entity.hpp>

namespace owl::scene {
/**
 * @brief Class describing a scene entity.
 */
class OWL_API Entity final {
public:
	Entity() = default;
	Entity(const Entity&) = default;
	Entity(Entity&&) = default;
	auto operator=(const Entity&) -> Entity& = default;
	auto operator=(Entity&&) -> Entity& = default;

	/**
	 * @brief Destructor.
	 */
	~Entity() = default;

	/**
	 * @brief Constructor.
	 * @param[in] iHandle Entity handle.
	 * @param[in] iScene Parent scene.
	 */
	Entity(entt::entity iHandle, Scene* iScene);


	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wundefined-func-template")
	/**
	 * @brief Add component to this entity.
	 * @tparam T Type of the component.
	 * @tparam Args Type of the component's constructor arguments.
	 * @param[in] iArgs Arguments for the component's constructor
	 * @return The entity's new component.
	 */
	template<typename T, typename... Args>
	auto addComponent(Args&&... iArgs) -> T& {
		OWL_CORE_ASSERT(!hasComponent<T>(), "Entity already has component!")
		T& component = mp_scene->registry.emplace<T>(m_entityHandle, std::forward<Args>(iArgs)...);
		mp_scene->onComponentAdded<T>(*this, component);
		return component;
	}

	/**
	 * @brief Add or replace component to this entity.
	 * @tparam T Type of the component.
	 * @tparam Args Type of the component's constructor arguments.
	 * @param[in] iArgs Arguments for the component's constructor
	 * @return The entity's new or updated component.
	 */
	template<typename T, typename... Args>
	auto addOrReplaceComponent(Args&&... iArgs) -> T& {
		T& component = mp_scene->registry.emplace_or_replace<T>(m_entityHandle, std::forward<Args>(iArgs)...);
		mp_scene->onComponentAdded<T>(*this, component);
		return component;
	}

	OWL_DIAG_POP

	/**
	 * @brief Access to the component of the given type.
	 * @tparam T The type of component.
	 * @return The entity's component.
	 */
	template<typename T>
	[[nodiscard]] auto getComponent() const -> T& {
		OWL_CORE_ASSERT(hasComponent<T>(), "Entity does not have component!")
		return mp_scene->registry.get<T>(m_entityHandle);
	}

	/**
	 * @brief Check if the entity has the component.
	 * @tparam T The type of component.
	 * @return true if the entity has the component.
	 */
	template<typename T>
	[[nodiscard]] auto hasComponent() const -> bool {
		return mp_scene->registry.all_of<T>(m_entityHandle);
	}

	/**
	 * @brief Remove the component from entity.
	 * @tparam T The type of component.
	 */
	template<typename T>
	void removeComponent() const {
		OWL_CORE_ASSERT(hasComponent<T>(), "Entity does not have component!")
		mp_scene->registry.remove<T>(m_entityHandle);
	}

	/**
	 * @brief Get entity as boolean.
	 * @return True if handle exists.
	 */
	explicit operator bool() const { return m_entityHandle != entt::null; }

	/**
	 * @brief Get entity as handle.
	 * @return The entity's handle.
	 */
	explicit operator entt::entity() const { return m_entityHandle; }

	/**
	 * @brief Get the entity as unsigned int.
	 * @return The entity's handle code.
	 */
	explicit operator uint32_t() const { return static_cast<uint32_t>(m_entityHandle); }

	/**
	 * @brief Get entity's UUID.
	 * @return Entity's UUID.
	 */
	[[nodiscard]] auto getUUID() const -> core::UUID { return getComponent<component::ID>().id; }

	/**
	 * @brief Get entity's name.
	 * @return Entity's name.
	 */
	[[nodiscard]] auto getName() const -> std::string { return getComponent<component::Tag>().tag; }

	/**
	 * @brief Comparison operator.
	 * @param[in] iOther Other entity to compare
	 * @return true if entities are the same.
	 */
	auto operator==(const Entity& iOther) const -> bool {
		return m_entityHandle == iOther.m_entityHandle && mp_scene == iOther.mp_scene;
	}

	/**
	 * @brief Comparison operator.
	 * @param[in] iOther Other entity to compare
	 * @return true if entities are not the same.
	 */
	auto operator!=(const Entity& iOther) const -> bool { return !(*this == iOther); }

	/**
	 * @brief Access to the scene.
	 * @return The scene.
	 */
	[[nodiscard]] auto getScene() const -> Scene* { return mp_scene; }

private:
	/// Entity's handle.
	entt::entity m_entityHandle{entt::null};
	/// Parent scene.
	Scene* mp_scene = nullptr;
	friend class Scene;
};
}// namespace owl::scene
