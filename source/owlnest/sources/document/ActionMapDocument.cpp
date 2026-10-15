/**
 * @file ActionMapDocument.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ActionMapDocument.h"

#include "EditorLayer.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <imgui_stdlib.h>
#include <input/Input.h>
#include <input/InputNames.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <tuple>
#include <utility>

namespace owl::nest {

namespace {

constexpr float g_barWidth = 90.f;

class ModifyActionMapCommand final : public UndoCommand<input::ActionMap> {
public:
	ModifyActionMapCommand(input::ActionMap iBefore, input::ActionMap iAfter, std::string iDescription,
						   const bool iMerge)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)},
		  m_merge{iMerge} {}

	void undo(input::ActionMap& ioTarget) override { ioTarget = m_before; }

	void redo(input::ActionMap& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return m_merge ? 1 : 0; }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto& other = static_cast<const ModifyActionMapCommand&>(iOther);
		if (other.m_description != m_description)
			return false;
		m_after = other.m_after;
		return true;
	}

private:
	input::ActionMap m_before;
	input::ActionMap m_after;
	std::string m_description;
	bool m_merge;
};

using CodeList = std::vector<std::pair<uint16_t, std::string>>;

auto keyChoices() -> const CodeList& {
	static const CodeList choices = []() -> CodeList {
		CodeList list;
		for (std::size_t code = 0; code < input::g_KeyCount; ++code) {
			const auto name = input::names::keyName(static_cast<input::KeyCode>(code));
			if (!name.empty() && !std::isdigit(static_cast<unsigned char>(name.front())))
				list.emplace_back(static_cast<uint16_t>(code), name);
		}
		return list;
	}();
	return choices;
}

auto mouseChoices() -> const CodeList& {
	static const CodeList choices = []() -> CodeList {
		CodeList list;
		for (std::size_t code = 0; code < input::g_MouseButtonCount; ++code)
			list.emplace_back(static_cast<uint16_t>(code),
							  input::names::mouseButtonName(static_cast<input::MouseCode>(code)));
		return list;
	}();
	return choices;
}

auto padButtonChoices() -> const CodeList& {
	static const CodeList choices = []() -> CodeList {
		CodeList list;
		for (std::size_t code = 0; code < input::g_GamepadButtonCount; ++code)
			list.emplace_back(static_cast<uint16_t>(code),
							  input::names::gamepadButtonName(static_cast<input::GamepadButton>(code)));
		return list;
	}();
	return choices;
}

auto padAxisChoices() -> const CodeList& {
	static const CodeList choices = []() -> CodeList {
		CodeList list;
		for (std::size_t code = 0; code < input::g_GamepadAxisCount; ++code)
			list.emplace_back(static_cast<uint16_t>(code),
							  input::names::gamepadAxisName(static_cast<input::GamepadAxis>(code)));
		return list;
	}();
	return choices;
}

auto stickChoices() -> const CodeList& {
	static const CodeList choices{{0, "left"}, {1, "right"}};
	return choices;
}

auto choicesOf(const input::BindingSource iSource) -> const CodeList& {
	switch (iSource) {
		case input::BindingSource::Key:
			return keyChoices();
		case input::BindingSource::MouseButton:
			return mouseChoices();
		case input::BindingSource::GamepadButton:
			return padButtonChoices();
		case input::BindingSource::GamepadAxis:
			return padAxisChoices();
		case input::BindingSource::GamepadStick:
			return stickChoices();
	}
	return keyChoices();
}

auto sourceLabel(const input::BindingSource iSource) -> const char* {
	switch (iSource) {
		case input::BindingSource::Key:
			return "Key";
		case input::BindingSource::MouseButton:
			return "Mouse button";
		case input::BindingSource::GamepadButton:
			return "Gamepad button";
		case input::BindingSource::GamepadAxis:
			return "Gamepad axis";
		case input::BindingSource::GamepadStick:
			return "Gamepad stick";
	}
	return "Key";
}

auto kindLabel(const input::ActionKind iKind) -> const char* {
	switch (iKind) {
		case input::ActionKind::Button:
			return "Button";
		case input::ActionKind::Axis:
			return "Axis";
		case input::ActionKind::Vector:
			return "Vector";
	}
	return "Button";
}

auto codeLabel(const input::ActionBinding& iBinding) -> std::string {
	const auto& choices = choicesOf(iBinding.source);
	const auto found = std::ranges::find(choices, iBinding.code, &CodeList::value_type::first);
	return found != choices.end() ? found->second : std::to_string(iBinding.code);
}

auto describeBinding(const input::ActionBinding& iBinding) -> std::string {
	auto text = std::format("{}: {}", sourceLabel(iBinding.source), codeLabel(iBinding));
	if (iBinding.pad >= 0)
		text += std::format("  (pad {})", iBinding.pad);
	if (iBinding.component != input::BindingComponent::Both)
		text += iBinding.component == input::BindingComponent::X ? "  x" : "  y";
	if (std::abs(iBinding.scale - 1.f) > 1e-4f)
		text += std::format("  scale {:.2f}", iBinding.scale);
	return text;
}

auto isAnalog(const input::BindingSource iSource) -> bool {
	return iSource == input::BindingSource::GamepadAxis || iSource == input::BindingSource::GamepadStick;
}

auto usesPad(const input::BindingSource iSource) -> bool {
	return iSource == input::BindingSource::GamepadButton || isAnalog(iSource);
}

auto withAction(const input::ActionMap& iMap, const std::size_t iIndex, const input::ActionDefinition& iAction)
		-> input::ActionMap {
	input::ActionMap next;
	const auto& actions = iMap.getActions();
	for (std::size_t i = 0; i < actions.size(); ++i) std::ignore = next.setAction(i == iIndex ? iAction : actions[i]);
	return next;
}

auto codeCombo(const char* iLabel, const input::BindingSource iSource, uint16_t& ioCode) -> bool {
	bool changed = false;
	const auto& choices = choicesOf(iSource);
	const auto current = std::ranges::find(choices, ioCode, &CodeList::value_type::first);
	const auto preview = current != choices.end() ? current->second : std::to_string(ioCode);
	if (gui::widgets::beginCombo(iLabel, preview.c_str())) {
		for (const auto& [code, name]: choices) {
			const bool selected = code == ioCode;
			if (ImGui::Selectable(name.c_str(), selected) && !selected) {
				ioCode = code;
				changed = true;
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	return changed;
}

struct BindingEdit {
	bool changed{false};
	bool merge{false};
};

auto stateLabel(const input::ActionState& iState) -> const char* {
	if (iState.pressed)
		return "Just pressed";
	if (iState.released)
		return "Just released";
	return iState.held ? "Held" : "Released";
}

auto drawSourceRows(input::ActionBinding& ioBinding, const input::ActionKind iKind) -> BindingEdit {
	BindingEdit edit;
	gui::widgets::propertyLabel("Source", "The device input the binding reads.");
	if (gui::widgets::beginCombo("##source", sourceLabel(ioBinding.source))) {
		for (const auto source:
			 {input::BindingSource::Key, input::BindingSource::MouseButton, input::BindingSource::GamepadButton,
			  input::BindingSource::GamepadAxis, input::BindingSource::GamepadStick}) {
			if (ImGui::Selectable(sourceLabel(source), ioBinding.source == source) && ioBinding.source != source) {
				ioBinding.source = source;
				ioBinding.code = 0;
				edit.changed = true;
			}
		}
		ImGui::EndCombo();
	}
	std::ignore = gui::widgets::propertyReset(false);
	gui::widgets::propertyLabel("Input", "The key, button or axis.");
	if (codeCombo("##code", ioBinding.source, ioBinding.code))
		edit.changed = true;
	std::ignore = gui::widgets::propertyReset(false);
	if (usesPad(ioBinding.source)) {
		auto pad = static_cast<int>(ioBinding.pad);
		if (gui::widgets::propertyInt("Pad Slot", pad, -1, -1, static_cast<int>(input::g_GamepadCount) - 1, "%d",
									  "Gamepad slot read, -1 for any connected gamepad.")) {
			ioBinding.pad = pad;
			edit = {.changed = true, .merge = true};
		}
	}
	if (iKind == input::ActionKind::Vector && ioBinding.source != input::BindingSource::GamepadStick) {
		const bool isY = ioBinding.component == input::BindingComponent::Y;
		gui::widgets::propertyLabel("Component", "Which component of the vector the input feeds.");
		if (gui::widgets::beginCombo("##component", isY ? "Y (down)" : "X (right)")) {
			if (ImGui::Selectable("X (right)", !isY) && isY) {
				ioBinding.component = input::BindingComponent::X;
				edit.changed = true;
			}
			if (ImGui::Selectable("Y (down)", isY) && !isY) {
				ioBinding.component = input::BindingComponent::Y;
				edit.changed = true;
			}
			ImGui::EndCombo();
		}
		std::ignore = gui::widgets::propertyReset(false);
	}
	return edit;
}

auto drawShapeRows(input::ActionBinding& ioBinding) -> BindingEdit {
	BindingEdit edit;
	if (gui::widgets::propertyFloat("Scale", ioBinding.scale, 1.f, 0.05f, "%.2f",
									"Factor applied to the reading: -1 reverses it."))
		edit = {.changed = true, .merge = true};
	if (!isAnalog(ioBinding.source))
		return edit;
	const input::AxisResponse defaults;
	auto& response = ioBinding.response;
	if (gui::widgets::propertyFloat("Dead Zone", response.deadZone, defaults.deadZone, 0.01f, "%.2f",
									"Readings of this size or less count as zero.")) {
		response.deadZone = std::clamp(response.deadZone, 0.f, 0.99f);
		edit = {.changed = true, .merge = true};
	}
	static constexpr std::array<const char*, 4> curves{"Linear", "Quadratic", "Cubic", "Custom"};
	auto curve = static_cast<int>(response.curve);
	gui::widgets::propertyLabel("Curve", "Response of the axis: finer control near the centre.");
	if (gui::widgets::combo("##curve", &curve, curves)) {
		response.curve = static_cast<input::ResponseCurve>(curve);
		edit.changed = true;
	}
	std::ignore = gui::widgets::propertyReset(response.curve != defaults.curve);
	if (response.curve == input::ResponseCurve::Custom &&
		gui::widgets::propertyFloat("Exponent", response.exponent, defaults.exponent, 0.05f, "%.2f",
									"Exponent of the custom curve, 0.1 to 10.")) {
		response.exponent = std::clamp(response.exponent, 0.1f, 10.f);
		edit = {.changed = true, .merge = true};
	}
	return edit;
}

}// namespace

ActionMapDocument::ActionMapDocument() = default;

ActionMapDocument::~ActionMapDocument() = default;

auto ActionMapDocument::title() const -> std::string {
	if (!m_path.empty())
		return m_path.stem().string();
	return "Untitled";
}

void ActionMapDocument::onAttach(EditorLayer* iEditor) { mp_editorLayer = iEditor; }

void ActionMapDocument::onDetach() {
	cancelCapture();
	if (mp_editorLayer != nullptr)
		mp_editorLayer->setShortcutsSuspended(false);
	mp_editorLayer = nullptr;
}

void ActionMapDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {
	if (mp_editorLayer == nullptr)
		return;
	std::ignore = updateInput(input::Input::readState());
	mp_editorLayer->setShortcutsSuspended(isCapturing());
}

void ActionMapDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto ActionMapDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto ActionMapDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (!m_map.saveFile(iPath)) {
		OWL_CORE_ERROR("ActionMapDocument: failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_savedMap = m_map;
	m_undo.markSaved();
	if (mp_editorLayer != nullptr)
		mp_editorLayer->onActionMapSaved(iPath);
	return true;
}

auto ActionMapDocument::recoverySnapshot() const -> std::optional<std::string> { return m_map.toYaml(); }

auto ActionMapDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	auto map = input::ActionMap::fromYaml(iSnapshot, "autosave");
	if (!map) {
		OWL_WARN("Recovery: Cannot restore the autosave of action map '{}'.", title())
		return false;
	}
	m_map = *map;
	m_undo.clear();
	m_undo.markUnsaved();
	clampSelection();
	refreshMonitor();
	return true;
}

void ActionMapDocument::performUndo() {
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_map);
	clampSelection();
	refreshMonitor();
}

void ActionMapDocument::performRedo() {
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_map);
	clampSelection();
	refreshMonitor();
}

auto ActionMapDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto map = input::ActionMap::loadFile(iPath);
	if (!map) {
		OWL_CORE_ERROR("ActionMapDocument: failed to load '{}': {}.", iPath.string(), map.error())
		return false;
	}
	m_path = iPath;
	m_map = *map;
	m_savedMap = m_map;
	m_undo.clear();
	m_selected = 0;
	refreshMonitor();
	return true;
}

void ActionMapDocument::startFromDefaults(const std::filesystem::path& iPath) {
	m_path = iPath;
	m_map = input::ActionMap::createDefault();
	m_savedMap = input::ActionMap{};
	m_undo.clear();
	m_undo.markUnsaved();
	m_selected = 0;
	refreshMonitor();
}

auto ActionMapDocument::isNameAvailable(const std::string_view iName, const std::optional<std::size_t> iIgnored) const
		-> bool {
	if (iName.empty())
		return false;
	const auto& actions = m_map.getActions();
	for (std::size_t i = 0; i < actions.size(); ++i) {
		if (actions[i].name == iName && i != iIgnored)
			return false;
	}
	return true;
}

auto ActionMapDocument::uniqueName(const std::string_view iBase) const -> std::string {
	if (isNameAvailable(iBase))
		return std::string{iBase};
	for (int suffix = 2;; ++suffix) {
		if (auto candidate = std::format("{}_{}", iBase, suffix); isNameAvailable(candidate))
			return candidate;
	}
}

auto ActionMapDocument::commit(input::ActionMap iNext, const std::string& iDescription, const bool iMerge) -> bool {
	if (iNext == m_map)
		return false;
	auto before = std::move(m_map);
	m_map = iNext;
	m_undo.push(mkUniq<ModifyActionMapCommand>(std::move(before), std::move(iNext), iDescription, iMerge));
	clampSelection();
	refreshMonitor();
	return true;
}

template<typename Mutator>
auto ActionMapDocument::modifyAction(const std::size_t iIndex, const std::string& iDescription, Mutator&& iMutator,
									 const bool iMerge) -> bool {
	if (iIndex >= m_map.size())
		return false;
	auto action = m_map.getActions()[iIndex];
	if (!std::forward<Mutator>(iMutator)(action))
		return false;
	return commit(withAction(m_map, iIndex, action), iDescription, iMerge);
}

auto ActionMapDocument::addAction(const std::string& iName, const input::ActionKind iKind) -> bool {
	if (!isNameAvailable(iName))
		return false;
	auto next = m_map;
	std::ignore = next.setAction({.name = iName, .kind = iKind, .bindings = {}});
	if (!commit(std::move(next), std::format("Add action {}", iName)))
		return false;
	m_selected = m_map.size() - 1;
	return true;
}

auto ActionMapDocument::removeAction(const std::size_t iIndex) -> bool {
	if (iIndex >= m_map.size())
		return false;
	input::ActionMap next;
	const auto& actions = m_map.getActions();
	for (std::size_t i = 0; i < actions.size(); ++i) {
		if (i != iIndex)
			std::ignore = next.setAction(actions[i]);
	}
	return commit(std::move(next), std::format("Remove action {}", actions[iIndex].name));
}

auto ActionMapDocument::renameAction(const std::size_t iIndex, const std::string& iName) -> bool {
	if (iIndex >= m_map.size() || !isNameAvailable(iName, iIndex))
		return false;
	return modifyAction(iIndex, std::format("Rename action to {}", iName),
						[&iName](input::ActionDefinition& ioAction) -> bool {
							ioAction.name = iName;
							return true;
						});
}

auto ActionMapDocument::setActionKind(const std::size_t iIndex, const input::ActionKind iKind) -> bool {
	return modifyAction(iIndex, std::format("Set kind to {}", kindLabel(iKind)),
						[iKind](input::ActionDefinition& ioAction) -> bool {
							ioAction.kind = iKind;
							return true;
						});
}

auto ActionMapDocument::addBinding(const std::size_t iAction, const input::ActionBinding& iBinding) -> bool {
	return modifyAction(iAction, std::format("Add binding {}", describeBinding(iBinding)),
						[&iBinding](input::ActionDefinition& ioAction) -> bool {
							ioAction.bindings.push_back(iBinding);
							return true;
						});
}

auto ActionMapDocument::removeBinding(const std::size_t iAction, const std::size_t iBinding) -> bool {
	return modifyAction(iAction, "Remove binding", [iBinding](input::ActionDefinition& ioAction) -> bool {
		if (iBinding >= ioAction.bindings.size())
			return false;
		ioAction.bindings.erase(ioAction.bindings.begin() + static_cast<std::ptrdiff_t>(iBinding));
		return true;
	});
}

auto ActionMapDocument::setBinding(const std::size_t iAction, const std::size_t iBindingIndex,
								   const input::ActionBinding& iBinding, const bool iMerge) -> bool {
	return modifyAction(
			iAction, "Edit binding",
			[&iBinding, iBindingIndex](input::ActionDefinition& ioAction) -> bool {
				if (iBindingIndex >= ioAction.bindings.size())
					return false;
				ioAction.bindings[iBindingIndex] = iBinding;
				return true;
			},
			iMerge);
}

auto ActionMapDocument::overrideBuiltIn(const std::string_view iName) -> bool {
	if (m_map.find(iName) != nullptr)
		return false;
	const auto defaults = input::ActionMap::createDefault();
	const auto* builtIn = defaults.find(iName);
	if (builtIn == nullptr)
		return false;
	auto next = m_map;
	std::ignore = next.setAction(*builtIn);
	if (!commit(std::move(next), std::format("Override built-in action {}", iName)))
		return false;
	m_selected = m_map.size() - 1;
	return true;
}

auto ActionMapDocument::resetToDefaults() -> bool {
	return commit(input::ActionMap::createDefault(), "Reset to built-in actions");
}

auto ActionMapDocument::missingBuiltIns() const -> std::vector<std::string> {
	std::vector<std::string> missing;
	for (const auto& action: input::ActionMap::createDefault().getActions()) {
		if (m_map.find(action.name) == nullptr)
			missing.push_back(action.name);
	}
	return missing;
}

void ActionMapDocument::startCapture(const std::size_t iAction, const std::optional<std::size_t> iReplaced,
									 const input::InputState& iBaseline) {
	if (iAction >= m_map.size())
		return;
	m_captureAction = iAction;
	m_captureReplaced = iReplaced;
	m_capture.begin(iBaseline, m_map.getActions()[iAction].kind);
}

void ActionMapDocument::cancelCapture() { m_capture.cancel(); }

auto ActionMapDocument::updateInput(const input::InputState& iState) -> bool {
	const auto escapeDown = iState.isKeyPressed(input::key::Escape);
	const auto escapePressed = escapeDown && !m_escapeHeld;
	m_escapeHeld = escapeDown;
	m_inputFrame.advance(iState);
	refreshMonitor();
	if (!m_capture.isArmed())
		return false;
	if (escapePressed) {
		m_capture.cancel();
		return false;
	}
	auto captured = m_capture.feed(iState);
	if (!captured || m_captureAction >= m_map.size())
		return false;
	if (m_captureReplaced && *m_captureReplaced < m_map.getActions()[m_captureAction].bindings.size()) {
		const auto& old = m_map.getActions()[m_captureAction].bindings[*m_captureReplaced];
		captured->scale = old.scale;
		captured->component = old.component;
		if (isAnalog(captured->source) && isAnalog(old.source))
			captured->response = old.response;
		std::ignore = setBinding(m_captureAction, *m_captureReplaced, *captured);
	} else {
		std::ignore = addBinding(m_captureAction, *captured);
	}
	return true;
}

void ActionMapDocument::clampSelection() {
	if (m_map.size() == 0)
		m_selected = 0;
	else
		m_selected = std::min(m_selected, m_map.size() - 1);
}

void ActionMapDocument::refreshMonitor() {
	const auto& actions = m_map.getActions();
	m_monitor.resize(actions.size());
	for (std::size_t i = 0; i < actions.size(); ++i) m_monitor[i] = input::evaluateAction(actions[i], m_inputFrame);
}

void ActionMapDocument::onImGuiRender() {
	const auto display =
			(mp_editorLayer != nullptr) ? mp_editorLayer->getDocumentManager().displayTitleFor(this) : title();
	const auto winTitle = documentWindowTitle(DocumentType::ActionMap, display, static_cast<uint64_t>(id()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;

	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);

	const bool wantFocus = consumeFocusRequest();
	if (wantFocus)
		ImGui::SetNextWindowFocus();
	const bool open = ImGui::Begin(winTitle.c_str(), &m_pOpen, flags);
	if (mp_editorLayer != nullptr)
		mp_editorLayer->renderDocumentTabMenu(*this);
	if (wantFocus) {
		ImGui::SetWindowFocus();
		if (mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
	}
	if (open) {
		const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (windowFocused && !m_wasFocused && mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
		if (!windowFocused && !ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
			cancelCapture();
		m_wasFocused = windowFocused;
		if (windowFocused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) && !isCapturing())
			std::ignore = save();
		const float listWidth = std::max(220.f, ImGui::GetContentRegionAvail().x * 0.32f);
		ImGui::BeginChild("##actionList", ImVec2{listWidth, 0.f}, ImGuiChildFlags_Borders);
		renderActionList();
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginChild("##actionDetails", ImVec2{0.f, 0.f}, ImGuiChildFlags_Borders);
		renderActionDetails();
		ImGui::EndChild();
	} else {
		cancelCapture();
	}
	ImGui::End();
}

void ActionMapDocument::renderActionList() {
	if (gui::widgets::iconButton("##addAction",
								 {.glyph = gui::icon::plus,
								  .tooltip = "Add an action",
								  .description = "A button, an axis or a vector scripts read by name."})) {
		std::ignore = addAction(uniqueName("new_action"), input::ActionKind::Button);
	}
	ImGui::SameLine();
	if (gui::widgets::iconButton("##removeAction", {.glyph = gui::icon::trash2,
													.tooltip = "Remove the selected action",
													.disabled = !selectedAction().has_value()})) {
		if (const auto selected = selectedAction())
			std::ignore = removeAction(*selected);
	}
	ImGui::SameLine();
	if (gui::widgets::iconButton("##resetActions",
								 {.glyph = gui::icon::rotateCcw,
								  .tooltip = "Reset to the built-in actions",
								  .description = "Replaces every action by the engine defaults (undoable)."}))
		std::ignore = resetToDefaults();
	ImGui::Separator();
	const auto& actions = m_map.getActions();
	if (actions.empty())
		ImGui::TextDisabled("No action: the built-in ones apply.");
	for (std::size_t i = 0; i < actions.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		const auto state = i < m_monitor.size() ? m_monitor[i] : input::ActionState{};
		if (ImGui::Selectable(actions[i].name.c_str(), m_selected == i, ImGuiSelectableFlags_AllowOverlap))
			m_selected = i;
		const float barWidth = g_barWidth;
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - barWidth);
		const auto level = actions[i].kind == input::ActionKind::Vector ? std::min(state.vector.norm(), 1.f)
																		: std::min(std::abs(state.value), 1.f);
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::GetColorU32(gui::vec(gui::UiLayer::getTheme().accent)));
		ImGui::ProgressBar(level, ImVec2{barWidth, ImGui::GetTextLineHeight() * 0.7f}, "");
		ImGui::PopStyleColor();
		ImGui::PopID();
	}
	const auto missing = missingBuiltIns();
	if (missing.empty())
		return;
	ImGui::Spacing();
	ImGui::SeparatorText("Built-in, not in the file");
	for (const auto& name: missing) {
		ImGui::PushID(name.c_str());
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", name.c_str());
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::GetFrameHeight());
		if (gui::widgets::iconButton("##override",
									 {.glyph = gui::icon::plus,
									  .tooltip = "Edit this built-in action",
									  .description = "Copies it, with its default bindings, into the file."}))
			std::ignore = overrideBuiltIn(name);
		ImGui::PopID();
	}
}

void ActionMapDocument::renderActionDetails() {
	const auto selected = selectedAction();
	if (!selected) {
		ImGui::TextDisabled("Select or add an action.");
		return;
	}
	const auto index = *selected;
	const auto action = m_map.getActions()[index];
	if (gui::widgets::sectionHeader("Action", gui::icon::gamepad2, false).open) {
		if (gui::widgets::beginProperties("##action")) {
			if (m_nameBufferOf != std::optional{index} || (!ImGui::IsAnyItemActive() && m_nameBuffer != action.name)) {
				m_nameBuffer = action.name;
				m_nameBufferOf = index;
			}
			gui::widgets::propertyLabel("Name", "The name scripts and systems read the action by.");
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::InputText("##name", &m_nameBuffer);
			if (ImGui::IsItemDeactivatedAfterEdit() && !renameAction(index, m_nameBuffer))
				m_nameBuffer = action.name;
			if (!isNameAvailable(m_nameBuffer, index))
				ImGui::TextColored(ImVec4{0.9f, 0.4f, 0.3f, 1.f}, "Taken or empty: not applied.");
			std::ignore = gui::widgets::propertyReset(false);
			gui::widgets::propertyLabel("Kind", "Button: on or off. Axis: one value in [-1, 1]. Vector: two values in "
												"the unit disc.");
			if (gui::widgets::beginCombo("##kind", kindLabel(action.kind))) {
				for (const auto kind: {input::ActionKind::Button, input::ActionKind::Axis, input::ActionKind::Vector}) {
					if (ImGui::Selectable(kindLabel(kind), action.kind == kind))
						std::ignore = setActionKind(index, kind);
				}
				ImGui::EndCombo();
			}
			std::ignore = gui::widgets::propertyReset(false);
			gui::widgets::endProperties();
		}
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Live reading", gui::icon::gauge, false).open) {
		renderReading(index);
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Bindings", gui::icon::keyboard, false).open) {
		for (std::size_t i = 0; i < m_map.getActions()[index].bindings.size(); ++i) {
			const auto before = m_map.getActions()[index].bindings.size();
			renderBinding(index, i);
			if (m_map.getActions()[index].bindings.size() != before)
				break;
		}
		const bool capturingNew = isCapturing() && !m_captureReplaced;
		if (capturingNew)
			ImGui::TextColored(ImVec4{0.95f, 0.8f, 0.3f, 1.f},
							   "Press a key, a mouse button or a gamepad input... (Esc cancels)");
		else if (gui::widgets::button("##captureNew",
									  {.glyph = gui::icon::keyboard,
									   .label = "Add by pressing an input",
									   .tooltip = "Capture the next key, mouse button or gamepad input"}))
			startCapture(index, std::nullopt, input::Input::readState());
		ImGui::SameLine();
		if (gui::widgets::button("##addEmpty", {.glyph = gui::icon::plus, .label = "Add binding"}))
			std::ignore = addBinding(index, input::ActionBinding{});
		gui::widgets::endSection();
	}
}

void ActionMapDocument::renderReading(const std::size_t iAction) {
	const auto& action = m_map.getActions()[iAction];
	const auto& state = m_monitor[iAction];
	ImGui::TextUnformatted(stateLabel(state));
	if (action.kind == input::ActionKind::Vector) {
		ImGui::Text("x %+.2f   y %+.2f", static_cast<double>(state.vector.x()), static_cast<double>(state.vector.y()));
		ImGui::ProgressBar(std::min(state.vector.norm(), 1.f), ImVec2{-FLT_MIN, 0.f});
	} else {
		ImGui::Text("value %+.2f", static_cast<double>(state.value));
		ImGui::ProgressBar(std::min(std::abs(state.value), 1.f), ImVec2{-FLT_MIN, 0.f});
	}
}

void ActionMapDocument::renderBinding(const std::size_t iAction, const std::size_t iBindingIndex) {
	auto binding = m_map.getActions()[iAction].bindings[iBindingIndex];
	const auto kind = m_map.getActions()[iAction].kind;
	ImGui::PushID(static_cast<int>(iBindingIndex));
	const bool capturingThis = isCapturing() && m_captureAction == iAction && m_captureReplaced == iBindingIndex;
	const auto summary = capturingThis ? std::string{"Press an input... (Esc cancels)"} : describeBinding(binding);
	const bool open = ImGui::TreeNodeEx("##binding",
										ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
												ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth,
										"%s", summary.c_str());
	const float buttons = ImGui::GetFrameHeight() * 2.f + ImGui::GetStyle().ItemSpacing.x;
	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - buttons);
	if (gui::widgets::iconButton("##rebind", {.glyph = gui::icon::keyboard,
											  .tooltip = "Rebind by pressing an input",
											  .description = "Keeps the scale and the component."}))
		startCapture(iAction, iBindingIndex, input::Input::readState());
	ImGui::SameLine();
	const bool remove =
			gui::widgets::iconButton("##removeBinding", {.glyph = gui::icon::trash2, .tooltip = "Remove the binding"});
	if (remove) {
		std::ignore = removeBinding(iAction, iBindingIndex);
		if (open)
			ImGui::TreePop();
		ImGui::PopID();
		return;
	}
	if (open) {
		BindingEdit edit;
		if (gui::widgets::beginProperties("##binding")) {
			edit = drawSourceRows(binding, kind);
			const auto shape = drawShapeRows(binding);
			edit.changed = edit.changed || shape.changed;
			edit.merge = edit.merge || shape.merge;
			gui::widgets::endProperties();
		}
		if (edit.changed)
			std::ignore = setBinding(iAction, iBindingIndex, binding, edit.merge);
		ImGui::TreePop();
	}
	ImGui::PopID();
}

}// namespace owl::nest
