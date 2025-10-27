/**
 * @file Scene.h
 * @author Silmaen
 * @date 22/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Timestep.h"
#include "core/UUID.h"
#include "renderer/Camera.h"

#include <entt/entt.hpp>
/**
 * @brief Namespace for the scene elements
 */
namespace owl::scene {
class Entity;
class ScriptableEntity;

/**
 * @brief Class describing a scene.
 */
class OWL_API Scene final {
public:
	Scene(const Scene&) = delete;
	Scene(Scene&&) = delete;
	auto operator=(const Scene&) -> Scene& = delete;
	auto operator=(Scene&&) -> Scene& = delete;
	/**
	 * @brief Default constructor.
	 */
	Scene();
	/**
	 * @brief Destructor.
	 */
	~Scene();

	/**
	 * @brief Create a copy of the scene.
	 * @param[in] iOther The scene to copy.
	 * @return Pointer to the new scene.
	 */
	static auto copy(const shared<Scene>& iOther) -> shared<Scene>;

	/**
	 * @brief Create entity and add it to registry.
	 * @param[in] iName Entity's name.
	 * @return The Entity.
	 */
	auto createEntity(const std::string& iName = std::string()) -> Entity;

	/**
	 * @brief Create entity with UUID and add it to registry.
	 * @param[in] iName Entity's name.
	 * @param[in] iUuid The Entity's UUID.
	 * @return The Entity.
	 */
	auto createEntityWithUUID(core::UUID iUuid, const std::string& iName = std::string()) -> Entity;

	/**
	 * @brief Destroy n entity.
	 * @param[in,out] ioEntity Entity to destroy.
	 */
	void destroyEntity(Entity& ioEntity);

	/**
	 * @brief Beginning the scene as runtime (enabling physics).
	 */
	void onStartRuntime();

	/**
	 * @brief End scene runtime mode.
	 */
	void onEndRuntime();

	/**
	 * @brief Update actions for the runtime.
	 * @param[in] iTimeStep The time step.
	 */
	void onUpdateRuntime(const core::Timestep& iTimeStep);

	/**
	 * @brief Update actions in the editor, external camera, no gameplay.
	 * @param[in] iTimeStep The time step.
	 * @param[in] iCamera The editor camera.
	 */
	void onUpdateEditor(const core::Timestep& iTimeStep, const renderer::Camera& iCamera);

	/**
	 * @brief Action when viewport resized.
	 * @param[in] iSize New viewport's size.
	 */
	void onViewportResize(const math::vec2ui& iSize);

	/**
	 * @brief Duplicate an entity.
	 * @param[in] iEntity Entity to duplicate.
	 * @return The created entity.
	 */
	auto duplicateEntity(const Entity& iEntity) -> Entity;

	/**
	 * @brief Access to the primary Camera.
	 * @return The primary camera.
	 */
	auto getPrimaryCamera() -> Entity;

	/**
	 * @brief Access to the primary player
	 * @return The primary player.
	 */
	auto getPrimaryPlayer() -> Entity;

	/**
	 * @brief Get the list of all entities.
	 * @return List of all entities.
	 */
	[[nodiscard]] auto getAllEntities() const -> std::vector<Entity>;

	/// Entities registry.
	entt::registry registry;

	/**
	 * @brief List the statuses
	 */
	enum struct Status : uint8_t {
		Editing,///< Editing.
		Playing,///< Playing.
		Victory,///< player won.
		Death,///< Player loose.
	};
	/// The scene status.
	Status status = Status::Editing;

	/**
	 * @brief Count the entities in the scene.
	 * @return The count of Entity in the scene.
	 */
	[[nodiscard]] auto getEntityCount() const -> uint32_t;

private:
	/**
	 * @brief Action when component is added to an entity.
	 * @tparam T Type of the added component.
	 * @param[in] iEntity Entity receiving new component.
	 * @param[in,out] ioComponent The new component.
	 */
	template<typename T>
	void onComponentAdded(const Entity& iEntity, T& ioComponent);

	/// Draw the elements.
	void render();
	/// The viewport's size.
	math::vec2ui m_viewportSize = {0, 0};

	friend class Entity;
	friend class ScriptableEntity;
};

}// namespace owl::scene
