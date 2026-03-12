/**
 * @file SceneHierarchy.cpp
 * @author Silmaen
 * @date 26/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneHierarchy.h"

#include <gui/IconBank.h>
#include <gui/utils.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <renderer/Renderer.h>

using namespace owl::scene::component;

namespace owl::nest::panel {

[[maybe_unused]] SceneHierarchy::SceneHierarchy(const shared<scene::Scene>& iScene) { setContext(iScene); }

void SceneHierarchy::setContext(const shared<scene::Scene>& iContext) {
	m_context = iContext;
	m_selection = {};
}

void SceneHierarchy::onImGuiRender() {
	renderHierarchy();
	renderProperties();
}

// Function displaying the Hierarchy panel.

void SceneHierarchy::renderHierarchy() {
	ImGui::Begin("Scene Hierarchy");

	if (m_context) {
		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
			m_selection = {};

		// Right-click on blank space
		ImGui::PushID("...");
		if (ImGui::BeginPopupContextWindow(nullptr, 1)) {
			if (owl::gui::IconBank::instance().menuItem("add_entity", "Create Empty Entity"))
				m_context->createEntity("Empty Entity");
			ImGui::EndPopup();
		}
		ImGui::PopID();

		if (constexpr ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen; ImGui::TreeNodeEx("root", flag)) {
			for (auto& entity: m_context->getAllEntities()) { drawEntityNode(entity); }
			ImGui::TreePop();
		}
	}
	ImGui::End();
}

void SceneHierarchy::drawEntityNode(scene::Entity& ioEntity) {
	const auto& tag = ioEntity.getComponent<Tag>().tag;

	ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_DefaultOpen;// (need tree management)
	if (ioEntity == m_selection)
		flag |= ImGuiTreeNodeFlags_Selected;
	const bool open = ImGui::TreeNodeEx(tag.c_str(), flag);
	const bool treeNodeClicked = ImGui::IsItemClicked();

	// Visibility toggle buttons (right-aligned)
	{
		auto& vis = ioEntity.getComponent<Visibility>();
		auto& iconBank = owl::gui::IconBank::instance();

		const float btnSize = ImGui::GetTextLineHeight();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;

		// Position: right edge minus space for 2 buttons
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - (btnSize + spacing) * 2);

		// Transparent button background
		constexpr math::vec4 transparent{0.f, 0.f, 0.f, 0.f};
		constexpr math::vec4 subtleHighlight{1.f, 1.f, 1.f, 0.15f};
		ImGui::PushStyleColor(ImGuiCol_Button, gui::vec(transparent));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, gui::vec(subtleHighlight));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, gui::vec(math::vec2{0.f, 0.f}));

		constexpr math::vec4 fullTint{1.0f, 1.0f, 1.0f, 1.0f};
		constexpr math::vec4 dimTint{0.5f, 0.5f, 0.5f, 0.5f};
		const math::vec2 btnSizeVec{btnSize, btnSize};

		// --- Editor visibility button (left) — eye icon ---
		ImGui::PushID("editorVis");
		const auto* const edIconName = vis.editorVisible ? "eye_open" : "eye_closed";
		const auto edTint = vis.editorVisible ? fullTint : dimTint;
		if (const auto iconInfo = iconBank.getIcon(edIconName)) {
			if (ImGui::ImageButton("##edVis", static_cast<ImTextureID>(iconInfo->textureId), gui::vec(btnSizeVec),
								   gui::vec(iconInfo->uv0), gui::vec(iconInfo->uv1), gui::vec(transparent),
								   gui::vec(edTint)))
				vis.editorVisible = !vis.editorVisible;
		} else {
			if (ImGui::Button(vis.editorVisible ? "E" : "-", gui::vec(btnSizeVec)))
				vis.editorVisible = !vis.editorVisible;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Editor Visibility");
		ImGui::PopID();

		ImGui::SameLine();

		// --- Game visibility button (right) — camera icon ---
		ImGui::PushID("gameVis");
		const auto* const gameIconName = vis.gameVisible ? "camera_on" : "camera_off";
		const auto gameTint = vis.gameVisible ? fullTint : dimTint;
		if (const auto iconInfo = iconBank.getIcon(gameIconName)) {
			if (ImGui::ImageButton("##gameVis", static_cast<ImTextureID>(iconInfo->textureId), gui::vec(btnSizeVec),
								   gui::vec(iconInfo->uv0), gui::vec(iconInfo->uv1), gui::vec(transparent),
								   gui::vec(gameTint)))
				vis.gameVisible = !vis.gameVisible;
		} else {
			if (ImGui::Button(vis.gameVisible ? "V" : "-", gui::vec(btnSizeVec)))
				vis.gameVisible = !vis.gameVisible;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Game Visibility");
		ImGui::PopID();

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);
	}

	if (open) {
		// Draw the content (need tree management)
	}
	ImGui::PushID("...");
	if (ImGui::BeginPopupContextItem()) {
		if (owl::gui::IconBank::instance().menuItem("delete_entity", "Delete Entity")) {
			if (m_selection == ioEntity)
				m_selection = {};
			m_context->destroyEntity(ioEntity);
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
	if (open)
		ImGui::TreePop();

	if (treeNodeClicked) {
		m_selection = ioEntity;
	}
}

// Function displaying the Entity Property panel.

void SceneHierarchy::renderProperties() {
	ImGui::Begin("Properties");
	if (m_selection)
		drawComponents(m_selection);
	ImGui::End();
}

namespace {

template<isNamedComponent Comp>
void addComponentPop(scene::Entity& ioEntity) {
	if (!ioEntity.hasComponent<Comp>()) {
		if (ImGui::MenuItem(Comp::name())) {
			ioEntity.addComponent<Comp>();
			ImGui::CloseCurrentPopup();
		}
	}
}

template<isNamedComponent T>
void drawComponent(scene::Entity& ioEntity) {
	constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
												 ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap |
												 ImGuiTreeNodeFlags_FramePadding;
	if (ioEntity.hasComponent<T>()) {
		auto& component = ioEntity.getComponent<T>();
		const ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
		const float lineHeight = ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
		ImGui::Separator();
		const bool open = ImGui::TreeNodeEx(T::name(), treeNodeFlags);

		ImGui::PopStyleVar();
		ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
		if (ImGui::Button("+", ImVec2{lineHeight, lineHeight})) {
			ImGui::OpenPopup("ComponentSettings");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Component Settings");
		bool removeComponent = false;
		if (ImGui::BeginPopup("ComponentSettings")) {
			if (ImGui::MenuItem("Remove component"))
				removeComponent = true;
			ImGui::EndPopup();
		}
		if (open) {
			gui::component::renderProps(component);
			ImGui::TreePop();
		}
		if (removeComponent)
			ioEntity.removeComponent<T>();
	}
}


template<isNamedComponent... Component>
void addComponentsFromTuple(scene::Entity& ioEntity, const std::tuple<Component...>&) {
	(..., addComponentPop<Component>(ioEntity));
}

template<isNamedComponent... Component>
void drawComponentsFromTuple(scene::Entity& ioEntity, const std::tuple<Component...>&) {
	(..., drawComponent<Component>(ioEntity));
}

}// namespace


void SceneHierarchy::drawComponents(scene::Entity& ioEntity) {
	if (ioEntity.hasComponent<Tag>()) {
		auto& tag = ioEntity.getComponent<Tag>().tag;
		ImGui::InputText("##Tag", &tag);
	}
	ImGui::SameLine();
	ImGui::Text("Entity name");

	ImGui::PushItemWidth(-1);
	if (ImGui::Button("Add Component"))
		ImGui::OpenPopup("AddComponent");
	if (ImGui::BeginPopup("AddComponent")) {
		addComponentsFromTuple(m_selection, OptionalComponents{});
		ImGui::EndPopup();
	}
	ImGui::PopItemWidth();
	drawComponentsFromTuple(m_selection, gui::component::DrawableComponents{});
}

}// namespace owl::nest::panel
