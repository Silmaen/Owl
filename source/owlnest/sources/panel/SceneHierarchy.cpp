/**
 * @file SceneHierarchy.cpp
 * @author Silmaen
 * @date 26/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneHierarchy.h"

#include <imgui_internal.h>
#include <magic_enum.hpp>

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

		constexpr ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen;
		if (ImGui::TreeNodeEx("root", flag)) {
			for (auto&& [e]: m_context->registry.storage<entt::entity>().each()) {
				scene::Entity entity = {e, m_context.get()};
				drawEntityNode(entity);
			}
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


void renderProps(Transform& ioComponent) {
	drawVec3Control("Translation", ioComponent.transform.translation());
	math::vec3 rotation = math::degrees(ioComponent.transform.rotation());
	drawVec3Control("Rotation", rotation);
	ioComponent.transform.rotation() = math::radians(rotation);
	drawVec3Control("Scale", ioComponent.transform.scale(), 1.0f);
}

void renderProps(Camera& ioComponent) {
	auto& camera = ioComponent.camera;
	ImGui::Checkbox("Primary", &ioComponent.primary);
	if (ImGui::BeginCombo("Projection", std::string(magic_enum::enum_name(camera.getProjectionType())).c_str())) {
		for (const scene::SceneCamera::ProjectionType& projType:
			 magic_enum::enum_values<scene::SceneCamera::ProjectionType>()) {
			const bool isSelected = camera.getProjectionType() == projType;
			if (ImGui::Selectable(std::string(magic_enum::enum_name(projType)).c_str(), isSelected)) {
				camera.setProjectionType(projType);
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
		ImGui::Checkbox("Fixed Aspect Ratio", &ioComponent.fixedAspectRatio);
	}
}

void renderProps(SpriteRenderer& ioComponent) {
	ImGui::ColorEdit4("Color", ioComponent.color.data());
	if (const auto tex = gui::imTexture(ioComponent.texture); tex.has_value())
		ImGui::ImageButton("Texture", tex.value(), ImVec2(100.0f, 100.0f), ImVec2(0, 1), ImVec2(1, 0));
	else
		ImGui::Button("Texture", ImVec2(100.0f, 100.0f));

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const auto* const path = static_cast<const char*>(payload->Data);
			const std::filesystem::path texturePath = core::Application::get().getFullAssetPath(path).value();
			ioComponent.texture = renderer::Texture2D::create(texturePath);
		}
		ImGui::EndDragDropTarget();
	}
	ImGui::DragFloat("Tiling Factor", &ioComponent.tilingFactor, 0.1f, 0.0f, 100.0f);
}

void renderProps(CircleRenderer& ioComponent) {
	ImGui::ColorEdit4("Color", ioComponent.color.data());
	ImGui::DragFloat("Thickness", &ioComponent.thickness, 0.025f, 0.0f, 1.0f);
	ImGui::DragFloat("Fade", &ioComponent.fade, 0.00025f, 0.0f, 1.0f);
}

void renderProps(Text& ioComponent) {
	const size_t size = 500 + ioComponent.text.size();
	std::string temp{ioComponent.text};
	temp.resize(size);
	if (ImGui::InputTextMultiline("Text String", &temp[0], size, {0, 70})) {
		temp.resize(strlen(temp.c_str()));
		ioComponent.text = temp;
	}
	auto& fontLib = core::Application::get().getFontLibrary();
	const std::string display = ioComponent.font->isDefault() ? "(default)" : ioComponent.font->getName();
	if (ImGui::BeginCombo("Font", display.c_str())) {
		for (const auto& font: fontLib.getFoundFontNames()) {
			if (ImGui::Selectable(font.c_str(), ioComponent.font->getName() == font)) {
				ioComponent.font = fontLib.getFont(font);
			}
		}
		ImGui::EndCombo();
	}

	ImGui::ColorEdit4("Color", ioComponent.color.data());
	ImGui::DragFloat("Kerning", &ioComponent.kerning, 0.025f);
	ImGui::DragFloat("Line Spacing", &ioComponent.lineSpacing, 0.025f);
}

template<typename T>
void drawComponent(const std::string& iName, scene::Entity& ioEntity) {
	constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
												 ImGuiTreeNodeFlags_SpanAvailWidth |
												 ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
	if (ioEntity.hasComponent<T>()) {
		auto& component = ioEntity.getComponent<T>();
		const ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
		const float lineHeight = GImGui->Font->FontSize + (GImGui->Style.FramePadding.y * 2.0f);
		ImGui::Separator();
		const bool open = ImGui::TreeNodeEx(iName.c_str(), treeNodeFlags);

		ImGui::PopStyleVar();
		ImGui::SameLine(contentRegionAvailable.x - (lineHeight * 0.5f));
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
			renderProps(component);
			ImGui::TreePop();
		}
		if (removeComponent)
			ioEntity.removeComponent<T>();
	}
}

}// namespace

void SceneHierarchy::drawComponents(scene::Entity& ioEntity) {
	if (ioEntity.hasComponent<Tag>()) {
		auto& tag = ioEntity.getComponent<Tag>().tag;
		std::string buffer{tag};
		buffer.resize(256);
		if (ImGui::InputText("##Tag", &buffer[0], 256)) {
			buffer.resize(strlen(buffer.c_str()));
			tag = buffer;
		}
	}

	ImGui::SameLine();
	ImGui::PushItemWidth(-1);
	if (ImGui::Button("Add Component"))
		ImGui::OpenPopup("AddComponent");
	if (ImGui::BeginPopup("AddComponent")) {
		if (!m_selection.hasComponent<Camera>()) {
			if (ImGui::MenuItem("Camera")) {
				m_selection.addComponent<Camera>();
				ImGui::CloseCurrentPopup();
			}
		}
		if (!m_selection.hasComponent<SpriteRenderer>()) {
			if (ImGui::MenuItem("Sprite Renderer")) {
				m_selection.addComponent<SpriteRenderer>();
				ImGui::CloseCurrentPopup();
			}
		}
		if (!m_selection.hasComponent<CircleRenderer>()) {
			if (ImGui::MenuItem("Circle Renderer")) {
				m_selection.addComponent<CircleRenderer>();
				ImGui::CloseCurrentPopup();
			}
		}
		if (!m_selection.hasComponent<Text>()) {
			if (ImGui::MenuItem("Text Renderer")) {
				m_selection.addComponent<Text>();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopItemWidth();

	drawComponent<Transform>("Transform", ioEntity);
	drawComponent<Camera>("Camera", ioEntity);
	drawComponent<SpriteRenderer>("Sprite Renderer", ioEntity);
	drawComponent<CircleRenderer>("Circle Renderer", ioEntity);
	drawComponent<Text>("Text Renderer", ioEntity);
}

}// namespace owl::nest::panel
