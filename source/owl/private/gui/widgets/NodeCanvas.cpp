/**
 * @file NodeCanvas.cpp
 * @author Silmaen
 * @date 23/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/widgets/NodeCanvas.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_CLANG("-Wunused-parameter")
#include <GraphEditor.h>
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <chrono>

namespace owl::gui::widgets {

namespace {

// Floor for an empty node — keeps the title bar legible even on very short titles.
constexpr auto g_minNodeSize = ImVec2{120.0f, 40.0f};
// Padding around the title text + every pin label, in pixels.
constexpr float g_horizontalPadding = 24.0f;
// Vertical pixels per pin row in GraphEditor's layout.
constexpr float g_pinRowHeight = 22.0f;
// Vertical pixels reserved for the title bar.
constexpr float g_titleBarHeight = 28.0f;
// Bottom padding under the last pin row.
constexpr float g_bottomPadding = 12.0f;
constexpr auto g_doubleClickIntervalMs = std::chrono::milliseconds{300};

auto toImVec2(const math::vec2f& iV) -> ImVec2 { return {iV.x(), iV.y()}; }

auto toImU32(const math::vec4& iColor) -> ImU32 {
	return IM_COL32(static_cast<uint8_t>(iColor.x() * 255.0f), static_cast<uint8_t>(iColor.y() * 255.0f),
					static_cast<uint8_t>(iColor.z() * 255.0f), static_cast<uint8_t>(iColor.w() * 255.0f));
}

/// @brief Compute a node's bounding box from its title and pin labels using the live ImGui font.
auto computeNodeSize(const Node& iNode) -> ImVec2 {
	float maxWidth = ImGui::CalcTextSize(iNode.title.c_str()).x;
	for (const auto& pin: iNode.inputs)
		maxWidth = std::max(maxWidth, ImGui::CalcTextSize(pin.label.c_str()).x);
	for (const auto& pin: iNode.outputs)
		maxWidth = std::max(maxWidth, ImGui::CalcTextSize(pin.label.c_str()).x);
	const auto rows = std::max(iNode.inputs.size(), iNode.outputs.size());
	const ImVec2 size{
			std::max(g_minNodeSize.x, maxWidth + 2.0f * g_horizontalPadding),
			std::max(g_minNodeSize.y,
					 g_titleBarHeight + static_cast<float>(rows) * g_pinRowHeight + g_bottomPadding)};
	return size;
}

}// namespace

struct NodeCanvas::Impl final : public GraphEditor::Delegate {
	explicit Impl(NodeCanvas& ioOwner) : m_owner{ioOwner} {}
	Impl(const Impl&) = delete;
	Impl(Impl&&) = delete;
	auto operator=(const Impl&) -> Impl& = delete;
	auto operator=(Impl&&) -> Impl& = delete;
	~Impl() override;

	// Runtime state kept in the wrapper (GraphEditor is stateless w.r.t. node IDs).
	std::vector<bool> m_selected;///< Selection flag per node index.
	std::vector<GraphEditor::Template> m_templates;///< One template per node — each node uses its own slot definitions.
	GraphEditor::Options m_options{};
	GraphEditor::ViewState m_viewState{};
	bool m_enabled = true;

	// Double-click tracking.
	core::UUID m_lastClickedNodeId{0};
	std::chrono::steady_clock::time_point m_lastClickTime;

	// Callbacks.
	std::function<bool(core::UUID, core::UUID)> m_linkValidator{[](auto, auto) -> bool { return true; }};
	std::function<void(core::UUID, core::UUID, core::UUID)> m_onLinkCreated;
	std::function<void(core::UUID)> m_onLinkDeleted;
	std::function<void(core::UUID, math::vec2f)> m_onNodeMoved;
	std::function<void(core::UUID)> m_onNodeSelected;
	std::function<void(core::UUID)> m_onNodeDoubleClicked;
	std::function<void(std::optional<core::UUID>)> m_onContextMenu;

	// Top-left screen position of the GraphEditor canvas, captured each frame just before
	// `Show()` runs. Used by RightClick to do a fallback hit-test on the node bodies — GraphEditor
	// only sets `nodeOver` when a slot circle is hovered.
	ImVec2 m_canvasOriginScreen{0.0f, 0.0f};

	[[nodiscard]] auto findNodeIndex(core::UUID iId) const -> std::optional<size_t> {
		for (size_t i = 0; i < m_owner.m_nodes.size(); ++i) {
			if (m_owner.m_nodes[i].id == iId)
				return i;
		}
		return std::nullopt;
	}

	[[nodiscard]] auto findLinkIndex(core::UUID iId) const -> std::optional<size_t> {
		for (size_t i = 0; i < m_owner.m_links.size(); ++i) {
			if (m_owner.m_links[i].id == iId)
				return i;
		}
		return std::nullopt;
	}

	void rebuildTemplates() {
		m_templates.clear();
		m_templates.reserve(m_owner.m_nodes.size());

		// We pass `nullptr` for input/output names so GraphEditor does NOT render labels outside
		// the node frame. Labels are rendered manually inside the rect via `CustomDraw` below,
		// using `pin.labelColor` for per-pin styling.
		for (const auto& node: m_owner.m_nodes) {
			GraphEditor::Template tpl{};
			tpl.mHeaderColor = toImU32(node.titleColor);
			tpl.mBackgroundColor = IM_COL32(60, 60, 60, 255);
			tpl.mBackgroundColorOver = IM_COL32(80, 80, 80, 255);
			tpl.mInputCount = static_cast<ImU8>(node.inputs.size());
			tpl.mInputNames = nullptr;
			tpl.mInputColors = nullptr;
			tpl.mOutputCount = static_cast<ImU8>(node.outputs.size());
			tpl.mOutputNames = nullptr;
			tpl.mOutputColors = nullptr;
			m_templates.push_back(tpl);
		}

		m_selected.assign(m_owner.m_nodes.size(), false);
	}

	// --- GraphEditor::Delegate implementation --------------------------------

	auto AllowedLink(GraphEditor::NodeIndex iFrom, GraphEditor::NodeIndex iTo) -> bool override {
		// GraphEditor's AllowedLink is a coarse node-to-node check (no pin granularity here).
		// Accept everything here; fine-grained validation (pin types) happens in AddLink using the
		// real pin UUIDs, which lets us early-abort and NOT create a Link if the validator refuses.
		if (iFrom >= m_owner.m_nodes.size() || iTo >= m_owner.m_nodes.size())
			return false;
		return iFrom != iTo;// forbid self-loops
	}

	void SelectNode(GraphEditor::NodeIndex iNodeIndex, bool iSelected) override {
		if (iNodeIndex >= m_selected.size())
			return;
		m_selected[iNodeIndex] = iSelected;
		if (!iSelected)
			return;

		const auto nodeId = m_owner.m_nodes[iNodeIndex].id;
		const auto now = std::chrono::steady_clock::now();
		const bool isDouble = (nodeId == m_lastClickedNodeId) && (now - m_lastClickTime) <= g_doubleClickIntervalMs;
		m_lastClickedNodeId = nodeId;
		m_lastClickTime = now;

		if (isDouble) {
			if (m_onNodeDoubleClicked)
				m_onNodeDoubleClicked(nodeId);
		} else if (m_onNodeSelected) {
			m_onNodeSelected(nodeId);
		}
	}

	void MoveSelectedNodes(const ImVec2 iDelta) override {
		for (size_t i = 0; i < m_selected.size() && i < m_owner.m_nodes.size(); ++i) {
			if (!m_selected[i])
				continue;
			m_owner.m_nodes[i].position += math::vec2f{iDelta.x, iDelta.y};
			if (m_onNodeMoved)
				m_onNodeMoved(m_owner.m_nodes[i].id, m_owner.m_nodes[i].position);
		}
	}

	void AddLink(GraphEditor::NodeIndex iInputNode, GraphEditor::SlotIndex iInputSlot,
				 GraphEditor::NodeIndex iOutputNode, GraphEditor::SlotIndex iOutputSlot) override {
		if (iInputNode >= m_owner.m_nodes.size() || iOutputNode >= m_owner.m_nodes.size())
			return;
		const auto& srcNode = m_owner.m_nodes[iInputNode];
		const auto& dstNode = m_owner.m_nodes[iOutputNode];
		if (iInputSlot >= srcNode.outputs.size() || iOutputSlot >= dstNode.inputs.size()) {
			// GraphEditor's slot convention: "Input" node is the source (drag-from) and "Output"
			// node is the destination (drop-on). Map accordingly — the source pin is an output,
			// the destination pin is an input.
			return;
		}
		const auto fromPin = srcNode.outputs[iInputSlot].id;
		const auto toPin = dstNode.inputs[iOutputSlot].id;

		if (m_linkValidator && !m_linkValidator(fromPin, toPin))
			return;

		const Link link{.id = core::UUID{}, .fromPin = fromPin, .toPin = toPin};
		m_owner.m_links.push_back(link);
		if (m_onLinkCreated)
			m_onLinkCreated(link.id, fromPin, toPin);
	}

	void DelLink(GraphEditor::LinkIndex iLinkIndex) override {
		if (iLinkIndex >= m_owner.m_links.size())
			return;
		const auto linkId = m_owner.m_links[iLinkIndex].id;
		m_owner.m_links.erase(m_owner.m_links.begin() + static_cast<ptrdiff_t>(iLinkIndex));
		if (m_onLinkDeleted)
			m_onLinkDeleted(linkId);
	}

	void CustomDraw(ImDrawList* iDrawList, ImRect iRectangle, GraphEditor::NodeIndex iNodeIndex) override {
		// `iRectangle` is in screen space (post pan/zoom). Render pin labels just inside the rect
		// so they stay visible even at modest zoom levels — and pick `pin.labelColor` so consumers
		// can color-code (Death/Victory/Lua) without the canvas knowing those domain concepts.
		if (iNodeIndex >= m_owner.m_nodes.size())
			return;
		const auto& node = m_owner.m_nodes[iNodeIndex];
		const float titleEnd = iRectangle.Min.y + g_titleBarHeight * m_viewState.mFactor;
		const float bodyTop = titleEnd;
		const float bodyHeight = std::max(1.0f, iRectangle.Max.y - bodyTop - g_bottomPadding * m_viewState.mFactor);
		const auto rowCount = std::max<size_t>(1, std::max(node.inputs.size(), node.outputs.size()));
		const float rowHeight = bodyHeight / static_cast<float>(rowCount);
		const float xPad = 14.0f * m_viewState.mFactor;

		for (size_t i = 0; i < node.inputs.size(); ++i) {
			const auto& pin = node.inputs[i];
			if (pin.label.empty())
				continue;
			const auto textSize = ImGui::CalcTextSize(pin.label.c_str());
			const ImVec2 pos{iRectangle.Min.x + xPad,
							 bodyTop + (static_cast<float>(i) + 0.5f) * rowHeight - textSize.y * 0.5f};
			iDrawList->AddText(pos, toImU32(pin.labelColor), pin.label.c_str());
		}
		for (size_t i = 0; i < node.outputs.size(); ++i) {
			const auto& pin = node.outputs[i];
			if (pin.label.empty())
				continue;
			const auto textSize = ImGui::CalcTextSize(pin.label.c_str());
			const ImVec2 pos{iRectangle.Max.x - xPad - textSize.x,
							 bodyTop + (static_cast<float>(i) + 0.5f) * rowHeight - textSize.y * 0.5f};
			iDrawList->AddText(pos, toImU32(pin.labelColor), pin.label.c_str());
		}
	}

	/// @brief Translate the current ImGui mouse position into canvas-space coordinates so we can
	///        hit-test against our stored node rects (the canvas pans + zooms, the node positions
	///        are in canvas space).
	[[nodiscard]] auto mouseInCanvasSpace() const -> ImVec2 {
		const auto m = ImGui::GetMousePos();
		return ImVec2{(m.x - m_canvasOriginScreen.x - m_viewState.mPosition.x) / m_viewState.mFactor,
					  (m.y - m_canvasOriginScreen.y - m_viewState.mPosition.y) / m_viewState.mFactor};
	}

	void RightClick(GraphEditor::NodeIndex iNodeIndex, [[maybe_unused]] GraphEditor::SlotIndex iSlotInput,
					[[maybe_unused]] GraphEditor::SlotIndex iSlotOutput) override {
		if (!m_onContextMenu)
			return;
		// GraphEditor only sets `iNodeIndex` when the user clicks ON a slot; clicks on the node
		// body return -1. Fall back to a manual hit-test against our stored canvas-space rects.
		auto resolved = iNodeIndex;
		if (resolved >= m_owner.m_nodes.size()) {
			const auto canvasMouse = mouseInCanvasSpace();
			for (size_t i = 0; i < m_owner.m_nodes.size(); ++i) {
				const auto size = computeNodeSize(m_owner.m_nodes[i]);
				const auto& pos = m_owner.m_nodes[i].position;
				if (canvasMouse.x >= pos.x() && canvasMouse.x <= pos.x() + size.x && canvasMouse.y >= pos.y() &&
					canvasMouse.y <= pos.y() + size.y) {
					resolved = i;
					break;
				}
			}
		}
		if (resolved < m_owner.m_nodes.size())
			m_onContextMenu(m_owner.m_nodes[resolved].id);
		else
			m_onContextMenu(std::nullopt);
	}

	// GraphEditor::Delegate declares these with `const` on the return type. Both Clang and GCC
	// warn (`-Wignored-qualifiers`, promoted by `-Werror`) about the useless qualifier; we cannot
	// change the base signature so we locally silence the warning for the whole override block.
	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wignored-qualifiers")
	OWL_DIAG_DISABLE_GCC("-Wignored-qualifiers")
	auto GetTemplateCount() -> const size_t override { return m_templates.size(); }
	auto GetTemplate(GraphEditor::TemplateIndex iIndex) -> const GraphEditor::Template override {
		return m_templates[iIndex];
	}

	auto GetNodeCount() -> const size_t override { return m_owner.m_nodes.size(); }
	auto GetNode(GraphEditor::NodeIndex iIndex) -> const GraphEditor::Node override {
		const auto& node = m_owner.m_nodes[iIndex];
		const auto topLeft = toImVec2(node.position);
		const auto size = computeNodeSize(node);
		GraphEditor::Node out{};
		out.mName = node.title.c_str();
		out.mTemplateIndex = iIndex;
		out.mRect = ImRect{topLeft, {topLeft.x + size.x, topLeft.y + size.y}};
		out.mSelected = iIndex < m_selected.size() && m_selected[iIndex];
		return out;
	}

	auto GetLinkCount() -> const size_t override { return m_owner.m_links.size(); }
	auto GetLink(GraphEditor::LinkIndex iIndex) -> const GraphEditor::Link override {
		const auto& link = m_owner.m_links[iIndex];
		GraphEditor::Link out{};
		// Resolve both endpoints; if the link references removed pins, return zeroes — GraphEditor
		// will draw nothing degenerate for a same-node self-loop.
		for (size_t n = 0; n < m_owner.m_nodes.size(); ++n) {
			const auto& node = m_owner.m_nodes[n];
			for (size_t p = 0; p < node.outputs.size(); ++p) {
				if (node.outputs[p].id == link.fromPin) {
					out.mInputNodeIndex = n;
					out.mInputSlotIndex = p;
				}
			}
			for (size_t p = 0; p < node.inputs.size(); ++p) {
				if (node.inputs[p].id == link.toPin) {
					out.mOutputNodeIndex = n;
					out.mOutputSlotIndex = p;
				}
			}
		}
		return out;
	}
	OWL_DIAG_POP

	NodeCanvas& m_owner;
};

NodeCanvas::Impl::~Impl() = default;

NodeCanvas::NodeCanvas() : mp_impl{mkUniq<Impl>(*this)} {
	// Always render pin labels — `GraphEditor` defaults to hover-only which makes Scene Flow
	// graphs unreadable at a glance.
	mp_impl->m_options.mDrawIONameOnHover = false;
}
NodeCanvas::~NodeCanvas() = default;

auto NodeCanvas::measureNode(const Node& iNode) -> math::vec2f {
	const auto size = computeNodeSize(iNode);
	return {size.x, size.y};
}

auto NodeCanvas::addNode(Node iNode) -> core::UUID {
	if (static_cast<uint64_t>(iNode.id) == 0)
		iNode.id = core::UUID{};
	m_nodes.push_back(std::move(iNode));
	mp_impl->rebuildTemplates();
	return m_nodes.back().id;
}

void NodeCanvas::removeNode(core::UUID iId) {
	const auto idx = mp_impl->findNodeIndex(iId);
	if (!idx)
		return;
	// Drop every link touching one of this node's pins.
	const auto& node = m_nodes[*idx];
	std::vector<core::UUID> pinIds;
	pinIds.reserve(node.inputs.size() + node.outputs.size());
	for (const auto& pin: node.inputs)
		pinIds.push_back(pin.id);
	for (const auto& pin: node.outputs)
		pinIds.push_back(pin.id);
	std::erase_if(m_links, [&](const Link& iLink) -> bool {
		return std::ranges::find(pinIds, iLink.fromPin) != pinIds.end() ||
			   std::ranges::find(pinIds, iLink.toPin) != pinIds.end();
	});
	m_nodes.erase(m_nodes.begin() + static_cast<ptrdiff_t>(*idx));
	mp_impl->rebuildTemplates();
}

auto NodeCanvas::addLink(core::UUID iFromPin, core::UUID iToPin) -> core::UUID {
	if (mp_impl->m_linkValidator && !mp_impl->m_linkValidator(iFromPin, iToPin))
		return core::UUID{0};
	const Link link{.id = core::UUID{}, .fromPin = iFromPin, .toPin = iToPin};
	m_links.push_back(link);
	return link.id;
}

void NodeCanvas::removeLink(core::UUID iId) {
	std::erase_if(m_links, [iId](const Link& iLink) -> bool { return iLink.id == iId; });
}

void NodeCanvas::clear() {
	m_nodes.clear();
	m_links.clear();
	mp_impl->rebuildTemplates();
}

auto NodeCanvas::findNode(core::UUID iId) -> Node* {
	for (auto& node: m_nodes)
		if (node.id == iId)
			return &node;
	return nullptr;
}

auto NodeCanvas::findNode(core::UUID iId) const -> const Node* {
	for (const auto& node: m_nodes)
		if (node.id == iId)
			return &node;
	return nullptr;
}

auto NodeCanvas::findLink(core::UUID iId) const -> const Link* {
	for (const auto& link: m_links)
		if (link.id == iId)
			return &link;
	return nullptr;
}

auto NodeCanvas::findNodeByPin(core::UUID iPinId) const -> const Node* {
	for (const auto& node: m_nodes) {
		for (const auto& pin: node.inputs)
			if (pin.id == iPinId)
				return &node;
		for (const auto& pin: node.outputs)
			if (pin.id == iPinId)
				return &node;
	}
	return nullptr;
}

void NodeCanvas::setSelection(std::span<const core::UUID> iNodeIds) {
	mp_impl->m_selected.assign(m_nodes.size(), false);
	for (const auto id: iNodeIds) {
		if (const auto idx = mp_impl->findNodeIndex(id); idx)
			mp_impl->m_selected[*idx] = true;
	}
}

auto NodeCanvas::selection() const -> std::vector<core::UUID> {
	std::vector<core::UUID> out;
	for (size_t i = 0; i < mp_impl->m_selected.size() && i < m_nodes.size(); ++i)
		if (mp_impl->m_selected[i])
			out.push_back(m_nodes[i].id);
	return out;
}

void NodeCanvas::setLinkValidator(std::function<bool(core::UUID, core::UUID)> iValidator) {
	mp_impl->m_linkValidator = std::move(iValidator);
}
void NodeCanvas::setOnLinkCreated(std::function<void(core::UUID, core::UUID, core::UUID)> iCb) {
	mp_impl->m_onLinkCreated = std::move(iCb);
}
void NodeCanvas::setOnLinkDeleted(std::function<void(core::UUID)> iCb) { mp_impl->m_onLinkDeleted = std::move(iCb); }
void NodeCanvas::setOnNodeMoved(std::function<void(core::UUID, math::vec2f)> iCb) {
	mp_impl->m_onNodeMoved = std::move(iCb);
}
void NodeCanvas::setOnNodeSelected(std::function<void(core::UUID)> iCb) { mp_impl->m_onNodeSelected = std::move(iCb); }
void NodeCanvas::setOnNodeDoubleClicked(std::function<void(core::UUID)> iCb) {
	mp_impl->m_onNodeDoubleClicked = std::move(iCb);
}
void NodeCanvas::setOnContextMenu(std::function<void(std::optional<core::UUID>)> iCb) {
	mp_impl->m_onContextMenu = std::move(iCb);
}

void NodeCanvas::setEnabled(bool iEnabled) { mp_impl->m_enabled = iEnabled; }

void NodeCanvas::onRender() {
	// Ensure per-frame consistency between `m_nodes` and delegate bookkeeping — the caller may have
	// added/removed nodes between frames.
	if (mp_impl->m_templates.size() != m_nodes.size())
		mp_impl->rebuildTemplates();

	// Capture the canvas origin BEFORE GraphEditor draws — used by the body-rect hit-test in
	// `RightClick()` since GraphEditor only knows about slot circles.
	mp_impl->m_canvasOriginScreen = ImGui::GetCursorScreenPos();
	GraphEditor::Show(*mp_impl, mp_impl->m_options, mp_impl->m_viewState, mp_impl->m_enabled, nullptr);
}

}// namespace owl::gui::widgets
