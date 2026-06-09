/**
 * @file render.cpp
 * @author Silmaen
 * @date 12/30/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "app/Application.h"
#include "gui/FontPreviewCache.h"
#include "gui/component/render.h"
#include "gui/utils.h"
#include "gui/widgets/AssetField.h"
#include "gui/widgets/CurveEditor.h"

#include "input/KeyCodes.h"
#include "renderer/Renderer.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"
#include "sound/SoundCommand.h"
#include "sound/SoundSystem.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

using namespace owl::scene;
using namespace owl::scene::component;

namespace owl::gui::component {

namespace {
void fieldTooltip(const char* iText) {
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
		ImGui::SetTooltip("%s", iText);
}

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

auto tilePickField(const char* iLabel, const shared<scene::Tileset>& iTileset, uint32_t& ioTileIndex) -> bool {
	ImGui::PushID(iLabel);
	ImGui::TextUnformatted(iLabel);
	ImGui::SameLine();
	bool changed = false;
	constexpr float kThumbSize = 32.f;
	const auto popupId = std::format("##picker_{}", iLabel);
	// Thumbnail button — shows the currently-selected tile or a placeholder.
	if (iTileset && iTileset->texture && iTileset->texture->isLoaded() && ioTileIndex < iTileset->tileCount()) {
		const auto uvs = iTileset->getTileUv(ioTileIndex);
		const ImVec2 uv0 = gui::vec(uvs[3]);// top-left
		const ImVec2 uv1 = gui::vec(uvs[1]);// bottom-right
		if (ImGui::ImageButton("##thumb", static_cast<ImTextureID>(iTileset->texture->getRendererId()),
							   ImVec2(kThumbSize, kThumbSize), uv0, uv1))
			ImGui::OpenPopup(popupId.c_str());
	} else {
		if (ImGui::Button("##thumb", ImVec2(kThumbSize, kThumbSize)))
			ImGui::OpenPopup(popupId.c_str());
	}
	ImGui::SameLine();
	ImGui::Text("#%u", ioTileIndex);
	if (ImGui::BeginPopup(popupId.c_str())) {
		if (!iTileset || !iTileset->texture || !iTileset->texture->isLoaded()) {
			ImGui::TextDisabled("(no tileset assigned)");
		} else {
			constexpr float kPickerThumb = 32.f;
			constexpr uint32_t kColumns = 8;
			const auto tileCount = iTileset->tileCount();
			for (uint32_t i = 0; i < tileCount; ++i) {
				ImGui::PushID(static_cast<int>(i));
				const bool highlighted = i == ioTileIndex;
				if (highlighted) {
					ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 200, 60, 255));
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.f);
				}
				const auto uvs = iTileset->getTileUv(i);
				const ImVec2 uv0 = gui::vec(uvs[3]);
				const ImVec2 uv1 = gui::vec(uvs[1]);
				if (ImGui::ImageButton("##tile", static_cast<ImTextureID>(iTileset->texture->getRendererId()),
									   ImVec2(kPickerThumb, kPickerThumb), uv0, uv1)) {
					if (i != ioTileIndex) {
						ioTileIndex = i;
						changed = true;
					}
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::IsItemHovered()) {
					const auto& meta = iTileset->getTileMeta(i);
					if (meta.name.empty())
						ImGui::SetTooltip("#%u", i);
					else
						ImGui::SetTooltip("#%u %s", i, meta.name.c_str());
				}
				if (highlighted) {
					ImGui::PopStyleVar();
					ImGui::PopStyleColor();
				}
				ImGui::PopID();
				if ((i + 1) % kColumns != 0 && (i + 1) < tileCount)
					ImGui::SameLine();
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
	return changed;
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
	fieldTooltip("Mark this camera as the primary camera. Only one primary camera per scene at runtime.");
	if (ImGui::BeginCombo("Projection", std::string(magic_enum::enum_name(camera.getProjectionType())).c_str())) {
		for (const SceneCamera::ProjectionType& projType: magic_enum::enum_values<SceneCamera::ProjectionType>()) {
			const bool isSelected = camera.getProjectionType() == projType;
			if (ImGui::Selectable(std::string(magic_enum::enum_name(projType)).c_str(), isSelected))
				camera.setProjectionType(projType);
			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	fieldTooltip("Perspective: 3D with vanishing point. Orthographic: 2D/isometric with parallel projection.");
	if (camera.getProjectionType() == SceneCamera::ProjectionType::Perspective) {
		float perspectiveVerticalFov = math::degrees(camera.getPerspectiveVerticalFov());
		if (ImGui::DragFloat("Vertical FOV", &perspectiveVerticalFov))
			camera.setPerspectiveVerticalFov(math::radians(perspectiveVerticalFov));
		fieldTooltip("Vertical field of view in degrees. Typical values: 60-90 degrees.");
		float perspectiveNear = camera.getPerspectiveNearClip();
		if (ImGui::DragFloat("Near", &perspectiveNear))
			camera.setPerspectiveNearClip(perspectiveNear);
		fieldTooltip("Near clipping plane distance. Objects closer than this are not rendered.");
		float perspectiveFar = camera.getPerspectiveFarClip();
		if (ImGui::DragFloat("Far", &perspectiveFar))
			camera.setPerspectiveFarClip(perspectiveFar);
		fieldTooltip("Far clipping plane distance. Objects farther than this are not rendered.");
	}
	if (camera.getProjectionType() == SceneCamera::ProjectionType::Orthographic) {
		float orthoSize = camera.getOrthographicSize();
		if (ImGui::DragFloat("Size", &orthoSize))
			camera.setOrthographicSize(orthoSize);
		fieldTooltip("Vertical half-size of the visible area in world units. Smaller = more zoomed in.");
		float orthoNear = camera.getOrthographicNearClip();
		if (ImGui::DragFloat("Near", &orthoNear))
			camera.setOrthographicNearClip(orthoNear);
		fieldTooltip("Near clipping plane distance.");
		float orthoFar = camera.getOrthographicFarClip();
		if (ImGui::DragFloat("Far", &orthoFar))
			camera.setOrthographicFarClip(orthoFar);
		fieldTooltip("Far clipping plane distance.");
		ImGui::Checkbox("Fixed Aspect Ratio", &ioComponent.fixedAspectRatio);
		fieldTooltip("Lock aspect ratio to prevent distortion when the viewport resizes.");
	}
}

void renderProps(SpriteRenderer& ioComponent) {
	ImGui::ColorEdit4("Color", ioComponent.color.data());
	widgets::textureField("Texture", ioComponent.texture);
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
	if (ImGui::CollapsingHeader("Raycast (billboard)")) {
		fieldTooltip("Optional overrides used only when the entity is rendered through a "
					 "RendererRaycast layer. Leave at defaults to inherit from Transform.");
		ImGui::DragFloat2("Size", &ioComponent.raycastSize.x(), 0.05f, 0.0f, 16.0f, "%.2f");
		fieldTooltip("World size of the billboard in cells (X = width, Y = height). When either component is 0 "
					 "(default), the renderer falls back to Transform.scale.xy. Use this to keep the editor scale "
					 "small and the first-person size large (or vice-versa).");
		ImGui::DragFloat("Z Offset", &ioComponent.raycastZOffset, 0.025f, -4.0f, 4.0f, "%.3f");
		fieldTooltip("Vertical offset added to Transform.translation.z when the sprite is drawn in raycast view. "
					 "Positive values raise the sprite (lamps, ceiling decals); negative values lower it (floor "
					 "stains, half-buried props).");
	}
}

void renderProps(AnimatedSpriteRenderer& ioComponent) {
	ImGui::ColorEdit4("Color", ioComponent.color.data());
	widgets::textureField("Spritesheet", ioComponent.texture);

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
	if (ImGui::CollapsingHeader("Speed Curve")) {
		fieldTooltip("Optional speed remap. Empty = constant playback speed; non-empty multiplies dt by "
					 "curve.evaluate(progress).");
		widgets::curveEditor("##speedCurve", ioComponent.speedCurve);
	}
	if (ImGui::CollapsingHeader("Raycast (billboard)")) {
		fieldTooltip("Optional overrides used only when the entity is rendered through a "
					 "RendererRaycast layer. Leave at defaults to inherit from Transform.");
		ImGui::DragFloat2("Size", &ioComponent.raycastSize.x(), 0.05f, 0.0f, 16.0f, "%.2f");
		fieldTooltip("World size of the billboard in cells (X = width, Y = height). When either component is 0 "
					 "(default), the renderer falls back to Transform.scale.xy.");
		ImGui::DragFloat("Z Offset", &ioComponent.raycastZOffset, 0.025f, -4.0f, 4.0f, "%.3f");
		fieldTooltip("Vertical offset added to Transform.translation.z when drawn in raycast view. Positive raises, "
					 "negative lowers.");
	}
}

void renderProps(CircleRenderer& ioComponent) {
	ImGui::ColorEdit4("Color", ioComponent.color.data());
	ImGui::DragFloat("Thickness", &ioComponent.thickness, 0.025f, 0.0f, 1.0f);
	ImGui::DragFloat("Fade", &ioComponent.fade, 0.00025f, 0.0f, 1.0f);
}

void renderProps(Text& ioComponent) {
	ImGui::InputTextMultiline("Text String", &ioComponent.text, {0, 70});
	if (app::Application::instanced()) {
		auto& fontLib = app::Application::get().getFontLibrary();
		const std::string display = ioComponent.font->isDefault() ? "(default)" : ioComponent.font->getName();
		if (ImGui::BeginCombo("Font", display.c_str())) {
			for (const auto& font: data::fonts::FontLibrary::getFoundFontNames()) {
				if (ImGui::Selectable(font.c_str(), ioComponent.font->getName() == font)) {
					ioComponent.font = fontLib.getFont(font);
				}
			}
			ImGui::EndCombo();
		}
		std::filesystem::path dropped;
		if (widgets::assetDropTarget(widgets::AssetKind::Font, dropped))
			ioComponent.font = fontLib.getFont(dropped.stem().string());
		if (ioComponent.font && !ioComponent.font->isDefault()) {
			constexpr ImVec2 previewSize{220.f, 44.f};
			if (const auto previewFb = FontPreviewCache::get().request(ioComponent.font); previewFb) {
				if (const auto preview = imTexture(previewFb, 0); preview.has_value())
					ImGui::Image(preview.value(), previewSize, gui::vec(previewFb->getLowerData()),
								 gui::vec(previewFb->getUpperData()));
			} else if (const auto atlas = imTexture(ioComponent.font->getAtlasTexture()); atlas.has_value()) {
				// First-frame fallback: atlas thumbnail until pumpPending() renders the sample-string preview.
				ImGui::Image(atlas.value(), previewSize, {0, 1}, {1, 0});
			}
		}
	}

	ImGui::ColorEdit4("Color", ioComponent.color.data());
	ImGui::DragFloat("Kerning", &ioComponent.kerning, 0.025f);
	ImGui::DragFloat("Line Spacing", &ioComponent.lineSpacing, 0.025f);
}

void renderProps(PhysicBody& ioComponent) {
	constexpr auto bodyTypeDesc = [](const SceneBody::BodyType iType) -> const char* {
		switch (iType) {
			case SceneBody::BodyType::Static:
				return "Static: never moves. Participates in collisions but is never simulated.";
			case SceneBody::BodyType::Dynamic:
				return "Dynamic: fully simulated. Affected by gravity, forces, impulses, and collisions.";
			case SceneBody::BodyType::Kinematic:
				return "Kinematic: moved programmatically. Not affected by forces but can push dynamic bodies.";
		}
		return "";
	};
	const std::string currentName{magic_enum::enum_name(ioComponent.body.type)};
	if (ImGui::BeginCombo("Type", currentName.c_str())) {
		for (const auto& name: magic_enum::enum_names<SceneBody::BodyType>()) {
			const std::string sName{name};
			const auto type = magic_enum::enum_cast<SceneBody::BodyType>(sName).value_or(SceneBody::BodyType::Static);
			if (ImGui::Selectable(sName.c_str(), currentName == sName))
				ioComponent.body.type = type;
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
				ImGui::SetTooltip("%s", bodyTypeDesc(type));
		}
		ImGui::EndCombo();
	}
	fieldTooltip(bodyTypeDesc(ioComponent.body.type));
	ImGui::Checkbox("Fixed Rotation", &ioComponent.body.fixedRotation);
	fieldTooltip("Lock rotation — the body cannot spin (useful for platformer characters).");
	drawVec3Control("Size", ioComponent.body.colliderSize, 1.0f);
	ImGui::DragFloat("Density", &ioComponent.body.density, 0.00025f, 0.0f, 10.0f);
	fieldTooltip("Material density. Affects mass proportionally to collider size.");
	ImGui::DragFloat("Restitution", &ioComponent.body.restitution, 0.00025f, 0.0f, 1.0f);
	fieldTooltip("Bounciness. 0 = no bounce, 1 = perfectly elastic.");
	ImGui::DragFloat("Friction", &ioComponent.body.friction, 0.00025f, 0.0f, 1.0f);
	fieldTooltip("Surface friction coefficient. 0 = ice, 1 = rubber.");
}

void renderProps(Player& ioComponent) {
	ImGui::Checkbox("Primary", &ioComponent.primary);
	fieldTooltip("Mark this entity as the primary player. Only one primary player per scene.");
	ImGui::DragFloat("Linear Impulse", &ioComponent.player.linearImpulse, 0.025f, 0.0f, 10.0f);
	fieldTooltip("Horizontal impulse magnitude applied when moving left/right.");
	ImGui::DragFloat("Jump Impulse", &ioComponent.player.jumpImpulse, 0.025f, 0.0f, 10.0f);
	fieldTooltip("Vertical impulse magnitude applied on jump.");
	ImGui::Checkbox("Can jump", &ioComponent.player.canJump);
	fieldTooltip("Whether the player is allowed to jump. Toggle off for floating/swimming characters.");
}

void renderProps(Trigger& ioComponent) {
	// Tooltip explanations for each trigger type.
	constexpr auto triggerDescription = [](const SceneTrigger::TriggerType iType) -> const char* {
		switch (iType) {
			case SceneTrigger::TriggerType::Victory:
				return "Victory: player winning this level. Loads the victory scene or built-in screen.";
			case SceneTrigger::TriggerType::Death:
				return "Death: player losing this level. Loads the game-over scene or built-in screen.";
			case SceneTrigger::TriggerType::Target:
				return "Target: passive spawn/teleport destination marker. Does nothing on its own.";
			case SceneTrigger::TriggerType::Teleport:
				return "Teleport: load another scene when the player overlaps. Optional target entity name "
					   "determines spawn position.";
			case SceneTrigger::TriggerType::Timer:
				return "Timer: fires a Lua callback at a fixed interval. One-shot or repeating.";
			case SceneTrigger::TriggerType::Interaction:
				return "Interaction: fires a Lua callback when the player is within range and presses E.";
			case SceneTrigger::TriggerType::LuaCallback:
				return "LuaCallback: fires a generic Lua callback when the player overlaps.";
		}
		return "";
	};
	// the type.
	const std::string currentName{magic_enum::enum_name(ioComponent.trigger.type)};
	if (ImGui::BeginCombo("Type", currentName.c_str())) {
		for (const auto& name: magic_enum::enum_names<SceneTrigger::TriggerType>()) {
			const std::string sName{name};
			const auto type = magic_enum::enum_cast<SceneTrigger::TriggerType>(sName).value_or(
					SceneTrigger::TriggerType::Victory);
			if (ImGui::Selectable(sName.c_str(), currentName == sName))
				ioComponent.trigger.type = type;
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))

				ImGui::SetTooltip("%s", triggerDescription(type));
		}

		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))

		ImGui::SetTooltip("%s", triggerDescription(ioComponent.trigger.type));
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Victory ||
		ioComponent.trigger.type == SceneTrigger::TriggerType::Death) {
		ImGui::InputText("Scene", &ioComponent.trigger.levelName);

		fieldTooltip("Path to the scene to load (e.g. scenes/victory.owl). Leave empty to use the built-in screen.");
		if (ioComponent.trigger.levelName.empty())

			ImGui::TextDisabled("Empty = built-in screen");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Teleport) {
		ImGui::InputText("Level Name", &ioComponent.trigger.levelName);

		fieldTooltip("Destination scene path (e.g. scenes/level2.owl).");

		ImGui::InputText("Target Name", &ioComponent.trigger.targetName);

		fieldTooltip("Name of the entity (Target trigger) that marks where the player appears in the new scene.");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Timer) {
		ImGui::DragFloat("Duration (s)", &ioComponent.trigger.timerDuration, 0.1f, 0.01f, 3600.0f);

		fieldTooltip("Time in seconds before the callback fires.");

		ImGui::Checkbox("Repeating", &ioComponent.trigger.timerRepeating);

		fieldTooltip("If checked, the timer restarts after each fire. Otherwise it fires once.");

		ImGui::InputText("Callback", &ioComponent.trigger.callbackName);

		fieldTooltip("Lua function name to call. Defaults to on_timer if empty.");
		if (ioComponent.trigger.callbackName.empty())

			ImGui::TextDisabled("Default: on_timer");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Interaction) {
		ImGui::DragFloat("Interaction Range", &ioComponent.trigger.interactionRange, 0.05f, 0.1f, 20.0f);

		fieldTooltip("Distance within which the player can press E to interact.");

		ImGui::InputText("Callback", &ioComponent.trigger.callbackName);

		fieldTooltip("Lua function name to call. Defaults to on_interact if empty.");
		if (ioComponent.trigger.callbackName.empty())

			ImGui::TextDisabled("Default: on_interact");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::LuaCallback) {
		ImGui::InputText("Callback", &ioComponent.trigger.callbackName);

		fieldTooltip("Lua function name to call when the player overlaps. Defaults to on_triggered if empty.");
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
			widgets::textureField("Texture", ioComponent.texture);
		}
	} else {
		// Skybox mode
		widgets::textureField("Skybox Texture", ioComponent.texture);
	}
}

void renderProps(Visibility& ioComponent) {
	ImGui::Checkbox("Game Visible", &ioComponent.gameVisible);
	ImGui::Checkbox("Editor Visible", &ioComponent.editorVisible);
}

void renderProps(SoundSource& ioComponent) {
	ImGui::InputText("Sound Asset", &ioComponent.sound.soundAsset);
	fieldTooltip("Path to the audio file relative to the project (e.g. sounds/explosion.wav).");
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Sound, dropped))
		ioComponent.sound.soundAsset = dropped.string();
	// Inspector preview: play the asset to verify it.
	static sound::SoundHandle s_previewHandle = sound::invalidSoundHandle;
	const bool playing =
			s_previewHandle != sound::invalidSoundHandle && sound::SoundCommand::isPlaying(s_previewHandle);
	const bool canPlay =
			!ioComponent.sound.soundAsset.empty() && sound::SoundCommand::getState() == sound::SoundAPI::State::Ready;
	ImGui::BeginDisabled(!canPlay);
	if (ImGui::Button(playing ? "Stop##soundPreview" : "Play##soundPreview")) {
		if (playing) {
			sound::SoundCommand::stop(s_previewHandle);
			s_previewHandle = sound::invalidSoundHandle;
		} else {
			auto& library = sound::SoundSystem::getSoundLibrary();
			const auto data = library.exists(ioComponent.sound.soundAsset) ? library.get(ioComponent.sound.soundAsset)
																		   : library.load(ioComponent.sound.soundAsset);
			if (data) {
				sound::PlayParams params;
				params.volume = ioComponent.sound.volume;
				params.pitch = ioComponent.sound.pitch;
				s_previewHandle = sound::SoundCommand::play(data, params);
			}
		}
	}
	ImGui::EndDisabled();
	fieldTooltip("Preview the sound with current volume and pitch. Not spatialized.");
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
	fieldTooltip("Category used for per-category volume mixing. SFX, Music, or Ambient.");
	ImGui::DragFloat("Volume", &ioComponent.sound.volume, 0.01f, 0.0f, 2.0f);
	fieldTooltip("Gain factor (0 = silent, 1 = normal, 2 = 200%).");
	ImGui::DragFloat("Pitch", &ioComponent.sound.pitch, 0.01f, 0.1f, 3.0f);
	fieldTooltip("Pitch multiplier. 1 = normal, 0.5 = octave down, 2 = octave up.");
	ImGui::Checkbox("Loop", &ioComponent.sound.loop);
	fieldTooltip("If checked, the sound restarts automatically after it finishes.");
	ImGui::Checkbox("Spatial", &ioComponent.sound.spatial);
	fieldTooltip("Enable 3D positional audio (mono sources only; stereo is not spatialized).");
	ImGui::Checkbox("Play On Start", &ioComponent.sound.playOnStart);
	fieldTooltip("Automatically start playback when the scene enters Play mode.");
	if (ioComponent.sound.spatial) {
		ImGui::DragFloat("Max Distance", &ioComponent.sound.maxDistance, 0.5f, 1.0f, 500.0f);
		fieldTooltip("Maximum audible distance in world units.");
		ImGui::DragFloat("Rolloff", &ioComponent.sound.rolloff, 0.01f, 0.0f, 10.0f);
		fieldTooltip("Distance attenuation curve. Higher = faster falloff.");
	}
}

void renderProps(SoundListener& ioComponent) {
	ImGui::Checkbox("Primary", &ioComponent.primary);
	fieldTooltip("Mark as the active listener (the 'ear' for 3D audio). Only one primary per scene.");
}

void renderProps(LuaScript& ioComponent) {
	ImGui::InputText("Script Path", &ioComponent.scriptPath);
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::LuaScript, dropped)) {
		ioComponent.scriptPath = dropped.string();
		ioComponent.properties = script::ScriptEngine::extractProperties(ioComponent.scriptPath);
	}
	if (ImGui::Button("Refresh Properties") && !ioComponent.scriptPath.empty()) {
		ioComponent.properties = script::ScriptEngine::extractProperties(ioComponent.scriptPath);
	}
	if (!ioComponent.properties.empty()) {
		ImGui::Separator();
		ImGui::Text("Properties:");
		for (size_t idx = 0; idx < ioComponent.properties.size(); ++idx) {
			auto& prop = ioComponent.properties[idx];
			// Index-based PushID prevents collisions if two properties share a name.
			ImGui::PushID(static_cast<int>(idx));
			switch (prop.type) {
				case script::ScriptPropertyType::Float:
					{
						auto val = std::get<float>(prop.value);
						if (ImGui::DragFloat(prop.name.c_str(), &val, 0.1f))
							prop.value = val;
						break;
					}
				case script::ScriptPropertyType::Int:
					{
						auto val = static_cast<int>(std::get<int64_t>(prop.value));
						if (ImGui::DragInt(prop.name.c_str(), &val))
							prop.value = static_cast<int64_t>(val);
						break;
					}
				case script::ScriptPropertyType::String:
					{
						auto val = std::get<std::string>(prop.value);
						if (ImGui::InputText(prop.name.c_str(), &val))
							prop.value = val;
						break;
					}
				case script::ScriptPropertyType::Bool:
					{
						auto val = std::get<bool>(prop.value);
						if (ImGui::Checkbox(prop.name.c_str(), &val))
							prop.value = val;
						break;
					}
			}
			ImGui::PopID();
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
	fieldTooltip("ScreenOverlay: always on top, scaled to window. WorldSpace: rendered in the 3D scene.");
	ImGui::DragInt("Sort Order", &ioComponent.sortOrder);
	fieldTooltip("Higher sort order renders on top. Used to layer multiple canvases.");
}

void renderProps(UiRect& ioComponent) {
	const std::string currentAnchor{magic_enum::enum_name(ioComponent.anchor)};
	if (ImGui::BeginCombo("Anchor", currentAnchor.c_str())) {
		for (const auto& anchorName: magic_enum::enum_names<UiRect::Anchor>()) {
			const std::string sName{anchorName};
			if (ImGui::Selectable(sName.c_str(), currentAnchor == sName))
				ioComponent.anchor = magic_enum::enum_cast<UiRect::Anchor>(sName).value_or(UiRect::Anchor::Center);
		}
		ImGui::EndCombo();
	}
	fieldTooltip("Reference point on the parent canvas (e.g. TopLeft, Center). Widget is positioned relative to this.");
	float pivotArr[2] = {ioComponent.pivot.x(), ioComponent.pivot.y()};
	if (ImGui::DragFloat2("Pivot", pivotArr, 0.01f, 0.0f, 1.0f)) {
		ioComponent.pivot.x() = pivotArr[0];
		ioComponent.pivot.y() = pivotArr[1];
	}
	fieldTooltip("Origin within the widget itself (0,0) = top-left, (0.5,0.5) = center, (1,1) = bottom-right.");
	float sizeArr[2] = {ioComponent.size.x(), ioComponent.size.y()};
	if (ImGui::DragFloat2("Size", sizeArr, 1.0f, 0.0f, 10000.0f)) {
		ioComponent.size.x() = sizeArr[0];
		ioComponent.size.y() = sizeArr[1];
	}
	fieldTooltip("Width and height of the widget in pixels.");
	float offsetArr[2] = {ioComponent.anchorOffset.x(), ioComponent.anchorOffset.y()};
	if (ImGui::DragFloat2("Offset", offsetArr, 1.0f)) {
		ioComponent.anchorOffset.x() = offsetArr[0];
		ioComponent.anchorOffset.y() = offsetArr[1];
	}
	fieldTooltip("Pixel offset from the anchor point. Positive X goes right, positive Y goes down.");
}

void renderProps(UiText& ioComponent) {
	ImGui::InputTextMultiline("Text", &ioComponent.text, ImVec2(0, 60));
	ImGui::ColorEdit4("Color", reinterpret_cast<float*>(
									   &ioComponent.color));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::DragFloat("Font Size", &ioComponent.fontSize, 0.5f, 1.0f, 200.0f);
	const std::string currentAlign{magic_enum::enum_name(ioComponent.alignment)};
	if (ImGui::BeginCombo("Alignment", currentAlign.c_str())) {
		for (const auto& alignName: magic_enum::enum_names<UiText::Alignment>()) {
			const std::string sName{alignName};
			if (ImGui::Selectable(sName.c_str(), currentAlign == sName))
				ioComponent.alignment =
						magic_enum::enum_cast<UiText::Alignment>(sName).value_or(UiText::Alignment::Left);
		}
		ImGui::EndCombo();
	}
	ImGui::DragFloat("Kerning", &ioComponent.kerning, 0.1f);
	ImGui::DragFloat("Line Spacing", &ioComponent.lineSpacing, 0.1f);
}

void renderProps(UiImage& ioComponent) {
	widgets::textureField("Texture", ioComponent.texture);
	ImGui::ColorEdit4(
			"Tint", reinterpret_cast<float*>(&ioComponent.tint));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

void renderProps(UiPanel& ioComponent) {
	ImGui::ColorEdit4("Background",
					  reinterpret_cast<float*>(
							  &ioComponent.backgroundColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4(
			"Border Color",
			reinterpret_cast<float*>(&ioComponent.borderColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::DragFloat("Border Width", &ioComponent.borderWidth, 0.5f, 0.0f, 20.0f);
	const std::string currentLayout{magic_enum::enum_name(ioComponent.layout)};
	if (ImGui::BeginCombo("Layout", currentLayout.c_str())) {
		for (const auto& layoutName: magic_enum::enum_names<UiPanel::Layout>()) {
			const std::string sName{layoutName};
			if (ImGui::Selectable(sName.c_str(), currentLayout == sName))
				ioComponent.layout = magic_enum::enum_cast<UiPanel::Layout>(sName).value_or(UiPanel::Layout::None);
		}
		ImGui::EndCombo();
	}
	if (ioComponent.layout != UiPanel::Layout::None) {
		ImGui::DragFloat("Spacing", &ioComponent.spacing, 0.5f, 0.0f, 100.0f);
		ImGui::DragFloat("Padding", &ioComponent.padding, 0.5f, 0.0f, 100.0f);
	}
}

void renderProps(UiButton& ioComponent) {
	ImGui::ColorEdit4(
			"Normal Color",
			reinterpret_cast<float*>(&ioComponent.normalColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	fieldTooltip("Button background color in the default (not hovered, not pressed) state.");
	ImGui::ColorEdit4(
			"Hover Color",
			reinterpret_cast<float*>(&ioComponent.hoverColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	fieldTooltip("Button background color when the mouse is hovering over it.");
	ImGui::ColorEdit4(
			"Pressed Color",
			reinterpret_cast<float*>(&ioComponent.pressedColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	fieldTooltip("Button background color while the mouse button is held down.");
	ImGui::ColorEdit4(
			"Disabled Color",
			reinterpret_cast<float*>(&ioComponent.disabledColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	fieldTooltip("Button background color when disabled (cannot be clicked).");
	ImGui::InputText("On Click Callback", &ioComponent.onClickCallback);
	fieldTooltip("Lua function name to call when the button is clicked (e.g. on_play_clicked).");
}

void renderProps(UiSlider& ioComponent) {
	ImGui::DragFloat("Value", &ioComponent.value, 0.01f, ioComponent.minValue, ioComponent.maxValue);
	fieldTooltip("Current slider value, clamped between Min and Max.");
	ImGui::DragFloat("Min", &ioComponent.minValue, 0.1f);
	fieldTooltip("Minimum allowed value.");
	ImGui::DragFloat("Max", &ioComponent.maxValue, 0.1f);
	fieldTooltip("Maximum allowed value.");
	ImGui::ColorEdit4(
			"Track Color",
			reinterpret_cast<float*>(&ioComponent.trackColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4(
			"Fill Color",
			reinterpret_cast<float*>(&ioComponent.fillColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4(
			"Handle Color",
			reinterpret_cast<float*>(&ioComponent.handleColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::InputText("On Value Changed", &ioComponent.onValueChangedCallback);
	fieldTooltip("Lua function called when the value changes. Receives the new value in _slider_value.");
}

void renderProps(UiProgressBar& ioComponent) {
	ImGui::DragFloat("Value", &ioComponent.value, 0.01f, 0.0f, 1.0f);
	ImGui::ColorEdit4("Background",
					  reinterpret_cast<float*>(
							  &ioComponent.backgroundColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4(
			"Fill Color",
			reinterpret_cast<float*>(&ioComponent.fillColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

void renderProps(PrefabLink& ioComponent) {
	ImGui::Text("Asset: %s", ioComponent.prefabAssetPath.c_str());
	ImGui::Text("Synced Version: %u", ioComponent.syncedVersion);
	ImGui::Text("Mapped Entities: %zu", ioComponent.uuidMapping.size());
	if (!ioComponent.overriddenComponents.empty())
		ImGui::Text("Overrides: %zu", ioComponent.overriddenComponents.size());
}

void renderProps(RendererTag& ioComponent) {
	const auto& stack = renderer::Renderer::getRenderStack();
	const auto& layers = stack.getLayers();
	const char* preview =
			ioComponent.rendererName.empty() ? "(default — first layer)" : ioComponent.rendererName.c_str();
	if (ImGui::BeginCombo("Layer", preview)) {
		const bool defaultSelected = ioComponent.rendererName.empty();
		if (ImGui::Selectable("(default — first layer)", defaultSelected))
			ioComponent.rendererName.clear();
		if (defaultSelected)
			ImGui::SetItemDefaultFocus();
		for (const auto& layer: layers) {
			const auto& lname = layer->getName();
			const bool selected = (ioComponent.rendererName == lname);
			if (ImGui::Selectable(lname.c_str(), selected))
				ioComponent.rendererName = lname;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	fieldTooltip("Routes this entity to a specific renderer in the project's stack. "
				 "Empty falls back to the first layer.");
	// Surface a one-shot warning if the chosen name doesn't match any active layer.
	if (!ioComponent.rendererName.empty()) {
		const bool known = std::ranges::any_of(
				layers, [&](const auto& l) -> bool { return l->getName() == ioComponent.rendererName; });
		if (!known) {
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.10f, 1.0f), "Unknown layer — entity will be skipped.");
		}
	}
}

void renderProps(Tilemap& ioComponent) {
	// --- Tilemap asset slot (path drop target) --------------------------------------------
	const std::string label =
			ioComponent.tilemapPath.empty() ? "<drop a .owltilemap>" : ioComponent.tilemapPath.generic_string();
	ImGui::TextUnformatted("Tilemap");
	ImGui::SameLine();
	if (ImGui::Button(label.c_str(), ImVec2(-1.f, 0.f))) {
		ioComponent.tilemapPath.clear();
		ioComponent.asset.reset();
	}
	fieldTooltip("Drop a .owltilemap asset here, or click to clear. Tilemap data is authored in the dedicated Tilemap "
				 "Editor (double-click the asset in the Content Browser).");
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Tilemap, dropped)) {
		ioComponent.tilemapPath = dropped;
		ioComponent.asset.reset();// force lazy reload at next scene resolve
	}

	// --- Read-only summary ----------------------------------------------------------------
	if (ioComponent.asset) {
		const auto& asset = *ioComponent.asset;
		ImGui::Separator();
		ImGui::Text("Size: %u × %u cells", asset.width, asset.height);
		ImGui::Text("Cell size: %.3f", static_cast<double>(asset.cellSize));
		const auto tilesetLabel =
				asset.tilesetPath.empty() ? std::string{"<unset>"} : asset.tilesetPath.generic_string();
		ImGui::Text("Tileset: %s", tilesetLabel.c_str());
		ImGui::Text("Layers: %zu", asset.layers.size());
		for (size_t i = 0; i < asset.layers.size(); ++i) {
			const auto& layer = asset.layers[i];
			size_t occupied = 0;
			for (const auto t: layer.tiles)
				if (t >= 0)
					++occupied;
			ImGui::BulletText("[%zu] %s (%s, %zu / %u tiles, parallax %.2f, %.2f)", i,
							  layer.name.empty() ? "<unnamed>" : layer.name.c_str(),
							  layer.visible ? "visible" : "hidden", occupied, asset.width * asset.height,
							  static_cast<double>(layer.parallax.x()), static_cast<double>(layer.parallax.y()));
		}
		if (ioComponent.tilemapPath.empty()) {
			ImGui::Spacing();
			ImGui::TextColored({1.f, 0.7f, 0.4f, 1.f},
							   "Inline tilemap (no path) — open in the editor and Save As to persist.");
		}
	} else if (!ioComponent.tilemapPath.empty()) {
		ImGui::TextDisabled("(asset will be loaded on the next scene resolve)");
	} else {
		ImGui::TextDisabled("(no tilemap assigned)");
	}
}

namespace {
auto interactionKeyCombo(const char* iLabel, input::KeyCode& ioKey) -> bool {
	struct Entry {
		const char* name;
		input::KeyCode code;
	};
	static constexpr std::array entries{
			Entry{"(disabled)", 0},
			Entry{"E", input::key::E},
			Entry{"F", input::key::F},
			Entry{"Q", input::key::Q},
			Entry{"R", input::key::R},
			Entry{"Space", input::key::Space},
			Entry{"Enter", input::key::Enter},
	};
	const char* preview = "(custom)";
	for (const auto& entry: entries) {
		if (entry.code == ioKey) {
			preview = entry.name;
			break;
		}
	}
	bool changed = false;
	if (ImGui::BeginCombo(iLabel, preview)) {
		for (const auto& entry: entries) {
			const bool selected = entry.code == ioKey;
			if (ImGui::Selectable(entry.name, selected)) {
				ioKey = entry.code;
				changed = true;
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	return changed;
}
}// namespace

void renderProps(RaycastDoor& ioComponent) {
	// --- Tileset asset slot ---------------------------------------------------------------
	const std::string tilesetLabel = ioComponent.tilesetPath.empty() ? "<drop a .owltileset>" : ioComponent.tilesetPath;
	ImGui::TextUnformatted("Tileset");
	ImGui::SameLine();
	if (ImGui::Button(tilesetLabel.c_str(), ImVec2(-1.f, 0.f))) {
		ioComponent.tilesetPath.clear();
		ioComponent.tileset.reset();
	}
	fieldTooltip("Drop a .owltileset asset here, or click to clear. Point this at the same tileset "
				 "the world tilemap uses to share the atlas texture — no double-load.");
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Tileset, dropped)) {
		ioComponent.tilesetPath = dropped.generic_string();
		ioComponent.tileset.reset();// force lazy reload at next scene resolve
	}
	tilePickField("Face Tile", ioComponent.tileset, ioComponent.faceTileIndex);
	fieldTooltip("Click the thumbnail to pick the plate tile from the tileset atlas. Typical Wolf3D "
				 "atlas slots: tile 24 (steel door) or 25 (elevator).");
	tilePickField("Lateral Tile", ioComponent.tileset, ioComponent.lateralTileIndex);
	fieldTooltip("Click the thumbnail to pick the jamb tile. Sampled on the two cube inside faces "
				 "perpendicular to the opening direction. Typical Wolf3D atlas slots: tiles 98–105.");
	using OD = RaycastDoor::OpeningDirection;
	constexpr std::array kDirLabels{"North", "South", "East", "West"};
	const auto dirIndex = static_cast<size_t>(ioComponent.openingDirection);
	if (ImGui::BeginCombo("Opening Direction", kDirLabels[dirIndex])) {
		for (size_t i = 0; i < kDirLabels.size(); ++i) {
			const bool selected = i == dirIndex;
			if (ImGui::Selectable(kDirLabels[i], selected))
				ioComponent.openingDirection = static_cast<OD>(i);
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	fieldTooltip("Cardinal direction the plate retracts to when opening. The door always slides "
				 "exactly one cell (plus a 1-pixel margin so the plate is fully hidden when open).");
	ImGui::DragFloat("Open Speed", &ioComponent.slideSpeed, 0.1f, 0.0f, 32.0f, "%.2f c/s");
	fieldTooltip("Animation speed while opening, in cells per second.");
	ImGui::DragFloat("Hold Time", &ioComponent.holdTime, 0.1f, 0.0f, 60.0f, "%.2f s");
	fieldTooltip("How long the door stays fully open before automatically closing. 0 = close immediately.");
	ImGui::DragFloat("Close Speed", &ioComponent.closeSpeed, 0.1f, 0.0f, 32.0f, "%.2f c/s");
	fieldTooltip("Animation speed while closing, in cells per second.");
	interactionKeyCombo("Interaction Key", ioComponent.interactionKey);
	fieldTooltip("Key the player presses to open the door when in range. Pick (disabled) to drive activation "
				 "exclusively from Lua via door.activate(entity_id).");
	ImGui::DragFloat("Interaction Range", &ioComponent.interactionRange, 0.05f, 0.0f, 8.0f, "%.2f cells");
	fieldTooltip("Maximum distance between the player and the door's current centre for the built-in "
				 "interaction key to fire.");
	ImGui::Separator();
	const char* stateName = "idle";
	switch (ioComponent.state) {
		case RaycastDoor::State::Idle:
			stateName = "idle";
			break;
		case RaycastDoor::State::Opening:
			stateName = "opening";
			break;
		case RaycastDoor::State::Open:
			stateName = "open";
			break;
		case RaycastDoor::State::Closing:
			stateName = "closing";
			break;
	}
	ImGui::Text("Runtime: state=%s offset=%.3f hold=%.3f", stateName, static_cast<double>(ioComponent.currentOffset),
				static_cast<double>(ioComponent.holdTimer));
}

void renderProps(RaycastPushWall& ioComponent) {
	const std::string tilesetLabel = ioComponent.tilesetPath.empty() ? "<drop a .owltileset>" : ioComponent.tilesetPath;
	ImGui::TextUnformatted("Tileset");
	ImGui::SameLine();
	if (ImGui::Button(tilesetLabel.c_str(), ImVec2(-1.f, 0.f))) {
		ioComponent.tilesetPath.clear();
		ioComponent.tileset.reset();
	}
	fieldTooltip("Drop a .owltileset asset here, or click to clear. Point this at the same tileset "
				 "the world tilemap uses to share the atlas texture — no double-load.");
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Tileset, dropped)) {
		ioComponent.tilesetPath = dropped.generic_string();
		ioComponent.tileset.reset();
	}
	tilePickField("Tile", ioComponent.tileset, ioComponent.tileIndex);
	fieldTooltip("Click the thumbnail to pick the block tile from the tileset atlas. Sampled on every "
				 "face of the moving cube.");
	ImGui::DragFloat2("Slide Direction", ioComponent.slideDirection.data(), 0.05f, -1.0f, 1.0f, "%.2f");
	fieldTooltip("Unit-length vector along which the pushwall slides when triggered.");
	ImGui::DragFloat("Slide Distance", &ioComponent.slideDistance, 0.05f, 0.0f, 16.0f, "%.2f cells");
	fieldTooltip("Total travel distance, in cells. Pushwalls slide once and stay at Final.");
	ImGui::DragFloat("Slide Speed", &ioComponent.slideSpeed, 0.1f, 0.0f, 32.0f, "%.2f c/s");
	fieldTooltip("Animation speed while moving, in cells per second.");
	interactionKeyCombo("Interaction Key", ioComponent.interactionKey);
	fieldTooltip("Key the player presses to trigger the pushwall when in range. "
				 "Pick (disabled) to drive activation exclusively from Lua via pushwall.activate(entity_id).");
	ImGui::DragFloat("Interaction Range", &ioComponent.interactionRange, 0.05f, 0.0f, 8.0f, "%.2f cells");
	fieldTooltip("Maximum distance between the player and the pushwall's current centre for the built-in "
				 "interaction key to fire.");
	ImGui::Separator();
	const char* stateName = "idle";
	switch (ioComponent.state) {
		case RaycastPushWall::State::Idle:
			stateName = "idle";
			break;
		case RaycastPushWall::State::Moving:
			stateName = "moving";
			break;
		case RaycastPushWall::State::Final:
			stateName = "final";
			break;
	}
	ImGui::Text("Runtime: state=%s offset=%.3f", stateName, static_cast<double>(ioComponent.currentOffset));
}

void renderProps(VoxelWorld& ioComponent) {
	const size_t blockCount = ioComponent.registry.count() > 0 ? ioComponent.registry.count() - 1 : 0;
	ImGui::Text("Blocks: %zu", blockCount);
	ImGui::Text("Chunks: %zu", ioComponent.world.chunkCount());
	const std::string tilesetLabel =
			ioComponent.tilesetPath.empty() ? "<drop a .owltileset>" : ioComponent.tilesetPath.generic_string();
	ImGui::TextUnformatted("Tileset");
	ImGui::SameLine();
	if (ImGui::Button(tilesetLabel.c_str(), ImVec2(-1.f, 0.f))) {
		ioComponent.tilesetPath.clear();
		ioComponent.tileset.reset();
	}
	fieldTooltip("Drop a .owltileset asset here, or click to clear. Block face indices are tile indices into "
				 "this atlas; the renderer maps each face to its tile's UV sub-rect.");
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Tileset, dropped)) {
		ioComponent.tilesetPath = dropped.generic_string();
		ioComponent.tileset.reset();// force lazy reload at next scene resolve
	}
	ImGui::Separator();
	drawVec3Control("Sun Direction", ioComponent.sunDirection);
	ImGui::ColorEdit3("Ambient", ioComponent.ambient.data());
	ImGui::Separator();
	ImGui::Checkbox("Procedural Terrain", &ioComponent.proceduralTerrain);
	fieldTooltip("Stream chunks in/out around the camera from the seed below, instead of authored chunks.");
	if (ioComponent.proceduralTerrain) {
		auto& t = ioComponent.terrain;
		auto dragU32 = [](const char* iLabel, uint32_t& ioValue, const int iMin, const int iMax) -> void {
			int v = static_cast<int>(ioValue);
			if (ImGui::DragInt(iLabel, &v, 1.f, iMin, iMax))
				ioValue = static_cast<uint32_t>(std::clamp(v, iMin, iMax));
		};
		auto dragId = [](const char* iLabel, data::voxel::BlockId& ioValue) -> void {
			int v = static_cast<int>(ioValue);
			if (ImGui::DragInt(iLabel, &v, 1.f, 0, 65535))
				ioValue = static_cast<data::voxel::BlockId>(std::clamp(v, 0, 65535));
		};
		dragU32("Seed", t.seed, 0, 1000000);
		ImGui::DragFloat("Frequency", &t.frequency, 0.001f, 0.001f, 0.5f, "%.4f");
		dragU32("Octaves", t.octaves, 1, 8);
		ImGui::DragFloat("Lacunarity", &t.lacunarity, 0.05f, 1.f, 4.f);
		ImGui::DragFloat("Persistence", &t.persistence, 0.02f, 0.f, 1.f);
		ImGui::DragInt("Base Height", &t.baseHeight);
		ImGui::DragInt("Amplitude", &t.amplitude, 1.f, 0, 256);
		ImGui::DragInt("Sea Level", &t.seaLevel);
		ImGui::DragInt("Dirt Depth", &t.dirtDepth, 1.f, 0, 32);
		ImGui::DragFloat("Cave Frequency", &t.caveFrequency, 0.005f, 0.f, 0.5f, "%.3f");
		ImGui::DragFloat("Cave Threshold", &t.caveThreshold, 0.02f, 0.f, 2.f);
		fieldTooltip("Cave noise above this carves rock to air; >= 1 disables caves.");
		ImGui::Checkbox("Biomes", &t.biomes);
		fieldTooltip("Vary the surface block by a low-frequency biome field (desert / plains / snow / mountain).");
		if (t.biomes)
			ImGui::DragFloat("Biome Frequency", &t.biomeFrequency, 0.001f, 0.001f, 0.1f, "%.4f");
		ImGui::DragInt("Stream Radius", &ioComponent.streamRadius, 1.f, 0, 16);
		ImGui::DragInt("Stream Height", &ioComponent.streamHeight, 1.f, 0, 16);
		dragId("Stone Block", t.stone);
		dragId("Grass Block", t.grass);
		dragId("Dirt Block", t.dirt);
		dragId("Sand Block", t.sand);
		dragId("Water Block", t.water);
		fieldTooltip("Block id used to fill water up to the sea level; 0 (air) disables water.");
		if (t.biomes)
			dragId("Snow Block", t.snow);
		if (ImGui::Button("Regenerate")) {
			ioComponent.world.clear();
			ioComponent.pendingChunks.clear();
		}
		fieldTooltip("Clear streamed chunks so they regenerate from the current parameters.");
	}
}

void renderProps(FlyCamera& ioComponent) {
	ImGui::DragFloat("Move Speed", &ioComponent.moveSpeed, 0.1f, 0.1f, 200.f, "%.2f");
	fieldTooltip("Translation speed in world units per second (WASD move, Space/E up, Left-Shift/Q down).");
	ImGui::DragFloat("Look Speed", &ioComponent.lookSpeed, 0.05f, 0.05f, 10.f, "%.2f");
	fieldTooltip("Rotation speed in radians per second for the arrow-key look controls.");
}

}// namespace owl::gui::component
