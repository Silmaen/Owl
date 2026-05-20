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
constexpr auto g_minNodeSize = ImVec2{160.0f, 40.0f};
constexpr float g_horizontalPadding = 44.0f;
constexpr float g_graphEditorSlotYOffset = 8.0f;
// Vertical pixels per pin row in GraphEditor's layout.
constexpr float g_pinRowHeight = 22.0f;
// Vertical pixels reserved for the title bar.
constexpr float g_titleBarHeight = 28.0f;
// Bottom padding under the last pin row.
constexpr float g_bottomPadding = 12.0f;
constexpr auto g_doubleClickIntervalMs = std::chrono::milliseconds{300};
constexpr float g_pinLabelLodZoom = 0.6f;
constexpr float g_titleLodZoom = 0.3f;

auto toImVec2(const math::vec2f& iV) -> ImVec2 { return {iV.x(), iV.y()}; }

auto toImU32(const math::vec4& iColor) -> ImU32 {
	return IM_COL32(static_cast<uint8_t>(iColor.x() * 255.0f), static_cast<uint8_t>(iColor.y() * 255.0f),
					static_cast<uint8_t>(iColor.z() * 255.0f), static_cast<uint8_t>(iColor.w() * 255.0f));
}

auto computeNodeSize(const Node& iNode) -> ImVec2 {
	float maxWidth = ImGui::CalcTextSize(iNode.title.c_str()).x;
	for (const auto& pin: iNode.inputs) maxWidth = std::max(maxWidth, ImGui::CalcTextSize(pin.label.c_str()).x);
	for (const auto& pin: iNode.outputs) maxWidth = std::max(maxWidth, ImGui::CalcTextSize(pin.label.c_str()).x);
	const auto rows = std::max(iNode.inputs.size(), iNode.outputs.size());
	const ImVec2 size{
			std::max(g_minNodeSize.x, maxWidth + 2.0f * g_horizontalPadding),
			std::max(g_minNodeSize.y, g_titleBarHeight + static_cast<float>(rows) * g_pinRowHeight + g_bottomPadding)};
	return size;
}

}// namespace

auto shouldDrawPinLabels(const float iZoomFactor) -> bool { return iZoomFactor > g_pinLabelLodZoom; }

auto shouldDrawNodeTitles(const float iZoomFactor) -> bool { return iZoomFactor > g_titleLodZoom; }

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
	struct TemplateBuf {
		std::vector<ImU32> inputColors;
		std::vector<ImU32> outputColors;
	};
	std::vector<TemplateBuf> m_templateBufs;
	GraphEditor::Options m_options{};
	GraphEditor::ViewState m_viewState{};
	bool m_enabled = true;
	// Single-shot fit-to-content request; consumed by the next `onRender`.
	GraphEditor::FitOnScreen m_pendingFit = GraphEditor::Fit_None;
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
	ImVec2 m_canvasOriginScreen{0.0f, 0.0f};
	std::unordered_map<core::UUID, ImRect> m_nodeScreenRects;
	ImDrawList* mp_canvasDrawList = nullptr;

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
		m_templateBufs.clear();
		m_templateBufs.reserve(m_owner.m_nodes.size());

		for (const auto& node: m_owner.m_nodes) {
			auto& buf = m_templateBufs.emplace_back();
			buf.inputColors.reserve(node.inputs.size());
			buf.outputColors.reserve(node.outputs.size());
			for (const auto& pin: node.inputs) buf.inputColors.push_back(toImU32(pin.labelColor));
			for (const auto& pin: node.outputs) buf.outputColors.push_back(toImU32(pin.labelColor));

			GraphEditor::Template tpl{};
			tpl.mHeaderColor = IM_COL32(45, 60, 90, 255);
			tpl.mBackgroundColor = IM_COL32(60, 60, 60, 255);
			tpl.mBackgroundColorOver = IM_COL32(80, 80, 80, 255);
			tpl.mInputCount = static_cast<ImU8>(node.inputs.size());
			tpl.mInputNames = nullptr;
			tpl.mInputColors = buf.inputColors.empty() ? nullptr : buf.inputColors.data();
			tpl.mOutputCount = static_cast<ImU8>(node.outputs.size());
			tpl.mOutputNames = nullptr;
			tpl.mOutputColors = buf.outputColors.empty() ? nullptr : buf.outputColors.data();
			m_templates.push_back(tpl);
		}

		m_selected.assign(m_owner.m_nodes.size(), false);
	}

	// --- GraphEditor::Delegate implementation --------------------------------
	auto AllowedLink(GraphEditor::NodeIndex iFrom, GraphEditor::NodeIndex iTo) -> bool override {
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
		if (iNodeIndex >= m_owner.m_nodes.size())
			return;
		m_nodeScreenRects[m_owner.m_nodes[iNodeIndex].id] = iRectangle;
		mp_canvasDrawList = iDrawList;
		const auto& node = m_owner.m_nodes[iNodeIndex];

		// Font setup is shared between title (always rendered) and pin labels (LOD-skipped).
		ImFont* const font = ImGui::GetFont();
		const float fontSize = ImGui::GetFontSize() * m_viewState.mFactor;

		if (!node.title.empty()) {
			constexpr float kMinTitleFontSize = 8.0f;
			constexpr float kTopGap = 3.0f;
			const float zoom = m_viewState.mFactor;
			const float titleFontSize = std::max(kMinTitleFontSize, ImGui::GetFontSize() * zoom);
			const auto titleSize = font->CalcTextSizeA(titleFontSize, FLT_MAX, 0.0f, node.title.c_str());
			const float nodeTopScreen = iRectangle.Max.y - computeNodeSize(node).y * zoom;
			const float idealTopY = nodeTopScreen + kTopGap;
			const float maxTopY = iRectangle.Min.y - titleSize.y;
			const float titlePosY = std::min(idealTopY, maxTopY);
			const float titlePosX = (iRectangle.Min.x + iRectangle.Max.x) * 0.5f - titleSize.x * 0.5f;
			iDrawList->AddText(font, titleFontSize, ImVec2{titlePosX, titlePosY}, toImU32(node.titleColor),
							   node.title.c_str());
		}

		if (!shouldDrawPinLabels(m_viewState.mFactor))
			return;
		const float nodeHeightScreen = computeNodeSize(node).y * m_viewState.mFactor;
		const float nodeTopScreen = iRectangle.Max.y - nodeHeightScreen;
		const auto inCount = static_cast<float>(node.inputs.size());
		const auto outCount = static_cast<float>(node.outputs.size());
		const float xPad = 14.0f * m_viewState.mFactor;
		const auto measure = [&](const char* iText) -> ImVec2 {
			return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, iText);
		};

		for (size_t i = 0; i < node.inputs.size(); ++i) {
			const auto& pin = node.inputs[i];
			if (pin.label.empty())
				continue;
			const auto textSize = measure(pin.label.c_str());
			const float y = nodeTopScreen + nodeHeightScreen * (static_cast<float>(i) + 1.0f) / (inCount + 1.0f) +
							g_graphEditorSlotYOffset;
			const ImVec2 pos{iRectangle.Min.x + xPad, y - textSize.y * 0.5f};
			iDrawList->AddText(font, fontSize, pos, toImU32(pin.labelColor), pin.label.c_str());
		}
		for (size_t i = 0; i < node.outputs.size(); ++i) {
			const auto& pin = node.outputs[i];
			if (pin.label.empty())
				continue;
			const auto textSize = measure(pin.label.c_str());
			const float y = nodeTopScreen + nodeHeightScreen * (static_cast<float>(i) + 1.0f) / (outCount + 1.0f) +
							g_graphEditorSlotYOffset;
			const ImVec2 pos{iRectangle.Max.x - xPad - textSize.x, y - textSize.y * 0.5f};
			iDrawList->AddText(font, fontSize, pos, toImU32(pin.labelColor), pin.label.c_str());
		}
	}

	[[nodiscard]] auto mouseInCanvasSpace() const -> ImVec2 {
		const auto m = ImGui::GetMousePos();
		return ImVec2{(m.x - m_canvasOriginScreen.x - m_viewState.mPosition.x) / m_viewState.mFactor,
					  (m.y - m_canvasOriginScreen.y - m_viewState.mPosition.y) / m_viewState.mFactor};
	}

	[[nodiscard]] auto pinScreenInfo(core::UUID iPinId) const -> std::optional<std::pair<ImVec2, ImU32>> {
		const float zoom = m_viewState.mFactor;
		std::optional<std::pair<core::UUID, ImRect>> reference;
		if (!m_nodeScreenRects.empty())
			reference = std::make_pair(m_nodeScreenRects.begin()->first, m_nodeScreenRects.begin()->second);

		const auto rectFor = [&](const Node& iNode) -> std::optional<ImRect> {
			if (const auto it = m_nodeScreenRects.find(iNode.id); it != m_nodeScreenRects.end())
				return it->second;
			if (!reference)
				return std::nullopt;
			const Node* refNode = nullptr;
			for (const auto& cand: m_owner.m_nodes) {
				if (cand.id == reference->first) {
					refNode = &cand;
					break;
				}
			}
			if (refNode == nullptr)
				return std::nullopt;
			const auto sz = computeNodeSize(iNode);
			const float dx = (iNode.position.x() - refNode->position.x()) * zoom;
			const float dy = (iNode.position.y() - refNode->position.y()) * zoom;
			const ImVec2 minPt{reference->second.Min.x + dx, reference->second.Min.y + dy};
			return ImRect{minPt, ImVec2{minPt.x + sz.x * zoom, minPt.y + sz.y * zoom}};
		};

		for (const auto& node: m_owner.m_nodes) {
			const auto rectOpt = rectFor(node);
			if (!rectOpt)
				continue;
			const auto& rect = *rectOpt;
			const float nodeHeightScreen = computeNodeSize(node).y * zoom;
			const float nodeTopScreen = rect.Max.y - nodeHeightScreen;
			const auto inCount = static_cast<float>(node.inputs.size());
			const auto outCount = static_cast<float>(node.outputs.size());
			for (size_t i = 0; i < node.inputs.size(); ++i) {
				if (node.inputs[i].id == iPinId) {
					const float y = nodeTopScreen +
									nodeHeightScreen * (static_cast<float>(i) + 1.0f) / (inCount + 1.0f) +
									g_graphEditorSlotYOffset;
					return std::make_pair(ImVec2{rect.Min.x, y}, toImU32(node.inputs[i].labelColor));
				}
			}
			for (size_t i = 0; i < node.outputs.size(); ++i) {
				if (node.outputs[i].id == iPinId) {
					const float y = nodeTopScreen +
									nodeHeightScreen * (static_cast<float>(i) + 1.0f) / (outCount + 1.0f) +
									g_graphEditorSlotYOffset;
					return std::make_pair(ImVec2{rect.Max.x, y}, toImU32(node.outputs[i].labelColor));
				}
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] auto findHoveredPin() const -> core::UUID {
		const ImVec2 mouse = ImGui::GetMousePos();
		const float zoom = m_viewState.mFactor;
		const float radius = m_options.mNodeSlotRadius * zoom * 1.5f;
		const float radius2 = radius * radius;
		float bestDist2 = radius2;
		core::UUID bestPin{0};
		for (const auto& node: m_owner.m_nodes) {
			const auto rectIt = m_nodeScreenRects.find(node.id);
			if (rectIt == m_nodeScreenRects.end())
				continue;
			const auto& rect = rectIt->second;
			const float nodeHeightScreen = computeNodeSize(node).y * zoom;
			const float nodeTopScreen = rect.Max.y - nodeHeightScreen;
			const auto inCount = static_cast<float>(node.inputs.size());
			const auto outCount = static_cast<float>(node.outputs.size());
			for (size_t i = 0; i < node.inputs.size(); ++i) {
				const float y = nodeTopScreen + nodeHeightScreen * (static_cast<float>(i) + 1.0f) / (inCount + 1.0f) +
								g_graphEditorSlotYOffset;
				const float dx = mouse.x - rect.Min.x;
				const float dy = mouse.y - y;
				const float d2 = dx * dx + dy * dy;
				if (d2 < bestDist2) {
					bestDist2 = d2;
					bestPin = node.inputs[i].id;
				}
			}
			for (size_t i = 0; i < node.outputs.size(); ++i) {
				const float y = nodeTopScreen + nodeHeightScreen * (static_cast<float>(i) + 1.0f) / (outCount + 1.0f) +
								g_graphEditorSlotYOffset;
				const float dx = mouse.x - rect.Max.x;
				const float dy = mouse.y - y;
				const float d2 = dx * dx + dy * dy;
				if (d2 < bestDist2) {
					bestDist2 = d2;
					bestPin = node.outputs[i].id;
				}
			}
		}
		return bestPin;
	}

	[[nodiscard]] auto findHoveredLink() const -> core::UUID {
		const ImVec2 mouse = ImGui::GetMousePos();
		const float zoom = m_viewState.mFactor;
		const float threshold = m_options.mLineThickness * zoom * 2.0f;
		const float threshold2 = threshold * threshold;
		float bestDist2 = threshold2;
		core::UUID bestLink{0};
		constexpr int kSegments = 24;
		for (const auto& link: m_owner.m_links) {
			const auto from = pinScreenInfo(link.fromPin);
			const auto to = pinScreenInfo(link.toPin);
			if (!from || !to)
				continue;
			const auto& [p0, _0] = *from;
			const auto& [p3, _1] = *to;
			const float dx = p3.x - p0.x;
			const float ctrl = std::max(std::abs(dx) * 0.5f, 60.0f * zoom);
			const ImVec2 p1{p0.x + ctrl, p0.y};
			const ImVec2 p2{p3.x - ctrl, p3.y};
			ImVec2 prev = p0;
			for (int s = 1; s <= kSegments; ++s) {
				const float t = static_cast<float>(s) / static_cast<float>(kSegments);
				const float u = 1.0f - t;
				const float b0 = u * u * u;
				const float b1 = 3.0f * u * u * t;
				const float b2 = 3.0f * u * t * t;
				const float b3 = t * t * t;
				const ImVec2 cur{b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x,
								 b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y};
				const float sx = cur.x - prev.x;
				const float sy = cur.y - prev.y;
				const float seg2 = sx * sx + sy * sy;
				if (seg2 > 0.0f) {
					const float tProj =
							std::clamp(((mouse.x - prev.x) * sx + (mouse.y - prev.y) * sy) / seg2, 0.0f, 1.0f);
					const float qx = prev.x + tProj * sx;
					const float qy = prev.y + tProj * sy;
					const float ddx = mouse.x - qx;
					const float ddy = mouse.y - qy;
					const float d2 = ddx * ddx + ddy * ddy;
					if (d2 < bestDist2) {
						bestDist2 = d2;
						bestLink = link.id;
					}
				}
				prev = cur;
			}
		}
		return bestLink;
	}

	void renderColouredLinks() {
		if (m_owner.m_links.empty() || mp_canvasDrawList == nullptr)
			return;
		auto* drawList = mp_canvasDrawList;
		const float zoom = m_viewState.mFactor;
		const float baseThickness = m_options.mLineThickness * zoom;
		const auto hoveredPin = findHoveredPin();
		const auto hoveredLink = (static_cast<uint64_t>(hoveredPin) == 0) ? findHoveredLink() : core::UUID{0};
		// Cubic Bezier point evaluator — used for both rendering and collision detection.
		const auto bezierAt = [](const ImVec2& iP0, const ImVec2& iP1, const ImVec2& iP2, const ImVec2& iP3,
								 float iT) -> ImVec2 {
			const float u = 1.0f - iT;
			const float b0 = u * u * u;
			const float b1 = 3.0f * u * u * iT;
			const float b2 = 3.0f * u * iT * iT;
			const float b3 = iT * iT * iT;
			return {b0 * iP0.x + b1 * iP1.x + b2 * iP2.x + b3 * iP3.x,
					b0 * iP0.y + b1 * iP1.y + b2 * iP2.y + b3 * iP3.y};
		};
		for (const auto& link: m_owner.m_links) {
			const auto from = pinScreenInfo(link.fromPin);
			const auto to = pinScreenInfo(link.toPin);
			if (!from || !to)
				continue;
			const auto& [p0, srcColor] = *from;
			const auto& [p3, _] = *to;
			const auto* srcNode = m_owner.findNodeByPin(link.fromPin);
			const auto* dstNode = m_owner.findNodeByPin(link.toPin);
			const core::UUID srcId = (srcNode != nullptr) ? srcNode->id : core::UUID{0};
			const core::UUID dstId = (dstNode != nullptr) ? dstNode->id : core::UUID{0};
			const bool isHighlighted =
					(link.fromPin == hoveredPin) || (link.toPin == hoveredPin) || (link.id == hoveredLink);
			const float dx = p3.x - p0.x;
			const float ctrl = std::max(std::abs(dx) * 0.6f, 90.0f * zoom);
			ImVec2 p1{p0.x + ctrl, p0.y};
			ImVec2 p2{p3.x - ctrl, p3.y};
			constexpr int kSamples = 24;
			constexpr int kMaxDeflectIterations = 3;
			for (int iter = 0; iter < kMaxDeflectIterations; ++iter) {
				float maxDeflect = 0.0f;
				for (const auto& [otherId, otherRect]: m_nodeScreenRects) {
					if (otherId == srcId || otherId == dstId)
						continue;
					for (int s = 1; s < kSamples; ++s) {
						const float t = static_cast<float>(s) / static_cast<float>(kSamples);
						const ImVec2 pt = bezierAt(p0, p1, p2, p3, t);
						if (pt.x < otherRect.Min.x || pt.x > otherRect.Max.x || pt.y < otherRect.Min.y ||
							pt.y > otherRect.Max.y)
							continue;
						const float margin = 16.0f * zoom;
						const float upDist = pt.y - otherRect.Min.y + margin;
						const float downDist = otherRect.Max.y - pt.y + margin;
						const float deflect = (upDist < downDist) ? -upDist : downDist;
						if (std::abs(deflect) > std::abs(maxDeflect))
							maxDeflect = deflect;
						break;
					}
				}
				if (std::abs(maxDeflect) < 1e-3f)
					break;
				p1.y += maxDeflect;
				p2.y += maxDeflect;
			}
			ImU32 color = srcColor;
			float thickness = baseThickness;
			if (isHighlighted) {
				thickness *= 2.0f;
				// Brighten by OR-ing white channels — same trick GraphEditor uses for selection.
				color |= IM_COL32(0xa0, 0xa0, 0xa0, 0);
			}
			drawList->AddBezierCubic(p0, p1, p2, p3, color, thickness);
		}
		if (static_cast<uint64_t>(hoveredLink) != 0 && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
			m_owner.removeLink(hoveredLink);
			if (m_onLinkDeleted)

				m_onLinkDeleted(hoveredLink);
		}
	}

	void RightClick(GraphEditor::NodeIndex iNodeIndex, [[maybe_unused]] GraphEditor::SlotIndex iSlotInput,
					[[maybe_unused]] GraphEditor::SlotIndex iSlotOutput) override {
		if (!m_onContextMenu)
			return;
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
		out.mName = "";
		out.mTemplateIndex = iIndex;
		out.mRect = ImRect{topLeft, {topLeft.x + size.x, topLeft.y + size.y}};
		out.mSelected = iIndex < m_selected.size() && m_selected[iIndex];
		return out;
	}

	auto GetLinkCount() -> const size_t override { return 0; }

	auto GetLink(GraphEditor::LinkIndex iIndex) -> const GraphEditor::Link override {
		const auto& link = m_owner.m_links[iIndex];
		GraphEditor::Link out{};
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

NodeCanvas::NodeCanvas() : mp_impl{mkUniq<Impl>(*this)} { mp_impl->m_options.mDrawIONameOnHover = false; }

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
	for (const auto& pin: node.inputs) pinIds.push_back(pin.id);
	for (const auto& pin: node.outputs) pinIds.push_back(pin.id);
	std::erase_if(m_links, [&](const Link& iLink) -> bool {
		return std::ranges::find(pinIds, iLink.fromPin) != pinIds.end() ||
			   std::ranges::find(pinIds, iLink.toPin) != pinIds.end();
	});
	m_nodes.erase(m_nodes.begin() + static_cast<ptrdiff_t>(*idx));
	mp_impl->rebuildTemplates();
}

auto NodeCanvas::addOutputPin(core::UUID iNodeId, NodePin iPin) -> core::UUID {
	const auto idx = mp_impl->findNodeIndex(iNodeId);
	if (!idx)
		return core::UUID{0};
	if (static_cast<uint64_t>(iPin.id) == 0)
		iPin.id = core::UUID{};
	iPin.kind = PinKind::Output;
	const auto pinId = iPin.id;
	m_nodes[*idx].outputs.push_back(std::move(iPin));
	mp_impl->rebuildTemplates();
	return pinId;
}

auto NodeCanvas::addInputPin(core::UUID iNodeId, NodePin iPin) -> core::UUID {
	const auto idx = mp_impl->findNodeIndex(iNodeId);
	if (!idx)
		return core::UUID{0};
	if (static_cast<uint64_t>(iPin.id) == 0)
		iPin.id = core::UUID{};
	iPin.kind = PinKind::Input;
	const auto pinId = iPin.id;
	m_nodes[*idx].inputs.push_back(std::move(iPin));
	mp_impl->rebuildTemplates();
	return pinId;
}

void NodeCanvas::removeOutputPin(core::UUID iNodeId, core::UUID iPinId) {
	const auto idx = mp_impl->findNodeIndex(iNodeId);
	if (!idx)
		return;
	auto& outputs = m_nodes[*idx].outputs;
	std::erase_if(outputs, [iPinId](const NodePin& iPin) -> bool { return iPin.id == iPinId; });
	std::erase_if(m_links,
				  [iPinId](const Link& iLink) -> bool { return iLink.fromPin == iPinId || iLink.toPin == iPinId; });
	mp_impl->rebuildTemplates();
}

void NodeCanvas::removeInputPin(core::UUID iNodeId, core::UUID iPinId) {
	const auto idx = mp_impl->findNodeIndex(iNodeId);
	if (!idx)
		return;
	auto& inputs = m_nodes[*idx].inputs;
	std::erase_if(inputs, [iPinId](const NodePin& iPin) -> bool { return iPin.id == iPinId; });
	std::erase_if(m_links,
				  [iPinId](const Link& iLink) -> bool { return iLink.fromPin == iPinId || iLink.toPin == iPinId; });
	mp_impl->rebuildTemplates();
}

auto NodeCanvas::addLink(core::UUID iFromPin, core::UUID iToPin) -> core::UUID {
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

void NodeCanvas::requestFitToContent() { mp_impl->m_pendingFit = GraphEditor::Fit_AllNodes; }

void NodeCanvas::onRender() {
	if (mp_impl->m_templates.size() != m_nodes.size())
		mp_impl->rebuildTemplates();

	mp_impl->m_canvasOriginScreen = ImGui::GetCursorScreenPos();
	mp_impl->m_nodeScreenRects.clear();
	mp_impl->mp_canvasDrawList = nullptr;
	GraphEditor::FitOnScreen* fitPtr =
			(mp_impl->m_pendingFit != GraphEditor::Fit_None) ? &mp_impl->m_pendingFit : nullptr;
	GraphEditor::Show(*mp_impl, mp_impl->m_options, mp_impl->m_viewState, mp_impl->m_enabled, fitPtr);
	mp_impl->renderColouredLinks();
}

}// namespace owl::gui::widgets
