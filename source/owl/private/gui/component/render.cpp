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
			if (const auto texturePath = renderer::Renderer::getTextureLibrary().find(path))
				ioComponent.texture = renderer::Texture2D::create(*texturePath);
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
	// Tiling: linked by default (same X and Y), checkbox to unlink.
	static bool s_tilingLinked = true;
	ImGui::Checkbox("Link Tiling X/Y", &s_tilingLinked);
	if (s_tilingLinked) {
		auto uniform = ioComponent.tilingFactor.x();
		if (ImGui::DragFloat("Tiling Factor", &uniform, 0.1f, 0.0f, 100.0f))
			ioComponent.tilingFactor = {uniform, uniform};
	} else {
		ImGui::DragFloat2("Tiling Factor", &ioComponent.tilingFactor.x(), 0.1f, 0.0f, 100.0f);
	}
}

void renderProps(AnimatedSpriteRenderer& ioComponent) {
	ImGui::ColorEdit4("Color", ioComponent.color.data());
	if (const auto tex = imTexture(ioComponent.texture); tex.has_value()) {
		if (ImGui::ImageButton("Spritesheet", tex.value(), {100.0f, 100.0f}, {0, 1}, {1, 0}) &&
			ioComponent.texture != nullptr) {
			ImGui::OpenPopup("AnimTextureSettings");
		}
	} else {
		if (ImGui::Button("Spritesheet", {100.0f, 100.0f}) && ioComponent.texture != nullptr) {
			ImGui::OpenPopup("AnimTextureSettings");
		}
	}
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const auto* const path = static_cast<const char*>(payload->Data);
			if (const auto texturePath = renderer::Renderer::getTextureLibrary().find(path))
				ioComponent.texture = renderer::Texture2D::create(*texturePath);
		}
		ImGui::EndDragDropTarget();
	}
	bool removeTexture = false;
	if (ImGui::BeginPopup("AnimTextureSettings")) {
		if (ImGui::MenuItem("Remove texture"))
			removeTexture = true;
		ImGui::EndPopup();
	}
	if (removeTexture)
		ioComponent.texture.reset();

	const int maxFrame = static_cast<int>(std::max(ioComponent.columns * ioComponent.rows, 1u) - 1);
	int cols = static_cast<int>(ioComponent.columns);
	if (ImGui::DragInt("Columns", &cols, 1, 1, 256))
		ioComponent.columns = static_cast<uint32_t>(std::max(cols, 1));
	int rows = static_cast<int>(ioComponent.rows);
	if (ImGui::DragInt("Rows", &rows, 1, 1, 256))
		ioComponent.rows = static_cast<uint32_t>(std::max(rows, 1));
	int first = static_cast<int>(ioComponent.firstFrame);
	if (ImGui::DragInt("First Frame", &first, 1, 0, maxFrame))
		ioComponent.firstFrame = static_cast<uint32_t>(std::clamp(first, 0, maxFrame));
	int last = static_cast<int>(ioComponent.lastFrame);
	if (ImGui::DragInt("Last Frame", &last, 1, static_cast<int>(ioComponent.firstFrame), maxFrame))
		ioComponent.lastFrame =
				static_cast<uint32_t>(std::clamp(last, static_cast<int>(ioComponent.firstFrame), maxFrame));
	ImGui::DragFloat("Frame Duration", &ioComponent.frameDuration, 0.01f, 0.001f, 10.0f, "%.3f s");
	ImGui::Checkbox("Loop", &ioComponent.loop);
	ImGui::Text("Current Frame: %u", ioComponent.m_currentFrame);
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
			for (const auto& font: data::fonts::FontLibrary::getFoundFontNames()) {
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
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Victory ||
		ioComponent.trigger.type == SceneTrigger::TriggerType::Death) {
		ImGui::InputText("Scene", &ioComponent.trigger.levelName);
		if (ioComponent.trigger.levelName.empty())
			ImGui::TextDisabled("Empty = built-in screen");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Teleport) {
		ImGui::InputText("Level Name", &ioComponent.trigger.levelName);
		ImGui::InputText("Target Name", &ioComponent.trigger.targetName);
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Timer) {
		ImGui::DragFloat("Duration (s)", &ioComponent.trigger.timerDuration, 0.1f, 0.01f, 3600.0f);
		ImGui::Checkbox("Repeating", &ioComponent.trigger.timerRepeating);
		ImGui::InputText("Callback", &ioComponent.trigger.callbackName);
		if (ioComponent.trigger.callbackName.empty())
			ImGui::TextDisabled("Default: on_timer");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Interaction) {
		ImGui::DragFloat("Interaction Range", &ioComponent.trigger.interactionRange, 0.05f, 0.1f, 20.0f);
		ImGui::InputText("Callback", &ioComponent.trigger.callbackName);
		if (ioComponent.trigger.callbackName.empty())
			ImGui::TextDisabled("Default: on_interact");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::LuaCallback) {
		ImGui::InputText("Callback", &ioComponent.trigger.callbackName);
		if (ioComponent.trigger.callbackName.empty())
			ImGui::TextDisabled("Default: on_triggered");
	}
	// Info: all overlap triggers also fire on_trigger_enter / on_trigger_exit.
	if (ioComponent.trigger.type != SceneTrigger::TriggerType::Victory &&
		ioComponent.trigger.type != SceneTrigger::TriggerType::Death &&
		ioComponent.trigger.type != SceneTrigger::TriggerType::Target) {
		ImGui::TextDisabled("Also fires: on_trigger_enter / on_trigger_exit");
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
					if (const auto texturePath = renderer::Renderer::getTextureLibrary().find(path))
						ioComponent.texture = renderer::Texture2D::create(*texturePath);
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

void renderProps(SoundSource& ioComponent) {
	ImGui::InputText("Sound Asset", &ioComponent.sound.soundAsset);
	const std::string currentCategory{magic_enum::enum_name(ioComponent.sound.category)};
	if (ImGui::BeginCombo("Category", currentCategory.c_str())) {
		for (const auto& catName: magic_enum::enum_names<SceneSound::Category>()) {
			const std::string sName{catName};
			if (ImGui::Selectable(sName.c_str(), currentCategory == sName))
				ioComponent.sound.category =
						magic_enum::enum_cast<SceneSound::Category>(sName).value_or(SceneSound::Category::SFX);
		}
		ImGui::EndCombo();
	}
	ImGui::DragFloat("Volume", &ioComponent.sound.volume, 0.01f, 0.0f, 2.0f);
	ImGui::DragFloat("Pitch", &ioComponent.sound.pitch, 0.01f, 0.1f, 3.0f);
	ImGui::Checkbox("Loop", &ioComponent.sound.loop);
	ImGui::Checkbox("Spatial", &ioComponent.sound.spatial);
	ImGui::Checkbox("Play On Start", &ioComponent.sound.playOnStart);
	if (ioComponent.sound.spatial) {
		ImGui::DragFloat("Max Distance", &ioComponent.sound.maxDistance, 0.5f, 1.0f, 500.0f);
		ImGui::DragFloat("Rolloff", &ioComponent.sound.rolloff, 0.01f, 0.0f, 10.0f);
	}
}

void renderProps(SoundListener& ioComponent) { ImGui::Checkbox("Primary", &ioComponent.primary); }

void renderProps(LuaScript& ioComponent) {
	ImGui::InputText("Script Path", &ioComponent.scriptPath);
	if (ImGui::Button("Refresh Properties") && !ioComponent.scriptPath.empty()) {
		ioComponent.properties = script::ScriptEngine::extractProperties(ioComponent.scriptPath);
	}
	if (!ioComponent.properties.empty()) {
		ImGui::Separator();
		ImGui::Text("Properties:");
		for (auto& prop: ioComponent.properties) {
			switch (prop.type) {
				case script::ScriptPropertyType::Float: {
					auto val = std::get<float>(prop.value);
					if (ImGui::DragFloat(prop.name.c_str(), &val, 0.1f))
						prop.value = val;
					break;
				}
				case script::ScriptPropertyType::Int: {
					auto val = static_cast<int>(std::get<int64_t>(prop.value));
					if (ImGui::DragInt(prop.name.c_str(), &val))
						prop.value = static_cast<int64_t>(val);
					break;
				}
				case script::ScriptPropertyType::String: {
					auto val = std::get<std::string>(prop.value);
					if (ImGui::InputText(prop.name.c_str(), &val))
						prop.value = val;
					break;
				}
				case script::ScriptPropertyType::Bool: {
					auto val = std::get<bool>(prop.value);
					if (ImGui::Checkbox(prop.name.c_str(), &val))
						prop.value = val;
					break;
				}
			}
		}
	}
}

void renderProps(Canvas& ioComponent) {
	const std::string currentSpace{magic_enum::enum_name(ioComponent.space)};
	if (ImGui::BeginCombo("Space", currentSpace.c_str())) {
		for (const auto& spaceName: magic_enum::enum_names<Canvas::Space>()) {
			const std::string sName{spaceName};
			if (ImGui::Selectable(sName.c_str(), currentSpace == sName))
				ioComponent.space = magic_enum::enum_cast<Canvas::Space>(sName).value_or(Canvas::Space::ScreenOverlay);
		}
		ImGui::EndCombo();
	}
	ImGui::DragInt("Sort Order", &ioComponent.sortOrder);
}

void renderProps(UIRect& ioComponent) {
	const std::string currentAnchor{magic_enum::enum_name(ioComponent.anchor)};
	if (ImGui::BeginCombo("Anchor", currentAnchor.c_str())) {
		for (const auto& anchorName: magic_enum::enum_names<UIRect::Anchor>()) {
			const std::string sName{anchorName};
			if (ImGui::Selectable(sName.c_str(), currentAnchor == sName))
				ioComponent.anchor = magic_enum::enum_cast<UIRect::Anchor>(sName).value_or(UIRect::Anchor::Center);
		}
		ImGui::EndCombo();
	}
	float pivotArr[2] = {ioComponent.pivot.x(), ioComponent.pivot.y()};
	if (ImGui::DragFloat2("Pivot", pivotArr, 0.01f, 0.0f, 1.0f)) {
		ioComponent.pivot.x() = pivotArr[0];
		ioComponent.pivot.y() = pivotArr[1];
	}
	float sizeArr[2] = {ioComponent.size.x(), ioComponent.size.y()};
	if (ImGui::DragFloat2("Size", sizeArr, 1.0f, 0.0f, 10000.0f)) {
		ioComponent.size.x() = sizeArr[0];
		ioComponent.size.y() = sizeArr[1];
	}
	float offsetArr[2] = {ioComponent.anchorOffset.x(), ioComponent.anchorOffset.y()};
	if (ImGui::DragFloat2("Offset", offsetArr, 1.0f)) {
		ioComponent.anchorOffset.x() = offsetArr[0];
		ioComponent.anchorOffset.y() = offsetArr[1];
	}
}

void renderProps(UIText& ioComponent) {
	ImGui::InputTextMultiline("Text", &ioComponent.text, ImVec2(0, 60));
	ImGui::ColorEdit4("Color", reinterpret_cast<float*>(&ioComponent.color));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::DragFloat("Font Size", &ioComponent.fontSize, 0.5f, 1.0f, 200.0f);
	const std::string currentAlign{magic_enum::enum_name(ioComponent.alignment)};
	if (ImGui::BeginCombo("Alignment", currentAlign.c_str())) {
		for (const auto& alignName: magic_enum::enum_names<UIText::Alignment>()) {
			const std::string sName{alignName};
			if (ImGui::Selectable(sName.c_str(), currentAlign == sName))
				ioComponent.alignment =
						magic_enum::enum_cast<UIText::Alignment>(sName).value_or(UIText::Alignment::Left);
		}
		ImGui::EndCombo();
	}
	ImGui::DragFloat("Kerning", &ioComponent.kerning, 0.1f);
	ImGui::DragFloat("Line Spacing", &ioComponent.lineSpacing, 0.1f);
}

void renderProps(UIImage& ioComponent) {
	ImGui::ColorEdit4("Tint", reinterpret_cast<float*>(&ioComponent.tint));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

void renderProps(UIPanel& ioComponent) {
	ImGui::ColorEdit4("Background", reinterpret_cast<float*>(&ioComponent.backgroundColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4("Border Color", reinterpret_cast<float*>(&ioComponent.borderColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::DragFloat("Border Width", &ioComponent.borderWidth, 0.5f, 0.0f, 20.0f);
	const std::string currentLayout{magic_enum::enum_name(ioComponent.layout)};
	if (ImGui::BeginCombo("Layout", currentLayout.c_str())) {
		for (const auto& layoutName: magic_enum::enum_names<UIPanel::Layout>()) {
			const std::string sName{layoutName};
			if (ImGui::Selectable(sName.c_str(), currentLayout == sName))
				ioComponent.layout =
						magic_enum::enum_cast<UIPanel::Layout>(sName).value_or(UIPanel::Layout::None);
		}
		ImGui::EndCombo();
	}
	if (ioComponent.layout != UIPanel::Layout::None) {
		ImGui::DragFloat("Spacing", &ioComponent.spacing, 0.5f, 0.0f, 100.0f);
		ImGui::DragFloat("Padding", &ioComponent.padding, 0.5f, 0.0f, 100.0f);
	}
}

void renderProps(UIButton& ioComponent) {
	ImGui::ColorEdit4("Normal Color", reinterpret_cast<float*>(&ioComponent.normalColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4("Hover Color", reinterpret_cast<float*>(&ioComponent.hoverColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4("Pressed Color", reinterpret_cast<float*>(&ioComponent.pressedColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4("Disabled Color", reinterpret_cast<float*>(&ioComponent.disabledColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::InputText("On Click Callback", &ioComponent.onClickCallback);
}

void renderProps(UISlider& ioComponent) {
	ImGui::DragFloat("Value", &ioComponent.value, 0.01f, ioComponent.minValue, ioComponent.maxValue);
	ImGui::DragFloat("Min", &ioComponent.minValue, 0.1f);
	ImGui::DragFloat("Max", &ioComponent.maxValue, 0.1f);
	ImGui::ColorEdit4("Track Color", reinterpret_cast<float*>(&ioComponent.trackColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4("Fill Color", reinterpret_cast<float*>(&ioComponent.fillColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4("Handle Color", reinterpret_cast<float*>(&ioComponent.handleColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::InputText("On Value Changed", &ioComponent.onValueChangedCallback);
}

void renderProps(UIProgressBar& ioComponent) {
	ImGui::DragFloat("Value", &ioComponent.value, 0.01f, 0.0f, 1.0f);
	ImGui::ColorEdit4("Background", reinterpret_cast<float*>(&ioComponent.backgroundColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4("Fill Color", reinterpret_cast<float*>(&ioComponent.fillColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

void renderProps(PrefabLink& ioComponent) {
	ImGui::Text("Asset: %s", ioComponent.prefabAssetPath.c_str());
	ImGui::Text("Synced Version: %u", ioComponent.syncedVersion);
	ImGui::Text("Mapped Entities: %zu", ioComponent.uuidMapping.size());
	if (!ioComponent.overriddenComponents.empty())
		ImGui::Text("Overrides: %zu", ioComponent.overriddenComponents.size());
}

}// namespace owl::gui::component
