/**
 * @file MaterialDocument.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "commands/MaterialCommands.h"
#include "viewer/MeshPreview.h"

#include <core/UUID.h>
#include <gui/widgets/NodeCanvas.h>
#include <math/vectors.h>
#include <renderer/Material.h>
#include <renderer/MaterialGraph.h>
#include <renderer/MeshCache.h>
#include <renderer/gpu/Framebuffer.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  Undo manager typed for `renderer::MaterialAsset` edits.
 *
 * Each command keeps the whole asset before and after the edit: a material is a few dozen values.
 */
using MaterialUndoManager = UndoManager<renderer::MaterialAsset>;

/**
 * @brief
 *  A `.owlmat` material edited in a tab: its parent, every value (or, with a parent, every override), its graph on a
 *  node canvas and a live preview on a sphere or a cube, with its own undo stack.
 *
 * A gesture of the tab runs a `material.*` command (`commands/MaterialCommands.h`), which edits the document through
 * `setMaterial`; a drag of a slider merges into one undo step. A graph material shows its nodes on the canvas, the
 * palette and the Parameters, Custom and Functions tabs in the Hierarchy panel, the selected node in the Properties
 * panel, the diagnostics of the compiler on their nodes and a thumbnail of each node output. Saving writes the file
 * atomically and drops the material from the engine's `MaterialLibrary`, so every entity drawing it updates at once.
 */
class MaterialDocument final : public Document, public commands::MaterialEditor {
public:
	MaterialDocument(const MaterialDocument&) = delete;

	MaterialDocument(MaterialDocument&&) = delete;

	auto operator=(const MaterialDocument&) -> MaterialDocument& = delete;

	auto operator=(MaterialDocument&&) -> MaterialDocument& = delete;

	/**
	 * @brief
	 *  Create an empty material document (the default root material).
	 */
	MaterialDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~MaterialDocument() override;

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Kind of document.
	 * @return `DocumentType::Material`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Material; }

	/**
	 * @brief
	 *  Title of the tab.
	 * @return The file name, "Untitled" without a path.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  Path of the file.
	 * @return The path, empty when none was set.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Whether the material differs from the saved one.
	 * @return True when there are unsaved edits.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return m_undo.isDirty(); }

	/**
	 * @brief
	 *  Attach the document to the editor.
	 * @param[in] iEditor The editor.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Detach the document from the editor and free the previews.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Render the preview and the thumbnails of the nodes.
	 * @param[in] iTimeStep Time since the last frame.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Event hook (nothing to do: ImGui consumes its own events).
	 * @param[in,out] ioEvent The event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Draw the tab: the parameters (or the graph canvas) on the left, the preview on the right.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Write the material to its file and drop it from the material library.
	 * @return True when written; false without a path or when the write failed.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Write the material to another file, which becomes the document's.
	 * @param[in] iPath The file.
	 * @return True when written.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  The material as YAML, for the autosave.
	 * @return The text.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Put back the material of an autosave.
	 * @param[in] iSnapshot The YAML text.
	 * @return True when it parsed.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Placeholder of the `Document` base (the typed manager is `materialUndoManager`).
	 * @return The empty scene undo manager.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Placeholder of the `Document` base.
	 * @return The empty scene undo manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Whether an edit can be undone.
	 * @return True when the stack is not empty.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether an undone edit can be redone.
	 * @return True when the redo stack is not empty.
	 */
	[[nodiscard]] auto canRedo() const -> bool override { return m_undo.canRedo(); }

	/**
	 * @brief
	 *  Undo the last edit.
	 */
	void performUndo() override;

	/**
	 * @brief
	 *  Redo the last undone edit.
	 */
	void performRedo() override;

	/**
	 * @brief
	 *  Whether the Hierarchy and Properties panels show the graph (palette, tabs, selected node).
	 * @return True for a graph material.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return isGraph(); }

	/**
	 * @brief
	 *  Draw the Hierarchy panel: the node palette and the Parameters, Custom and Functions tabs.
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Draw the Properties panel: the selected node.
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title of the Hierarchy panel.
	 * @return "Material Graph".
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Material Graph"; }

	/**
	 * @brief
	 *  Title of the Properties panel.
	 * @return "Node".
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Node"; }

	// --- MaterialEditor interface ---------------------------------------------
	/**
	 * @brief
	 *  The material being edited.
	 * @return The asset.
	 */
	[[nodiscard]] auto material() const -> const renderer::MaterialAsset& override { return m_asset; }

	/**
	 * @brief
	 *  Replace the material as one undoable step.
	 * @param[in] iAsset The material after the edit.
	 * @param[in] iDescription Name of the step.
	 * @param[in] iMerge Whether the step joins the previous one of the same name.
	 * @return False when nothing changes.
	 */
	auto setMaterial(const renderer::MaterialAsset& iAsset, const std::string& iDescription, bool iMerge)
			-> bool override;

	/**
	 * @brief
	 *  Choose the parent material (see `setParent`).
	 * @param[in] iParent Asset path of the parent `.owlmat`, empty for a root material.
	 * @return True when it changed.
	 */
	auto setMaterialParent(const std::filesystem::path& iParent) -> bool override { return setParent(iParent); }

	/**
	 * @brief
	 *  Replace the values (see `setValues`).
	 * @param[in] iValues The new values.
	 * @param[in] iOverrides Mask of the fields the material sets.
	 * @param[in] iMerge True for a drag of a slider.
	 * @return True when it changed.
	 */
	auto setMaterialValues(const renderer::MaterialParameters& iValues, const uint32_t iOverrides, const bool iMerge)
			-> bool override {
		return setValues(iValues, iOverrides, iMerge);
	}

	// --- Material API --------------------------------------------------------
	/**
	 * @brief
	 *  The material being edited.
	 * @return The asset.
	 */
	[[nodiscard]] auto asset() const -> const renderer::MaterialAsset& { return m_asset; }

	/**
	 * @brief
	 *  The typed undo stack.
	 * @return The manager.
	 */
	[[nodiscard]] auto materialUndoManager() const -> const MaterialUndoManager& { return m_undo; }

	/**
	 * @brief
	 *  Read a material file into the document.
	 * @param[in] iPath The `.owlmat` file.
	 * @return True when it was read.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Choose the parent material, or none.
	 * @param[in] iParent Asset path of the parent `.owlmat`, empty for a root material.
	 * @return True when it changed.
	 */
	auto setParent(const std::filesystem::path& iParent) -> bool;

	/**
	 * @brief
	 *  Replace the values and the overridden fields.
	 * @param[in] iValues The new values.
	 * @param[in] iOverrides Mask of the fields the material sets (used with a parent).
	 * @param[in] iMerge True for a drag of a slider: the edits of the next second join one undo step.
	 * @return True when it changed.
	 */
	auto setValues(const renderer::MaterialParameters& iValues, uint32_t iOverrides, bool iMerge = false) -> bool;

	/**
	 * @brief
	 *  The material as it is drawn: the parent's resolved material with the overridden fields and graph values, the
	 *  compiled graph, or the values alone.
	 * @return The material.
	 */
	[[nodiscard]] auto buildMaterial() const -> renderer::Material;

	/**
	 * @brief
	 *  Shape of the preview.
	 * @return The shape.
	 */
	[[nodiscard]] auto previewShape() const -> viewer::PreviewShape { return m_shape; }

	/**
	 * @brief
	 *  Choose the shape of the preview.
	 * @param[in] iShape The shape.
	 */
	void setPreviewShape(viewer::PreviewShape iShape);

	// --- Graph ---------------------------------------------------------------
	/**
	 * @brief
	 *  Whether the material is a root material with a graph.
	 * @return True when it has a graph.
	 */
	[[nodiscard]] auto isGraph() const -> bool { return m_asset.parent.empty() && m_asset.graph.has_value(); }

	/**
	 * @brief
	 *  The graph compiled as it is (its diagnostics say what is wrong), an empty program without a graph.
	 * @return The program.
	 */
	[[nodiscard]] auto getProgram() const -> const renderer::MaterialProgram& { return m_program; }

	/**
	 * @brief
	 *  The function whose body the canvas shows.
	 * @return Its name, empty for the graph itself.
	 */
	[[nodiscard]] auto getScope() const -> const std::string& { return m_scope; }

	/**
	 * @brief
	 *  Show the body of a function on the canvas, or the graph itself.
	 * @param[in] iFunction The function, empty for the graph.
	 * @return False for an unknown function.
	 */
	auto setScope(const std::string& iFunction) -> bool;

	/**
	 * @brief
	 *  The canvas of the graph (or of the function body shown).
	 * @return The canvas.
	 */
	[[nodiscard]] auto canvas() const -> const gui::widgets::NodeCanvas& { return m_canvas; }

	/**
	 * @brief
	 *  Bring the canvas in line with the graph now (it is otherwise rebuilt before it is drawn).
	 */
	void syncCanvas();

	/**
	 * @brief
	 *  The node shown in the Properties panel.
	 * @return The node, none when nothing is selected.
	 */
	[[nodiscard]] auto getSelectedNode() const -> const std::optional<core::UUID>& { return m_selectedNode; }

	/**
	 * @brief
	 *  Select a node of the scope.
	 * @param[in] iNode The node, none to clear.
	 */
	void selectNode(std::optional<core::UUID> iNode);

	/**
	 * @brief
	 *  Run a `material.*` command on this material through the editor (journaled, undoable).
	 * @param[in] iName Command name.
	 * @param[in] iArgs Arguments, without `path` (and `function`, set to the scope for the node commands).
	 * @return What the command did, nothing when it failed or there is no editor.
	 */
	auto runCommand(std::string_view iName, commands::CommandArgs iArgs) -> std::optional<commands::CommandResult>;

	/**
	 * @brief
	 *  Whether the nodes show the thumbnail of their output.
	 * @return True when shown.
	 */
	[[nodiscard]] auto hasThumbnails() const -> bool { return m_showThumbnails; }

	/**
	 * @brief
	 *  Show or hide the thumbnails of the nodes.
	 * @param[in] iShow True to show them.
	 */
	void setThumbnails(bool iShow);

	// --- Tab -----------------------------------------------------------------
	/**
	 * @brief
	 *  True while the user has not clicked the close button of the tab.
	 * @return True until ImGui flips the close flag.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Set the open flag.
	 * @param[in] iOpen The new value.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/// A pin of the canvas: its node, name and side.
	struct PinRef {
		/// Owning node.
		core::UUID node{0};
		/// Pin name.
		std::string pin;
		/// Whether it is an output.
		bool output{false};
	};

	/// The preview of one node output.
	struct Thumbnail {
		/// Image of the node, unlit on a sphere.
		shared<renderer::gpu::Framebuffer> framebuffer;
		/// Key of the program drawn into it.
		std::string key;
		/// Whether the node output compiles.
		bool valid{false};
		/// Edit of the material it shows (`m_editTime` then).
		std::chrono::steady_clock::time_point revision;
	};

	/**
	 * @brief
	 *  Make an asset the current one and record the step.
	 * @param[in] iNext The asset after the edit.
	 * @param[in] iDescription Description of the step.
	 * @param[in] iMerge Whether the step joins the previous one of the same description.
	 * @return True when the asset changed.
	 */
	auto commit(renderer::MaterialAsset iNext, const std::string& iDescription, bool iMerge) -> bool;

	/**
	 * @brief
	 *  Compile the graph again, mark the canvas and the preview stale.
	 */
	void onAssetChanged();

	/**
	 * @brief
	 *  Draw the parent field and the fields of the material.
	 */
	void renderParameters();

	/**
	 * @brief
	 *  Draw the preview, its shape buttons and handle its orbit.
	 */
	void renderPreview();

	/**
	 * @brief
	 *  Draw the graph view: the canvas on the left, the preview, the settings and the diagnostics on the right.
	 * @param[in] iFocused Whether the tab has the focus (shortcuts).
	 */
	void renderGraphView(bool iFocused);

	/**
	 * @brief
	 *  Draw the toolbar of the tab (conversion, scope, thumbnails, diagnostics).
	 */
	void renderToolbar();

	/**
	 * @brief
	 *  Draw the canvas and handle its gestures.
	 * @param[in] iFocused Whether the tab has the focus (shortcuts).
	 */
	void renderCanvas(bool iFocused);

	/**
	 * @brief
	 *  Hook the canvas callbacks to the commands.
	 */
	void bindCanvas();

	/**
	 * @brief
	 *  Rebuild the canvas from the nodes of the scope.
	 */
	void rebuildCanvas();

	/**
	 * @brief
	 *  Draw the context menu of the canvas.
	 */
	void renderCanvasMenu();

	/**
	 * @brief
	 *  Draw the node palette.
	 * @param[in] iPosition Canvas position a click adds the node at.
	 * @param[in] iPopup Whether it is the right-click menu (a click closes it) or the panel (entries drag).
	 */
	void renderPalette(std::array<float, 2> iPosition, bool iPopup);

	/**
	 * @brief
	 *  Add a node of the palette.
	 * @param[in] iEntry `type` or `type|item` (the parameter, custom node or function it names).
	 * @param[in] iPosition Canvas position.
	 */
	void addPaletteNode(std::string_view iEntry, std::array<float, 2> iPosition);

	/**
	 * @brief
	 *  Paste the nodes of the clipboard.
	 * @param[in] iPosition Canvas position of the top-left node.
	 */
	void pasteClipboard(std::array<float, 2> iPosition);

	/**
	 * @brief
	 *  Draw the settings of a graph material that are not part of the surface (shading, alpha, IOR, toon).
	 */
	void renderGraphSettings();

	/**
	 * @brief
	 *  Draw the list of the diagnostics.
	 */
	void renderDiagnostics();

	/**
	 * @brief
	 *  Draw the parameters of the graph.
	 */
	void renderGraphParameters();

	/**
	 * @brief
	 *  Draw the custom nodes of the graph.
	 */
	void renderCustomNodes();

	/**
	 * @brief
	 *  Draw the functions of the graph.
	 */
	void renderFunctions();

	/**
	 * @brief
	 *  Draw the overrides of the parameters of the parent graph (a child material).
	 */
	void renderGraphOverrides();

	/**
	 * @brief
	 *  Draw the settings and the unlinked inputs of the selected node.
	 */
	void renderNodeInspector();

	/**
	 * @brief
	 *  Draw the row of one setting of the selected node.
	 * @param[in] iNode The node (a copy: an edit replaces the graph).
	 * @param[in] iSetting The setting.
	 */
	void renderNodeSetting(const renderer::MaterialGraphNode& iNode, const renderer::MaterialSettingSpec& iSetting);

	/**
	 * @brief
	 *  Draw the row of one input of the selected node: its value while it is not linked.
	 * @param[in] iNode The node (a copy: an edit replaces the graph).
	 * @param[in] iInput The input.
	 */
	void renderNodeInput(const renderer::MaterialGraphNode& iNode, const renderer::MaterialPinSpec& iInput);

	/**
	 * @brief
	 *  Run `material.set_node_value` on a node of the scope.
	 * @param[in] iNode The node.
	 * @param[in] iName The setting or input.
	 * @param[in] iValue The value, none to give back the default.
	 * @param[in] iMerge Whether the step joins the previous one (a drag).
	 */
	void setNodeValue(core::UUID iNode, const std::string& iName, const std::optional<std::string>& iValue,
					  bool iMerge);

	/**
	 * @brief
	 *  Draw the editor of typed pins.
	 * @param[in] iTitle Title of the list.
	 * @param[in] iPins The pins.
	 * @param[in] iBase Name of a new pin.
	 * @return The pins after an edit, nothing without one.
	 */
	auto renderPins(const char* iTitle, const std::vector<renderer::MaterialPinSpec>& iPins, const char* iBase)
			-> std::optional<std::vector<renderer::MaterialPinSpec>>;

	/**
	 * @brief
	 *  Render the thumbnails of the nodes seen on the canvas last frame, one new shader per frame at most.
	 */
	void updateThumbnails();

	/**
	 * @brief
	 *  Draw the thumbnail of a node in its footer.
	 * @param[in] iNode The node.
	 * @param[in] iMin Top-left corner of the footer, in screen pixels.
	 * @param[in] iMax Bottom-right corner of the footer, in screen pixels.
	 */
	void drawThumbnail(core::UUID iNode, const math::vec2f& iMin, const math::vec2f& iMax);

	/**
	 * @brief
	 *  Whether a slider edit joins the previous one (the mouse is still down on consecutive frames).
	 * @return True during a drag.
	 */
	auto isDragging() -> bool;

	/// Disk path of the material.
	std::filesystem::path m_path;
	/// Material being edited.
	renderer::MaterialAsset m_asset;
	/// Undo stack of the material.
	MaterialUndoManager m_undo;
	/// Editor back-pointer.
	EditorLayer* mp_editorLayer{nullptr};
	/// Preview framebuffer.
	shared<renderer::gpu::Framebuffer> m_framebuffer;
	/// Size of the preview area in pixels.
	math::vec2ui m_canvasSize{0, 0};
	/// Preview shape.
	viewer::PreviewShape m_shape{viewer::PreviewShape::Sphere};
	/// Meshes of the shapes, built on first use.
	shared<renderer::MeshResource> m_shapeMeshes[2];
	/// Material drawn by the preview.
	std::optional<renderer::Material> m_previewMaterial;
	/// Whether the preview material must be built again.
	bool m_previewDirty{true};
	/// Generation of the material library the preview material was built from.
	uint64_t m_libraryGeneration{0};
	/// Orbit of the preview camera.
	viewer::MeshOrbit m_orbit;
	/// Frame of the last edit from the parameters, to tell a drag from separate edits.
	int m_lastEditFrame{-2};
	/// Tracks the ImGui "Open" state of the tab.
	bool m_pOpen{true};
	/// Whether the tab was focused last frame.
	bool m_wasFocused{false};
	/// Graph compiled as it is.
	renderer::MaterialProgram m_program;
	/// Shared copy of the graph drawn by the previews, null without a graph.
	shared<const renderer::MaterialGraph> m_sharedGraph;
	/// Time of the last edit, for the debounce of the preview shader.
	std::chrono::steady_clock::time_point m_editTime;
	/// Seconds of the preview, for the `input.time` node.
	float m_time{0.f};
	/// Canvas of the nodes of the scope.
	gui::widgets::NodeCanvas m_canvas;
	/// Whether the canvas must be rebuilt from the graph.
	bool m_canvasStale{true};
	/// Frames during which the canvas frames every node.
	uint32_t m_fitFrames{3};
	/// Function whose body the canvas shows, empty for the graph.
	std::string m_scope;
	/// Canvas pins by id.
	std::map<core::UUID, PinRef> m_pins;
	/// Canvas pin ids by node, side and name.
	std::map<std::tuple<core::UUID, bool, std::string>, core::UUID> m_pinIds;
	/// Node shown in the Properties panel.
	std::optional<core::UUID> m_selectedNode;
	/// Node the canvas menu was opened on.
	std::optional<core::UUID> m_menuNode;
	/// Canvas position the menu adds nodes at.
	std::array<float, 2> m_menuPosition{0.f, 0.f};
	/// Whether the canvas asked for its menu this frame.
	bool m_openMenu{false};
	/// Whether nodes are being dragged (their positions are committed on release).
	bool m_moving{false};
	/// Whether a link was cut this frame (the Delete key is then not for the nodes).
	bool m_linkDeleted{false};
	/// Filter of the palette.
	std::string m_paletteFilter;
	/// Whether the nodes show their thumbnail.
	bool m_showThumbnails{true};
	/// Thumbnails by node.
	std::map<core::UUID, Thumbnail> m_thumbnails;
	/// Nodes whose footer was on screen last frame.
	std::set<core::UUID> m_visibleNodes;
	/// Placeholder required by `Document::undoManager()`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
