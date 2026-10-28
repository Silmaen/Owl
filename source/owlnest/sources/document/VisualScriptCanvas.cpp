/**
 * @file VisualScriptCanvas.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "VisualScriptCanvas.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/NodeStyle.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::nest {

namespace {

namespace vs = script::visual;

constexpr float g_headerHeight = 30.f;
constexpr float g_rowHeight = 26.f;
constexpr float g_bodyTop = 6.f;
constexpr float g_bodyBottom = 8.f;
constexpr float g_padding = 12.f;
constexpr float g_pinRadius = 5.f;
constexpr float g_pinSpace = 20.f;
constexpr float g_columnGap = 28.f;
constexpr float g_fieldGap = 6.f;
constexpr float g_minWidth = 140.f;
constexpr float g_glyphWidth = 22.f;
constexpr float g_minZoom = 0.25f;
constexpr float g_maxZoom = 2.f;
constexpr float g_fieldZoom = 0.55f;
constexpr float g_labelZoom = 0.4f;
constexpr float g_snap = 8.f;
constexpr float g_dragThreshold = 4.f;

using PinKey = std::tuple<uint64_t, std::string, bool>;

auto rgb(const float iRed, const float iGreen, const float iBlue) -> math::vec4 { return {iRed, iGreen, iBlue, 1.f}; }

auto toU32(const math::vec4& iColor, const float iAlpha = 1.f) -> ImU32 {
	return ImGui::ColorConvertFloat4ToU32({iColor.x(), iColor.y(), iColor.z(), iColor.w() * iAlpha});
}

auto scaled(const math::vec4& iColor, const float iFactor) -> math::vec4 {
	return {iColor.x() * iFactor, iColor.y() * iFactor, iColor.z() * iFactor, iColor.w()};
}

auto lighter(const math::vec4& iColor, const float iAmount) -> math::vec4 {
	return {iColor.x() + (1.f - iColor.x()) * iAmount, iColor.y() + (1.f - iColor.y()) * iAmount,
			iColor.z() + (1.f - iColor.z()) * iAmount, iColor.w()};
}

auto valueOf(const vs::GraphNode& iNode, const std::string& iKey) -> std::string {
	const auto it = iNode.values.find(iKey);
	return it == iNode.values.end() ? std::string{} : it->second;
}

// `get_position` reads `Get Position`.
auto humanize(const std::string_view iName) -> std::string {
	std::string out;
	bool upper = true;
	for (const char c: iName) {
		if (c == '_') {
			out += ' ';
			upper = true;
			continue;
		}
		out += upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
		upper = false;
	}
	return out;
}

auto isDefaultExec(const vs::PinSpec& iPin) -> bool {
	return iPin.type == vs::PinType::Exec && (iPin.name == "Exec" || iPin.name == "Then");
}

// Width of the inline value field of an unlinked input, 0 for a type typed nowhere.
auto fieldWidth(const vs::PinType iType) -> float {
	switch (iType) {
		case vs::PinType::Boolean:
			return 18.f;
		case vs::PinType::Integer:
		case vs::PinType::Number:
		case vs::PinType::Entity:
			return 56.f;
		case vs::PinType::String:
			return 96.f;
		case vs::PinType::Color:
			return 36.f;
		case vs::PinType::Vector2:
			return 104.f;
		case vs::PinType::Vector3:
			return 156.f;
		case vs::PinType::Any:
			return 64.f;
		case vs::PinType::Exec:
		case vs::PinType::Table:
			break;
	}
	return 0.f;
}

auto parseFloats(const std::string& iText, std::span<float> oValues) -> bool {
	size_t start = 0;
	for (auto& value: oValues) {
		if (start > iText.size())
			return false;
		const auto end = std::min(iText.find(',', start), iText.size());
		auto part = iText.substr(start, end - start);
		std::erase(part, ' ');
		const auto* last = part.c_str() + part.size();
		if (const auto [ptr, ec] = std::from_chars(part.c_str(), last, value); ec != std::errc{} || ptr != last)
			return false;
		start = end + 1;
	}
	return true;
}

auto joinFloats(const std::span<const float> iValues) -> std::string {
	std::string out;
	for (const float value: iValues) {
		if (!out.empty())
			out += ", ";
		out += std::format("{:.3g}", value);
	}
	return out;
}

auto componentCount(const vs::PinSpec& iPin) -> size_t {
	const auto all = vs::getPinComponents(iPin.type).size();
	return iPin.group == iPin.name && !iPin.components.empty() ? std::min(iPin.components.size(), all) : all;
}

// Inline value field of an unlinked input, at the cursor; returns the new text when it changed.
auto editField(const vs::PinSpec& iPin, const std::string& iValue) -> std::optional<std::string> {
	std::optional<std::string> edited;
	switch (iPin.type) {
		case vs::PinType::Boolean:
			{
				bool checked = (iValue.empty() ? iPin.defaultValue : iValue) == "true";
				if (ImGui::Checkbox("##v", &checked))
					edited = checked ? "true" : "false";
			}
			break;
		case vs::PinType::Color:
			{
				std::array<float, 4> rgba{1.f, 1.f, 1.f, 1.f};
				const auto count = componentCount(iPin);
				if (!iValue.empty())
					std::ignore = parseFloats(iValue, std::span{rgba}.first(count));
				const auto flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
								   ImGuiColorEditFlags_AlphaPreviewHalf |
								   (count < 4 ? ImGuiColorEditFlags_NoAlpha : ImGuiColorEditFlags_None);
				if (ImGui::ColorEdit4("##v", rgba.data(), flags))
					edited = joinFloats(std::span{rgba}.first(count));
			}
			break;
		case vs::PinType::Vector2:
		case vs::PinType::Vector3:
			{
				std::array<float, 3> xyz{0.f, 0.f, 0.f};
				const auto count = std::min<size_t>(componentCount(iPin), 3);
				if (!iValue.empty())
					std::ignore = parseFloats(iValue, std::span{xyz}.first(count));
				const bool changed = count == 2 ? ImGui::DragFloat2("##v", xyz.data(), 0.05f, 0.f, 0.f, "%.2f")
												: ImGui::DragFloat3("##v", xyz.data(), 0.05f, 0.f, 0.f, "%.2f");
				if (changed)
					edited = joinFloats(std::span{xyz}.first(count));
			}
			break;
		case vs::PinType::Integer:
		case vs::PinType::Number:
		case vs::PinType::String:
		case vs::PinType::Entity:
		case vs::PinType::Any:
			{
				auto text = iValue;
				const auto hint = iPin.type == vs::PinType::Entity ? std::string{"self"} : iPin.defaultValue;
				if (ImGui::InputTextWithHint("##v", hint.c_str(), &text))
					edited = text;
			}
			break;
		case vs::PinType::Exec:
		case vs::PinType::Table:
			break;
	}
	if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
		ImGui::SetTooltip("%s: %s%s", iPin.name.c_str(), std::string{getPinTypeName(iPin.type)}.c_str(),
						  iPin.optional ? ", optional" : "");
	return edited;
}

auto toVec(const ImVec2& iPoint) -> math::vec2f { return {iPoint.x, iPoint.y}; }

auto measureWithImGui(const std::string_view iText) -> float {
	return ImGui::CalcTextSize(iText.data(), iText.data() + iText.size()).x;
}

}// namespace

auto getNodeGlyph(const vs::NodeType& iType) -> const char* {
	static const std::unordered_map<std::string_view, const char*> tables{
			{"transform", gui::icon::move3d},   {"physics", gui::icon::atom},       {"input", gui::icon::keyboard},
			{"sound", gui::icon::volume2},      {"scene", gui::icon::clapperboard}, {"time", gui::icon::timer},
			{"log", gui::icon::scrollText},     {"entity", gui::icon::box},         {"ui", gui::icon::layoutDashboard},
			{"gamestate", gui::icon::flag},     {"save", gui::icon::save},          {"settings", gui::icon::settings},
			{"trigger", gui::icon::scan},       {"door", gui::icon::doorOpen},      {"pushwall", gui::icon::brickWall},
			{"isometric", gui::icon::grid3x3},  {"path", gui::icon::route},         {"engine", gui::icon::settings2},
			{"Math", gui::icon::calculator},    {"Compare", gui::icon::equal},      {"Logic", gui::icon::binary},
			{"Text", gui::icon::type},          {"Vector", gui::icon::axis3d},      {"Color", gui::icon::palette},
			{"Variables", gui::icon::variable}, {"Events", gui::icon::zap}};
	switch (iType.kind) {
		case vs::NodeKind::Branch:
			return gui::icon::gitBranch;
		case vs::NodeKind::Sequence:
			return gui::icon::listOrdered;
		case vs::NodeKind::ForLoop:
			return gui::icon::repeat;
		case vs::NodeKind::FunctionEntry:
			return gui::icon::logOut;
		case vs::NodeKind::FunctionReturn:
			return gui::icon::undo2;
		case vs::NodeKind::FunctionCall:
			return gui::icon::component;
		case vs::NodeKind::Event:
		case vs::NodeKind::CustomEvent:
		case vs::NodeKind::Call:
		case vs::NodeKind::Operator:
		case vs::NodeKind::GetVariable:
		case vs::NodeKind::SetVariable:
			break;
	}
	if (iType.id == "value.self")
		return gui::icon::userRound;
	if (iType.id == "value.color")
		return gui::icon::palette;
	if (iType.id == "value.vector3")
		return gui::icon::axis3d;
	if (iType.category == "Constant")
		return gui::icon::braces;
	if (const auto it = tables.find(iType.category); it != tables.end())
		return it->second;
	return gui::icon::squareFunction;
}

auto getPinGlyph(const vs::PinType iType) -> const char* {
	switch (iType) {
		case vs::PinType::Exec:
			return gui::icon::play;
		case vs::PinType::Boolean:
			return gui::icon::toggleLeft;
		case vs::PinType::Integer:
			return gui::icon::hash;
		case vs::PinType::Number:
			return gui::icon::sigma;
		case vs::PinType::String:
			return gui::icon::type;
		case vs::PinType::Entity:
			return gui::icon::box;
		case vs::PinType::Vector2:
			return gui::icon::move;
		case vs::PinType::Vector3:
			return gui::icon::axis3d;
		case vs::PinType::Color:
			return gui::icon::palette;
		case vs::PinType::Table:
			return gui::icon::braces;
		case vs::PinType::Any:
			break;
	}
	return gui::icon::circle;
}

auto getPinTypeIcon(const vs::PinType iType) -> const char* {
	switch (iType) {
		case vs::PinType::Boolean:
			return "vs_type_bool";
		case vs::PinType::Integer:
			return "vs_type_int";
		case vs::PinType::Number:
			return "vs_type_float";
		case vs::PinType::String:
			return "vs_type_string";
		case vs::PinType::Entity:
			return "vs_type_entity";
		case vs::PinType::Vector2:
			return "vs_type_vec2";
		case vs::PinType::Vector3:
			return "vs_type_vec3";
		case vs::PinType::Color:
			return "vs_type_color";
		case vs::PinType::Exec:
		case vs::PinType::Table:
		case vs::PinType::Any:
			break;
	}
	return "vs_node_constant";
}

auto getNodeIcon(const vs::NodeType& iType) -> const char* {
	switch (iType.kind) {
		case vs::NodeKind::Event:
		case vs::NodeKind::CustomEvent:
			return "vs_node_event";
		case vs::NodeKind::Branch:
		case vs::NodeKind::Sequence:
		case vs::NodeKind::ForLoop:
			return "vs_node_flow";
		case vs::NodeKind::GetVariable:
		case vs::NodeKind::SetVariable:
			return "vs_node_variable";
		case vs::NodeKind::Call:
		case vs::NodeKind::FunctionCall:
			return "vs_node_call";
		case vs::NodeKind::FunctionEntry:
		case vs::NodeKind::FunctionReturn:
			return "vs_node_flow";
		case vs::NodeKind::Operator:
			break;
	}
	return iType.category == "Constant" ? "vs_node_constant" : "vs_node_math";
}

auto getPinColor(const vs::PinType iType) -> math::vec4 {
	switch (iType) {
		case vs::PinType::Exec:
			return rgb(0.95f, 0.95f, 0.95f);
		case vs::PinType::Boolean:
			return rgb(0.85f, 0.2f, 0.2f);
		case vs::PinType::Integer:
			return rgb(0.12f, 0.87f, 0.62f);
		case vs::PinType::Number:
			return rgb(0.62f, 0.93f, 0.25f);
		case vs::PinType::String:
			return rgb(0.98f, 0.27f, 0.74f);
		case vs::PinType::Entity:
			return rgb(0.2f, 0.65f, 1.f);
		case vs::PinType::Vector2:
			return rgb(0.95f, 0.6f, 0.25f);
		case vs::PinType::Vector3:
			return rgb(0.99f, 0.78f, 0.15f);
		case vs::PinType::Color:
			return rgb(0.35f, 0.45f, 1.f);
		case vs::PinType::Table:
			return rgb(0.62f, 0.48f, 0.9f);
		case vs::PinType::Any:
			break;
	}
	return rgb(0.62f, 0.64f, 0.68f);
}

auto getPinTypeName(const vs::PinType iType) -> std::string_view {
	switch (iType) {
		case vs::PinType::Exec:
			return "execution";
		case vs::PinType::Boolean:
			return "boolean";
		case vs::PinType::Integer:
			return "integer";
		case vs::PinType::Number:
			return "number";
		case vs::PinType::String:
			return "string";
		case vs::PinType::Table:
			return "table";
		case vs::PinType::Entity:
			return "entity";
		case vs::PinType::Vector2:
			return "vector 2";
		case vs::PinType::Vector3:
			return "vector 3";
		case vs::PinType::Color:
			return "color";
		case vs::PinType::Any:
			break;
	}
	return "any";
}

auto getNodeColor(const vs::VisualScript& iScript, const vs::GraphNode& iNode, const vs::NodeType& iType)
		-> math::vec4 {
	switch (iType.kind) {
		case vs::NodeKind::Event:
		case vs::NodeKind::CustomEvent:
			return rgb(0.6f, 0.14f, 0.14f);
		case vs::NodeKind::Call:
			return iType.pure ? rgb(0.17f, 0.45f, 0.26f) : rgb(0.15f, 0.33f, 0.6f);
		case vs::NodeKind::GetVariable:
		case vs::NodeKind::SetVariable:
			{
				const auto pins = vs::getNodePins(iScript, iNode);
				const auto value = std::ranges::find_if(
						pins.outputs, [](const vs::PinSpec& iPin) -> bool { return iPin.type != vs::PinType::Exec; });
				return scaled(getPinColor(value == pins.outputs.end() ? vs::PinType::Any : value->type), 0.5f);
			}
		case vs::NodeKind::Operator:
			if (iType.category == "Constant" && !iType.outputs.empty())
				return scaled(getPinColor(iType.outputs.front().type), 0.5f);
			return rgb(0.16f, 0.42f, 0.4f);
		case vs::NodeKind::FunctionEntry:
		case vs::NodeKind::FunctionReturn:
		case vs::NodeKind::FunctionCall:
			return rgb(0.42f, 0.24f, 0.62f);
		case vs::NodeKind::Branch:
		case vs::NodeKind::Sequence:
		case vs::NodeKind::ForLoop:
			break;
	}
	return rgb(0.3f, 0.31f, 0.35f);
}

auto getNodeTitle(const vs::GraphNode& iNode, const vs::NodeType& iType) -> std::string {
	switch (iType.kind) {
		case vs::NodeKind::GetVariable:
			return std::format("Get {}", valueOf(iNode, "variable"));
		case vs::NodeKind::SetVariable:
			return std::format("Set {}", valueOf(iNode, "variable"));
		case vs::NodeKind::CustomEvent:
		case vs::NodeKind::FunctionEntry:
		case vs::NodeKind::FunctionCall:
			return valueOf(iNode, "function");
		case vs::NodeKind::FunctionReturn:
			return iNode.values.contains("exit") ? std::format("Exit {}", valueOf(iNode, "exit")) : "Return";
		case vs::NodeKind::Call:
			{
				const auto dot = iType.title.find('.');
				return humanize(dot == std::string::npos ? iType.title : iType.title.substr(dot + 1));
			}
		case vs::NodeKind::Event:
		case vs::NodeKind::Operator:
		case vs::NodeKind::Branch:
		case vs::NodeKind::Sequence:
		case vs::NodeKind::ForLoop:
			break;
	}
	return iType.title;
}

auto getNodeSubtitle(const vs::NodeType& iType) -> std::string {
	switch (iType.kind) {
		case vs::NodeKind::Call:
			return iType.category;
		case vs::NodeKind::CustomEvent:
			return "Custom Event";
		case vs::NodeKind::GetVariable:
		case vs::NodeKind::SetVariable:
			return "Variable";
		case vs::NodeKind::FunctionEntry:
			return "Function Entry";
		case vs::NodeKind::FunctionCall:
			return "Function";
		case vs::NodeKind::FunctionReturn:
		case vs::NodeKind::Event:
		case vs::NodeKind::Operator:
		case vs::NodeKind::Branch:
		case vs::NodeKind::Sequence:
		case vs::NodeKind::ForLoop:
			break;
	}
	return {};
}

auto layoutNode(const vs::VisualScript& iScript, const vs::GraphNode& iNode, const TextMeasure& iMeasure)
		-> VisualScriptNodeLayout {
	VisualScriptNodeLayout layout;
	const auto* type = vs::findNodeType(iNode.type);
	if (type == nullptr) {
		layout.size = {g_minWidth, g_headerHeight + g_bodyTop + g_bodyBottom};
		layout.headerHeight = g_headerHeight;
		return layout;
	}
	const auto pins = vs::getNodePins(iScript, iNode);
	std::set<std::string> linked;
	for (const auto& link: iScript.links)
		if (link.toNode == iNode.id)
			linked.insert(link.toPin);
	float inputWidth = 0.f;
	for (const auto& pin: pins.inputs) {
		float width = pin.field ? -g_fieldGap : g_pinSpace;
		if (!isDefaultExec(pin) && !pin.field)
			width += iMeasure(pin.name);
		float field = 0.f;
		if (pin.type != vs::PinType::Exec && !linked.contains(pin.name))
			field = fieldWidth(pin.type);
		layout.fields.push_back({g_padding + width + g_fieldGap, field});
		if (field > 0.f)
			width += g_fieldGap + field;
		inputWidth = std::max(inputWidth, width);
	}
	float outputWidth = 0.f;
	for (const auto& pin: pins.outputs)
		outputWidth = std::max(outputWidth, g_pinSpace + (isDefaultExec(pin) ? 0.f : iMeasure(pin.name)));
	const auto subtitle = getNodeSubtitle(*type);
	const float titleWidth = g_glyphWidth + std::max(iMeasure(getNodeTitle(iNode, *type)), iMeasure(subtitle) * 0.8f);
	const float headerHeight = subtitle.empty() ? g_headerHeight : g_headerHeight + 10.f;
	const float body = inputWidth + outputWidth + (inputWidth > 0.f && outputWidth > 0.f ? g_columnGap : 0.f);
	const float width = std::max({g_minWidth, body + 2.f * g_padding, titleWidth + 2.f * g_padding});
	const auto rows = std::max(pins.inputs.size(), pins.outputs.size());
	layout.headerHeight = headerHeight;
	layout.size = {width, headerHeight + g_bodyTop + static_cast<float>(rows) * g_rowHeight + g_bodyBottom};
	const auto rowCenter = [&](const size_t iRow) -> float {
		return headerHeight + g_bodyTop + (static_cast<float>(iRow) + 0.5f) * g_rowHeight;
	};
	for (size_t i = 0; i < pins.inputs.size(); ++i) layout.inputs.push_back({g_padding + g_pinRadius, rowCenter(i)});
	for (size_t i = 0; i < pins.outputs.size(); ++i)
		layout.outputs.push_back({width - g_padding - g_pinRadius * 1.6f, rowCenter(i)});
	return layout;
}

// What one frame computes once: where each node and pin is on the screen, what the mouse is over.
struct VisualScriptCanvas::Frame {
	struct NodeView {
		const vs::GraphNode* node{nullptr};
		const vs::NodeType* type{nullptr};
		vs::NodePins pins;
		VisualScriptNodeLayout layout;
		ImVec2 min;
		ImVec2 max;
		bool selected{false};
		std::string error;
	};

	const vs::VisualScript& script;
	ImDrawList* draw{nullptr};
	ImVec2 origin;
	ImVec2 size;
	float zoom{1.f};
	ImVec2 mouse;
	std::vector<NodeView> nodes;
	std::map<PinKey, std::pair<ImVec2, vs::PinType>> pins;
	std::set<PinKey> linked;
	std::optional<VisualScriptPinRef> hoveredPin;
	std::optional<size_t> hoveredNode;
	std::optional<size_t> hoveredLink;
	bool hovered{false};
	Result result;

	[[nodiscard]] auto toScreen(const float iX, const float iY, const std::array<float, 2>& iOffset) const -> ImVec2 {
		return {origin.x + (iX + iOffset[0]) * zoom, origin.y + (iY + iOffset[1]) * zoom};
	}
};

VisualScriptCanvas::VisualScriptCanvas() = default;

VisualScriptCanvas::~VisualScriptCanvas() = default;

auto VisualScriptCanvas::getViewCenter() const -> std::array<float, 2> {
	return {m_viewSize[0] * 0.5f / m_zoom - m_offset[0], m_viewSize[1] * 0.5f / m_zoom - m_offset[1]};
}

auto VisualScriptCanvas::render(const vs::VisualScript& iScript,
								const std::span<const vs::CompileDiagnostic> iDiagnostics) -> Result {
	ImGui::PushStyleColor(ImGuiCol_ChildBg, gui::vec(gui::widgets::getNodeCanvasBackground()));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
	const bool visible = ImGui::BeginChild("##vsView", {0.f, 0.f}, ImGuiChildFlags_None,
										   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
												   ImGuiWindowFlags_NoMove);
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	Frame frame{.script = iScript,
				.draw = ImGui::GetWindowDrawList(),
				.origin = ImGui::GetWindowPos(),
				.size = ImGui::GetWindowSize(),
				.zoom = m_zoom,
				.mouse = ImGui::GetMousePos(),
				.nodes = {},
				.pins = {},
				.linked = {},
				.hoveredPin = {},
				.hoveredNode = {},
				.hoveredLink = {},
				.hovered = false,
				.result = {}};
	if (!visible) {
		ImGui::EndChild();
		return {};
	}
	m_viewSize = {frame.size.x, frame.size.y};
	frame.hovered = ImGui::IsWindowHovered() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
	std::erase_if(m_selection, [&](const core::UUID iId) -> bool { return iScript.findNode(iId) == nullptr; });
	applyViewRequests(frame);
	frame.zoom = m_zoom;
	drawGrid(frame);

	std::unordered_map<uint64_t, std::string> errors;
	for (const auto& diagnostic: iDiagnostics) errors.try_emplace(diagnostic.node, diagnostic.message);
	for (const auto& link: iScript.links) {
		frame.linked.insert({link.fromNode, link.fromPin, true});
		frame.linked.insert({link.toNode, link.toPin, false});
	}
	const float pickRadius = std::max(8.f, g_pinRadius * m_zoom * 1.8f);
	float bestPin = pickRadius * pickRadius;
	for (const auto& node: iScript.nodes) {
		Frame::NodeView view{.node = &node,
							 .type = vs::findNodeType(node.type),
							 .pins = vs::getNodePins(iScript, node),
							 .layout = layoutNode(iScript, node, measureWithImGui),
							 .min = {},
							 .max = {},
							 .selected = std::ranges::find(m_selection, node.id) != m_selection.end(),
							 .error = {}};
		if (const auto it = errors.find(node.id); it != errors.end())
			view.error = it->second;
		const bool moved = m_gesture == Gesture::Move && view.selected;
		const std::array offset{m_offset[0] + (moved ? m_moveOffset[0] : 0.f),
								m_offset[1] + (moved ? m_moveOffset[1] : 0.f)};
		view.min = frame.toScreen(node.position[0], node.position[1], offset);
		view.max =
				frame.toScreen(node.position[0] + view.layout.size[0], node.position[1] + view.layout.size[1], offset);
		const auto place = [&](const std::vector<vs::PinSpec>& iSpecs, const std::vector<std::array<float, 2>>& iAt,
							   const bool iOutput) -> void {
			for (size_t i = 0; i < iSpecs.size() && i < iAt.size(); ++i) {
				if (iSpecs[i].field)
					continue;
				const ImVec2 at{view.min.x + iAt[i][0] * m_zoom, view.min.y + iAt[i][1] * m_zoom};
				frame.pins[{node.id, iSpecs[i].name, iOutput}] = {at, iSpecs[i].type};
				const float dx = frame.mouse.x - at.x;
				const float dy = frame.mouse.y - at.y;
				if (const float distance = dx * dx + dy * dy; distance < bestPin) {
					bestPin = distance;
					frame.hoveredPin = VisualScriptPinRef{.node = node.id,
														  .pin = iSpecs[i].name,
														  .output = iOutput,
														  .type = iSpecs[i].type};
				}
			}
		};
		place(view.pins.inputs, view.layout.inputs, false);
		place(view.pins.outputs, view.layout.outputs, true);
		frame.nodes.push_back(std::move(view));
	}
	for (size_t i = frame.nodes.size(); i-- > 0;) {
		const auto& view = frame.nodes[i];
		if (frame.mouse.x >= view.min.x && frame.mouse.x <= view.max.x && frame.mouse.y >= view.min.y &&
			frame.mouse.y <= view.max.y) {
			frame.hoveredNode = i;
			break;
		}
	}
	drawLinks(frame);
	drawNodes(frame);
	handleMouse(frame);

	if (frame.hovered && frame.hoveredLink.has_value() && !frame.hoveredPin.has_value() &&
		ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
		auto next = iScript;
		next.links.erase(next.links.begin() + static_cast<std::ptrdiff_t>(*frame.hoveredLink));
		frame.result.edit = VisualScriptEdit{.script = std::move(next), .description = "Remove Link", .merge = false};
		frame.result.deleteUsed = true;
	}
	if (ImGui::BeginDragDropTargetCustom(
				ImRect{frame.origin, {frame.origin.x + frame.size.x, frame.origin.y + frame.size.y}},
				ImGui::GetID("##vsDrop"))) {
		if (const auto* payload = ImGui::AcceptDragDropPayload(g_VisualScriptNodePayload);
			payload != nullptr && payload->DataSize > 0)
			frame.result.droppedType =
					std::string{static_cast<const char*>(payload->Data), static_cast<size_t>(payload->DataSize)};
		ImGui::EndDragDropTarget();
	}
	frame.result.mouse = {(frame.mouse.x - frame.origin.x) / m_zoom - m_offset[0],
						  (frame.mouse.y - frame.origin.y) / m_zoom - m_offset[1]};
	ImGui::EndChild();
	return std::move(frame.result);
}

void VisualScriptCanvas::applyViewRequests(const Frame& iFrame) {
	if (iFrame.size.x < 1.f || iFrame.size.y < 1.f)
		return;
	const auto boundsOf = [&](const vs::GraphNode& iNode) -> std::array<float, 4> {
		const auto layout = layoutNode(iFrame.script, iNode, measureWithImGui);
		return {iNode.position[0], iNode.position[1], iNode.position[0] + layout.size[0],
				iNode.position[1] + layout.size[1]};
	};
	if (m_focus.has_value()) {
		if (const auto* node = iFrame.script.findNode(*m_focus); node != nullptr) {
			const auto box = boundsOf(*node);
			m_offset = {iFrame.size.x * 0.5f / m_zoom - (box[0] + box[2]) * 0.5f,
						iFrame.size.y * 0.5f / m_zoom - (box[1] + box[3]) * 0.5f};
		}
		m_focus.reset();
	}
	if (!m_frameAll)
		return;
	m_frameAll = false;
	if (iFrame.script.nodes.empty())
		return;
	std::array bounds{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
					  std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
	for (const auto& node: iFrame.script.nodes) {
		const auto box = boundsOf(node);
		bounds = {std::min(bounds[0], box[0]), std::min(bounds[1], box[1]), std::max(bounds[2], box[2]),
				  std::max(bounds[3], box[3])};
	}
	constexpr float margin = 60.f;
	const float width = bounds[2] - bounds[0] + 2.f * margin;
	const float height = bounds[3] - bounds[1] + 2.f * margin;
	m_zoom = std::clamp(std::min(iFrame.size.x / width, iFrame.size.y / height), 0.4f, 1.f);
	m_offset = {iFrame.size.x * 0.5f / m_zoom - (bounds[0] + bounds[2]) * 0.5f,
				iFrame.size.y * 0.5f / m_zoom - (bounds[1] + bounds[3]) * 0.5f};
}

void VisualScriptCanvas::drawGrid(const Frame& iFrame) const {
	gui::widgets::drawNodeGrid(iFrame.draw, toVec(iFrame.origin), toVec(iFrame.size), m_offset, m_zoom);
}

void VisualScriptCanvas::drawLinks(Frame& ioFrame) const {
	const float tolerance = 6.f;
	float best = tolerance;
	for (size_t i = 0; i < ioFrame.script.links.size(); ++i) {
		const auto& link = ioFrame.script.links[i];
		const auto from = ioFrame.pins.find({link.fromNode, link.fromPin, true});
		const auto to = ioFrame.pins.find({link.toNode, link.toPin, false});
		if (from == ioFrame.pins.end() || to == ioFrame.pins.end())
			continue;
		if (ioFrame.hovered && !ioFrame.hoveredPin.has_value() && !ioFrame.hoveredNode.has_value()) {
			if (const float distance = gui::widgets::getNodeLinkDistance(
						toVec(from->second.first), toVec(to->second.first), toVec(ioFrame.mouse), m_zoom);
				distance < best) {
				best = distance;
				ioFrame.hoveredLink = i;
			}
		}
	}
	for (size_t i = 0; i < ioFrame.script.links.size(); ++i) {
		const auto& link = ioFrame.script.links[i];
		const auto from = ioFrame.pins.find({link.fromNode, link.fromPin, true});
		const auto to = ioFrame.pins.find({link.toNode, link.toPin, false});
		if (from == ioFrame.pins.end() || to == ioFrame.pins.end())
			continue;
		const bool exec = from->second.second == vs::PinType::Exec;
		const bool highlighted =
				ioFrame.hoveredLink == i || std::ranges::any_of(m_selection, [&](const core::UUID iId) -> bool {
					return iId == link.fromNode || iId == link.toNode;
				});
		const float thickness = (exec ? 3.f : 2.4f) * m_zoom * (highlighted ? 1.5f : 1.f);
		auto color = getPinColor(from->second.second);
		if (highlighted)
			color = lighter(color, 0.35f);
		gui::widgets::drawNodeLink(ioFrame.draw, toVec(from->second.first), toVec(to->second.first), color, thickness,
								   m_zoom);
		if (exec || m_debug.values.empty())
			continue;
		if (const auto value = m_debug.values.find(vs::getProbeKey(link.fromNode, link.fromPin));
			value != m_debug.values.end())
			drawWireValue(ioFrame, toVec(from->second.first), toVec(to->second.first), value->second);
	}
}

void VisualScriptCanvas::drawWireValue(const Frame& iFrame, const math::vec2f& iFrom, const math::vec2f& iTo,
									   const std::string& iValue) const {
	if (m_zoom < 0.4f)
		return;
	const auto curve = gui::widgets::getNodeLinkCurve(iFrom, iTo, m_zoom);
	const ImVec2 middle{(curve[0].x() + 3.f * curve[1].x() + 3.f * curve[2].x() + curve[3].x()) / 8.f,
						(curve[0].y() + 3.f * curve[1].y() + 3.f * curve[2].y() + curve[3].y()) / 8.f};
	const std::string text = iValue.size() > 18 ? iValue.substr(0, 17) + "…" : iValue;
	const float fontSize = ImGui::GetFontSize() * std::min(m_zoom, 1.2f) * 0.85f;
	const ImVec2 size = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text.c_str());
	const ImVec2 pad{5.f * m_zoom, 2.f * m_zoom};
	const ImVec2 min{middle.x - size.x * 0.5f - pad.x, middle.y - size.y * 0.5f - pad.y};
	const ImVec2 max{middle.x + size.x * 0.5f + pad.x, middle.y + size.y * 0.5f + pad.y};
	iFrame.draw->AddRectFilled(min, max, IM_COL32(24, 26, 31, 230), size.y);
	iFrame.draw->AddRect(min, max, IM_COL32(240, 190, 70, 200), size.y, 0, 1.f);
	iFrame.draw->AddText(ImGui::GetFont(), fontSize, {min.x + pad.x, min.y + pad.y}, IM_COL32(240, 220, 160, 255),
						 text.c_str());
}

void VisualScriptCanvas::drawNodes(Frame& ioFrame) const {
	for (size_t index = 0; index < ioFrame.nodes.size(); ++index) {
		const auto& view = ioFrame.nodes[index];
		if (view.max.x < ioFrame.origin.x || view.min.x > ioFrame.origin.x + ioFrame.size.x ||
			view.max.y < ioFrame.origin.y || view.min.y > ioFrame.origin.y + ioFrame.size.y)
			continue;
		drawNodeFrame(ioFrame, index);
		drawNodePins(ioFrame, index);
	}
	drawOverlays(ioFrame);
}

void VisualScriptCanvas::drawNodeFrame(const Frame& iFrame, const size_t iIndex) const {
	auto* draw = iFrame.draw;
	const float zoom = m_zoom;
	ImFont* font = ImGui::GetFont();
	const float fontSize = ImGui::GetFontSize() * zoom;
	const auto& view = iFrame.nodes[iIndex];
	const auto& node = *view.node;
	const ImVec2 min = view.min;
	const ImVec2 max = view.max;
	gui::UiLayer::pushTextStyle(gui::TextStyle::Strong);
	ImFont* strong = ImGui::GetFont();
	gui::UiLayer::popTextStyle();
	const auto& theme = gui::UiLayer::getTheme();
	const float header = view.layout.headerHeight * zoom;
	const bool faulty = !view.error.empty() || view.type == nullptr;
	gui::widgets::drawNodeFrame(
			draw, toVec(min), toVec(max),
			{.header = view.type == nullptr ? rgb(0.4f, 0.1f, 0.1f) : getNodeColor(iFrame.script, node, *view.type),
			 .headerHeight = view.layout.headerHeight,
			 .selected = view.selected,
			 .hovered = iFrame.hoveredNode == iIndex,
			 .faulty = faulty},
			zoom);
	if (m_debug.stopped == node.id) {
		const float grow = 4.f * zoom;
		draw->AddRect({min.x - grow, min.y - grow}, {max.x + grow, max.y + grow}, IM_COL32(240, 190, 70, 255),
					  10.f * zoom, 0, 3.f * zoom);
	}
	if (m_debug.breakpoints.contains(node.id)) {
		const float radius = 6.f * zoom;
		const ImVec2 center{min.x + radius * 0.4f, min.y + radius * 0.4f};
		draw->AddCircleFilled(center, radius, IM_COL32(229, 72, 77, 255));
		draw->AddCircle(center, radius, IM_COL32(255, 255, 255, 200), 0, 1.5f * zoom);
	}

	const auto title = view.type == nullptr ? std::format("Unknown {}", node.type) : getNodeTitle(node, *view.type);
	const auto subtitle = view.type == nullptr ? std::string{} : getNodeSubtitle(*view.type);
	if (zoom > 0.3f) {
		const float textX = min.x + g_padding * zoom;
		const float titleY = min.y + (subtitle.empty() ? (header - fontSize) * 0.5f : 5.f * zoom);
		const char* glyph = view.type == nullptr ? gui::icon::triangleAlert : getNodeGlyph(*view.type);
		draw->AddText(font, fontSize, {textX, titleY}, IM_COL32(255, 255, 255, 230), glyph);
		draw->AddText(strong, fontSize, {textX + g_glyphWidth * zoom, titleY}, IM_COL32(255, 255, 255, 255),
					  title.c_str());
		if (!subtitle.empty())
			draw->AddText(font, fontSize * 0.8f, {textX + g_glyphWidth * zoom, titleY + fontSize},
						  IM_COL32(255, 255, 255, 150), subtitle.c_str());
		if (faulty)
			draw->AddText(font, fontSize, {max.x - (g_padding + 14.f) * zoom, titleY}, toU32(theme.error),
						  gui::icon::triangleAlert);
	}
}

void VisualScriptCanvas::drawNodePins(Frame& ioFrame, const size_t iIndex) const {
	auto* draw = ioFrame.draw;
	const float zoom = m_zoom;
	ImFont* font = ImGui::GetFont();
	const float fontSize = ImGui::GetFontSize() * zoom;
	const auto& view = ioFrame.nodes[iIndex];
	const auto& node = *view.node;
	const ImVec2 min = view.min;
	const float baseSize = ImGui::GetFontSize();
	const bool labels = zoom > g_labelZoom;
	const bool fields = zoom >= g_fieldZoom;
	const auto drawPin = [&](const vs::PinSpec& iPin, const ImVec2& iAt, const bool iOutput) -> void {
		if (iPin.field)
			return;
		const bool linked = ioFrame.linked.contains({node.id, iPin.name, iOutput});
		const bool isHovered = ioFrame.hoveredPin.has_value() && ioFrame.hoveredPin->node == node.id &&
							   ioFrame.hoveredPin->pin == iPin.name && ioFrame.hoveredPin->output == iOutput;
		auto pinColor = getPinColor(iPin.type);
		if (isHovered) {
			draw->AddCircleFilled(iAt, g_pinRadius * 2.2f * zoom, toU32(pinColor, 0.25f));
			pinColor = lighter(pinColor, 0.3f);
		}
		if (iPin.type == vs::PinType::Exec)
			gui::widgets::drawExecPin(draw, toVec(iAt), g_pinRadius * 1.15f * zoom, pinColor, linked);
		else
			gui::widgets::drawDataPin(draw, toVec(iAt), g_pinRadius * zoom, pinColor, linked);
		if (!labels || isDefaultExec(iPin))
			return;
		const auto size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, iPin.name.c_str());
		const float x = iOutput ? iAt.x - (g_pinRadius + 8.f) * zoom - size.x : iAt.x + (g_pinRadius + 8.f) * zoom;
		draw->AddText(font, fontSize, {x, iAt.y - size.y * 0.5f}, IM_COL32(225, 228, 232, 255), iPin.name.c_str());
	};
	for (size_t i = 0; i < view.pins.outputs.size() && i < view.layout.outputs.size(); ++i)
		drawPin(view.pins.outputs[i],
				{min.x + view.layout.outputs[i][0] * zoom, min.y + view.layout.outputs[i][1] * zoom}, true);
	for (size_t i = 0; i < view.pins.inputs.size() && i < view.layout.inputs.size(); ++i) {
		const auto& pin = view.pins.inputs[i];
		const ImVec2 at{min.x + view.layout.inputs[i][0] * zoom, min.y + view.layout.inputs[i][1] * zoom};
		drawPin(pin, at, false);
		const auto [fieldX, fieldW] = view.layout.fields[i];
		if (fieldW <= 0.f)
			continue;
		const auto value = valueOf(node, pin.name);
		const ImVec2 fieldAt{min.x + fieldX * zoom, at.y};
		if (!fields) {
			if (labels && !value.empty())
				draw->AddText(font, fontSize, {fieldAt.x, at.y - fontSize * 0.5f}, IM_COL32(170, 175, 182, 255),
							  value.c_str());
			continue;
		}
		ImGui::PushID(std::format("{}:{}", static_cast<uint64_t>(node.id), pin.name).c_str());
		ImGui::PushFont(nullptr, baseSize * zoom);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.f * zoom, 1.f * zoom});
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f * zoom);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(10, 11, 13, 230));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(40, 43, 50, 255));
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 232, 236, 255));
		ImGui::PushStyleColor(ImGuiCol_TextDisabled, IM_COL32(120, 125, 132, 255));
		const float frameHeight = ImGui::GetFrameHeight();
		ImGui::SetCursorScreenPos({fieldAt.x, at.y - frameHeight * 0.5f});
		ImGui::SetNextItemWidth(fieldW * zoom);
		auto edited = editField(pin, value);
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(2);
		ImGui::PopFont();
		ImGui::PopID();
		if (edited.has_value()) {
			auto next = ioFrame.script;
			if (auto* target = next.findNode(node.id); target != nullptr) {
				if (edited->empty())
					target->values.erase(pin.name);
				else
					target->values[pin.name] = *edited;
			}
			ioFrame.result.edit =
					VisualScriptEdit{.script = std::move(next), .description = "Edit Node Value", .merge = true};
		}
	}
}

void VisualScriptCanvas::drawOverlays(const Frame& iFrame) const {
	auto* draw = iFrame.draw;
	const float zoom = m_zoom;
	const auto& theme = gui::UiLayer::getTheme();
	if (m_gesture == Gesture::Link && m_linkStart.has_value()) {
		if (const auto start = iFrame.pins.find({m_linkStart->node, m_linkStart->pin, m_linkStart->output});
			start != iFrame.pins.end()) {
			auto from = start->second.first;
			auto to = iFrame.mouse;
			if (!m_linkStart->output)
				std::swap(from, to);
			gui::widgets::drawNodeLink(draw, toVec(from), toVec(to), getPinColor(m_linkStart->type),
									   (m_linkStart->type == vs::PinType::Exec ? 3.f : 2.4f) * zoom, zoom);
		}
	}
	if (m_gesture == Gesture::Select) {
		const auto corner = iFrame.toScreen(m_gestureStart[0], m_gestureStart[1], m_offset);
		const ImVec2 lo{std::min(corner.x, iFrame.mouse.x), std::min(corner.y, iFrame.mouse.y)};
		const ImVec2 hi{std::max(corner.x, iFrame.mouse.x), std::max(corner.y, iFrame.mouse.y)};
		draw->AddRectFilled(lo, hi, toU32(theme.accent, 0.1f));
		draw->AddRect(lo, hi, toU32(theme.accent, 0.8f));
	}
	if (iFrame.hovered && iFrame.hoveredNode.has_value() && m_gesture == Gesture::None && !ImGui::IsAnyItemHovered()) {
		const auto& view = iFrame.nodes[*iFrame.hoveredNode];
		const bool overHeader = iFrame.mouse.y < view.min.y + view.layout.headerHeight * zoom;
		if (iFrame.hoveredPin.has_value() && iFrame.hoveredPin->node == view.node->id) {
			ImGui::SetTooltip("%s: %s", iFrame.hoveredPin->pin.c_str(),
							  std::string{getPinTypeName(iFrame.hoveredPin->type)}.c_str());
		} else if (overHeader && view.type != nullptr) {
			if (ImGui::BeginTooltip()) {
				ImGui::TextUnformatted(view.type->title.c_str());
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.f);
				ImGui::TextDisabled("%s", view.type->description.c_str());
				if (!view.error.empty())
					ImGui::TextColored(gui::vec(theme.error), "%s", view.error.c_str());
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}
	}
}

void VisualScriptCanvas::dropLink(Frame& ioFrame, const VisualScriptPinRef& iTarget) const {
	if (!m_linkStart.has_value() || iTarget.output == m_linkStart->output || iTarget.node == m_linkStart->node)
		return;
	const auto& from = m_linkStart->output ? *m_linkStart : iTarget;
	const auto& to = m_linkStart->output ? iTarget : *m_linkStart;
	if (!vs::canConnect(from.type, to.type))
		return;
	auto next = ioFrame.script;
	const bool exec = from.type == vs::PinType::Exec;
	std::erase_if(next.links, [&](const vs::GraphLink& iLink) -> bool {
		if (exec)
			return iLink.fromNode == from.node && iLink.fromPin == from.pin;
		return iLink.toNode == to.node && iLink.toPin == to.pin;
	});
	next.links.push_back({.fromNode = from.node, .fromPin = from.pin, .toNode = to.node, .toPin = to.pin});
	ioFrame.result.edit = VisualScriptEdit{.script = std::move(next), .description = "Add Link", .merge = false};
}

void VisualScriptCanvas::handleMouse(Frame& ioFrame) {
	auto& io = ImGui::GetIO();
	const auto toCanvas = [&](const ImVec2& iScreen) -> std::array<float, 2> {
		return {(iScreen.x - ioFrame.origin.x) / m_zoom - m_offset[0],
				(iScreen.y - ioFrame.origin.y) / m_zoom - m_offset[1]};
	};
	const auto mouse = toCanvas(ioFrame.mouse);
	const bool overWidget = ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive();

	if (ioFrame.hovered && io.MouseWheel != 0.f && !overWidget) {
		m_zoom = std::clamp(m_zoom * std::pow(1.15f, io.MouseWheel), g_minZoom, g_maxZoom);
		m_offset = {(ioFrame.mouse.x - ioFrame.origin.x) / m_zoom - mouse[0],
					(ioFrame.mouse.y - ioFrame.origin.y) / m_zoom - mouse[1]};
	}
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, g_dragThreshold) ||
		ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.f)) {
		if (m_gesture == Gesture::None && ioFrame.hovered)
			m_gesture = Gesture::Pan;
		if (m_gesture == Gesture::Pan) {
			m_offset[0] += io.MouseDelta.x / m_zoom;
			m_offset[1] += io.MouseDelta.y / m_zoom;
		}
	}
	if (m_gesture == Gesture::Pan && !ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
		!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		m_gesture = Gesture::None;
	} else if (m_gesture == Gesture::None && ioFrame.hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
		ioFrame.result.contextMenu = true;
		ioFrame.result.contextPin = ioFrame.hoveredPin;
		if (ioFrame.hoveredNode.has_value()) {
			const auto id = ioFrame.nodes[*ioFrame.hoveredNode].node->id;
			ioFrame.result.contextNode = id;
			if (std::ranges::find(m_selection, id) == m_selection.end())
				m_selection = {id};
		}
	}

	if (ioFrame.hovered && !overWidget && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		pressLeft(ioFrame, mouse);
	if (m_gesture == Gesture::Move && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		m_moveOffset = {mouse[0] - m_gestureStart[0], mouse[1] - m_gestureStart[1]};
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		releaseLeft(ioFrame, mouse);
}

void VisualScriptCanvas::pressLeft(Frame& ioFrame, const std::array<float, 2>& iMouse) {
	const auto& io = ImGui::GetIO();
	m_gestureStart = iMouse;
	m_moveOffset = {0.f, 0.f};
	m_pressedNode.reset();
	if (ioFrame.hoveredPin.has_value()) {
		const auto& pin = *ioFrame.hoveredPin;
		if (io.KeyAlt) {
			auto next = ioFrame.script;
			std::erase_if(next.links, [&](const vs::GraphLink& iLink) -> bool {
				return pin.output ? iLink.fromNode == pin.node && iLink.fromPin == pin.pin
								  : iLink.toNode == pin.node && iLink.toPin == pin.pin;
			});
			ioFrame.result.edit =
					VisualScriptEdit{.script = std::move(next), .description = "Break Links", .merge = false};
		} else {
			m_gesture = Gesture::Link;
			m_linkStart = pin;
		}
	} else if (ioFrame.hoveredNode.has_value()) {
		const auto id = ioFrame.nodes[*ioFrame.hoveredNode].node->id;
		const bool selected = std::ranges::find(m_selection, id) != m_selection.end();
		if (io.KeyCtrl && selected)
			std::erase(m_selection, id);
		else if (io.KeyCtrl || io.KeyShift)
			m_selection.push_back(id);
		else if (!selected)
			m_selection = {id};
		else
			m_pressedNode = id;
		m_gesture = Gesture::Move;
	} else {
		if (!io.KeyCtrl && !io.KeyShift)
			m_selection.clear();
		m_gesture = Gesture::Select;
	}
}

void VisualScriptCanvas::releaseLeft(Frame& ioFrame, const std::array<float, 2>& iMouse) {
	const auto& mouse = iMouse;
	switch (m_gesture) {
		case Gesture::Move:
			if (std::abs(m_moveOffset[0]) + std::abs(m_moveOffset[1]) > g_dragThreshold / m_zoom) {
				auto next = ioFrame.script;
				for (auto& node: next.nodes)
					if (std::ranges::find(m_selection, node.id) != m_selection.end())
						node.position = {std::round((node.position[0] + m_moveOffset[0]) / g_snap) * g_snap,
										 std::round((node.position[1] + m_moveOffset[1]) / g_snap) * g_snap};
				ioFrame.result.edit =
						VisualScriptEdit{.script = std::move(next),
										 .description = m_selection.size() > 1 ? "Move Nodes" : "Move Node",
										 .merge = false};
			} else if (m_pressedNode.has_value()) {
				m_selection = {*m_pressedNode};
			}
			break;
		case Gesture::Select:
			{
				const std::array lo{std::min(m_gestureStart[0], mouse[0]), std::min(m_gestureStart[1], mouse[1])};
				const std::array hi{std::max(m_gestureStart[0], mouse[0]), std::max(m_gestureStart[1], mouse[1])};
				for (const auto& view: ioFrame.nodes) {
					const auto& position = view.node->position;
					const bool inside = position[0] < hi[0] && position[0] + view.layout.size[0] > lo[0] &&
										position[1] < hi[1] && position[1] + view.layout.size[1] > lo[1];
					if (inside && std::ranges::find(m_selection, view.node->id) == m_selection.end())
						m_selection.push_back(view.node->id);
				}
			}
			break;
		case Gesture::Link:
			if (ioFrame.hoveredPin.has_value())
				dropLink(ioFrame, *ioFrame.hoveredPin);
			else if (!ioFrame.hoveredNode.has_value() && ioFrame.hovered)
				ioFrame.result.linkDropped = m_linkStart;
			m_linkStart.reset();
			break;
		case Gesture::Pan:
		case Gesture::None:
			break;
	}
	m_gesture = Gesture::None;
	m_moveOffset = {0.f, 0.f};
}

}// namespace owl::nest
