/**
 * @file SceneHierarchy.cpp
 * @author Silmaen
 * @date 26/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneHierarchy.h"

#include <imgui_internal.h>

namespace owl::panel {

[[maybe_unused]] SceneHierarchy::SceneHierarchy(const shared<scene::Scene>& iScene) { setContext(iScene); }

void SceneHierarchy::setContext(const shared<scene::Scene>& iContext) {
	m_context = iContext;
	m_selection = {};
}

void SceneHierarchy::onImGuiRender() {
	ImGui::Begin("Scene Hierarchy");

	if (m_context) {
		for (auto&& [e]: m_context->registry.storage<entt::entity>().each()) {
			scene::Entity entity = {e, m_context.get()};
			drawEntityNode(entity);
		}

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
			m_selection = {};

		// Right-click on blank space
		if (ImGui::BeginPopupContextWindow(nullptr, 1)) {
			if (ImGui::MenuItem("Create Empty Entity"))
				m_context->createEntity("Empty Entity");
			ImGui::EndPopup();
		}
	}
	ImGui::End();

	ImGui::Begin("Properties");
	if (m_selection)
		drawComponents(m_selection);

	ImGui::End();
}

// NOLINTBEGIN(performance-no-int-to-ptr)
void SceneHierarchy::drawEntityNode(scene::Entity& ioEntity) {
	const auto& tag = ioEntity.getComponent<scene::component::Tag>().tag;

	const auto flags = static_cast<ImGuiTreeNodeFlags>(
			((m_selection == ioEntity) ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None) |
			ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth);
	const bool opened = ImGui::TreeNodeEx(
			reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<uint32_t>(ioEntity))), flags, "%s", tag.c_str());
	if (ImGui::IsItemClicked()) {
		m_selection = ioEntity;
	}

	bool entityDeleted = false;
	if (ImGui::BeginPopupContextItem()) {
		if (ImGui::MenuItem("Delete Entity"))
			entityDeleted = true;
		ImGui::EndPopup();
	}

	if (opened) {
		if (ImGui::TreeNodeEx(reinterpret_cast<void*>(9817239),
							  ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", tag.c_str()))
			ImGui::TreePop();
		ImGui::TreePop();
	}

	if (entityDeleted) {
		m_context->destroyEntity(ioEntity);
		if (m_selection == ioEntity)
			m_selection = {};
	}
}
// NOLINTEND(performance-no-int-to-ptr)

namespace {
void drawVec3Control(const std::string& iLabel, math::vec3& iValues, const float iResetValue = 0.0f,
					 const float iColumnWidth = 100.0f) {
	const ImGuiIO& io = ImGui::GetIO();
	auto* const boldFont = io.Fonts->Fonts[0];
	ImGui::PushID(iLabel.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, iColumnWidth);
	ImGui::Text("%s", iLabel.c_str());
	ImGui::NextColumn();

	ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});

	const float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
	const ImVec2 buttonSize = {lineHeight + 3.0f, lineHeight};

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.0f});
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
	ImGui::PushFont(boldFont);
	if (ImGui::Button("X", buttonSize))
		iValues.x() = iResetValue;
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::DragFloat("##X", &iValues.x(), 0.1f, 0.0f, 0.0f, "%.2f");
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.8f, 0.3f, 1.0f});
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
	ImGui::PushFont(boldFont);
	if (ImGui::Button("Y", buttonSize))
		iValues.y() = iResetValue;
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::DragFloat("##Y", &iValues.y(), 0.1f, 0.0f, 0.0f, "%.2f");
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.2f, 0.35f, 0.9f, 1.0f});
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
	ImGui::PushFont(boldFont);
	if (ImGui::Button("Z", buttonSize))
		iValues.z() = iResetValue;
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::DragFloat("##Z", &iValues.z(), 0.1f, 0.0f, 0.0f, "%.2f");
	ImGui::PopItemWidth();

	ImGui::PopStyleVar();

	ImGui::Columns(1);

	ImGui::PopID();
}

// NOLINTBEGIN(performance-no-int-to-ptr)
template<typename T, typename UIFunction>
void drawComponent(const std::string& iName, scene::Entity& ioEntity, UIFunction iUiFunction) {
	constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
												 ImGuiTreeNodeFlags_SpanAvailWidth |
												 ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
	if (ioEntity.hasComponent<T>()) {
		auto& component = ioEntity.getComponent<T>();
		const ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
		const float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImGui::Separator();
		const bool open =
				ImGui::TreeNodeEx(reinterpret_cast<void*>(typeid(T).hash_code()), treeNodeFlags, "%s", iName.c_str());
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
			iUiFunction(component);
			ImGui::TreePop();
		}
		if (removeComponent)
			ioEntity.removeComponent<T>();
	}
}
// NOLINTEND(performance-no-int-to-ptr)
}// namespace

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void SceneHierarchy::drawComponents(scene::Entity& ioEntity) {
	if (ioEntity.hasComponent<scene::component::Tag>()) {
		auto& tag = ioEntity.getComponent<scene::component::Tag>().tag;

		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		memcpy(buffer, tag.c_str(), sizeof(buffer));
		if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
			tag = std::string(buffer);
		}
	}

	ImGui::SameLine();
	ImGui::PushItemWidth(-1);
	if (ImGui::Button("Add Component"))
		ImGui::OpenPopup("AddComponent");
	if (ImGui::BeginPopup("AddComponent")) {
		if (!m_selection.hasComponent<scene::component::Camera>()) {
			if (ImGui::MenuItem("Camera")) {
				m_selection.addComponent<scene::component::Camera>();
				ImGui::CloseCurrentPopup();
			}
		}
		if (!m_selection.hasComponent<scene::component::SpriteRenderer>()) {
			if (ImGui::MenuItem("Sprite Renderer")) {
				m_selection.addComponent<scene::component::SpriteRenderer>();
				ImGui::CloseCurrentPopup();
			}
		}
		if (!m_selection.hasComponent<scene::component::CircleRenderer>()) {
			if (ImGui::MenuItem("Circle Renderer")) {
				m_selection.addComponent<scene::component::CircleRenderer>();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopItemWidth();

	drawComponent<scene::component::Transform>("Transform", ioEntity, [](auto& component) {
		drawVec3Control("Translation", component.translation);
		math::vec3 rotation = math::degrees(component.rotation);
		drawVec3Control("Rotation", rotation);
		component.rotation = math::radians(rotation);
		drawVec3Control("Scale", component.scale, 1.0f);
	});
	drawComponent<scene::component::Camera>("Camera", ioEntity, [](auto& component) {
		auto& camera = component.camera;
		ImGui::Checkbox("Primary", &component.primary);
		const char* projectionTypeStrings[] = {"Perspective", "Orthographic"};
		const char* currentProjectionTypeString = projectionTypeStrings[static_cast<int>(camera.getProjectionType())];
		if (ImGui::BeginCombo("Projection", currentProjectionTypeString)) {
			for (int i = 0; i < 2; i++) {
				const bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
				if (ImGui::Selectable(projectionTypeStrings[i], isSelected)) {
					currentProjectionTypeString = projectionTypeStrings[i];
					camera.setProjectionType(static_cast<scene::SceneCamera::ProjectionType>(i));
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (camera.getProjectionType() == scene::SceneCamera::ProjectionType::Perspective) {
			float perspectiveVerticalFov = math::degrees(camera.getPerspectiveVerticalFOV());
			if (ImGui::DragFloat("Vertical FOV", &perspectiveVerticalFov))
				camera.setPerspectiveVerticalFOV(math::radians(perspectiveVerticalFov));
			float perspectiveNear = camera.getPerspectiveNearClip();
			if (ImGui::DragFloat("Near", &perspectiveNear))
				camera.setPerspectiveNearClip(perspectiveNear);
			float perspectiveFar = camera.getPerspectiveFarClip();
			if (ImGui::DragFloat("Far", &perspectiveFar))
				camera.setPerspectiveFarClip(perspectiveFar);
		}
		if (camera.getProjectionType() == scene::SceneCamera::ProjectionType::Orthographic) {
			float orthoSize = camera.getOrthographicSize();
			if (ImGui::DragFloat("Size", &orthoSize))
				camera.setOrthographicSize(orthoSize);
			float orthoNear = camera.getOrthographicNearClip();
			if (ImGui::DragFloat("Near", &orthoNear))
				camera.setOrthographicNearClip(orthoNear);
			float orthoFar = camera.getOrthographicFarClip();
			if (ImGui::DragFloat("Far", &orthoFar))
				camera.setOrthographicFarClip(orthoFar);
			ImGui::Checkbox("Fixed Aspect Ratio", &component.fixedAspectRatio);
		}
	});
	drawComponent<scene::component::SpriteRenderer>("Sprite Renderer", ioEntity, [](auto& component) {
		ImGui::ColorEdit4("Color", component.color.data());
		ImGui::Button("Texture", ImVec2(100.0f, 0.0f));
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				const auto* const path = static_cast<const char*>(payload->Data);
				const std::filesystem::path texturePath = core::Application::get().getAssetDirectory() / path;
				component.texture = renderer::Texture2D::create(texturePath);
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::DragFloat("Tiling Factor", &component.tilingFactor, 0.1f, 0.0f, 100.0f);
	});
	drawComponent<scene::component::CircleRenderer>("Circle Renderer", ioEntity, [](auto& component) {
		ImGui::ColorEdit4("Color", component.color.data());
		ImGui::DragFloat("Thickness", &component.thickness, 0.025f, 0.0f, 1.0f);
		ImGui::DragFloat("Fade", &component.fade, 0.00025f, 0.0f, 1.0f);
	});
}
OWL_DIAG_POP

}// namespace owl::panel
