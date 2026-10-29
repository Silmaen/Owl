/**
 * @file NodeCanvas.cpp
 * @author Silmaen
 * @date 23/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/widgets/NodeCanvas.h"

#include "gui/UiLayer.h"
#include "gui/utils.h"
#include "gui/widgets/NodeStyle.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::gui::widgets {

namespace {

constexpr float g_headerHeight = 30.f;
constexpr float g_rowHeight = 26.f;
constexpr float g_bodyTop = 6.f;
constexpr float g_bodyBottom = 8.f;
constexpr float g_padding = 12.f;
constexpr float g_pinRadius = 5.f;
constexpr float g_pinSpace = 20.f;
constexpr float g_columnGap = 28.f;
constexpr float g_minWidth = 160.f;
constexpr float g_minZoom = 0.25f;
constexpr float g_maxZoom = 2.f;
constexpr float g_dragThreshold = 4.f;
constexpr float g_pinLabelLodZoom = 0.6f;
constexpr float g_titleLodZoom = 0.3f;
constexpr auto g_doubleClickInterval = std::chrono::milliseconds{300};
constexpr math::vec4 g_headerColor{0.16f, 0.33f, 0.55f, 1.f};

// Text width at the base size; a fixed width per character without an ImGui context (tests, batch layout).
auto measure(const std::string_view iText) -> float {
	if (ImGui::GetCurrentContext() == nullptr)
		return 7.f * static_cast<float>(iText.size());
	return ImGui::CalcTextSize(iText.data(), iText.data() + iText.size()).x;
}

auto rowCenter(const size_t iRow) -> float {
	return g_headerHeight + g_bodyTop + (static_cast<float>(iRow) + 0.5f) * g_rowHeight;
}

auto computeNodeSize(const Node& iNode) -> math::vec2f {
	float inputs = 0.f;
	for (const auto& pin: iNode.inputs) inputs = std::max(inputs, g_pinSpace + measure(pin.label));
	float outputs = 0.f;
	for (const auto& pin: iNode.outputs) outputs = std::max(outputs, g_pinSpace + measure(pin.label));
	const float body = inputs + outputs + (inputs > 0.f && outputs > 0.f ? g_columnGap : 0.f);
	const float width = std::max({g_minWidth, body + 2.f * g_padding, measure(iNode.title) + 2.f * g_padding});
	const auto rows = std::max(iNode.inputs.size(), iNode.outputs.size());
	return {width, g_headerHeight + g_bodyTop + static_cast<float>(rows) * g_rowHeight + g_bodyBottom};
}

auto toIm(const math::vec2f& iPoint) -> ImVec2 { return {iPoint.x(), iPoint.y()}; }

auto toVec(const ImVec2& iPoint) -> math::vec2f { return {iPoint.x, iPoint.y}; }

auto lighter(const math::vec4& iColor, const float iAmount) -> math::vec4 {
	return {iColor.x() + (1.f - iColor.x()) * iAmount, iColor.y() + (1.f - iColor.y()) * iAmount,
			iColor.z() + (1.f - iColor.z()) * iAmount, iColor.w()};
}

auto toU32(const math::vec4& iColor) -> ImU32 {
	return ImGui::ColorConvertFloat4ToU32({iColor.x(), iColor.y(), iColor.z(), iColor.w()});
}

}// namespace

auto shouldDrawPinLabels(const float iZoomFactor) -> bool { return iZoomFactor > g_pinLabelLodZoom; }

auto shouldDrawNodeTitles(const float iZoomFactor) -> bool { return iZoomFactor > g_titleLodZoom; }

struct NodeCanvas::Impl final {
	enum struct Gesture : uint8_t { None, Pan, Select, Move, Link, Waypoint };

	struct WaypointRef {
		size_t link{0};
		size_t index{0};
	};

	struct PinView {
		math::vec2f position;
		math::vec4 color;
		bool output{false};
		size_t node{0};
	};

	explicit Impl(NodeCanvas& ioOwner) : m_owner{ioOwner} {}

	NodeCanvas& m_owner;
	std::vector<core::UUID> m_selection;
	std::array<float, 2> m_offset{40.f, 40.f};
	float m_zoom{1.f};
	bool m_enabled{true};
	bool m_fitPending{false};
	Gesture m_gesture{Gesture::None};
	std::array<float, 2> m_gestureStart{0.f, 0.f};
	std::array<float, 2> m_lastMouse{0.f, 0.f};
	std::optional<core::UUID> m_pressedNode;
	bool m_moved{false};
	core::UUID m_linkStart{0};
	core::UUID m_lastClickedNode{0};
	std::chrono::steady_clock::time_point m_lastClickTime;
	ImVec2 m_origin{0.f, 0.f};
	ImVec2 m_size{0.f, 0.f};
	std::function<bool(core::UUID, core::UUID)> m_linkValidator{[](auto, auto) -> bool { return true; }};
	std::function<void(core::UUID, core::UUID, core::UUID)> m_onLinkCreated;
	std::function<void(core::UUID)> m_onLinkDeleted;
	std::function<void(core::UUID, const std::vector<math::vec2f>&)> m_onLinkWaypointsChanged;
	core::UUID m_draggedLink{0};
	size_t m_draggedWaypoint{0};
	std::vector<math::vec2f> m_waypointsBefore;
	std::vector<std::pair<core::UUID, std::vector<math::vec2f>>> m_movedLinks;
	std::function<void(core::UUID, math::vec2f)> m_onNodeMoved;
	std::function<void(core::UUID)> m_onNodeSelected;
	std::function<void(core::UUID)> m_onNodeDoubleClicked;
	std::function<void(std::optional<core::UUID>)> m_onContextMenu;
	// Per frame.
	std::unordered_map<uint64_t, PinView> m_pins;
	std::vector<std::pair<ImVec2, ImVec2>> m_rects;
	core::UUID m_hoveredPin{0};
	std::optional<size_t> m_hoveredNode;
	std::optional<size_t> m_hoveredLink;
	std::optional<WaypointRef> m_hoveredWaypoint;
	bool m_hovered{false};

	[[nodiscard]] auto toCanvas(const ImVec2& iScreen) const -> std::array<float, 2> {
		return {(iScreen.x - m_origin.x) / m_zoom - m_offset[0], (iScreen.y - m_origin.y) / m_zoom - m_offset[1]};
	}

	[[nodiscard]] auto toScreen(const math::vec2f& iCanvas) const -> ImVec2 {
		return {m_origin.x + (iCanvas.x() + m_offset[0]) * m_zoom, m_origin.y + (iCanvas.y() + m_offset[1]) * m_zoom};
	}

	[[nodiscard]] auto toScreenPoints(const std::vector<math::vec2f>& iCanvas) const -> std::vector<math::vec2f> {
		std::vector<math::vec2f> points;
		points.reserve(iCanvas.size());
		for (const auto& point: iCanvas) points.push_back(toVec(toScreen(point)));
		return points;
	}

	[[nodiscard]] auto isInnerLink(const Link& iLink) const -> bool {
		const auto* from = m_owner.findNodeByPin(iLink.fromPin);
		const auto* to = m_owner.findNodeByPin(iLink.toPin);
		return from != nullptr && to != nullptr && isSelected(from->id) && isSelected(to->id);
	}

	[[nodiscard]] auto findNodeIndex(const core::UUID iId) const -> std::optional<size_t> {
		for (size_t i = 0; i < m_owner.m_nodes.size(); ++i)
			if (m_owner.m_nodes[i].id == iId)
				return i;
		return std::nullopt;
	}

	[[nodiscard]] auto isSelected(const core::UUID iId) const -> bool {
		return std::ranges::find(m_selection, iId) != m_selection.end();
	}

	void fitToContent() {
		m_fitPending = false;
		if (m_owner.m_nodes.empty() || m_size.x < 1.f || m_size.y < 1.f)
			return;
		std::array bounds{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
						  std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
		for (const auto& node: m_owner.m_nodes) {
			const auto size = computeNodeSize(node);
			bounds = {std::min(bounds[0], node.position.x()), std::min(bounds[1], node.position.y()),
					  std::max(bounds[2], node.position.x() + size.x()),
					  std::max(bounds[3], node.position.y() + size.y())};
		}
		constexpr float margin = 60.f;
		m_zoom = std::clamp(std::min(m_size.x / (bounds[2] - bounds[0] + 2.f * margin),
									 m_size.y / (bounds[3] - bounds[1] + 2.f * margin)),
							0.4f, 1.f);
		m_offset = {m_size.x * 0.5f / m_zoom - (bounds[0] + bounds[2]) * 0.5f,
					m_size.y * 0.5f / m_zoom - (bounds[1] + bounds[3]) * 0.5f};
	}

	void placeNodes() {
		m_pins.clear();
		m_rects.clear();
		m_hoveredPin = core::UUID{0};
		m_hoveredNode.reset();
		const ImVec2 mouse = ImGui::GetMousePos();
		const float pick = std::max(8.f, g_pinRadius * m_zoom * 1.8f);
		float best = pick * pick;
		for (size_t index = 0; index < m_owner.m_nodes.size(); ++index) {
			const auto& node = m_owner.m_nodes[index];
			const auto size = computeNodeSize(node);
			const ImVec2 min = toScreen(node.position);
			const ImVec2 max{min.x + size.x() * m_zoom, min.y + size.y() * m_zoom};
			m_rects.emplace_back(min, max);
			if (mouse.x >= min.x && mouse.x <= max.x && mouse.y >= min.y && mouse.y <= max.y)
				m_hoveredNode = index;
			const auto place = [&](const std::vector<NodePin>& iPins, const bool iOutput) -> void {
				for (size_t i = 0; i < iPins.size(); ++i) {
					const float x = iOutput ? size.x() - g_padding - g_pinRadius * 1.6f : g_padding + g_pinRadius;
					const math::vec2f at{min.x + x * m_zoom, min.y + rowCenter(i) * m_zoom};
					m_pins[static_cast<uint64_t>(iPins[i].id)] = {.position = at,
																  .color = iPins[i].labelColor,
																  .output = iOutput,
																  .node = index};
					const float dx = mouse.x - at.x();
					const float dy = mouse.y - at.y();
					if (const float distance = dx * dx + dy * dy; distance < best) {
						best = distance;
						m_hoveredPin = iPins[i].id;
					}
				}
			};
			place(node.inputs, false);
			place(node.outputs, true);
		}
	}

	void drawLinks(ImDrawList* ioDraw) {
		m_hoveredLink.reset();
		m_hoveredWaypoint.reset();
		const ImVec2 mouse = ImGui::GetMousePos();
		const bool free = m_hovered && m_hoveredPin == 0 && !m_hoveredNode.has_value();
		const float handle = std::max(4.f, g_pinRadius * 0.9f * m_zoom);
		const float pick = std::max(8.f, handle * 1.8f);
		float best = 6.f;
		float bestWaypoint = pick * pick;
		std::vector<std::vector<math::vec2f>> screen(m_owner.m_links.size());
		for (size_t i = 0; i < m_owner.m_links.size(); ++i) {
			const auto& link = m_owner.m_links[i];
			const auto from = m_pins.find(static_cast<uint64_t>(link.fromPin));
			const auto to = m_pins.find(static_cast<uint64_t>(link.toPin));
			if (from == m_pins.end() || to == m_pins.end())
				continue;
			screen[i] = toScreenPoints(link.waypoints);
			if (!free)
				continue;
			for (size_t w = 0; w < screen[i].size(); ++w) {
				const float dx = mouse.x - screen[i][w].x();
				const float dy = mouse.y - screen[i][w].y();
				if (const float distance = dx * dx + dy * dy; distance < bestWaypoint) {
					bestWaypoint = distance;
					m_hoveredWaypoint = WaypointRef{.link = i, .index = w};
				}
			}
			if (const float distance = getNodeLinkDistance(from->second.position, to->second.position, screen[i],
														   toVec(mouse), m_zoom);
				distance < best) {
				best = distance;
				m_hoveredLink = i;
			}
		}
		if (m_hoveredWaypoint.has_value())
			m_hoveredLink = m_hoveredWaypoint->link;
		for (size_t i = 0; i < m_owner.m_links.size(); ++i) {
			const auto& link = m_owner.m_links[i];
			const auto from = m_pins.find(static_cast<uint64_t>(link.fromPin));
			const auto to = m_pins.find(static_cast<uint64_t>(link.toPin));
			if (from == m_pins.end() || to == m_pins.end())
				continue;
			const bool highlighted = m_hoveredLink == i || link.fromPin == m_hoveredPin || link.toPin == m_hoveredPin ||
									 isSelected(m_owner.m_nodes[from->second.node].id) ||
									 isSelected(m_owner.m_nodes[to->second.node].id);
			const auto color = highlighted ? lighter(from->second.color, 0.35f) : from->second.color;
			drawNodeLink(ioDraw, from->second.position, to->second.position, screen[i], color,
						 2.4f * m_zoom * (highlighted ? 1.5f : 1.f), m_zoom);
			for (size_t w = 0; w < screen[i].size(); ++w)
				drawNodeLinkWaypoint(ioDraw, screen[i][w], handle, color,
									 m_hoveredWaypoint.has_value() && m_hoveredWaypoint->link == i &&
											 m_hoveredWaypoint->index == w);
		}
	}

	void insertWaypoint() {
		auto& link = m_owner.m_links[*m_hoveredLink];
		const auto from = m_pins.find(static_cast<uint64_t>(link.fromPin));
		const auto to = m_pins.find(static_cast<uint64_t>(link.toPin));
		if (from == m_pins.end() || to == m_pins.end())
			return;
		const auto before = link.waypoints;
		const auto index = getNodeLinkInsertIndex(from->second.position, to->second.position,
												  toScreenPoints(link.waypoints), toVec(ImGui::GetMousePos()), m_zoom);
		const auto mouse = toCanvas(ImGui::GetMousePos());
		link.waypoints.insert(link.waypoints.begin() + static_cast<ptrdiff_t>(index), math::vec2f{mouse[0], mouse[1]});
		if (m_onLinkWaypointsChanged)
			m_onLinkWaypointsChanged(link.id, before);
	}

	void deleteHovered() {
		if (m_hoveredWaypoint.has_value()) {
			auto& link = m_owner.m_links[m_hoveredWaypoint->link];
			const auto before = link.waypoints;
			link.waypoints.erase(link.waypoints.begin() + static_cast<ptrdiff_t>(m_hoveredWaypoint->index));
			if (m_onLinkWaypointsChanged)
				m_onLinkWaypointsChanged(link.id, before);
			return;
		}
		const auto id = m_owner.m_links[*m_hoveredLink].id;
		m_owner.removeLink(id);
		if (m_onLinkDeleted)
			m_onLinkDeleted(id);
	}

	void drawNodes(ImDrawList* ioDraw) const {
		ImFont* font = ImGui::GetFont();
		const float fontSize = ImGui::GetFontSize() * m_zoom;
		UiLayer::pushTextStyle(TextStyle::Strong);
		ImFont* strong = ImGui::GetFont();
		UiLayer::popTextStyle();
		for (size_t index = 0; index < m_owner.m_nodes.size(); ++index) {
			const auto& node = m_owner.m_nodes[index];
			const auto& [min, max] = m_rects[index];
			drawNodeFrame(ioDraw, toVec(min), toVec(max),
						  {.header = g_headerColor,
						   .headerHeight = g_headerHeight,
						   .selected = isSelected(node.id),
						   .hovered = m_hoveredNode == index,
						   .faulty = false},
						  m_zoom);
			if (shouldDrawNodeTitles(m_zoom) && !node.title.empty())
				ioDraw->AddText(strong, fontSize,
								{min.x + g_padding * m_zoom, min.y + (g_headerHeight * m_zoom - fontSize) * 0.5f},
								toU32(node.titleColor), node.title.c_str());
			const auto drawPins = [&](const std::vector<NodePin>& iPins) -> void {
				for (const auto& pin: iPins) {
					const auto& view = m_pins.at(static_cast<uint64_t>(pin.id));
					const bool linked = std::ranges::any_of(m_owner.m_links, [&](const Link& iLink) -> bool {
						return iLink.fromPin == pin.id || iLink.toPin == pin.id;
					});
					auto color = pin.labelColor;
					if (pin.id == m_hoveredPin) {
						ioDraw->AddCircleFilled(toIm(view.position), g_pinRadius * 2.2f * m_zoom,
												toU32({color.x(), color.y(), color.z(), 0.25f}));
						color = lighter(color, 0.3f);
					}
					drawDataPin(ioDraw, view.position, g_pinRadius * m_zoom, color, linked);
					if (!shouldDrawPinLabels(m_zoom) || pin.label.empty())
						continue;
					const auto size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, pin.label.c_str());
					const float offset = (g_pinRadius + 8.f) * m_zoom;
					const float x = view.output ? view.position.x() - offset - size.x : view.position.x() + offset;
					ioDraw->AddText(font, fontSize, {x, view.position.y() - size.y * 0.5f},
									IM_COL32(225, 228, 232, 255), pin.label.c_str());
				}
			};
			drawPins(node.inputs);
			drawPins(node.outputs);
		}
	}

	void clickNode(const size_t iIndex) {
		const auto id = m_owner.m_nodes[iIndex].id;
		const auto& io = ImGui::GetIO();
		const bool selected = isSelected(id);
		if (io.KeyCtrl && selected)
			std::erase(m_selection, id);
		else if (io.KeyCtrl || io.KeyShift)
			m_selection.push_back(id);
		else if (!selected)
			m_selection = {id};
		else
			m_pressedNode = id;
		const auto now = std::chrono::steady_clock::now();
		const bool isDouble = id == m_lastClickedNode && now - m_lastClickTime <= g_doubleClickInterval;
		m_lastClickedNode = id;
		m_lastClickTime = now;
		if (isDouble && m_onNodeDoubleClicked)
			m_onNodeDoubleClicked(id);
		else if (!isDouble && m_onNodeSelected)
			m_onNodeSelected(id);
		m_gesture = Gesture::Move;
	}

	void finishLink() {
		const auto start = m_pins.find(static_cast<uint64_t>(m_linkStart));
		const auto target = m_pins.find(static_cast<uint64_t>(m_hoveredPin));
		if (start == m_pins.end() || target == m_pins.end() || start->second.output == target->second.output ||
			start->second.node == target->second.node)
			return;
		const auto fromPin = start->second.output ? m_linkStart : m_hoveredPin;
		const auto toPin = start->second.output ? m_hoveredPin : m_linkStart;
		if (m_linkValidator && !m_linkValidator(fromPin, toPin))
			return;
		const Link link{.id = core::UUID{}, .fromPin = fromPin, .toPin = toPin, .waypoints = {}};
		m_owner.m_links.push_back(link);
		if (m_onLinkCreated)
			m_onLinkCreated(link.id, fromPin, toPin);
	}

	void releaseLeft(const std::array<float, 2>& iMouse) {
		switch (m_gesture) {
			case Gesture::Move:
				if (!m_moved && m_pressedNode.has_value())
					m_selection = {*m_pressedNode};
				if (m_moved && m_onLinkWaypointsChanged)
					for (const auto& [id, before]: m_movedLinks) m_onLinkWaypointsChanged(id, before);
				m_movedLinks.clear();
				break;
			case Gesture::Waypoint:
				if (const auto* link = m_owner.findLink(m_draggedLink);
					link != nullptr && link->waypoints != m_waypointsBefore && m_onLinkWaypointsChanged)
					m_onLinkWaypointsChanged(m_draggedLink, m_waypointsBefore);
				m_draggedLink = core::UUID{0};
				break;
			case Gesture::Select:
				{
					const std::array lo{std::min(m_gestureStart[0], iMouse[0]), std::min(m_gestureStart[1], iMouse[1])};
					const std::array hi{std::max(m_gestureStart[0], iMouse[0]), std::max(m_gestureStart[1], iMouse[1])};
					for (const auto& node: m_owner.m_nodes) {
						const auto size = computeNodeSize(node);
						if (node.position.x() < hi[0] && node.position.x() + size.x() > lo[0] &&
							node.position.y() < hi[1] && node.position.y() + size.y() > lo[1] && !isSelected(node.id))
							m_selection.push_back(node.id);
					}
				}
				break;
			case Gesture::Link:
				finishLink();
				m_linkStart = core::UUID{0};
				break;
			case Gesture::Pan:
			case Gesture::None:
				break;
		}
		m_gesture = Gesture::None;
	}

	void pressLeft(const std::array<float, 2>& iMouse) {
		const auto& io = ImGui::GetIO();
		m_gestureStart = iMouse;
		m_lastMouse = iMouse;
		m_pressedNode.reset();
		m_moved = false;
		if (io.KeyAlt && m_hoveredPin == 0 && m_hoveredLink.has_value()) {
			const auto id = m_owner.m_links[*m_hoveredLink].id;
			m_owner.removeLink(id);
			if (m_onLinkDeleted)
				m_onLinkDeleted(id);
		} else if (m_hoveredPin != 0) {
			m_gesture = Gesture::Link;
			m_linkStart = m_hoveredPin;
		} else if (m_hoveredWaypoint.has_value()) {
			const auto& link = m_owner.m_links[m_hoveredWaypoint->link];
			m_gesture = Gesture::Waypoint;
			m_draggedLink = link.id;
			m_draggedWaypoint = m_hoveredWaypoint->index;
			m_waypointsBefore = link.waypoints;
		} else if (m_hoveredNode.has_value()) {
			clickNode(*m_hoveredNode);
			m_movedLinks.clear();
			for (const auto& link: m_owner.m_links)
				if (!link.waypoints.empty() && isInnerLink(link))
					m_movedLinks.emplace_back(link.id, link.waypoints);
		} else {
			if (!io.KeyCtrl && !io.KeyShift)
				m_selection.clear();
			m_gesture = Gesture::Select;
		}
	}

	void dragLeft(const std::array<float, 2>& iMouse) {
		if (m_gesture == Gesture::Waypoint) {
			for (auto& link: m_owner.m_links)
				if (link.id == m_draggedLink && m_draggedWaypoint < link.waypoints.size())
					link.waypoints[m_draggedWaypoint] = {iMouse[0], iMouse[1]};
			return;
		}
		if (m_gesture != Gesture::Move)
			return;
		const math::vec2f delta{iMouse[0] - m_lastMouse[0], iMouse[1] - m_lastMouse[1]};
		m_lastMouse = iMouse;
		if (std::abs(iMouse[0] - m_gestureStart[0]) + std::abs(iMouse[1] - m_gestureStart[1]) >
			g_dragThreshold / m_zoom)
			m_moved = true;
		if (delta.x() == 0.f && delta.y() == 0.f)
			return;
		for (auto& node: m_owner.m_nodes)
			if (isSelected(node.id)) {
				node.position += delta;
				if (m_onNodeMoved)
					m_onNodeMoved(node.id, node.position);
			}
		for (const auto& moved: m_movedLinks | std::views::keys)
			for (auto& link: m_owner.m_links)
				if (link.id == moved)
					for (auto& point: link.waypoints) point += delta;
	}

	void handleMouse() {
		const auto& io = ImGui::GetIO();
		const auto mouse = toCanvas(ImGui::GetMousePos());
		if (m_hovered && io.MouseWheel != 0.f) {
			m_zoom = std::clamp(m_zoom * std::pow(1.15f, io.MouseWheel), g_minZoom, g_maxZoom);
			m_offset = {(ImGui::GetMousePos().x - m_origin.x) / m_zoom - mouse[0],
						(ImGui::GetMousePos().y - m_origin.y) / m_zoom - mouse[1]};
		}
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, g_dragThreshold) ||
			ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.f)) {
			if (m_gesture == Gesture::None && m_hovered)
				m_gesture = Gesture::Pan;
			if (m_gesture == Gesture::Pan) {
				m_offset[0] += io.MouseDelta.x / m_zoom;
				m_offset[1] += io.MouseDelta.y / m_zoom;
			}
		}
		if (m_gesture == Gesture::Pan && !ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
			!ImGui::IsMouseDown(ImGuiMouseButton_Middle))
			m_gesture = Gesture::None;
		else if (m_gesture == Gesture::None && m_hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
			if (m_hoveredLink.has_value() && !m_hoveredWaypoint.has_value())
				insertWaypoint();
			else if (m_onContextMenu)
				m_onContextMenu(m_hoveredNode.has_value() ? std::optional{m_owner.m_nodes[*m_hoveredNode].id}
														  : std::nullopt);
		}
		if (m_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			pressLeft(mouse);
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
			dragLeft(mouse);
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			releaseLeft(mouse);
		if (m_hovered && m_hoveredLink.has_value() && m_hoveredPin == 0 && m_gesture == Gesture::None &&
			ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			deleteHovered();
	}

	void drawOverlays(ImDrawList* ioDraw) const {
		const auto& theme = UiLayer::getTheme();
		const ImVec2 mouse = ImGui::GetMousePos();
		if (m_gesture == Gesture::Link)
			if (const auto start = m_pins.find(static_cast<uint64_t>(m_linkStart)); start != m_pins.end()) {
				auto from = start->second.position;
				auto to = toVec(mouse);
				if (!start->second.output)
					std::swap(from, to);
				drawNodeLink(ioDraw, from, to, start->second.color, 2.4f * m_zoom, m_zoom);
			}
		if (m_gesture == Gesture::Select) {
			const auto corner = toScreen({m_gestureStart[0], m_gestureStart[1]});
			const ImVec2 lo{std::min(corner.x, mouse.x), std::min(corner.y, mouse.y)};
			const ImVec2 hi{std::max(corner.x, mouse.x), std::max(corner.y, mouse.y)};
			ioDraw->AddRectFilled(lo, hi, ImGui::GetColorU32(vec(Theme::withAlpha(theme.accent, 0.1f))));
			ioDraw->AddRect(lo, hi, ImGui::GetColorU32(vec(Theme::withAlpha(theme.accent, 0.8f))));
		}
	}
};

NodeCanvas::NodeCanvas() : mp_impl{mkUniq<Impl>(*this)} {}

NodeCanvas::~NodeCanvas() = default;

auto NodeCanvas::measureNode(const Node& iNode) -> math::vec2f { return computeNodeSize(iNode); }

auto NodeCanvas::addNode(Node iNode) -> core::UUID {
	if (static_cast<uint64_t>(iNode.id) == 0)
		iNode.id = core::UUID{};
	m_nodes.push_back(std::move(iNode));
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
}

void NodeCanvas::removeInputPin(core::UUID iNodeId, core::UUID iPinId) {
	const auto idx = mp_impl->findNodeIndex(iNodeId);
	if (!idx)
		return;
	auto& inputs = m_nodes[*idx].inputs;
	std::erase_if(inputs, [iPinId](const NodePin& iPin) -> bool { return iPin.id == iPinId; });
	std::erase_if(m_links,
				  [iPinId](const Link& iLink) -> bool { return iLink.fromPin == iPinId || iLink.toPin == iPinId; });
}

auto NodeCanvas::addLink(core::UUID iFromPin, core::UUID iToPin, std::vector<math::vec2f> iWaypoints) -> core::UUID {
	m_links.push_back({.id = core::UUID{}, .fromPin = iFromPin, .toPin = iToPin, .waypoints = std::move(iWaypoints)});
	return m_links.back().id;
}

auto NodeCanvas::setLinkWaypoints(core::UUID iId, std::vector<math::vec2f> iWaypoints) -> bool {
	for (auto& link: m_links)
		if (link.id == iId) {
			link.waypoints = std::move(iWaypoints);
			return true;
		}
	OWL_CORE_WARN("NodeCanvas: Link {} not found, waypoints not set.", static_cast<uint64_t>(iId))
	return false;
}

void NodeCanvas::removeLink(core::UUID iId) {
	std::erase_if(m_links, [iId](const Link& iLink) -> bool { return iLink.id == iId; });
}

void NodeCanvas::clear() {
	m_nodes.clear();
	m_links.clear();
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
	mp_impl->m_selection.clear();
	for (const auto id: iNodeIds)
		if (findNode(id) != nullptr)
			mp_impl->m_selection.push_back(id);
}

auto NodeCanvas::selection() const -> std::vector<core::UUID> {
	std::vector<core::UUID> out;
	for (const auto id: mp_impl->m_selection)
		if (findNode(id) != nullptr)
			out.push_back(id);
	return out;
}

void NodeCanvas::setLinkValidator(std::function<bool(core::UUID, core::UUID)> iValidator) {
	mp_impl->m_linkValidator = std::move(iValidator);
}

void NodeCanvas::setOnLinkCreated(std::function<void(core::UUID, core::UUID, core::UUID)> iCb) {
	mp_impl->m_onLinkCreated = std::move(iCb);
}

void NodeCanvas::setOnLinkDeleted(std::function<void(core::UUID)> iCb) { mp_impl->m_onLinkDeleted = std::move(iCb); }

void NodeCanvas::setOnLinkWaypointsChanged(std::function<void(core::UUID, const std::vector<math::vec2f>&)> iCb) {
	mp_impl->m_onLinkWaypointsChanged = std::move(iCb);
}

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

auto NodeCanvas::getMouseCanvasPosition() const -> math::vec2f {
	const auto mouse = mp_impl->toCanvas(ImGui::GetMousePos());
	return {mouse[0], mouse[1]};
}

void NodeCanvas::requestFitToContent() { mp_impl->m_fitPending = true; }

void NodeCanvas::onRender() {
	auto& impl = *mp_impl;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, vec(getNodeCanvasBackground()));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
	const bool visible = ImGui::BeginChild("##nodeCanvas", {0.f, 0.f}, ImGuiChildFlags_None,
										   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
												   ImGuiWindowFlags_NoMove);
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	if (!visible) {
		ImGui::EndChild();
		return;
	}
	impl.m_origin = ImGui::GetWindowPos();
	impl.m_size = ImGui::GetWindowSize();
	std::erase_if(impl.m_selection, [this](const core::UUID iId) -> bool { return findNode(iId) == nullptr; });
	if (impl.m_fitPending)
		impl.fitToContent();
	impl.m_hovered = impl.m_enabled && ImGui::IsWindowHovered() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
	auto* draw = ImGui::GetWindowDrawList();
	drawNodeGrid(draw, toVec(impl.m_origin), toVec(impl.m_size), impl.m_offset, impl.m_zoom);
	impl.placeNodes();
	impl.drawLinks(draw);
	impl.drawNodes(draw);
	if (impl.m_enabled)
		impl.handleMouse();
	impl.drawOverlays(draw);
	ImGui::EndChild();
}

}// namespace owl::gui::widgets
