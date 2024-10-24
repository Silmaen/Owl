/**
 * @file SceneHierarchy.h
 * @author Silmaen
 * @date 26/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::nest::panel {
/**
 * @brief Class SceneHierarchy
 */
class SceneHierarchy final {
public:
	/**
	 * @brief Default copy constructor
	 */
	SceneHierarchy(const SceneHierarchy&) = default;
	/**
	 * @brief Default move constructor
	 */
	SceneHierarchy(SceneHierarchy&&) = default;
	/**
	 * @brief Default copy assignation
	 * @return this
	 */
	auto operator=(const SceneHierarchy&) -> SceneHierarchy& = default;
	/**
	 * @brief Default move assignation
	 * @return this
	 */
	auto operator=(SceneHierarchy&&) -> SceneHierarchy& = default;
	/**
	 * @brief Default constructor.
	 */
	SceneHierarchy() = default;
	/**
	 * @brief Destructor.
	 */
	~SceneHierarchy() = default;

	/**
	 * @brief Constructor
	 * @param[in] iScene The base scene
	 */
	[[maybe_unused]] explicit SceneHierarchy(const shared<scene::Scene>& iScene);
	/**
	 * @brief Define the Scene context
	 * @param[in] iContext The Scene
	 */
	void setContext(const shared<scene::Scene>& iContext);
	/**
	 * @brief Action on Gui Render
	 */
	void onImGuiRender();
	/**
	 * @brief Access to the selected entity
	 * @return The selected entity
	 */
	[[nodiscard]] auto getSelectedEntity() const -> scene::Entity { return m_selection; }
	void setSelectedEntity(const scene::Entity& iEntity) { m_selection = iEntity; }

private:
	void renderHierarchy();
	void renderProperties();

	/**
	 * @brief Draw one entity node
	 * @param[in,out] ioEntity The node to draw
	 */
	void drawEntityNode(scene::Entity& ioEntity);
	/**
	 * @brief Draw the properties of a component
	 * @param[in,out] ioEntity The entity
	 */
	void drawComponents(scene::Entity& ioEntity);
	/// The scene
	shared<scene::Scene> m_context;
	/// The selected item
	scene::Entity m_selection;
};
}// namespace owl::nest::panel
