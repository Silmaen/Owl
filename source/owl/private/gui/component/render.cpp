/**
 * @file render.cpp
 * @author Silmaen
 * @date 12/30/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/Application.h"
#include "gui/component/render.h"
#include "gui/utils.h"

#include "renderer/Renderer.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

using namespace owl::scene;
using namespace owl::scene::component;

namespace owl::gui::component {

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

	const float lineHeight = ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
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
}// namespace

void renderProps(Transform& ioComponent) {
	drawVec3Control("Translation", ioComponent.transform.translation());
	math::vec3 rotation = degrees(ioComponent.transform.rotation());
	drawVec3Control("Rotation", rotation);
	ioComponent.transform.rotation() = radians(rotation);
	drawVec3Control("Scale", ioComponent.transform.scale(), 1.0f);
}

void renderProps(Camera& ioComponent) {
	auto& camera = ioComponent.camera;
	ImGui::Checkbox("Primary", &ioComponent.primary);
	if (ImGui::BeginCombo("Projection", std::string(magic_enum::enum_name(camera.getProjectionType())).c_str())) {
		for (const SceneCamera::ProjectionType& projType: magic_enum::enum_values<SceneCamera::ProjectionType>()) {
			const bool isSelected = camera.getProjectionType() == projType;
			if (ImGui::Selectable(std::string(magic_enum::enum_name(projType)).c_str(), isSelected)) {
				camera.setProjectionType(projType);
			}
			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (camera.getProjectionType() == SceneCamera::ProjectionType::Perspective) {
		float perspectiveVerticalFov = math::degrees(camera.getPerspectiveVerticalFov());
		if (ImGui::DragFloat("Vertical FOV", &perspectiveVerticalFov))
			camera.setPerspectiveVerticalFov(math::radians(perspectiveVerticalFov));
		float perspectiveNear = camera.getPerspectiveNearClip();
		if (ImGui::DragFloat("Near", &perspectiveNear))
			camera.setPerspectiveNearClip(perspectiveNear);
		float perspectiveFar = camera.getPerspectiveFarClip();
		if (ImGui::DragFloat("Far", &perspectiveFar))
			camera.setPerspectiveFarClip(perspectiveFar);
	}
	if (camera.getProjectionType() == SceneCamera::ProjectionType::Orthographic) {
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
	if (const auto tex = imTexture(ioComponent.texture); tex.has_value()) {
		if (ImGui::ImageButton("Texture", tex.value(), {100.0f, 100.0f}, {0, 1}, {1, 0}) &&
			ioComponent.texture != nullptr) {
			ImGui::OpenPopup("TextureSettings");
		}
	} else {
		if (ImGui::Button("Texture", {100.0f, 100.0f}) && ioComponent.texture != nullptr) {
			ImGui::OpenPopup("TextureSettings");
		}
	}
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const auto* const path = static_cast<const char*>(payload->Data);
			const std::filesystem::path texturePath = renderer::Renderer::getTextureLibrary().find(path).value();
			ioComponent.texture = renderer::Texture2D::create(texturePath);
		}
		ImGui::EndDragDropTarget();
	}
	bool removeTexture = false;
	if (ImGui::BeginPopup("TextureSettings")) {
		if (ImGui::MenuItem("Remove texture"))
			removeTexture = true;
		ImGui::EndPopup();
	}
	if (removeTexture)
		ioComponent.texture.reset();
	ImGui::DragFloat("Tiling Factor", &ioComponent.tilingFactor, 0.1f, 0.0f, 100.0f);
}

void renderProps(CircleRenderer& ioComponent) {
	ImGui::ColorEdit4("Color", ioComponent.color.data());
	ImGui::DragFloat("Thickness", &ioComponent.thickness, 0.025f, 0.0f, 1.0f);
	ImGui::DragFloat("Fade", &ioComponent.fade, 0.00025f, 0.0f, 1.0f);
}

void renderProps(Text& ioComponent) {
	ImGui::InputTextMultiline("Text String", &ioComponent.text, {0, 70});
	if (core::Application::instanced()) {
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
	}

	ImGui::ColorEdit4("Color", ioComponent.color.data());
	ImGui::DragFloat("Kerning", &ioComponent.kerning, 0.025f);
	ImGui::DragFloat("Line Spacing", &ioComponent.lineSpacing, 0.025f);
}

void renderProps(PhysicBody& ioComponent) {
	// the type.
	const std::string currentName{magic_enum::enum_name(ioComponent.body.type)};
	if (ImGui::BeginCombo("Type", currentName.c_str())) {
		for (const auto& name: magic_enum::enum_names<SceneBody::BodyType>()) {
			const std::string sName{name};
			if (ImGui::Selectable(sName.c_str(), currentName == sName)) {
				ioComponent.body.type =
						magic_enum::enum_cast<SceneBody::BodyType>(sName).value_or(SceneBody::BodyType::Static);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::Checkbox("Fixed Rotation", &ioComponent.body.fixedRotation);
	drawVec3Control("Size", ioComponent.body.colliderSize, 1.0f);
	ImGui::DragFloat("Density", &ioComponent.body.density, 0.00025f, 0.0f, 10.0f);
	ImGui::DragFloat("Restitution", &ioComponent.body.restitution, 0.00025f, 0.0f, 1.0f);
	ImGui::DragFloat("Friction", &ioComponent.body.friction, 0.00025f, 0.0f, 1.0f);
}

void renderProps(Player& ioComponent) {
	ImGui::Checkbox("Primary", &ioComponent.primary);
	ImGui::DragFloat("Linear Impulse", &ioComponent.player.linearImpulse, 0.025f, 0.0f, 10.0f);
	ImGui::DragFloat("Jump Impulse", &ioComponent.player.jumpImpulse, 0.025f, 0.0f, 10.0f);
	ImGui::Checkbox("Can jump", &ioComponent.player.canJump);
}

void renderProps(Trigger& ioComponent) {
	// the type.
	const std::string currentName{magic_enum::enum_name(ioComponent.trigger.type)};
	if (ImGui::BeginCombo("Type", currentName.c_str())) {
		for (const auto& name: magic_enum::enum_names<SceneTrigger::TriggerType>()) {
			const std::string sName{name};
			if (ImGui::Selectable(sName.c_str(), currentName == sName)) {
				ioComponent.trigger.type = magic_enum::enum_cast<SceneTrigger::TriggerType>(sName).value_or(
						SceneTrigger::TriggerType::Victory);
			}
		}
		ImGui::EndCombo();
	}
}

void renderProps(EntityLink& ioComponent) { ImGui::InputText("linked Entity Name", &ioComponent.linkedEntityName); }

void renderProps(BackgroundTexture& ioComponent) {
	constexpr std::array modeNames = {"Background", "Skybox"};
	int modeIndex = static_cast<int>(ioComponent.mode);
	if (ImGui::Combo("Mode", &modeIndex, modeNames.data(), static_cast<int>(modeNames.size())))
		ioComponent.mode = static_cast<BackgroundTexture::Mode>(modeIndex);

	if (ioComponent.mode == BackgroundTexture::Mode::Background) {
		constexpr std::array typeNames = {"Solid Color", "Gradient", "Texture"};
		int typeIndex = static_cast<int>(ioComponent.type);
		if (ImGui::Combo("Type", &typeIndex, typeNames.data(), static_cast<int>(typeNames.size())))
			ioComponent.type = static_cast<BackgroundTexture::Type>(typeIndex);

		if (ioComponent.type == BackgroundTexture::Type::SolidColor) {
			ImGui::ColorEdit4("Color", ioComponent.color.data());
		} else if (ioComponent.type == BackgroundTexture::Type::Gradient) {
			ImGui::ColorEdit4("Bottom Color", ioComponent.color.data());
			ImGui::ColorEdit4("Top Color", ioComponent.topColor.data());
		} else {
			// Texture mode
			ImGui::ColorEdit4("Tint", ioComponent.color.data());
			if (const auto tex = imTexture(ioComponent.texture); tex.has_value()) {
				if (ImGui::ImageButton("Texture", tex.value(), {100.0f, 100.0f}, {0, 1}, {1, 0}) &&
					ioComponent.texture != nullptr) {
					ImGui::OpenPopup("BgTextureSettings");
				}
			} else {
				if (ImGui::Button("Texture", {100.0f, 100.0f}) && ioComponent.texture != nullptr) {
					ImGui::OpenPopup("BgTextureSettings");
				}
			}
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
					const auto* const path = static_cast<const char*>(payload->Data);
					const std::filesystem::path texturePath =
							renderer::Renderer::getTextureLibrary().find(path).value();
					ioComponent.texture = renderer::Texture2D::create(texturePath);
				}
				ImGui::EndDragDropTarget();
			}
			bool removeTexture = false;
			if (ImGui::BeginPopup("BgTextureSettings")) {
				if (ImGui::MenuItem("Remove texture"))
					removeTexture = true;
				ImGui::EndPopup();
			}
			if (removeTexture)
				ioComponent.texture.reset();
		}
	} else {
		// Skybox mode
		if (const auto tex = imTexture(ioComponent.texture); tex.has_value()) {
			if (ImGui::ImageButton("Skybox Texture", tex.value(), {100.0f, 100.0f}, {0, 1}, {1, 0}) &&
				ioComponent.texture != nullptr) {
				ImGui::OpenPopup("SkyboxTextureSettings");
			}
		} else {
			if (ImGui::Button("Skybox Texture", {100.0f, 100.0f}) && ioComponent.texture != nullptr) {
				ImGui::OpenPopup("SkyboxTextureSettings");
			}
		}
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				const auto* const path = static_cast<const char*>(payload->Data);
				const std::filesystem::path texturePath = renderer::Renderer::getTextureLibrary().find(path).value();
				ioComponent.texture = renderer::Texture2D::create(texturePath);
			}
			ImGui::EndDragDropTarget();
		}
		bool removeTexture = false;
		if (ImGui::BeginPopup("SkyboxTextureSettings")) {
			if (ImGui::MenuItem("Remove texture"))
				removeTexture = true;
			ImGui::EndPopup();
		}
		if (removeTexture)
			ioComponent.texture.reset();
	}
}

void renderProps(Visibility& ioComponent) {
	ImGui::Checkbox("Game Visible", &ioComponent.gameVisible);
	ImGui::Checkbox("Editor Visible", &ioComponent.editorVisible);
}

}// namespace owl::gui::component
