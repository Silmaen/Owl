/**
 * @file SceneHierarchy.h
 * @author Silmaen
 * @date 26/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include "../UndoManager.h"

namespace owl::nest {

class Document;
}

namespace owl::nest::panel {
/**
 * @brief
 *  Class SceneHierarchy.
 */
class SceneHierarchy final {
public:
	/**
	 * @brief
	 *  Default copy constructor.
	 */
	SceneHierarchy(const SceneHierarchy&) = default;

	/**
	 * @brief
	 *  Default move constructor.
	 */
	SceneHierarchy(SceneHierarchy&&) = default;

	/**
	 * @brief
	 *  Default copy assignation.
	 * @return this
	 */
	auto operator=(const SceneHierarchy&) -> SceneHierarchy& = default;

	/**
	 * @brief
	 *  Default move assignation.
	 * @return this
	 */
	auto operator=(SceneHierarchy&&) -> SceneHierarchy& = default;

	/**
	 * @brief
	 *  Default constructor.
	 */
	SceneHierarchy() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~SceneHierarchy() = default;

	/**
	 * @brief
	 *  Constructor
	 * @param[in] iScene The base scene
	 */
	[[maybe_unused]] explicit SceneHierarchy(const shared<scene::Scene>& iScene);

	/**
	 * @brief
	 *  Define the Scene context.
	 * @param[in] iContext The Scene
	 */
	void setContext(const shared<scene::Scene>& iContext);

	/**
	 * @brief
	 *  Action on Gui Render.
	 */
	void onImGuiRender();

	/**
	 * @brief
	 *  Access to the selected entity.
	 * @return The selected entity
	 */
	[[nodiscard]] auto getSelectedEntity() const -> scene::Entity { return m_selection; }

	/**
	 * @brief
	 *  Set the selected entity.
	 * @param[in] iEntity The target entity.
	 */
	void setSelectedEntity(const scene::Entity& iEntity) { m_selection = iEntity; }

	/**
	 * @brief
	 *  Set the undo manager for recording undoable operations.
	 */
	void setUndoManager(SceneUndoManager* iUndoManager) { mp_undoManager = iUndoManager; }

	/**
	 * @brief
	 *  Track the currently active document so its custom panels can override the scene-based
	 *        rendering when `Document::overridesGlobalPanels()` is true.
	 * @param[in] iDoc Active document (non-owning, may be null when no document is open).
	 */
	void setActiveDocument(Document* iDoc) { mp_activeDocument = iDoc; }

	/**
	 * @brief
	 *  Name of the component header most recently hovered while the inspector was rendered.
	 *
	 * Cleared on every frame at the start of `onImGuiRender`. Used by `EditorLayer::onContextualHelp`
	 * (F1) to pick the help page that documents the component currently under the mouse.
	 * @return The const std string.
	 */
	[[nodiscard]] static auto lastHoveredComponentName() -> const std::string&;

private:
	/**
	 * @brief
	 *  Render hierarchy.
	 */
	void renderHierarchy();

	/**
	 * @brief
	 *  Render properties.
	 */
	void renderProperties();

	/**
	 * @brief
	 *  Draw root entities. Falls back to a flat list when the active `RenderStack` has 0 or 1
	 * layer; otherwise groups roots into a tree node per layer (entities are routed by their
	 * `RendererTag` — untagged ones land under the first layer).
	 */
	void renderRootEntities();

	/**
	 * @brief
	 *  Draw the context menu for an entity node.
	 */
	void drawEntityContextMenu(const scene::Entity& iEntity, bool iHasChildren, core::UUID iParentId);

	/**
	 * @brief
	 *  Draw one entity node.
	 * @param[in] iEntity The node to draw.
	 */
	void drawEntityNode(const scene::Entity& iEntity);

	/**
	 * @brief
	 *  Draw the properties of a component.
	 * @param[in,out] iEntity The entity
	 */
	void drawComponents(const scene::Entity& iEntity);
	/// The scene
	shared<scene::Scene> m_context = nullptr;
	/// The selected item
	scene::Entity m_selection;
	/// Undo manager (non-owning, optional).
	SceneUndoManager* mp_undoManager = nullptr;
	/// Active document — when it `overridesGlobalPanels()` the panel delegates its content to it.
	Document* mp_activeDocument = nullptr;
};
}// namespace owl::nest::panel
