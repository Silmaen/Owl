/**
 * @file NodeCanvas.h
 * @author Silmaen
 * @date 23/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/UUID.h"
#include "math/vectors.h"

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace owl::gui::widgets {
/**
 * @brief
 *  Direction of a node pin.
 */
enum struct PinKind : uint8_t {
	Input,///< Connector on the left side of a node.
	Output,///< Connector on the right side of a node.
};

/**
 * @brief
 *  A single connector on a node.
 *
 * `typeTag` is an opaque string owned by the canvas client — use it to
 * categorise pins (e.g. `"scene_entry"`/`"scene_exit"` for the Scene Flow,
 * `"anim_float"` for an animation graph). The canvas uses it only to veto
 * illegal connections through `setLinkValidator`.
 */
struct NodePin {
	core::UUID id;///< Stable pin identifier.
	std::string label;///< Short text rendered inside the node, near the connector circle.
	std::string typeTag;///< Opaque type tag owned by the client.
	PinKind kind{PinKind::Input};///< Input or output side.
	math::vec4 labelColor{1.0f, 1.0f, 1.0f, 1.0f};///< RGBA colour used to render `label` (default white).
};

/**
 * @brief
 *  A node inside a `NodeCanvas`.
 *
 * `customData` is an opaque payload the caller associates with the node
 * (e.g. the relative scene path for a Scene Flow entry). It travels through
 * copy/paste and YAML serialization unchanged.
 */
struct Node {
	core::UUID id;///< Stable node identifier.
	std::string title;///< Text rendered in the header bar.
	math::vec2f position{0.0f, 0.0f};///< Top-left corner in canvas space.
	std::vector<NodePin> inputs;///< Input pins (left side).
	std::vector<NodePin> outputs;///< Output pins (right side).
	std::string customData;///< Opaque YAML blob owned by the caller.
	math::vec4 titleColor{1.0f, 1.0f, 1.0f, 1.0f};///< Overlay colour for the title text (e.g. red for orphans).
};

/**
 * @brief
 *  An edge between two pins.
 */
struct Link {
	core::UUID id;///< Stable link identifier.
	core::UUID fromPin;///< Output pin on the source node.
	core::UUID toPin;///< Input pin on the destination node.
};

/**
 * @brief
 *  Should the canvas draw pin labels at the given zoom level?
 * @param[in] iZoomFactor Current `GraphEditor` view zoom (1.0 = native).
 * @return True while the zoom is high enough to make labels readable.
 */
[[nodiscard]] OWL_API auto shouldDrawPinLabels(float iZoomFactor) -> bool;

/**
 * @brief
 *  Should the canvas draw a node's title text at the given zoom level?
 * @param[in] iZoomFactor Current `GraphEditor` view zoom (1.0 = native).
 * @return True while the zoom is high enough to make the title readable.
 *
 * The node silhouette and connectors are always drawn regardless.
 */
[[nodiscard]] OWL_API auto shouldDrawNodeTitles(float iZoomFactor) -> bool;

/**
 * @brief
 *  A generic node-graph canvas widget.
 *
 * Backed by a pimpl over `GraphEditor` from the ImGuizmo bundle, so the
 * underlying third-party header stays private. The API exposes owl types
 * (`core::UUID`, `math::vec2f`, `math::vec4`) and standard containers; the
 * widget maintains a bidirectional mapping between UUIDs and the integer
 * indices consumed by `GraphEditor`.
 *
 * The canvas is **data only**: it never mutates the scene or any business
 * domain. All domain side effects (e.g. creating a teleport trigger when a
 * link is drawn) must happen in the callbacks registered via the
 * `setOn*` methods.
 */
class OWL_API NodeCanvas final {
public:
	NodeCanvas(const NodeCanvas&) = delete;

	NodeCanvas(NodeCanvas&&) = delete;

	auto operator=(const NodeCanvas&) -> NodeCanvas& = delete;

	auto operator=(NodeCanvas&&) -> NodeCanvas& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	NodeCanvas();

	/**
	 * @brief
	 *  Destructor.
	 */
	~NodeCanvas();

	// --- Topology ------------------------------------------------------------
	/**
	 * @brief
	 *  Add a node to the canvas.
	 * @param[in] iNode The node to add. A fresh UUID is generated when `iNode.id` is zero.
	 * @return The node's UUID.
	 */
	auto addNode(Node iNode) -> core::UUID;

	/**
	 * @brief
	 *  Remove a node along with every attached link.
	 * @param[in] iId UUID of the node to remove.
	 */
	void removeNode(core::UUID iId);

	/**
	 * @brief
	 *  Append an output pin to an existing node.
	 * @param[in] iNodeId UUID of the target node.
	 * @param[in] iPin The pin to append. A fresh UUID is generated when `iPin.id` is zero.
	 * @return The pin's UUID, or 0 when `iNodeId` is unknown.
	 */
	auto addOutputPin(core::UUID iNodeId, NodePin iPin) -> core::UUID;

	/**
	 * @brief
	 *  Append an input pin to an existing node.
	 * @param[in] iNodeId UUID of the target node.
	 * @param[in] iPin The pin to append. A fresh UUID is generated when `iPin.id` is zero.
	 * @return The pin's UUID, or 0 when `iNodeId` is unknown.
	 */
	auto addInputPin(core::UUID iNodeId, NodePin iPin) -> core::UUID;

	/**
	 * @brief
	 *  Remove an output pin from a node and drop every link connected to it.
	 * @param[in] iNodeId UUID of the owning node.
	 * @param[in] iPinId UUID of the pin to remove.
	 */
	void removeOutputPin(core::UUID iNodeId, core::UUID iPinId);

	/**
	 * @brief
	 *  Remove an input pin from a node and drop every link connected to it.
	 * @param[in] iNodeId UUID of the owning node.
	 * @param[in] iPinId UUID of the pin to remove.
	 */
	void removeInputPin(core::UUID iNodeId, core::UUID iPinId);

	/**
	 * @brief
	 *  Add a link between two pins.
	 * @param[in] iFromPin UUID of the source (output) pin.
	 * @param[in] iToPin UUID of the destination (input) pin.
	 * @return The new link's UUID, or 0 if the validator vetoes the connection.
	 */
	auto addLink(core::UUID iFromPin, core::UUID iToPin) -> core::UUID;

	/**
	 * @brief
	 *  Remove a link by id.
	 * @param[in] iId UUID of the link to remove.
	 */
	void removeLink(core::UUID iId);

	/**
	 * @brief
	 *  Clear every node and link.
	 */
	void clear();

	/**
	 * @brief
	 *  Access the full node list (stable references within one frame).
	 * @return The vector of nodes.
	 */
	[[nodiscard]] auto nodes() const noexcept -> const std::vector<Node>& { return m_nodes; }

	/**
	 * @brief
	 *  Access the full link list.
	 * @return The vector of links.
	 */
	[[nodiscard]] auto links() const noexcept -> const std::vector<Link>& { return m_links; }

	/**
	 * @brief
	 *  Compute a node's bounding box from its title + pin labels using the live ImGui font.
	 *
	 * Must be called from inside an ImGui frame (uses `ImGui::CalcTextSize`).
	 * @param[in] iNode The node to measure (does not need to be on a canvas).
	 * @return Width and height in pixels matching the node's render size.
	 */
	[[nodiscard]] static auto measureNode(const Node& iNode) -> math::vec2f;

	/**
	 * @brief
	 *  Lookup a node by id.
	 * @param[in] iId UUID of the node to find.
	 * @return Pointer to the node, or nullptr when not found.
	 */
	[[nodiscard]] auto findNode(core::UUID iId) -> Node*;

	/**
	 * @brief
	 *  Lookup a node by id (const).
	 * @param[in] iId UUID of the node to find.
	 * @return Pointer to the node, or nullptr when not found.
	 */
	[[nodiscard]] auto findNode(core::UUID iId) const -> const Node*;

	/**
	 * @brief
	 *  Lookup a link by id.
	 * @param[in] iId UUID of the link to find.
	 * @return Pointer to the link, or nullptr when not found.
	 */
	[[nodiscard]] auto findLink(core::UUID iId) const -> const Link*;

	/**
	 * @brief
	 *  Find the node that owns `iPinId` (either input or output).
	 * @param[in] iPinId UUID of the pin to look up.
	 * @return Pointer to the owning node, or nullptr when unknown.
	 */
	[[nodiscard]] auto findNodeByPin(core::UUID iPinId) const -> const Node*;

	// --- Selection -----------------------------------------------------------
	/**
	 * @brief
	 *  Replace the current selection.
	 * @param[in] iNodeIds The UUIDs to select. Identifiers not in the canvas are ignored.
	 */
	void setSelection(std::span<const core::UUID> iNodeIds);

	/**
	 * @brief
	 *  Current selection snapshot.
	 * @return The UUIDs of the selected nodes.
	 */
	[[nodiscard]] auto selection() const -> std::vector<core::UUID>;

	// --- Callbacks -----------------------------------------------------------
	/**
	 * @brief
	 *  Set the validator that may veto a link creation between two pins.
	 * @param[in] iValidator Predicate `(iFromPin, iToPin) -> bool`. Default: always allow.
	 */
	void setLinkValidator(std::function<bool(core::UUID iFromPin, core::UUID iToPin)> iValidator);

	/**
	 * @brief
	 *  Set the callback fired after a user-drawn link has been accepted.
	 * @param[in] iCb Callback `(iLinkId, iFromPin, iToPin) -> void`.
	 */
	void setOnLinkCreated(std::function<void(core::UUID iLinkId, core::UUID iFromPin, core::UUID iToPin)> iCb);

	/**
	 * @brief
	 *  Set the callback fired when the user deletes a link (via keyboard or context menu).
	 * @param[in] iCb Callback `(iLinkId) -> void`.
	 */
	void setOnLinkDeleted(std::function<void(core::UUID iLinkId)> iCb);

	/**
	 * @brief
	 *  Set the callback fired when the user drags a node to a new position.
	 * @param[in] iCb Callback `(iNodeId, iNewPosition) -> void`.
	 */
	void setOnNodeMoved(std::function<void(core::UUID iNodeId, math::vec2f iNewPosition)> iCb);

	/**
	 * @brief
	 *  Set the callback fired when the user single-clicks a node.
	 * @param[in] iCb Callback `(iNodeId) -> void`.
	 */
	void setOnNodeSelected(std::function<void(core::UUID iNodeId)> iCb);

	/**
	 * @brief
	 *  Set the callback fired when the user double-clicks a node.
	 *
	 * Detected in the wrapper, not native in `GraphEditor`.
	 * @param[in] iCb Callback `(iNodeId) -> void`.
	 */
	void setOnNodeDoubleClicked(std::function<void(core::UUID iNodeId)> iCb);

	/**
	 * @brief
	 *  Set the callback fired when the user right-clicks on the canvas.
	 * @param[in] iCb Callback `(iNodeUnderCursor) -> void`. The optional is set when a
	 * specific node was hit.
	 */
	void setOnContextMenu(std::function<void(std::optional<core::UUID> iNodeUnderCursor)> iCb);

	// --- Rendering -----------------------------------------------------------
	/**
	 * @brief
	 *  Draw the canvas inside the current ImGui window.
	 */
	void onRender();

	/**
	 * @brief
	 *  Enable or disable user edits (still rendered, but ignores input).
	 * @param[in] iEnabled True to allow edits, false to render the canvas as read-only.
	 */
	void setEnabled(bool iEnabled);

	/**
	 * @brief
	 *  Request that the next `onRender()` call zoom + pan the view so every node fits.
	 *
	 * Single-shot — the canvas resets the request automatically once handled, matching the
	 * `GraphEditor::Fit_AllNodes` parameter convention.
	 */
	void requestFitToContent();

private:
	struct Impl;
	uniq<Impl> mp_impl;

	std::vector<Node> m_nodes;
	std::vector<Link> m_links;
};

}// namespace owl::gui::widgets
