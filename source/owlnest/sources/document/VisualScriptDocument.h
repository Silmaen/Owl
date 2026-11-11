/**
 * @file VisualScriptDocument.h
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "NodeGraphDocument.h"
#include "VisualScriptCanvas.h"

#include <script/ScriptDebugger.h>
#include <script/visual/VisualScript.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

class TextEditor;

namespace owl::nest {

/**
 * @brief
 *  Undo manager of a visual script: every edit stores the graph before and after.
 */
using VisualScriptUndoManager = UndoManager<script::visual::VisualScript>;

/**
 * @brief
 *  What the visual script document shows.
 */
enum struct VisualScriptViewMode : uint8_t {
	Graph,///< The nodes.
	Split,///< The nodes and the generated Lua side by side.
	Code///< The generated Lua.
};

/**
 * @brief
 *  Visual script document (`.owlvs`): an event graph on the node canvas, compiled to Lua as it is edited.
 *
 * The `script::visual::VisualScript` is the document, drawn by a `VisualScriptCanvas` and compiled after each edit.
 * A right click on the canvas opens the node palette; the Hierarchy panel lists every node to drag onto the canvas,
 * the Properties panel edits the variables and the selected node.
 * The toolbar shows the graph, the generated Lua (coloured, read-only) or both side by side.
 */
class VisualScriptDocument final : public NodeGraphDocument {
public:
	VisualScriptDocument(const VisualScriptDocument&) = delete;

	VisualScriptDocument(VisualScriptDocument&&) = delete;

	auto operator=(const VisualScriptDocument&) -> VisualScriptDocument& = delete;

	auto operator=(VisualScriptDocument&&) -> VisualScriptDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	VisualScriptDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~VisualScriptDocument() override;

	/**
	 * @brief
	 *  Tab title displayed in the document tab bar.
	 * @return The file name, or `Untitled`.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  Whether the graph changed since the last save.
	 * @return True when the document is dirty.
	 */
	[[nodiscard]] auto isDirty() const -> bool override;

	/**
	 * @brief
	 *  Render the toolbar, then the graph, the generated Lua or both.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Attach to the editor.
	 * @param[in] iEditor The editor layer.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  The graph replaces the scene panels: node library in the Hierarchy, details in the Properties.
	 * @return Always true.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Node library: every node, by group, to click or drag onto the canvas.
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Variables and selected node tabs.
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title of the Hierarchy panel.
	 * @return `Nodes`.
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Nodes"; }

	/**
	 * @brief
	 *  Title of the Properties panel.
	 * @return `Graph`.
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Graph"; }

	/**
	 * @brief
	 *  Save the graph to an explicit path.
	 * @param[in] iPath Destination file.
	 * @return True on success.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Load a graph from disk.
	 * @param[in] iPath The `.owlvs` file.
	 * @return True on success.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Current graph, serialized.
	 * @return The YAML text.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Replace the graph with an autosaved one, keeping the path; the document becomes dirty.
	 * @param[in] iSnapshot The serialized graph.
	 * @return True when the snapshot was applied.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Whether an edit can be undone.
	 * @return True when the undo stack is not empty.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether an edit can be redone.
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
	 *  The edited graph.
	 * @return The visual script.
	 */
	[[nodiscard]] auto getScript() const -> const script::visual::VisualScript& { return m_script; }

	/**
	 * @brief
	 *  Undo history of the graph.
	 * @return The undo manager.
	 */
	[[nodiscard]] auto getUndoManager() const -> const VisualScriptUndoManager& { return m_undo; }

	/**
	 * @brief
	 *  Replace the graph as one undoable edit.
	 * @param[in] iNext The new graph.
	 * @param[in] iDescription Undo entry text; consecutive edits with the same text merge into one.
	 * @param[in] iMerge Whether the edit may merge with the previous one of the same text.
	 * @return True when the graph changed.
	 */
	auto commit(script::visual::VisualScript iNext, const std::string& iDescription, bool iMerge = false) -> bool;

	/**
	 * @brief
	 *  Add a node of a palette type, linked to a pin when one is given.
	 * @param[in] iType Node type identifier; `var.get|name` and `var.set|name` name the variable.
	 * @param[in] iPosition Top-left corner on the canvas.
	 * @param[in] iLinkFrom Pin to link to the first compatible pin of the new node.
	 * @return The new node, or 0 when the type is unknown.
	 */
	auto addNode(const std::string& iType, std::array<float, 2> iPosition,
				 const std::optional<VisualScriptPinRef>& iLinkFrom = std::nullopt) -> core::UUID;

	/**
	 * @brief
	 *  Show a grouped pin (`rgba`, `xyz`) as its components or as one pin, as one edit; its links are removed and
	 *  its value is carried over.
	 * @param[in] iNode The node.
	 * @param[in] iGroup Group name.
	 * @param[in] iSplit True to show the components.
	 */
	void setPinSplit(core::UUID iNode, const std::string& iGroup, bool iSplit);

	/**
	 * @brief
	 *  Change the number of item pins of a Make List or a Break List node, dropping the links and values of the pins
	 *  it loses, as one undoable edit.
	 * @param[in] iNode The node.
	 * @param[in] iCount Number of items, kept between 1 and `visual::g_MaxListItems`.
	 */
	void setListItemCount(core::UUID iNode, size_t iCount);

	/**
	 * @brief
	 *  What the document shows.
	 * @return The view mode.
	 */
	[[nodiscard]] auto getViewMode() const -> VisualScriptViewMode { return m_viewMode; }

	/**
	 * @brief
	 *  Show the graph, the generated Lua or both.
	 * @param[in] iMode The view mode.
	 */
	void setViewMode(const VisualScriptViewMode iMode) { m_viewMode = iMode; }

	/**
	 * @brief
	 *  Selected nodes.
	 * @return Their identifiers.
	 */
	[[nodiscard]] auto getSelection() const -> const std::vector<core::UUID>& { return m_view.getSelection(); }

	/**
	 * @brief
	 *  Replace the selected nodes.
	 * @param[in] iNodes The nodes.
	 */
	void setSelection(std::vector<core::UUID> iNodes) { m_view.setSelection(std::move(iNodes)); }

	/**
	 * @brief
	 *  Delete nodes and their links, as one edit.
	 * @param[in] iNodes The nodes.
	 */
	void deleteNodes(std::span<const core::UUID> iNodes);

	/**
	 * @brief
	 *  Copy nodes and the links between them, as a graph text.
	 * @param[in] iNodes The nodes.
	 * @return The YAML of the partial graph.
	 */
	[[nodiscard]] auto copyNodes(std::span<const core::UUID> iNodes) const -> std::string;

	/**
	 * @brief
	 *  Add a function or a macro, with its entry and return nodes linked at the centre of the view.
	 * @param[in] iMacro True for a macro with two exits.
	 * @return Its name.
	 */
	auto addFunction(bool iMacro) -> std::string;

	/**
	 * @brief
	 *  Remove a function with its entry, return and call nodes.
	 * @param[in] iName Function name.
	 */
	void removeFunction(const std::string& iName);

	/**
	 * @brief
	 *  Rename a function and the nodes that name it.
	 * @param[in] iName Current name.
	 * @param[in] iNewName New name.
	 */
	void renameFunction(const std::string& iName, const std::string& iNewName);

	/**
	 * @brief
	 *  Replace the signature of a function, dropping the links of the pins it loses.
	 * @param[in] iName Function name.
	 * @param[in] iFunction The new definition (its name is kept).
	 * @param[in] iMerge Merge with the previous edit of the same kind (typing in a field).
	 */
	void setFunction(const std::string& iName, script::visual::GraphFunction iFunction, bool iMerge = false);

	/**
	 * @brief
	 *  Make a function pure or not: its calls of this graph change node type and lose the links of the pins they lose.
	 * @param[in] iName Function name.
	 * @param[in] iPure Whether its calls have no execution pins.
	 */
	void setFunctionPure(const std::string& iName, bool iPure);

	/**
	 * @brief
	 *  Frame nodes in a new group fitted around them.
	 * @param[in] iNodes The nodes.
	 * @param[in] iTitle Title of the group.
	 * @return The group, 0 when no node is given.
	 */
	auto groupNodes(std::span<const core::UUID> iNodes, const std::string& iTitle = "Group") -> core::UUID;

	/**
	 * @brief
	 *  Replace a group (title, colour, place), as one undoable edit that merges with the next of the same kind.
	 * @param[in] iGroup The group, found by its identifier.
	 */
	void setGroup(const script::visual::GraphGroup& iGroup);

	/**
	 * @brief
	 *  Remove a group, its nodes kept or removed with it.
	 * @param[in] iGroup Group identifier.
	 * @param[in] iWithNodes Whether the nodes it holds go too.
	 */
	void removeGroup(core::UUID iGroup, bool iWithNodes);

	/**
	 * @brief
	 *  Move nodes up or down so that their wires run straight, from left to right.
	 *
	 * A node lines up the pin of its incoming execution wire, else of its first incoming wire, else of its first
	 * outgoing wire, with the pin at the other end; the waypoints of that wire go.
	 * @param[in] iNodes The nodes.
	 * @return How many nodes moved.
	 */
	auto straightenNodes(std::span<const core::UUID> iNodes) -> size_t;

	/**
	 * @brief
	 *  Frame each event and function entry outside a group in a group named after it, with its execution chain and
	 *  the pure nodes feeding it; chains whose frames would cross share one group.
	 * @return How many groups were made.
	 */
	auto groupEvents() -> size_t;

	/**
	 * @brief
	 *  Compile the graph and type-check its Lua against the engine API; the problems mark their nodes until the next
	 *  edit.
	 */
	void check();

	/**
	 * @brief
	 *  Problems the last check found.
	 * @return The problems, empty when the graph is clean or changed since.
	 */
	[[nodiscard]] auto getCheckResults() const -> std::span<const script::visual::CompileDiagnostic> {
		return m_checked ? std::span{m_checkResults} : std::span<const script::visual::CompileDiagnostic>{};
	}

	/**
	 * @brief
	 *  Turn nodes into a new function and put its call in their place.
	 *
	 * The values the nodes read from outside become the parameters, the values read outside the results; the execution
	 * enters at one pin and leaves by one output at most. Nodes without execution pins give a pure function. The body
	 * moves below the graph, framed by a group named after the function.
	 * @param[in] iNodes The nodes.
	 * @return The function name, empty when the nodes cannot form one (an event, a function entry or return, several
	 *  execution entries or exits).
	 */
	auto collapseToFunction(std::span<const core::UUID> iNodes) -> std::string;

	/**
	 * @brief
	 *  Move a function and its body to a library graph that every graph can call, and make the calls of this graph
	 *  call the library.
	 *
	 * The library file is created, or the function added to it; it is written at once, the edit of this graph is
	 * undoable. Links between the body and the rest of the graph are dropped: a library function reads its parameters
	 * only.
	 * @param[in] iName Function name.
	 * @param[in] iAssetDirectory Asset folder the library path is relative to.
	 * @param[in] iLibrary Asset path of the library graph (`scripts/lib/movement.owlvs`).
	 * @return True when the function moved; false when it is unknown, a macro, or the library holds one of its name.
	 */
	auto moveFunctionToLibrary(const std::string& iName, const std::filesystem::path& iAssetDirectory,
							   const std::string& iLibrary) -> bool;

	/**
	 * @brief
	 *  Add a custom node: a Lua function of the graph with one input and one output, called by a new node at the
	 *  centre of the view.
	 * @return Its name.
	 */
	auto addCustomNode() -> std::string;

	/**
	 * @brief
	 *  Remove a custom node and the nodes that call it.
	 * @param[in] iName Custom node name.
	 */
	void removeCustomNode(const std::string& iName);

	/**
	 * @brief
	 *  Rename a custom node and the nodes that call it.
	 * @param[in] iName Current name.
	 * @param[in] iNewName New name.
	 */
	void renameCustomNode(const std::string& iName, const std::string& iNewName);

	/**
	 * @brief
	 *  Replace the definition of a custom node: a pin renamed in place keeps its links, the links of a lost pin go,
	 *  and its calls follow when it turns pure or not.
	 * @param[in] iName Custom node name.
	 * @param[in] iCustom The new definition (its name is kept).
	 * @param[in] iMerge Merge with the previous edit of the same kind (typing in a field).
	 */
	void setCustomNode(const std::string& iName, script::visual::CustomNode iCustom, bool iMerge = false);

	/**
	 * @brief
	 *  A Lua module of the project and its annotated functions.
	 */
	struct LuaModuleEntry {
		/// Module name, as `require` takes it (`scripts.lib.maths`).
		std::string module;
		/// Its annotated functions.
		std::vector<script::visual::LuaFunction> functions;
	};

	/**
	 * @brief
	 *  Replace the Lua modules the palette offers (Owl Nest lists the project ones).
	 * @param[in] iModules The modules.
	 */
	void setLuaModules(std::vector<LuaModuleEntry> iModules) { m_luaModules = std::move(iModules); }

	/**
	 * @brief
	 *  A library graph of the project and the functions it offers.
	 */
	struct LibraryEntry {
		/// Asset path of the `.owlvs`, as the call nodes name it.
		std::string path;
		/// Its functions and macros.
		std::vector<script::visual::GraphFunction> functions;
	};

	/**
	 * @brief
	 *  Replace the library graphs the palette offers (Owl Nest lists the project ones).
	 * @param[in] iLibraries The libraries.
	 */
	void setLibraries(std::vector<LibraryEntry> iLibraries) { m_libraries = std::move(iLibraries); }

	/**
	 * @brief
	 *  Paste copied nodes with fresh identifiers, as one edit; they become the selection.
	 * @param[in] iText Text from `copyNodes`.
	 * @param[in] iOffset Offset added to every pasted position.
	 * @return The pasted nodes.
	 */
	auto pasteNodes(const std::string& iText, std::array<float, 2> iOffset) -> std::vector<core::UUID>;

	/**
	 * @brief
	 *  Errors of the last compilation.
	 * @return The diagnostics, empty when the graph compiles.
	 */
	[[nodiscard]] auto getDiagnostics() const -> const std::vector<script::visual::CompileDiagnostic>& {
		return m_diagnostics;
	}

	/**
	 * @brief
	 *  Lua generated by the last successful compilation.
	 * @return The Lua source.
	 */
	[[nodiscard]] auto getCompiledLua() const -> const std::string& { return m_compiledLua; }

	/**
	 * @brief
	 *  Set or remove the breakpoint of a node: the game pauses when the node runs (F9, or the node menu).
	 * @param[in] iNode The node.
	 */
	void toggleBreakpoint(core::UUID iNode);

	/**
	 * @brief
	 *  Nodes holding a breakpoint.
	 * @return Their identifiers.
	 */
	[[nodiscard]] auto getBreakpoints() const -> const std::set<core::UUID>& { return m_breakpoints; }

	/**
	 * @brief
	 *  Node a line of the compiled Lua belongs to.
	 * @param[in] iLine Line, from 1.
	 * @return The node, or nothing for a line of no node.
	 */
	[[nodiscard]] auto findNodeAtLine(int iLine) -> std::optional<core::UUID>;

	/**
	 * @brief
	 *  Select and centre the node of a line of the compiled Lua.
	 * @param[in] iLine Line, from 1.
	 */
	void revealLine(int iLine);

	/**
	 * @brief
	 *  What the running game shows on the graph, given by the editor every frame of Play.
	 * @param[in] iValues Probe values of an instance of the script, empty when none runs.
	 * @param[in] iStoppedLine Line the game stopped on in this script, or nothing.
	 */
	void setDebugState(const std::vector<script::WatchValue>& iValues, std::optional<int> iStoppedLine);

	/**
	 * @brief
	 *  Select the custom event of a Lua function, adding it at the centre of the view when the graph has none.
	 * @param[in] iFunction Lua name of the event (a HUD widget callback).
	 * @return The event node.
	 */
	auto ensureCustomEvent(const std::string& iFunction) -> core::UUID;

	/**
	 * @brief
	 *  The visual script a scene or a HUD gets when one of its handlers is first edited in a graph:
	 *  `scripts/<file>.owlvs` (`<file>_hud` for a HUD) in the asset folder of the file, written from the starter graph
	 *  when missing.
	 * @param[in] iSceneFile Path of the `.owl` or `.owlhud` on disk.
	 * @return The path on disk and the asset path, or nothing when the file is in no asset folder or cannot be written.
	 */
	[[nodiscard]] static auto prepareSceneGraph(const std::filesystem::path& iSceneFile)
			-> std::optional<std::pair<std::filesystem::path, std::string>>;

	/**
	 * @brief
	 *  What the graph shows of the debugger.
	 * @return Breakpoints, stopped node and wire values.
	 */
	[[nodiscard]] auto getDebugView() const -> const VisualScriptDebugView& { return m_view.getDebugView(); }

	/**
	 * @brief
	 *  A new graph: an `On Create` and an `On Update` event.
	 * @return The starter graph.
	 */
	[[nodiscard]] static auto starterScript() -> script::visual::VisualScript;

private:
	/**
	 * @brief
	 *  Compile the graph when it changed.
	 */
	void compileIfStale();

	/**
	 * @brief
	 *  Edit shortcuts of the canvas: Delete, Home, Ctrl+A / C / X / V / D.
	 * @param[in] iDeleteUsed Whether the canvas used Delete on a link this frame.
	 */
	void handleShortcuts(bool iDeleteUsed);

	/**
	 * @brief
	 *  Toolbar above the canvas: framing, view mode, compilation status.
	 */
	void renderToolbar();

	/**
	 * @brief
	 *  Context menu: the palette on the background, node actions on a node, split and links on a pin.
	 */
	void renderContextMenu();

	/**
	 * @brief
	 *  Nodes by group, with the search field.
	 * @param[in] iPopup Whether it is the right-click palette (click adds at the cursor) or the library panel
	 * (click adds in the view, drag drops).
	 */
	void renderNodeList(bool iPopup);

	/// Entry of the palette: its key (node type, or `func.call|name`), its label and its node type.
	using PaletteEntry = std::function<void(const std::string&, const std::string&, const script::visual::NodeType&)>;

	/**
	 * @brief
	 *  Nodes matching the search of the palette, under the header of their category.
	 * @param[in] iHeads First node type of each category, in palette order.
	 * @param[in] iEntry Draws an entry from its key, label and type.
	 * @param[in] iItem Draws the entry of a node type.
	 * @param[in] iAccepts Whether a node type takes the link being dropped.
	 */
	void renderNodeSearch(std::span<const script::visual::NodeType* const> iHeads, const PaletteEntry& iEntry,
						  const std::function<void(const script::visual::NodeType&)>& iItem,
						  const std::function<bool(const script::visual::NodeType&)>& iAccepts);

	/**
	 * @brief
	 *  Variable list.
	 */
	void renderVariables();

	/**
	 * @brief
	 *  Min, max and step rows of a numeric variable: an empty field leaves it unbounded.
	 * @param[in] iIndex Index of the variable.
	 */
	void drawVariableBounds(size_t iIndex);

	/**
	 * @brief
	 *  Function and macro list.
	 */
	void renderFunctions();

	/**
	 * @brief
	 *  Editors of the parameters, results or exits of a function.
	 * @param[in] iFunction The function.
	 */
	void renderSignature(const script::visual::GraphFunction& iFunction);

	/**
	 * @brief
	 *  Custom node list: name, title, pins, kind and Lua body of each.
	 */
	void renderCustomNodes();

	/**
	 * @brief
	 *  Editors of a list of typed pins.
	 * @param[in] iTitle Title of the list.
	 * @param[in] iParameters The pins.
	 * @param[in] iAddName Name of a new pin, numbered when taken.
	 * @param[in] iAddLabel Label of the add button.
	 * @return The edited list when it changed.
	 */
	[[nodiscard]] static auto renderParameters(const char* iTitle,
											   const std::vector<script::visual::GraphParameter>& iParameters,
											   const std::string& iAddName, const char* iAddLabel)
			-> std::optional<std::vector<script::visual::GraphParameter>>;

	/**
	 * @brief
	 *  Palette entries of the functions of the graph and of the libraries.
	 * @param[in] iItem Draws one entry from its node key, label and type.
	 */
	void renderFunctionEntries(const std::function<void(const std::string&, const std::string&,
														const script::visual::NodeType&)>& iItem) const;

	/**
	 * @brief
	 *  List the library graphs of the project, at most every few seconds.
	 */
	void refreshLibraries();

	/**
	 * @brief
	 *  Settings, grouped pins and unlinked input values of the selected node.
	 */
	void renderSelectedNode();

	/**
	 * @brief
	 *  Set or clear a value of a node, as one edit merged with the next of the same kind.
	 * @param[in] iNode The node.
	 * @param[in] iKey Pin name or setting.
	 * @param[in] iValue The value; empty clears it.
	 */
	void setNodeValue(core::UUID iNode, const std::string& iKey, const std::string& iValue);

	/**
	 * @brief
	 *  Title section of the selected node: description, errors, variable or function name.
	 * @param[in] iNode The node.
	 * @param[in] iType Its type.
	 */
	void renderNodeHeader(const script::visual::GraphNode& iNode, const script::visual::NodeType& iType);

	/**
	 * @brief
	 *  Split / recombine buttons of the grouped pins of the selected node.
	 * @param[in] iNode The node.
	 */
	void renderGroupedPins(const script::visual::GraphNode& iNode);

	/**
	 * @brief
	 *  Values of the unlinked inputs of the selected node.
	 * @param[in] iNode The node.
	 */
	void renderNodeInputs(const script::visual::GraphNode& iNode);

	/**
	 * @brief
	 *  The generated Lua, read-only and coloured.
	 */
	void renderLua();

	/// The graph.
	script::visual::VisualScript m_script;
	/// Undo history.
	VisualScriptUndoManager m_undo;
	/// Graph view.
	VisualScriptCanvas m_view;
	/// Errors of the last compilation.
	std::vector<script::visual::CompileDiagnostic> m_diagnostics;
	/// Problems the last check found, shown while the graph is unchanged.
	std::vector<script::visual::CompileDiagnostic> m_checkResults;
	/// Whether the check results are those of the graph as it is.
	bool m_checked{false};
	/// Lua of the last successful compilation.
	std::string m_compiledLua;
	/// Lines of the debug build each node runs, from the last successful compilation.
	std::map<core::UUID, std::vector<int>> m_nodeLines;
	/// Nodes holding a breakpoint.
	std::set<core::UUID> m_breakpoints;
	/// Library graphs offered by the palette.
	std::vector<LibraryEntry> m_libraries;
	/// Lua modules offered by the palette.
	std::vector<LuaModuleEntry> m_luaModules;
	/// Frame count of the last library scan.
	int m_libraryScanFrame{-1000};

	/**
	 * @brief
	 *  Give the ScriptDebugger the Lua lines of the nodes holding a breakpoint.
	 */
	void syncBreakpoints() const;
	/// Graph text at the last save.
	std::string m_savedText;
	/// Search text of the palette and of the library.
	std::string m_paletteFilter;
	/// Copied nodes in the clipboard when the palette opened, empty when there are none.
	std::string m_paletteClipboard;
	/// Canvas position where the palette adds a node.
	std::array<float, 2> m_paletteTarget{0.f, 0.f};
	/// Node under the cursor at the last right click.
	std::optional<core::UUID> m_contextNode;
	/// Group whose header the context menu was opened on.
	std::optional<core::UUID> m_contextGroup;
	/// Asset path typed in the Move to Library popup of a function.
	std::string m_libraryTarget;
	/// Pin under the cursor at the last right click.
	std::optional<VisualScriptPinRef> m_contextPin;
	/// Pin whose link was dropped on the background: the palette links the new node to it.
	std::optional<VisualScriptPinRef> m_pendingLink;
	/// Open the context menu on the next frame.
	bool m_openContextMenu{false};
	/// The graph changed since the last compilation.
	bool m_compileStale{true};
	/// What the document shows.
	VisualScriptViewMode m_viewMode{VisualScriptViewMode::Graph};
	/// Read-only Lua view.
	uniq<TextEditor> mp_luaView;
	/// Text shown by the Lua view.
	std::string m_luaViewText;
	/// Counts the pastes, to shift each one a little more.
	uint32_t m_pasteCount{0};
};

}// namespace owl::nest
