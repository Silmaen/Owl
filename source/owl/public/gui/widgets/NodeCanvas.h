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

/// @brief Direction of a node pin.
enum struct PinKind : uint8_t {
	Input,///< Connector on the left side of a node.
	Output,///< Connector on the right side of a node.
};

/**
 * @brief A single connector on a node.
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
	math::vec4 labelColor{1.0f, 1.0f, 1.0f, 1.0f};///< RGBA color used to render `label` (default white).
};

/**
 * @brief A node inside a `NodeCanvas`.
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
	math::vec4 titleColor{1.0f, 1.0f, 1.0f, 1.0f};///< Overlay color for the title text (e.g. red for orphans).
};

/// @brief An edge between two pins.
struct Link {
	core::UUID id;///< Stable link identifier.
	core::UUID fromPin;///< Output pin on the source node.
	core::UUID toPin;///< Input pin on the destination node.
};

/**
 * @brief A generic node-graph canvas widget.
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
	NodeCanvas();
	~NodeCanvas();

	// --- Topology ------------------------------------------------------------

	/// @brief Add a node. Returns its UUID (generated if zero).
	auto addNode(Node iNode) -> core::UUID;
	/// @brief Remove a node along with every attached link.
	void removeNode(core::UUID iId);
	/// @brief Add a link between two pins. Returns 0 if the validator vetoes it.
	auto addLink(core::UUID iFromPin, core::UUID iToPin) -> core::UUID;
	/// @brief Remove a link by id.
	void removeLink(core::UUID iId);
	/// @brief Clear every node and link.
	void clear();

	/// @brief Access to the full node list (stable references within one frame).
	[[nodiscard]] auto nodes() const -> const std::vector<Node>& { return m_nodes; }
	/// @brief Access to the full link list.
	[[nodiscard]] auto links() const -> const std::vector<Link>& { return m_links; }

	/// @brief Compute a node's bounding box from its title + pin labels using the live ImGui font.
	/// Must be called from inside an ImGui frame (uses `ImGui::CalcTextSize`).
	/// @param[in] iNode The node to measure (does not need to be on a canvas).
	/// @return Width and height in pixels matching the node's render size.
	[[nodiscard]] static auto measureNode(const Node& iNode) -> math::vec2f;

	/// @brief Lookup a node by id.
	[[nodiscard]] auto findNode(core::UUID iId) -> Node*;
	/// @brief Lookup a node by id (const).
	[[nodiscard]] auto findNode(core::UUID iId) const -> const Node*;
	/// @brief Lookup a link by id.
	[[nodiscard]] auto findLink(core::UUID iId) const -> const Link*;
	/// @brief Find the node that owns `iPinId` (either input or output).
	[[nodiscard]] auto findNodeByPin(core::UUID iPinId) const -> const Node*;

	// --- Selection -----------------------------------------------------------

	/// @brief Replace the current selection. UUIDs not in the canvas are ignored.
	void setSelection(std::span<const core::UUID> iNodeIds);
	/// @brief Current selection snapshot.
	[[nodiscard]] auto selection() const -> std::vector<core::UUID>;

	// --- Callbacks -----------------------------------------------------------

	/// @brief Veto a link creation (`iFromPin`/`iToPin`). Default: always allow.
	void setLinkValidator(std::function<bool(core::UUID iFromPin, core::UUID iToPin)> iValidator);
	/// @brief Fires after a user-drawn link has been accepted.
	void setOnLinkCreated(std::function<void(core::UUID iLinkId, core::UUID iFromPin, core::UUID iToPin)> iCb);
	/// @brief Fires when the user deletes a link (via keyboard or context menu).
	void setOnLinkDeleted(std::function<void(core::UUID iLinkId)> iCb);
	/// @brief Fires when the user drags a node to a new position.
	void setOnNodeMoved(std::function<void(core::UUID iNodeId, math::vec2f iNewPosition)> iCb);
	/// @brief Fires when the user single-clicks a node.
	void setOnNodeSelected(std::function<void(core::UUID iNodeId)> iCb);
	/// @brief Fires when the user double-clicks a node (detected in the wrapper, not native in GraphEditor).
	void setOnNodeDoubleClicked(std::function<void(core::UUID iNodeId)> iCb);
	/// @brief Fires when the user right-clicks on the canvas. The optional is set when a specific node was hit.
	void setOnContextMenu(std::function<void(std::optional<core::UUID> iNodeUnderCursor)> iCb);

	// --- Rendering -----------------------------------------------------------

	/// @brief Draw the canvas inside the current ImGui window.
	void onRender();

	/// @brief Enable/disable user edits (still rendered, but ignores input).
	void setEnabled(bool iEnabled);

private:
	struct Impl;
	uniq<Impl> mp_impl;

	std::vector<Node> m_nodes;
	std::vector<Link> m_links;
};

}// namespace owl::gui::widgets
