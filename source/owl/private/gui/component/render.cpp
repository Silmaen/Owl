/**
 * @file render.cpp
 * @author Silmaen
 * @date 12/30/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/Application.h"
#include "gui/FontPreviewCache.h"
#include "gui/component/render.h"
#include "gui/utils.h"
#include "gui/widgets/AssetField.h"
#include "gui/widgets/CurveEditor.h"

#include "renderer/Renderer.h"
#include "sound/SoundCommand.h"
#include "sound/SoundSystem.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

using namespace owl::scene;
using namespace owl::scene::component;

namespace owl::gui::component {

namespace {
/// Attach a tooltip to the last rendered item (shown after a short hover delay).
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
	const bool canPlay = !ioComponent.sound.soundAsset.empty() &&
						 sound::SoundCommand::getState() == sound::SoundAPI::State::Ready;
	ImGui::BeginDisabled(!canPlay);
	if (ImGui::Button(playing ? "Stop##soundPreview" : "Play##soundPreview")) {
		if (playing) {
			sound::SoundCommand::stop(s_previewHandle);
			s_previewHandle = sound::invalidSoundHandle;
		} else {
			auto& library = sound::SoundSystem::getSoundLibrary();
			const auto data = library.exists(ioComponent.sound.soundAsset)
									  ? library.get(ioComponent.sound.soundAsset)
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
	widgets::textureField("Texture", ioComponent.texture);
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
	fieldTooltip("Button background color in the default (not hovered, not pressed) state.");
	ImGui::ColorEdit4("Hover Color", reinterpret_cast<float*>(&ioComponent.hoverColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	fieldTooltip("Button background color when the mouse is hovering over it.");
	ImGui::ColorEdit4("Pressed Color", reinterpret_cast<float*>(&ioComponent.pressedColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	fieldTooltip("Button background color while the mouse button is held down.");
	ImGui::ColorEdit4("Disabled Color", reinterpret_cast<float*>(&ioComponent.disabledColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	fieldTooltip("Button background color when disabled (cannot be clicked).");
	ImGui::InputText("On Click Callback", &ioComponent.onClickCallback);
	fieldTooltip("Lua function name to call when the button is clicked (e.g. on_play_clicked).");
}

void renderProps(UISlider& ioComponent) {
	ImGui::DragFloat("Value", &ioComponent.value, 0.01f, ioComponent.minValue, ioComponent.maxValue);
	fieldTooltip("Current slider value, clamped between Min and Max.");
	ImGui::DragFloat("Min", &ioComponent.minValue, 0.1f);
	fieldTooltip("Minimum allowed value.");
	ImGui::DragFloat("Max", &ioComponent.maxValue, 0.1f);
	fieldTooltip("Maximum allowed value.");
	ImGui::ColorEdit4("Track Color", reinterpret_cast<float*>(&ioComponent.trackColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4("Fill Color", reinterpret_cast<float*>(&ioComponent.fillColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::ColorEdit4("Handle Color", reinterpret_cast<float*>(&ioComponent.handleColor));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	ImGui::InputText("On Value Changed", &ioComponent.onValueChangedCallback);
	fieldTooltip("Lua function called when the value changes. Receives the new value in _slider_value.");
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

void renderProps(Tilemap& ioComponent) {
	// --- Tileset asset slot ----------------------------------------------------------------
	const std::string label =
			ioComponent.tilesetPath.empty() ? "<drop a .owltileset>" : ioComponent.tilesetPath.generic_string();
	ImGui::Text("Tileset");
	ImGui::SameLine();
	if (ImGui::Button(label.c_str(), ImVec2(-1.f, 0.f))) {
		// Click clears the tileset reference.
		ioComponent.tilesetPath.clear();
		ioComponent.tileset.reset();
	}
	fieldTooltip("Drop a .owltileset asset here, or click to clear.");
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Tileset, dropped)) {
		ioComponent.tilesetPath = dropped;
		ioComponent.tileset.reset();// force lazy reload at next render
	}

	// --- Grid dimensions -------------------------------------------------------------------
	int width = static_cast<int>(ioComponent.width);
	int height = static_cast<int>(ioComponent.height);
	if (ImGui::DragInt("Width", &width, 1.f, 1, 1024)) {
		ioComponent.resize(static_cast<uint32_t>(std::max(1, width)), ioComponent.height);
	}
	fieldTooltip("Number of cells horizontally. Existing tiles are preserved on resize.");
	if (ImGui::DragInt("Height", &height, 1.f, 1, 1024)) {
		ioComponent.resize(ioComponent.width, static_cast<uint32_t>(std::max(1, height)));
	}
	fieldTooltip("Number of cells vertically. Existing tiles are preserved on resize.");
	ImGui::DragFloat("Cell Size", &ioComponent.cellSize, 0.05f, 0.01f, 100.f, "%.3f");
	fieldTooltip("World-space size of one cell. The grid is centred on the entity origin.");

	// --- Layers ----------------------------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Layers (%zu)", ioComponent.layers.size());
	ImGui::SameLine();
	if (ImGui::SmallButton("+ Add layer")) {
		ioComponent.addLayer(std::format("layer{}", ioComponent.layers.size()));
	}
	int eraseIndex = -1;
	int moveUpIndex = -1;
	int moveDownIndex = -1;
	for (size_t i = 0; i < ioComponent.layers.size(); ++i) {
		auto& layer = ioComponent.layers[i];
		ImGui::PushID(static_cast<int>(i));
		ImGui::Separator();
		ImGui::InputText("Name", &layer.name);
		ImGui::Checkbox("Visible", &layer.visible);
		float parallax[2] = {layer.parallax.x(), layer.parallax.y()};
		if (ImGui::DragFloat2("Parallax", parallax, 0.01f, 0.f, 4.f, "%.2f")) {
			layer.parallax = math::vec2{parallax[0], parallax[1]};
		}
		fieldTooltip("Per-axis camera-position multiplier. (1, 1) = move with world; (0, 0) = camera-locked.");
		// Tile count occupied (helpful summary).
		size_t occupied = 0;
		for (const auto t: layer.tiles)
			if (t >= 0)
				++occupied;
		ImGui::Text("Tiles: %zu / %u", occupied, ioComponent.width * ioComponent.height);
		// Reorder + delete row.
		if (ImGui::SmallButton("up") && i > 0)
			moveUpIndex = static_cast<int>(i);
		ImGui::SameLine();
		if (ImGui::SmallButton("down") && i + 1 < ioComponent.layers.size())
			moveDownIndex = static_cast<int>(i);
		ImGui::SameLine();
		if (ImGui::SmallButton("delete"))
			eraseIndex = static_cast<int>(i);
		ImGui::PopID();
	}
	if (eraseIndex >= 0) {
		const auto idx = static_cast<size_t>(eraseIndex);
		ioComponent.layers.erase(ioComponent.layers.begin() + static_cast<ptrdiff_t>(idx));
	}
	if (moveUpIndex > 0) {
		const auto idx = static_cast<size_t>(moveUpIndex);
		std::swap(ioComponent.layers[idx], ioComponent.layers[idx - 1]);
	}
	if (moveDownIndex >= 0) {
		const auto idx = static_cast<size_t>(moveDownIndex);
		std::swap(ioComponent.layers[idx], ioComponent.layers[idx + 1]);
	}
}

}// namespace owl::gui::component
