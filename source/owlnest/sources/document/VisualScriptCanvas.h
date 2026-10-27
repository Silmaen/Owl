/**
 * @file VisualScriptCanvas.h
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/UUID.h>
#include <math/vectors.h>
#include <script/visual/VisualScript.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  Drag and drop payload of a node type dragged from the node library onto the canvas.
 */
inline constexpr auto g_VisualScriptNodePayload = "OWL_VS_NODE";

/**
 * @brief
 *  A pin of a graph node.
 */
struct VisualScriptPinRef {
	/// Owning node.
	core::UUID node{0};
	/// Pin name.
	std::string pin;
	/// Whether it is an output.
	bool output{false};
	/// Carried type.
	script::visual::PinType type{script::visual::PinType::Any};
};

/**
 * @brief
 *  A change of the graph asked by the canvas, applied by the document as one undoable edit.
 */
struct VisualScriptEdit {
	/// The graph after the change.
	script::visual::VisualScript script;
	/// Undo entry text.
	std::string description;
	/// Whether it may merge with the previous edit of the same text.
	bool merge{false};
};

/**
 * @brief
 *  Place of a node and of its pins, in canvas units, from its top-left corner.
 */
struct VisualScriptNodeLayout {
	/// Width and height.
	std::array<float, 2> size{0.f, 0.f};
	/// Height of the title bar.
	float headerHeight{0.f};
	/// Centre of each input pin, in the order of `getNodePins`.
	std::vector<std::array<float, 2>> inputs;
	/// Centre of each output pin, in the order of `getNodePins`.
	std::vector<std::array<float, 2>> outputs;
	/// Left edge and width of the inline value field of each input, width 0 when it has none.
	std::vector<std::array<float, 2>> fields;
};

/**
 * @brief
 *  Width of a text at the canvas base size.
 */
using TextMeasure = std::function<float(std::string_view)>;

/**
 * @brief
 *  Glyph of a node type, shown in its title bar and in the node library.
 * @param[in] iType The node type.
 * @return A Lucide glyph.
 */
[[nodiscard]] auto getNodeGlyph(const script::visual::NodeType& iType) -> const char*;

/**
 * @brief
 *  Colour of the title bar of a node.
 * @param[in] iScript The graph holding the node.
 * @param[in] iNode The node.
 * @param[in] iType Its type.
 * @return The colour.
 */
[[nodiscard]] auto getNodeColor(const script::visual::VisualScript& iScript, const script::visual::GraphNode& iNode,
								const script::visual::NodeType& iType) -> math::vec4;

/**
 * @brief
 *  Colour of a pin type, also used by its links.
 * @param[in] iType The pin type.
 * @return The colour.
 */
[[nodiscard]] auto getPinColor(script::visual::PinType iType) -> math::vec4;

/**
 * @brief
 *  Glyph of a pin type, beside a variable type or a pin name.
 * @param[in] iType The pin type.
 * @return A Lucide glyph.
 */
[[nodiscard]] auto getPinGlyph(script::visual::PinType iType) -> const char*;

/**
 * @brief
 *  Two-tone icon of a pin type, from the icon bank: section headers and combo entries of the Graph panel.
 * @param[in] iType The pin type.
 * @return The icon name (`vs_type_float`).
 */
[[nodiscard]] auto getPinTypeIcon(script::visual::PinType iType) -> const char*;

/**
 * @brief
 *  Two-tone icon of the family of a node type, from the icon bank.
 * @param[in] iType The node type.
 * @return The icon name (`vs_node_call`).
 */
[[nodiscard]] auto getNodeIcon(const script::visual::NodeType& iType) -> const char*;

/**
 * @brief
 *  Name of a pin type, for the tooltips.
 * @param[in] iType The pin type.
 * @return The name (`number`, `color`).
 */
[[nodiscard]] auto getPinTypeName(script::visual::PinType iType) -> std::string_view;

/**
 * @brief
 *  Title of a node: `Get Position` for `transform.get_position`, `Set climbing` for a variable.
 * @param[in] iNode The node.
 * @param[in] iType Its type.
 * @return The title.
 */
[[nodiscard]] auto getNodeTitle(const script::visual::GraphNode& iNode, const script::visual::NodeType& iType)
		-> std::string;

/**
 * @brief
 *  Line under the title of a node: the binding table, `Event`, `Variable`.
 * @param[in] iType The node type.
 * @return The subtitle, empty when the title says enough.
 */
[[nodiscard]] auto getNodeSubtitle(const script::visual::NodeType& iType) -> std::string;

/**
 * @brief
 *  Place a node and its pins: one row per pin, inputs and outputs aligned row by row.
 * @param[in] iScript The graph holding the node.
 * @param[in] iNode The node.
 * @param[in] iMeasure Text width at the base size.
 * @return The layout.
 */
[[nodiscard]] auto layoutNode(const script::visual::VisualScript& iScript, const script::visual::GraphNode& iNode,
							  const TextMeasure& iMeasure) -> VisualScriptNodeLayout;

/**
 * @brief
 *  Graph view of a visual script: nodes with their title bar and pins, links, inline values of the unlinked inputs.
 *
 * The canvas reads the graph and never changes it: each gesture comes back as a `VisualScriptEdit` (link, move,
 * value) or a request (context menu, link dropped on the background, node dropped from the library) the document
 * acts on. It keeps the view (pan, zoom) and the selection.
 */
class VisualScriptCanvas final {
public:
	/**
	 * @brief
	 *  What one frame of the canvas asks the document to do.
	 */
	struct Result {
		/// Change of the graph.
		std::optional<VisualScriptEdit> edit;
		/// Whether a right click asks for the context menu.
		bool contextMenu{false};
		/// Node under the right click.
		std::optional<core::UUID> contextNode;
		/// Pin under the right click.
		std::optional<VisualScriptPinRef> contextPin;
		/// Pin whose link was dropped on the background: the document offers the nodes to link it to.
		std::optional<VisualScriptPinRef> linkDropped;
		/// Node type dropped from the library.
		std::optional<std::string> droppedType;
		/// Canvas position of the mouse.
		std::array<float, 2> mouse{0.f, 0.f};
		/// Whether the canvas used the Delete key (on a link).
		bool deleteUsed{false};
	};

	VisualScriptCanvas(const VisualScriptCanvas&) = delete;

	VisualScriptCanvas(VisualScriptCanvas&&) = delete;

	auto operator=(const VisualScriptCanvas&) -> VisualScriptCanvas& = delete;

	auto operator=(VisualScriptCanvas&&) -> VisualScriptCanvas& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	VisualScriptCanvas();

	/**
	 * @brief
	 *  Destructor.
	 */
	~VisualScriptCanvas();

	/**
	 * @brief
	 *  Draw the graph in the remaining space of the current window and handle the gestures.
	 * @param[in] iScript The graph.
	 * @param[in] iDiagnostics Compilation errors, drawn on their nodes.
	 * @return The requests of the frame.
	 */
	auto render(const script::visual::VisualScript& iScript,
				std::span<const script::visual::CompileDiagnostic> iDiagnostics) -> Result;

	/**
	 * @brief
	 *  Selected nodes.
	 * @return Their identifiers.
	 */
	[[nodiscard]] auto getSelection() const -> const std::vector<core::UUID>& { return m_selection; }

	/**
	 * @brief
	 *  Replace the selection.
	 * @param[in] iNodes The nodes to select.
	 */
	void setSelection(std::vector<core::UUID> iNodes) { m_selection = std::move(iNodes); }

	/**
	 * @brief
	 *  Frame every node at the next render.
	 */
	void requestFrameAll() { m_frameAll = true; }

	/**
	 * @brief
	 *  Centre the view on a node at the next render.
	 * @param[in] iNode The node.
	 */
	void requestFocus(const core::UUID iNode) { m_focus = iNode; }

	/**
	 * @brief
	 *  Canvas position at the centre of the view.
	 * @return The position.
	 */
	[[nodiscard]] auto getViewCenter() const -> std::array<float, 2>;

	/**
	 * @brief
	 *  Zoom factor.
	 * @return 1 at the base size.
	 */
	[[nodiscard]] auto getZoom() const -> float { return m_zoom; }

private:
	struct Frame;

	/// Gesture in progress.
	enum struct Gesture : uint8_t {
		None,///< Nothing.
		Pan,///< Moving the view.
		Select,///< Drawing a selection rectangle.
		Move,///< Moving the selected nodes.
		Link,///< Dragging a link from a pin.
	};

	/**
	 * @brief
	 *  Draw the background grid.
	 * @param[in] iFrame The frame.
	 */
	void drawGrid(const Frame& iFrame) const;

	/**
	 * @brief
	 *  Draw the links and find the one under the mouse.
	 * @param[in,out] ioFrame The frame.
	 */
	void drawLinks(Frame& ioFrame) const;

	/**
	 * @brief
	 *  Draw the nodes in view and their inline values, then the gesture overlays.
	 * @param[in,out] ioFrame The frame.
	 */
	void drawNodes(Frame& ioFrame) const;

	/**
	 * @brief
	 *  Draw the shadow, body, title bar and border of a node.
	 * @param[in] iFrame The frame.
	 * @param[in] iIndex Index of the node in the frame.
	 */
	void drawNodeFrame(const Frame& iFrame, size_t iIndex) const;

	/**
	 * @brief
	 *  Draw the pins, labels and inline value fields of a node.
	 * @param[in,out] ioFrame The frame.
	 * @param[in] iIndex Index of the node in the frame.
	 */
	void drawNodePins(Frame& ioFrame, size_t iIndex) const;

	/**
	 * @brief
	 *  Draw the dragged link, the selection rectangle and the tooltips.
	 * @param[in] iFrame The frame.
	 */
	void drawOverlays(const Frame& iFrame) const;

	/**
	 * @brief
	 *  Mouse gestures: selection, moves, links, pan and zoom.
	 * @param[in,out] ioFrame The frame.
	 */
	void handleMouse(Frame& ioFrame);

	/**
	 * @brief
	 *  Left button pressed on the canvas: start a link, a move or a selection rectangle, or break links (Alt).
	 * @param[in,out] ioFrame The frame.
	 * @param[in] iMouse Canvas position of the mouse.
	 */
	void pressLeft(Frame& ioFrame, const std::array<float, 2>& iMouse);

	/**
	 * @brief
	 *  Left button released: finish the gesture in progress.
	 * @param[in,out] ioFrame The frame.
	 * @param[in] iMouse Canvas position of the mouse.
	 */
	void releaseLeft(Frame& ioFrame, const std::array<float, 2>& iMouse);

	/**
	 * @brief
	 *  Finish a link dragged onto a pin, replacing the link it takes the place of.
	 * @param[in,out] ioFrame The frame.
	 * @param[in] iTarget The pin under the mouse.
	 */
	void dropLink(Frame& ioFrame, const VisualScriptPinRef& iTarget) const;

	/**
	 * @brief
	 *  Apply a pending framing request.
	 * @param[in] iFrame The frame.
	 */
	void applyViewRequests(const Frame& iFrame);

	/// Pan, in canvas units.
	std::array<float, 2> m_offset{40.f, 40.f};
	/// Zoom factor.
	float m_zoom{1.f};
	/// Size of the view, in pixels.
	std::array<float, 2> m_viewSize{0.f, 0.f};
	/// Selected nodes.
	std::vector<core::UUID> m_selection;
	/// Gesture in progress.
	Gesture m_gesture{Gesture::None};
	/// Canvas position where the gesture started.
	std::array<float, 2> m_gestureStart{0.f, 0.f};
	/// Offset of the moved nodes, in canvas units.
	std::array<float, 2> m_moveOffset{0.f, 0.f};
	/// Node pressed by the left button, selected alone when the press ends without a move.
	std::optional<core::UUID> m_pressedNode;
	/// Pin a dragged link starts from.
	std::optional<VisualScriptPinRef> m_linkStart;
	/// Frame every node at the next render.
	bool m_frameAll{true};
	/// Node to centre at the next render.
	std::optional<core::UUID> m_focus;
};

}// namespace owl::nest
