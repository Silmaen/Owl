/**
 * @file SceneFlowDocument.h
 * @author Silmaen
 * @date 24/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "NodeGraphDocument.h"

#include <unordered_map>

namespace owl::nest {

struct Project;

/**
 * @brief Read-only (for now) node-graph view of the project's scenes and their teleport links.
 *
 * The first consumer of the `NodeCanvas` framework. Specialises `NodeGraphDocument` by:
 * - scanning the project directory for every `.owl` scene,
 * - parsing each scene's YAML to extract Teleport triggers (with the source entity name),
 * - best-effort scanning of attached Lua scripts for `scene.load_scene("...")` calls,
 * - creating one node per scene (entry pin + one output pin per teleport / Lua transition),
 * - linking output pins to the destination scene's entry pin,
 * - detecting unreachable scenes via a BFS from the project's first scene and marking them red,
 * - navigating to a scene on double-click (delegates to `EditorLayer::openScene`).
 *
 * Right-clicking a node opens a context menu with **Edit** (open the scene) and **Delete** (remove
 * the file). Right-clicking on empty space exposes **Add new scene**. The bottom-left overlay shows
 * project metadata and, when a node is selected, a quick summary of that scene.
 *
 * Link create/delete and per-pin target editing (writing `Trigger` components back into scene
 * files) are scheduled for a follow-up — they require composite undo commands that mix
 * `SceneUndoCommand` and `NodeGraphUndoCommand` and are kept out of the first slice for simplicity.
 */
class SceneFlowDocument final : public NodeGraphDocument {
public:
	SceneFlowDocument(const SceneFlowDocument&) = delete;
	SceneFlowDocument(SceneFlowDocument&&) = delete;
	auto operator=(const SceneFlowDocument&) -> SceneFlowDocument& = delete;
	auto operator=(SceneFlowDocument&&) -> SceneFlowDocument& = delete;

	SceneFlowDocument();
	~SceneFlowDocument() override;

	[[nodiscard]] auto title() const -> std::string override { return "Scene Flow"; }

	/// @brief Rescan the active project and rebuild the canvas from scratch.
	void refreshFromProject(const Project& iProject);

	/// @brief Re-render with the right-click popup + modals on top of the base canvas.
	void onImGuiRender() override;

	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }
	void renderHierarchyPanel() override;
	void renderPropertiesPanel() override;

protected:
	void onCanvasReady() override;

private:
	/// @brief Write a minimal valid `.owl` YAML file to `iAbsolutePath` and rescan if it succeeded.
	auto createSceneFile(const std::filesystem::path& iAbsolutePath) -> bool;
	/// @brief Delete the scene file at `iAbsolutePath` from disk and rescan.
	void deleteSceneFile(const std::filesystem::path& iAbsolutePath);
	/// @brief Resolve a node id to its absolute scene path through `customData`.
	[[nodiscard]] auto absolutePathFor(core::UUID iNodeId) const -> std::filesystem::path;
	/// @brief Resolve a node id to its project-relative scene path (e.g. `scenes/level1.owl`).
	[[nodiscard]] auto relativePathFor(core::UUID iNodeId) const -> std::string;
	/// @brief Render the right-click popup if pending, plus any modal dialogs.
	void renderPopups();
	/// @brief Render the right-click context menu (over a node OR over empty canvas space).
	void renderContextMenu();
	/// @brief Render the "Add new scene…" modal triggered from the context menu.
	void renderAddSceneModal();
	/// @brief Render the "Delete scene?" confirmation modal.
	void renderDeleteSceneModal();
	/// @brief Validate a drag from `iFromPin` to `iToPin`. Only ghost-pin → entry-pin is accepted today.
	[[nodiscard]] auto validateLinkDraft(core::UUID iFromPin, core::UUID iToPin) const -> bool;
	/// @brief Handle a successful link draft: cancel the auto-link, create a Trigger entity and a
	///        real output pin, push a composite undo step.
	void onLinkDrafted(core::UUID iLinkId, core::UUID iFromPin, core::UUID iToPin);
	/// @brief Handle a link erase: restore the link, push a composite that destroys the matching
	///        Trigger entity, removes the pin, and removes the link.
	void onLinkErased(core::UUID iLinkId);
	/// @brief Open the targetName modal for the given Teleport output pin.
	void openTargetNameModal(core::UUID iPinId);
	/// @brief Render the targetName modal if pending.
	void renderTargetNameModal();

	/// Absolute project root — stored so double-click / context actions can resolve relative paths.
	std::filesystem::path m_projectDirectory;
	/// Project name (cached so the overlay does not have to walk the project metadata each frame).
	std::string m_projectName;
	/// Project first-scene relative path (cached for the orphan flag in the overlay).
	std::string m_projectFirstScene;

	/// Set the next frame to open the right-click popup.
	bool m_openContextMenu = false;
	/// Node hit by the last right-click (zero = right-clicked on empty space).
	core::UUID m_contextNodeId{0};
	/// Last node selected via single click, used by the info overlay.
	core::UUID m_selectedNodeId{0};

	/// Set the next frame to open the "Add new scene" modal.
	bool m_openAddSceneModal = false;
	/// Buffer for the new scene name input (kept across frames for ImGui InputText).
	char m_newSceneNameBuf[128] = {};

	/// Path of the scene pending deletion confirmation; empty = no modal.
	std::filesystem::path m_pendingDeletePath;

	/// @brief Per-link bookkeeping for Teleport links — used to find the live Trigger entity on delete.
	struct LinkOrigin {
		std::filesystem::path scenePath;///< Source scene on disk.
		core::UUID triggerEntityUuid{0};///< Trigger entity in the source scene.
		core::UUID sourceNodeId{0};///< Canvas node owning the output pin.
		core::UUID outputPinId{0};///< The output pin id.
		core::UUID inputPinId{0};///< The input pin (destination scene's entry).
	};
	/// Live links → Teleport bookkeeping. Refreshed by `refreshFromProject`, mutated on link create/delete.
	std::unordered_map<core::UUID, LinkOrigin> m_linkOrigins;
	/// Per-pin Teleport bookkeeping (for the targetName edit popup which only knows the pin).
	std::unordered_map<core::UUID, LinkOrigin> m_pinToOrigin;
	/// One ghost "+ Add teleport" pin per scene node. Drag from this pin = create a new Trigger.
	std::unordered_map<core::UUID, core::UUID> m_nodeToGhostPin;
	/// Pin that owns the open targetName modal; 0 = no modal.
	core::UUID m_targetNameModalPin{0};
	/// Buffer for the targetName ImGui input.
	char m_targetNameBuf[128] = {};
};

}// namespace owl::nest
