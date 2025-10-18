/**
 * @file SceneHierarchy.cpp
 * @author Silmaen
 * @date 26/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneHierarchy.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

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
			if (ImGui::MenuItem("Create Empty Entity"))
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
	if (open) {
		// Draw the content (need tree management)
	}
	ImGui::PushID("...");
	if (ImGui::BeginPopupContextItem()) {
		if (ImGui::MenuItem("Delete Entity")) {
			if (m_selection == ioEntity)
				m_selection = {};
			m_context->destroyEntity(ioEntity);
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
	if (open)
		ImGui::TreePop();

	if (ImGui::IsItemClicked()) {
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
