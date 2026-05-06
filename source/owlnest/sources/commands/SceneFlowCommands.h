/**
 * @file SceneFlowCommands.h
 * @author Silmaen
 * @date 27/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "NodeGraphCommands.h"

#include <filesystem>

namespace owl::nest {

class EditorLayer;
}

namespace owl::nest::commands {
/**
 * @brief
 *  Atomic undo step that pairs a scene mutation with a canvas mutation for Scene Flow edits.
 *
 * Scene Flow has a 1:1 relationship between a Teleport `Trigger` entity in a scene and an output
 * pin/link on the canvas — drawing a link must create the entity, deleting the link must destroy
 * it, and a single undo must reverse both. The composite is itself a `NodeGraphUndoCommand` so it
 * lives on the Scene Flow document's `NodeGraphUndoManager` and the user only ever sees one undo
 * step on the stack.
 *
 * The source scene is referenced by **path**, not by `shared<Scene>`. On each undo/redo the
 * composite asks `EditorLayer::loadOrOpenSceneDocument(path)` to resolve the live `Scene` —
 * this keeps the command robust if the user closed the source scene's tab between operations.
 *
 * Order:
 *  - `redo`: scene side first (the new entity must exist before the link is drawn), canvas second.
 *  - `undo`: canvas side first (the link must be gone before the entity disappears), scene second.
 */
class SceneFlowCompositeCommand final : public NodeGraphUndoCommand {
public:
	SceneFlowCompositeCommand(const SceneFlowCompositeCommand&) = delete;

	SceneFlowCompositeCommand(SceneFlowCompositeCommand&&) = delete;

	auto operator=(const SceneFlowCompositeCommand&) -> SceneFlowCompositeCommand& = delete;

	auto operator=(SceneFlowCompositeCommand&&) -> SceneFlowCompositeCommand& = delete;

	/**
	 * @brief
	 *  Construct a composite scene+canvas command.
	 * @param[in] iSceneCmd Scene-side command (e.g. CreateEntity / DeleteEntity for the Trigger).
	 * @param[in] iCanvasCmd Canvas-side command (e.g. AddLink + AddOutputPin compound).
	 * @param[in] iSourceScenePath On-disk path of the scene to mutate.
	 * @param[in] iEditor Editor used to resolve the live `Scene` on each undo/redo.
	 * @param[in] iDescription Human-readable description for the undo menu.
	 */
	SceneFlowCompositeCommand(uniq<SceneUndoCommand> iSceneCmd, uniq<NodeGraphUndoCommand> iCanvasCmd,
							  std::filesystem::path iSourceScenePath, EditorLayer* iEditor,
							  std::string iDescription);

	/**
	 * @brief
	 *  Destructor.
	 */
	~SceneFlowCompositeCommand() override;

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioCanvas The target canvas the action is applied to.
	 */
	void undo(gui::widgets::NodeCanvas& ioCanvas) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioCanvas The target canvas the action is applied to.
	 */
	void redo(gui::widgets::NodeCanvas& ioCanvas) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override { return m_description; }

private:
	uniq<SceneUndoCommand> m_sceneCmd;///< Scene-side half of the composite (create/delete Trigger entity).
	uniq<NodeGraphUndoCommand> m_canvasCmd;///< Canvas-side half (link + output pin maintenance).
	std::filesystem::path m_sourceScenePath;///< Path resolved to a live `Scene` per undo/redo call.
	EditorLayer* mp_editor;///< Used to resolve `m_sourceScenePath` to a live `SceneDocument`.
	std::string m_description;///< Human-readable label for the menu.
};
/**
 * @brief
 *  Compound canvas command that adds a link AND an output pin in one undo step.
 *
 * The Scene Flow uses one output pin per Teleport trigger; creating a link from a fresh node →
 * destination requires both creating the new pin AND wiring the link. Bundling them lets the
 * `SceneFlowCompositeCommand` glue scene+canvas in a single undo step.
 */
class AddPinAndLinkCommand final : public NodeGraphUndoCommand {
public:
	/**
	 * @brief
	 *  Construct.
	 * @param[in] iSourceNodeId Node where the new output pin is appended.
	 * @param[in] iPin Output pin to add.
	 * @param[in] iDestInputPinId Input pin of the destination scene node — link target.
	 */
	AddPinAndLinkCommand(core::UUID iSourceNodeId, gui::widgets::NodePin iPin, core::UUID iDestInputPinId);

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void undo(gui::widgets::NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void redo(gui::widgets::NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override { return "Add Teleport Link"; }

private:
	core::UUID m_sourceNodeId;///< Node owning the new pin.
	gui::widgets::NodePin m_pin;///< Snapshot of the pin (id reused on every redo).
	core::UUID m_destInputPinId;///< Destination pin (input on the destination scene node).
	core::UUID m_linkId{0};///< Populated on first redo so undo can target the exact link.
};
/**
 * @brief
 *  Compound canvas command that removes a link AND its source output pin in one undo step.
 *
 * Symmetric counterpart to `AddPinAndLinkCommand`. Captures both the link and the pin snapshots so
 * the original ids are preserved across undo/redo cycles.
 */
class RemovePinAndLinkCommand final : public NodeGraphUndoCommand {
public:
	/**
	 * @brief
	 *  Construct.
	 * @param[in] iSourceNodeId Node that owns the pin being removed.
	 * @param[in] iPin Snapshot of the pin (used to restore on undo).
	 * @param[in] iLink Snapshot of the link (used to restore on undo).
	 */
	RemovePinAndLinkCommand(core::UUID iSourceNodeId, gui::widgets::NodePin iPin, gui::widgets::Link iLink);

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void undo(gui::widgets::NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void redo(gui::widgets::NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override { return "Remove Teleport Link"; }

private:
	/// Source teleport node owning the dynamically-added pin.
	core::UUID m_sourceNodeId;
	/// Pin snapshot — re-attached to the node when the link is undone.
	gui::widgets::NodePin m_pin;
	/// Link snapshot — recreated on undo.
	gui::widgets::Link m_link;
};

}// namespace owl::nest::commands
