/**
 * @file render.cpp
 * @author Silmaen
 * @date 12/30/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "app/Application.h"
#include "app/EngineContext.h"
#include "gui/FontPreviewCache.h"
#include "gui/IconFont.h"
#include "gui/UiLayer.h"
#include "gui/component/render.h"
#include "gui/utils.h"
#include "gui/widgets/AssetField.h"
#include "gui/widgets/Badge.h"
#include "gui/widgets/Combo.h"
#include "gui/widgets/CurveEditor.h"
#include "gui/widgets/IconButton.h"
#include "gui/widgets/ImageTile.h"
#include "gui/widgets/PropertyGrid.h"
#include "gui/widgets/SectionHeader.h"
#include "gui/widgets/Toggle.h"

#include "input/ActionMap.h"
#include "renderer/MeshCache.h"
#include "renderer/Renderer.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"
#include "scene/ui/HudLayout.h"
#include "script/ScriptEngine.h"
#include "sound/SoundCommand.h"
#include "sound/SoundSystem.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

using namespace owl::scene;
using namespace owl::scene::component;

namespace owl::gui::component {

namespace {
namespace prop {

// Inspector rows, same signatures as the ImGui calls they replace: the label on the left (38 %), the widget after it.
template<typename Draw>
auto row(const char* iLabel, Draw&& iDraw) -> bool {
	const char* hidden = std::strstr(iLabel, "##");
	const size_t visible = hidden != nullptr ? static_cast<size_t>(hidden - iLabel) : std::strlen(iLabel);
	if (visible == 0)
		return std::forward<Draw>(iDraw)(iLabel);
	const float startX = ImGui::GetCursorPosX();
	const float labelWidth = std::floor(ImGui::GetContentRegionAvail().x * 0.38f);
	ImGui::AlignTextToFramePadding();
	ImGui::PushStyleColor(ImGuiCol_Text, vec(UiLayer::getTheme().textMuted));
	ImGui::TextUnformatted(iLabel, iLabel + visible);
	ImGui::PopStyleColor();
	ImGui::SameLine(startX + labelWidth);
	ImGui::SetNextItemWidth(-FLT_MIN);
	const std::string id = std::string{"##"} + iLabel;
	return std::forward<Draw>(iDraw)(id.c_str());
}

auto DragFloat(const char* iLabel, float* ioValue, const float iSpeed = 1.f, const float iMin = 0.f,
			   const float iMax = 0.f, const char* iFormat = "%.3f", const ImGuiSliderFlags iFlags = 0) -> bool {
	return row(iLabel, [&](const char* iId) -> bool {
		return ImGui::DragFloat(iId, ioValue, iSpeed, iMin, iMax, iFormat, iFlags);
	});
}

auto DragFloat2(const char* iLabel, float* ioValues, const float iSpeed = 1.f, const float iMin = 0.f,
				const float iMax = 0.f, const char* iFormat = "%.3f", const ImGuiSliderFlags iFlags = 0) -> bool {
	return row(iLabel, [&](const char* iId) -> bool {
		return ImGui::DragFloat2(iId, ioValues, iSpeed, iMin, iMax, iFormat, iFlags);
	});
}

auto DragInt(const char* iLabel, int* ioValue, const float iSpeed = 1.f, const int iMin = 0, const int iMax = 0,
			 const char* iFormat = "%d", const ImGuiSliderFlags iFlags = 0) -> bool {
	return row(iLabel, [&](const char* iId) -> bool {
		return ImGui::DragInt(iId, ioValue, iSpeed, iMin, iMax, iFormat, iFlags);
	});
}

template<typename... Args>
auto ColorEdit4(const char* iLabel, Args&&... iArgs) -> bool {
	return row(iLabel, [&](const char* iId) -> bool { return ImGui::ColorEdit4(iId, std::forward<Args>(iArgs)...); });
}

template<typename... Args>
auto ColorEdit3(const char* iLabel, Args&&... iArgs) -> bool {
	return row(iLabel, [&](const char* iId) -> bool { return ImGui::ColorEdit3(iId, std::forward<Args>(iArgs)...); });
}

template<typename... Args>
auto Checkbox(const char* iLabel, Args&&... iArgs) -> bool {
	return row(iLabel, [&](const char* iId) -> bool { return widgets::toggle(iId, std::forward<Args>(iArgs)...); });
}

template<typename... Args>
auto InputText(const char* iLabel, Args&&... iArgs) -> bool {
	return row(iLabel, [&](const char* iId) -> bool { return ImGui::InputText(iId, std::forward<Args>(iArgs)...); });
}

auto InputTextMultiline(const char* iLabel, std::string* ioText, const ImVec2& iSize = {0.f, 0.f},
						const ImGuiInputTextFlags iFlags = 0) -> bool {
	return row(iLabel, [&](const char* iId) -> bool { return ImGui::InputTextMultiline(iId, ioText, iSize, iFlags); });
}

template<typename... Args>
auto BeginCombo(const char* iLabel, Args&&... iArgs) -> bool {
	return row(iLabel, [&](const char* iId) -> bool { return widgets::beginCombo(iId, std::forward<Args>(iArgs)...); });
}

template<typename... Args>
auto Combo(const char* iLabel, Args&&... iArgs) -> bool {
	return row(iLabel, [&](const char* iId) -> bool { return widgets::combo(iId, std::forward<Args>(iArgs)...); });
}

}// namespace prop
}// namespace


namespace {

auto openAssetHandler() -> std::function<void(const std::filesystem::path&)>& {
	static std::function<void(const std::filesystem::path&)> handler;
	return handler;
}

// Asset path field: its atlas swatch, the path (read only), then Open in its editor and Clear.
auto assetPathRow(const char* iLabel, const std::string& iPath, const char* iHint, const char* iOpenTip,
				  const shared<renderer::gpu::Texture2D>& iPreview = nullptr) -> bool {
	bool cleared = false;
	std::ignore = prop::row(iLabel, [&](const char* iId) -> bool {
		const float side = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		if (iPreview && iPreview->isLoaded()) {
			ImGui::PushID(iId);
			std::ignore = widgets::imageTile("##preview", {.textureId = iPreview->getRendererId(),
														   .uv0 = {0.f, 1.f},
														   .uv1 = {1.f, 0.f},
														   .size = side,
														   .outline = {}});
			ImGui::PopID();
			ImGui::SameLine();
		}
		std::string shown = iPath;
		ImGui::SetNextItemWidth(std::max(side, ImGui::GetContentRegionAvail().x - (side + spacing) * 2.f));
		ImGui::InputTextWithHint(iId, iHint, &shown, ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		ImGui::PushID(iId);
		if (widgets::iconButton("##open", {.glyph = icon::externalLink,
										   .tooltip = "Open",
										   .description = iOpenTip,
										   .disabled = iPath.empty() || !openAssetHandler()}))
			std::ignore = component::openAsset(iPath);
		ImGui::SameLine();
		cleared = widgets::iconButton("##clear", {.glyph = icon::x, .tooltip = "Clear", .disabled = iPath.empty()});
		ImGui::PopID();
		return cleared;
	});
	return cleared;
}

// Read the properties a script declares again when its path or its file changes, keeping the values already set.
void syncScriptProperties(LuaScript& ioComponent) {
	struct Scan {
		std::string path;
		std::filesystem::file_time_type time;
		double checkedAt = -1.0;
	};
	static std::unordered_map<const LuaScript*, Scan> scans;
	if (ioComponent.scriptPath.empty())
		return;
	auto& scan = scans[&ioComponent];
	const double now = ImGui::GetTime();
	if (scan.path == ioComponent.scriptPath && now - scan.checkedAt < 1.0)
		return;
	scan.checkedAt = now;
	std::filesystem::path resolved{ioComponent.scriptPath};
	if (app::Application::instanced()) {
		for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
			if (const auto candidate = assetsPath / ioComponent.scriptPath; exists(candidate)) {
				resolved = candidate;
				break;
			}
		}
	}
	std::error_code error;
	const auto time = std::filesystem::last_write_time(resolved, error);
	if (error || (scan.path == ioComponent.scriptPath && scan.time == time))
		return;
	scan.path = ioComponent.scriptPath;
	scan.time = time;
	auto fresh = script::ScriptEngine::extractProperties(resolved);
	// An unreadable script (syntax error) yields nothing: keep the values set until it reads again.
	if (fresh.empty() && !ioComponent.properties.empty())
		return;
	for (auto& property: fresh) {
		const auto old = std::ranges::find_if(ioComponent.properties, [&property](const auto& iOld) -> bool {
			return iOld.name == property.name && iOld.type == property.type;
		});
		if (old != ioComponent.properties.end())
			property.value = old->value;
	}
	ioComponent.properties = std::move(fresh);
}

void fieldTooltip(const char* iText) {
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
		ImGui::SetTooltip("%s", iText);
}

void drawVec3Control(const std::string& iLabel, math::vec3& iValues, const float iResetValue = 0.0f) {
	std::ignore = prop::row(iLabel.c_str(), [&](const char* iId) -> bool {
		const float resetWidth = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
		std::array values{iValues.x(), iValues.y(), iValues.z()};
		const bool changed =
				widgets::vectorField(iId, values, 0.1f, "%.2f", ImGui::GetContentRegionAvail().x - resetWidth);
		if (changed)
			iValues = {values[0], values[1], values[2]};
		const bool modified = std::abs(iValues.x() - iResetValue) > 1e-6f ||
							  std::abs(iValues.y() - iResetValue) > 1e-6f ||
							  std::abs(iValues.z() - iResetValue) > 1e-6f;
		if (!modified)
			return changed;
		ImGui::SameLine();
		// One reset per row: its ID must carry the row, or Translation, Rotation and Scale would share it.
		ImGui::PushID(iId);
		const bool reset = widgets::iconButton("##reset", {.glyph = icon::rotateCcw, .tooltip = "Reset to default"});
		ImGui::PopID();
		if (reset) {
			iValues = {iResetValue, iResetValue, iResetValue};
			return true;
		}
		return changed;
	});
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
	prop::Checkbox("Primary", &ioComponent.primary);
	fieldTooltip("Mark this camera as the primary camera. Only one primary camera per scene at runtime.");
	if (prop::BeginCombo("Projection", std::string(magic_enum::enum_name(camera.getProjectionType())).c_str())) {
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
		if (prop::DragFloat("Vertical FOV", &perspectiveVerticalFov))
			camera.setPerspectiveVerticalFov(math::radians(perspectiveVerticalFov));
		fieldTooltip("Vertical field of view in degrees. Typical values: 60-90 degrees.");
		float perspectiveNear = camera.getPerspectiveNearClip();
		if (prop::DragFloat("Near", &perspectiveNear))
			camera.setPerspectiveNearClip(perspectiveNear);
		fieldTooltip("Near clipping plane distance. Objects closer than this are not rendered.");
		float perspectiveFar = camera.getPerspectiveFarClip();
		if (prop::DragFloat("Far", &perspectiveFar))
			camera.setPerspectiveFarClip(perspectiveFar);
		fieldTooltip("Far clipping plane distance. Objects farther than this are not rendered.");
	}
	if (camera.getProjectionType() == SceneCamera::ProjectionType::Orthographic) {
		float orthoSize = camera.getOrthographicSize();
		if (prop::DragFloat("Size", &orthoSize))
			camera.setOrthographicSize(orthoSize);
		fieldTooltip("Vertical half-size of the visible area in world units. Smaller = more zoomed in.");
		float orthoNear = camera.getOrthographicNearClip();
		if (prop::DragFloat("Near", &orthoNear))
			camera.setOrthographicNearClip(orthoNear);
		fieldTooltip("Near clipping plane distance.");
		float orthoFar = camera.getOrthographicFarClip();
		if (prop::DragFloat("Far", &orthoFar))
			camera.setOrthographicFarClip(orthoFar);
		fieldTooltip("Far clipping plane distance.");
		prop::Checkbox("Fixed Aspect Ratio", &ioComponent.fixedAspectRatio);
		fieldTooltip("Lock aspect ratio to prevent distortion when the viewport resizes.");
	}
}

void renderProps(SpriteRenderer& ioComponent) {
	prop::ColorEdit4("Color", ioComponent.color.data());
	widgets::textureField("Texture", ioComponent.texture);
	// Tiling: linked by default (same X and Y), checkbox to unlink.
	static bool s_tilingLinked = true;
	prop::Checkbox("Link Tiling X/Y", &s_tilingLinked);
	if (s_tilingLinked) {
		auto uniform = ioComponent.tilingFactor.x();
		if (prop::DragFloat("Tiling Factor", &uniform, 0.1f, 0.0f, 100.0f))
			ioComponent.tilingFactor = {uniform, uniform};
	} else {
		prop::DragFloat2("Tiling Factor", &ioComponent.tilingFactor.x(), 0.1f, 0.0f, 100.0f);
	}
	if (widgets::subHeader("Raycast (billboard)")) {
		fieldTooltip("Optional overrides used only when the entity is rendered through a "
					 "RendererRaycast layer. Leave at defaults to inherit from Transform.");
		prop::DragFloat2("Size", &ioComponent.raycastSize.x(), 0.05f, 0.0f, 16.0f, "%.2f");
		fieldTooltip("World size of the billboard in cells (X = width, Y = height). When either component is 0 "
					 "(default), the renderer falls back to Transform.scale.xy. Use this to keep the editor scale "
					 "small and the first-person size large (or vice-versa).");
		prop::DragFloat("Z Offset", &ioComponent.raycastZOffset, 0.025f, -4.0f, 4.0f, "%.3f");
		fieldTooltip("Vertical offset added to Transform.translation.z when the sprite is drawn in raycast view. "
					 "Positive values raise the sprite (lamps, ceiling decals); negative values lower it (floor "
					 "stains, half-buried props).");
	}
}

void renderProps(AnimatedSpriteRenderer& ioComponent) {
	prop::ColorEdit4("Color", ioComponent.color.data());
	widgets::textureField("Spritesheet", ioComponent.texture);
	if (ioComponent.texture == nullptr)
		widgets::notice("No sprite sheet: the entity draws a plain coloured quad. Drop a texture on the field.");

	const int maxFrame = static_cast<int>(std::max(ioComponent.columns * ioComponent.rows, 1u) - 1);
	int cols = static_cast<int>(ioComponent.columns);
	if (prop::DragInt("Columns", &cols, 1, 1, 256))
		ioComponent.columns = static_cast<uint32_t>(std::max(cols, 1));
	int rows = static_cast<int>(ioComponent.rows);
	if (prop::DragInt("Rows", &rows, 1, 1, 256))
		ioComponent.rows = static_cast<uint32_t>(std::max(rows, 1));
	int first = static_cast<int>(ioComponent.firstFrame);
	if (prop::DragInt("First Frame", &first, 1, 0, maxFrame))
		ioComponent.firstFrame = static_cast<uint32_t>(std::clamp(first, 0, maxFrame));
	int last = static_cast<int>(ioComponent.lastFrame);
	if (prop::DragInt("Last Frame", &last, 1, static_cast<int>(ioComponent.firstFrame), maxFrame))
		ioComponent.lastFrame =
				static_cast<uint32_t>(std::clamp(last, static_cast<int>(ioComponent.firstFrame), maxFrame));
	prop::DragFloat("Frame Duration", &ioComponent.frameDuration, 0.01f, 0.001f, 10.0f, "%.3f s");
	prop::Checkbox("Loop", &ioComponent.loop);
	ImGui::Text("Current Frame: %u", ioComponent.m_currentFrame);
	if (widgets::subHeader("Speed Curve")) {
		fieldTooltip("Optional speed remap. Empty = constant playback speed; non-empty multiplies dt by "
					 "curve.evaluate(progress).");
		widgets::curveEditor("##speedCurve", ioComponent.speedCurve);
	}
	if (widgets::subHeader("Raycast (billboard)")) {
		fieldTooltip("Optional overrides used only when the entity is rendered through a "
					 "RendererRaycast layer. Leave at defaults to inherit from Transform.");
		prop::DragFloat2("Size", &ioComponent.raycastSize.x(), 0.05f, 0.0f, 16.0f, "%.2f");
		fieldTooltip("World size of the billboard in cells (X = width, Y = height). When either component is 0 "
					 "(default), the renderer falls back to Transform.scale.xy.");
		prop::DragFloat("Z Offset", &ioComponent.raycastZOffset, 0.025f, -4.0f, 4.0f, "%.3f");
		fieldTooltip("Vertical offset added to Transform.translation.z when drawn in raycast view. Positive raises, "
					 "negative lowers.");
	}
}

void renderProps(CircleRenderer& ioComponent) {
	prop::ColorEdit4("Color", ioComponent.color.data());
	prop::DragFloat("Thickness", &ioComponent.thickness, 0.025f, 0.0f, 1.0f);
	prop::DragFloat("Fade", &ioComponent.fade, 0.00025f, 0.0f, 1.0f);
}

void renderProps(MeshRenderer& ioComponent) {
	const auto& mesh = ioComponent.mesh;
	auto preview = ioComponent.texture;
	if (!preview && mesh)
		preview = mesh->getBaseColorTexture();
	if (assetPathRow("Mesh", ioComponent.meshPath.generic_string(), "drop a .gltf, .glb or .obj", "Open the mesh file.",
					 preview))
		ioComponent.setMeshPath({});
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Mesh, dropped))
		ioComponent.setMeshPath(dropped);
	prop::ColorEdit4("Color", ioComponent.color.data());
	fieldTooltip("Multiplies the base colour of the mesh material.");
	widgets::textureField("Texture", ioComponent.texture);
	fieldTooltip("Replaces the base-colour texture of the mesh material; empty keeps the material's.");
	prop::Checkbox("Transparent", &ioComponent.transparent);
	fieldTooltip("Draw with alpha blending (the colour alpha), after the opaque meshes and sorted back-to-front.");
	if (mesh) {
		const auto& geometry = mesh->getGeometry();
		ImGui::Text("Vertices: %zu, triangles: %zu", geometry.vertices.size(), geometry.indices.size() / 3);
		ImGui::Text("Material texture: %s", mesh->getBaseColorTexture() ? "yes" : "none");
	} else if (!ioComponent.meshPath.empty()) {
		ImGui::TextDisabled("(loaded on the next frame, or the file cannot be read: see the log)");
	} else {
		widgets::notice("No mesh: drop a .gltf, .glb or .obj file on the field.");
	}
}

void renderProps(Text& ioComponent) {
	prop::InputTextMultiline("Text String", &ioComponent.text, {0, 70});
	if (app::Application::instanced()) {
		auto& fontLib = app::Application::get().getFontLibrary();
		const std::string display = ioComponent.font->isDefault() ? "(default)" : ioComponent.font->getName();
		if (prop::BeginCombo("Font", display.c_str())) {
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

	prop::ColorEdit4("Color", ioComponent.color.data());
	prop::DragFloat("Kerning", &ioComponent.kerning, 0.025f);
	prop::DragFloat("Line Spacing", &ioComponent.lineSpacing, 0.025f);
	prop::DragFloat("Outline Width", &ioComponent.outlineWidth, 0.005f, 0.0f, 1.0f);
	fieldTooltip("Outline around the glyphs, in text heights; 0 draws none.");
	if (ioComponent.outlineWidth > 0.f)
		prop::ColorEdit4("Outline Color", ioComponent.outlineColor.data());
	prop::ColorEdit4("Background", ioComponent.backgroundColor.data());
	fieldTooltip("Box behind the text block; a zero alpha draws none.");
	if (ioComponent.backgroundColor.a() > 0.f) {
		prop::DragFloat("Background Padding", &ioComponent.backgroundPadding, 0.01f, 0.0f, 5.0f);
		fieldTooltip("Margin between the text block and the edge of its background box, in text heights.");
	}
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
	if (prop::BeginCombo("Type", currentName.c_str())) {
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
	prop::Checkbox("Fixed Rotation", &ioComponent.body.fixedRotation);
	fieldTooltip("Lock rotation — the body cannot spin (useful for platformer characters).");
	drawVec3Control("Size", ioComponent.body.colliderSize, 1.0f);
	prop::DragFloat("Density", &ioComponent.body.density, 0.00025f, 0.0f, 10.0f);
	fieldTooltip("Material density. Affects mass proportionally to collider size.");
	prop::DragFloat("Restitution", &ioComponent.body.restitution, 0.00025f, 0.0f, 1.0f);
	fieldTooltip("Bounciness. 0 = no bounce, 1 = perfectly elastic.");
	prop::DragFloat("Friction", &ioComponent.body.friction, 0.00025f, 0.0f, 1.0f);
	fieldTooltip("Surface friction coefficient. 0 = ice, 1 = rubber.");
}

void renderProps(Player& ioComponent) {
	prop::Checkbox("Primary", &ioComponent.primary);
	fieldTooltip("Mark this entity as the primary player. Only one primary player per scene.");
	prop::DragFloat("Linear Impulse", &ioComponent.player.linearImpulse, 0.025f, 0.0f, 10.0f);
	fieldTooltip("Horizontal impulse magnitude applied when moving left/right.");
	prop::DragFloat("Jump Impulse", &ioComponent.player.jumpImpulse, 0.025f, 0.0f, 10.0f);
	fieldTooltip("Vertical impulse magnitude applied on jump.");
	prop::Checkbox("Can jump", &ioComponent.player.canJump);
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
	if (prop::BeginCombo("Type", currentName.c_str())) {
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
		prop::InputText("Scene", &ioComponent.trigger.levelName);

		fieldTooltip("Path to the scene to load (e.g. scenes/victory.owl). Leave empty to use the built-in screen.");
		if (ioComponent.trigger.levelName.empty())

			ImGui::TextDisabled("Empty = built-in screen");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Teleport) {
		prop::InputText("Level Name", &ioComponent.trigger.levelName);

		fieldTooltip("Destination scene path (e.g. scenes/level2.owl).");

		prop::InputText("Target Name", &ioComponent.trigger.targetName);

		fieldTooltip("Name of the entity (Target trigger) that marks where the player appears in the new scene.");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Timer) {
		prop::DragFloat("Duration (s)", &ioComponent.trigger.timerDuration, 0.1f, 0.01f, 3600.0f);

		fieldTooltip("Time in seconds before the callback fires.");

		prop::Checkbox("Repeating", &ioComponent.trigger.timerRepeating);

		fieldTooltip("If checked, the timer restarts after each fire. Otherwise it fires once.");

		prop::InputText("Callback", &ioComponent.trigger.callbackName);

		fieldTooltip("Lua function name to call. Defaults to on_timer if empty.");
		if (ioComponent.trigger.callbackName.empty())

			ImGui::TextDisabled("Default: on_timer");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Interaction) {
		prop::DragFloat("Interaction Range", &ioComponent.trigger.interactionRange, 0.05f, 0.1f, 20.0f);

		fieldTooltip("Distance within which the player can press E to interact.");

		prop::InputText("Callback", &ioComponent.trigger.callbackName);

		fieldTooltip("Lua function name to call. Defaults to on_interact if empty.");
		if (ioComponent.trigger.callbackName.empty())

			ImGui::TextDisabled("Default: on_interact");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::LuaCallback) {
		prop::InputText("Callback", &ioComponent.trigger.callbackName);

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

void renderProps(EntityLink& ioComponent) {
	// A new name designates a new target: drop the UUID so the link binds by name again.
	if (prop::InputText("linked Entity Name", &ioComponent.linkedEntityName))
		ioComponent.linkedEntityId = core::UUID{0};
}

void renderProps(BackgroundTexture& ioComponent) {
	constexpr std::array modeNames = {"Background", "Skybox"};
	int modeIndex = static_cast<int>(ioComponent.mode);
	if (prop::Combo("Mode", &modeIndex, std::span<const char* const>{modeNames}))
		ioComponent.mode = static_cast<BackgroundTexture::Mode>(modeIndex);

	if (ioComponent.mode == BackgroundTexture::Mode::Background) {
		constexpr std::array typeNames = {"Solid Color", "Gradient", "Texture"};
		int typeIndex = static_cast<int>(ioComponent.type);
		if (prop::Combo("Type", &typeIndex, std::span<const char* const>{typeNames}))
			ioComponent.type = static_cast<BackgroundTexture::Type>(typeIndex);

		if (ioComponent.type == BackgroundTexture::Type::SolidColor) {
			prop::ColorEdit4("Color", ioComponent.color.data());
		} else if (ioComponent.type == BackgroundTexture::Type::Gradient) {
			prop::ColorEdit4("Bottom Color", ioComponent.color.data());
			prop::ColorEdit4("Top Color", ioComponent.topColor.data());
		} else {
			// Texture mode
			prop::ColorEdit4("Tint", ioComponent.color.data());
			widgets::textureField("Texture", ioComponent.texture);
		}
	} else {
		// Skybox mode
		widgets::textureField("Skybox Texture", ioComponent.texture);
	}
}

void renderProps(Visibility& ioComponent) {
	prop::Checkbox("Game Visible", &ioComponent.gameVisible);
	prop::Checkbox("Editor Visible", &ioComponent.editorVisible);
}

void renderProps(SoundSource& ioComponent) {
	prop::InputText("Sound Asset", &ioComponent.sound.soundAsset);
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
	if (prop::BeginCombo("Category", currentCategory.c_str())) {
		for (const auto& catName: magic_enum::enum_names<SceneSound::Category>()) {
			const std::string sName{catName};
			if (ImGui::Selectable(sName.c_str(), currentCategory == sName))
				ioComponent.sound.category =
						magic_enum::enum_cast<SceneSound::Category>(sName).value_or(SceneSound::Category::SFX);
		}
		ImGui::EndCombo();
	}
	fieldTooltip("Category of the sound: SFX, Music, or Ambient (stored, not used for mixing yet).");
	prop::DragFloat("Volume", &ioComponent.sound.volume, 0.01f, 0.0f, 2.0f);
	fieldTooltip("Gain factor (0 = silent, 1 = normal, 2 = 200%).");
	prop::DragFloat("Pitch", &ioComponent.sound.pitch, 0.01f, 0.1f, 3.0f);
	fieldTooltip("Pitch multiplier. 1 = normal, 0.5 = octave down, 2 = octave up.");
	prop::Checkbox("Loop", &ioComponent.sound.loop);
	fieldTooltip("If checked, the sound restarts automatically after it finishes.");
	prop::Checkbox("Spatial", &ioComponent.sound.spatial);
	fieldTooltip("Enable 3D positional audio (mono sources only; stereo is not spatialized).");
	prop::Checkbox("Play On Start", &ioComponent.sound.playOnStart);
	fieldTooltip("Automatically start playback when the scene enters Play mode.");
	if (ioComponent.sound.spatial) {
		prop::DragFloat("Max Distance", &ioComponent.sound.maxDistance, 0.5f, 1.0f, 500.0f);
		fieldTooltip("Maximum audible distance in world units.");
		prop::DragFloat("Rolloff", &ioComponent.sound.rolloff, 0.01f, 0.0f, 10.0f);
		fieldTooltip("Distance attenuation curve. Higher = faster falloff.");
	}
}

void renderProps(SoundListener& ioComponent) {
	prop::Checkbox("Primary", &ioComponent.primary);
	fieldTooltip("Mark as the active listener (the 'ear' for 3D audio). Only one primary per scene.");
}

void renderProps(LuaScript& ioComponent) {
	if (assetPathRow("Script", ioComponent.scriptPath, "drop a .lua file", "Open the script in the code editor.")) {
		ioComponent.scriptPath.clear();
		ioComponent.properties.clear();
	}
	if (ioComponent.scriptPath.empty())
		widgets::notice("No script: drop a .lua file from the Content Browser on the field.");
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::LuaScript, dropped))
		ioComponent.scriptPath = dropped.string();
	syncScriptProperties(ioComponent);
	if (!ioComponent.properties.empty() && widgets::subHeader("Properties", true)) {
		for (size_t idx = 0; idx < ioComponent.properties.size(); ++idx) {
			auto& prop = ioComponent.properties[idx];
			// Index-based PushID prevents collisions if two properties share a name.
			ImGui::PushID(static_cast<int>(idx));
			switch (prop.type) {
				case script::ScriptPropertyType::Float:
					{
						auto val = std::get<float>(prop.value);
						if (prop::DragFloat(prop.name.c_str(), &val, 0.1f))
							prop.value = val;
						break;
					}
				case script::ScriptPropertyType::Int:
					{
						auto val = static_cast<int>(std::get<int64_t>(prop.value));
						if (prop::DragInt(prop.name.c_str(), &val))
							prop.value = static_cast<int64_t>(val);
						break;
					}
				case script::ScriptPropertyType::String:
					{
						auto val = std::get<std::string>(prop.value);
						if (prop::InputText(prop.name.c_str(), &val))
							prop.value = val;
						break;
					}
				case script::ScriptPropertyType::Bool:
					{
						auto val = std::get<bool>(prop.value);
						if (prop::Checkbox(prop.name.c_str(), &val))
							prop.value = val;
						break;
					}
			}
			ImGui::PopID();
		}
	}
}

// Combo over every value of an enum, named by magic_enum.
template<typename Enum>
void enumCombo(const char* iLabel, Enum& ioValue) {
	const std::string current{magic_enum::enum_name(ioValue)};
	if (!prop::BeginCombo(iLabel, current.c_str()))
		return;
	for (const auto value: magic_enum::enum_values<Enum>()) {
		const std::string name{magic_enum::enum_name(value)};
		if (ImGui::Selectable(name.c_str(), value == ioValue))
			ioValue = value;
	}
	ImGui::EndCombo();
}

void renderProps(Canvas& ioComponent) {
	enumCombo("Space", ioComponent.space);
	fieldTooltip("ScreenOverlay: drawn by the HUD pass, in pixels on top of every render layer.");
	prop::DragInt("Sort Order", &ioComponent.sortOrder);
	fieldTooltip("Higher sort order renders on top. Used to layer multiple canvases.");
	prop::DragFloat2("Reference Resolution", ioComponent.referenceResolution.data(), 1.0f, 1.0f, 16384.0f);
	fieldTooltip("Resolution the widgets are authored at, in pixels (1280 x 720 by default).");
	enumCombo("Scale Mode", ioComponent.scaleMode);
	fieldTooltip("ConstantPixels: one pixel per pixel. ScaleWithHeight: the reference height fills the screen. "
				 "MatchSmallerSide: the whole reference resolution fits in the screen.");
	prop::Checkbox("Wrap Focus", &ioComponent.wrapFocus);
	fieldTooltip("The keyboard and gamepad focus jumps to the opposite end of the menu when nothing lies ahead.");
}

namespace {

// Frame of the ratio units in reference pixels, from the last HUD layout (nothing when not laid out).
auto unitFrameOf(const Entity& iOwner) -> std::optional<UiRect::Frame> {
	const Scene* const scene = iOwner.getScene();
	if (scene == nullptr)
		return std::nullopt;
	const auto& layout = scene->getHudLayout();
	const auto* node = layout.findNode(static_cast<entt::entity>(iOwner));
	if (node == nullptr || node->scale <= 0.f)
		return std::nullopt;
	const math::vec2 screen =
			math::vec2{static_cast<float>(layout.getViewport().x()), static_cast<float>(layout.getViewport().y())} /
			node->scale;
	UiRect::Frame frame{.parent = {.position = {0.f, 0.f}, .size = screen}, .screen = screen};
	if (node->parent != scene::ui::HudNode::noParent) {
		const auto area = layout.getNodes()[node->parent].childArea();
		frame.parent = {.position = area.position / node->scale, .size = area.size / node->scale};
	}
	return frame;
}

// A number and its unit on one row: the value first, the unit selector at its right.
auto unitField(const char* iLabel, float& ioValue, UiRect::Unit& ioCurrent, UiRect::Unit& oPicked) -> bool {
	static constexpr float unitWidth = 112.f;
	const bool ratio = ioCurrent != UiRect::Unit::Pixels;
	bool picked = false;
	prop::row(iLabel, [&](const char* iId) -> bool {
		ImGui::SetNextItemWidth(-(unitWidth + ImGui::GetStyle().ItemSpacing.x));
		if (ratio)
			ImGui::DragFloat(iId, &ioValue, 0.005f, -10.f, 10.f, "%.3f");
		else
			ImGui::DragFloat(iId, &ioValue, 1.0f, -10000.f, 10000.f, "%.3f");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(unitWidth);
		const std::string unitId = std::string{"##unit"} + iLabel;
		if (widgets::beginCombo(unitId.c_str(), std::string{magic_enum::enum_name(ioCurrent)}.c_str())) {
			for (const auto value: magic_enum::enum_values<UiRect::Unit>()) {
				if (ImGui::Selectable(std::string{magic_enum::enum_name(value)}.c_str(), value == ioCurrent)) {
					oPicked = value;
					picked = true;
				}
			}
			ImGui::EndCombo();
		}
		return picked;
	});
	return picked;
}

}// namespace

void renderProps(UiRect& ioComponent, const Entity& iOwner) {
	const auto preset = ioComponent.getAnchor();
	const std::string currentAnchor = preset ? std::string{magic_enum::enum_name(*preset)} : std::string{"Custom"};
	if (prop::BeginCombo("Anchor Preset", currentAnchor.c_str())) {
		for (const auto value: magic_enum::enum_values<UiRect::Anchor>()) {
			if (ImGui::Selectable(std::string{magic_enum::enum_name(value)}.c_str(), preset == value))
				ioComponent.setAnchor(value);
		}
		ImGui::EndCombo();
	}
	fieldTooltip("Shortcut putting both anchors on one point of the parent (Custom when they differ or stretch).");
	prop::DragFloat2("Anchor Min", ioComponent.anchorMin.data(), 0.01f, 0.0f, 1.0f);
	fieldTooltip("Anchor of the top-left corner in the parent: (0,0) = top-left, (1,1) = bottom-right.");
	prop::DragFloat2("Anchor Max", ioComponent.anchorMax.data(), 0.01f, 0.0f, 1.0f);
	fieldTooltip("Anchor of the bottom-right corner; distinct from Anchor Min, the widget stretches with its parent.");
	prop::DragFloat2("Pivot", ioComponent.pivot.data(), 0.01f, 0.0f, 1.0f);
	fieldTooltip("Point of the widget placed at the anchors plus the offset: (0,0) = top-left, (1,1) = bottom-right.");
	// Changing a unit converts the value, so the widget keeps its place and size.
	const auto frame = unitFrameOf(iOwner);
	UiRect::Unit unit = UiRect::Unit::Pixels;
	const auto field = [&](const char* iLabel, float& ioValue, UiRect::Units& ioUnits, const UiRect::Axis iAxis,
						   const bool iSize) -> void {
		auto current = ioUnits.get(iAxis);
		if (!unitField(iLabel, ioValue, current, unit))
			return;
		if (frame && iSize)
			ioComponent.convertSizeUnit(iAxis, unit, *frame);
		else if (frame)
			ioComponent.convertOffsetUnit(iAxis, unit, *frame);
		else
			ioUnits.set(iAxis, unit);
	};
	field("Width", ioComponent.size.x(), ioComponent.sizeUnit, UiRect::Axis::X, true);
	fieldTooltip("Width added to the span of the anchors. Pixels (reference) or a ratio of the parent or screen width "
				 "or height (0.25 = a quarter).");
	field("Height", ioComponent.size.y(), ioComponent.sizeUnit, UiRect::Axis::Y, true);
	fieldTooltip("Height added to the span of the anchors. Use ParentHeight for both to keep a square.");
	field("Offset X", ioComponent.anchorOffset.x(), ioComponent.offsetUnit, UiRect::Axis::X, false);
	fieldTooltip("Offset of the pivot from its anchor, to the right. Pixels or a ratio, as the size.");
	field("Offset Y", ioComponent.anchorOffset.y(), ioComponent.offsetUnit, UiRect::Axis::Y, false);
	fieldTooltip("Offset of the pivot from its anchor, downwards. Pixels or a ratio, as the size.");
}

void renderProps(UiText& ioComponent) {
	prop::InputTextMultiline("Text", &ioComponent.text, ImVec2(0, 60));
	prop::ColorEdit4("Color", reinterpret_cast<float*>(&ioComponent.color));
	prop::DragFloat("Font Size", &ioComponent.fontSize, 0.5f, 1.0f, 200.0f);
	fieldTooltip("Ascender-to-descender height of the font, in pixels. Glyphs keep the font aspect ratio.");
	enumCombo("Horizontal", ioComponent.horizontalAlignment);
	fieldTooltip("Placement of each line in the rectangle.");
	enumCombo("Vertical", ioComponent.verticalAlignment);
	fieldTooltip("Placement of the block of lines in the rectangle; Baseline puts the first baseline on its centre.");
	prop::Checkbox("Wrap", &ioComponent.wrap);
	fieldTooltip("Break the lines on spaces to stay within the rectangle width.");
	enumCombo("Fit", ioComponent.fit);
	fieldTooltip(
			"Text larger than the rectangle: Overflow draws it whole, Clip cuts it, Shrink reduces the font size.");
	if (ioComponent.fit == data::fonts::TextFit::Shrink) {
		prop::DragFloat("Min Font Size", &ioComponent.minFontSize, 0.5f, 1.0f, 200.0f);
		fieldTooltip("Smallest font size Shrink may reach, in pixels.");
	}
	prop::DragFloat("Kerning", &ioComponent.kerning, 0.01f);
	prop::DragFloat("Line Spacing", &ioComponent.lineSpacing, 0.01f);
	prop::DragFloat("Outline Width", &ioComponent.outlineWidth, 0.1f, 0.0f, 20.0f);
	fieldTooltip("Outline around the glyphs, in reference pixels; 0 draws none. Keeps white text readable on a light "
				 "background.");
	if (ioComponent.outlineWidth > 0.f)
		prop::ColorEdit4("Outline Color", reinterpret_cast<float*>(&ioComponent.outlineColor));
	prop::ColorEdit4("Background", reinterpret_cast<float*>(&ioComponent.backgroundColor));
	fieldTooltip("Box behind the text block; a zero alpha draws none.");
	if (ioComponent.backgroundColor.a() > 0.f) {
		prop::DragFloat("Background Padding", &ioComponent.backgroundPadding, 0.5f, 0.0f, 100.0f);
		fieldTooltip("Margin between the text block and the edge of its background box, in reference pixels.");
	}
}

void renderProps(UiImage& ioComponent) {
	widgets::textureField("Texture", ioComponent.texture);
	prop::ColorEdit4("Tint", reinterpret_cast<float*>(&ioComponent.tint));
}

void renderProps(UiPanel& ioComponent) {
	prop::ColorEdit4("Background", reinterpret_cast<float*>(&ioComponent.backgroundColor));
	prop::ColorEdit4("Border Color", reinterpret_cast<float*>(&ioComponent.borderColor));
	prop::DragFloat("Border Width", &ioComponent.borderWidth, 0.5f, 0.0f, 20.0f);
	prop::Checkbox("Clip Children", &ioComponent.clipChildren);
	fieldTooltip("Cut the children at the panel edges (scissor).");
}

void renderProps(UiButton& ioComponent) {
	prop::ColorEdit4("Normal Color", reinterpret_cast<float*>(&ioComponent.normalColor));
	fieldTooltip("Button background color in the default (not hovered, not pressed) state.");
	prop::ColorEdit4("Hover Color", reinterpret_cast<float*>(&ioComponent.hoverColor));
	fieldTooltip("Button background color when the mouse is hovering over it.");
	prop::ColorEdit4("Pressed Color", reinterpret_cast<float*>(&ioComponent.pressedColor));
	fieldTooltip("Button background color while the mouse button is held down.");
	prop::ColorEdit4("Disabled Color", reinterpret_cast<float*>(&ioComponent.disabledColor));
	fieldTooltip("Button background color when disabled (cannot be clicked).");
	prop::InputText("On Click Callback", &ioComponent.onClickCallback);
	fieldTooltip("Lua function name to call when the button is clicked (e.g. on_play_clicked).");
}

void renderProps(UiSlider& ioComponent) {
	prop::DragFloat("Value", &ioComponent.value, 0.01f, ioComponent.minValue, ioComponent.maxValue);
	fieldTooltip("Current slider value, clamped between Min and Max.");
	prop::DragFloat("Min", &ioComponent.minValue, 0.1f);
	fieldTooltip("Minimum allowed value.");
	prop::DragFloat("Max", &ioComponent.maxValue, 0.1f);
	fieldTooltip("Maximum allowed value.");
	prop::ColorEdit4("Track Color", reinterpret_cast<float*>(&ioComponent.trackColor));
	prop::ColorEdit4("Fill Color", reinterpret_cast<float*>(&ioComponent.fillColor));
	prop::ColorEdit4("Handle Color", reinterpret_cast<float*>(&ioComponent.handleColor));
	prop::DragFloat("Step", &ioComponent.step, 0.01f, 0.0f, 1000.0f);
	fieldTooltip(
			"Change of the value per ui_left / ui_right press on the focused slider. 0: a twentieth of the range.");
	prop::InputText("On Value Changed", &ioComponent.onValueChangedCallback);
	fieldTooltip("Lua function called when the value changes. Receives the new value in _slider_value.");
}

void renderProps(UiProgressBar& ioComponent) {
	prop::DragFloat("Value", &ioComponent.value, 0.01f, 0.0f, 1.0f);
	prop::ColorEdit4("Background", reinterpret_cast<float*>(&ioComponent.backgroundColor));
	prop::ColorEdit4("Fill Color", reinterpret_cast<float*>(&ioComponent.fillColor));
}

void renderProps(UiWindow& ioComponent) {
	prop::InputText("Title", &ioComponent.title);
	fieldTooltip("Text of the title bar.");
	prop::DragFloat("Title Bar Height", &ioComponent.titleBarHeight, 0.5f, 0.0f, 200.0f);
	fieldTooltip("Height of the title bar in reference pixels; the children are laid out under it.");
	prop::DragFloat("Title Font Size", &ioComponent.titleFontSize, 0.5f, 1.0f, 200.0f);
	prop::ColorEdit4("Body Color", reinterpret_cast<float*>(&ioComponent.bodyColor));
	prop::ColorEdit4("Title Color", reinterpret_cast<float*>(&ioComponent.titleColor));
	prop::ColorEdit4("Title Text Color", reinterpret_cast<float*>(&ioComponent.titleTextColor));
	prop::Checkbox("Draggable", &ioComponent.draggable);
	fieldTooltip("The title bar drags the window; it stays inside the screen.");
	prop::Checkbox("Closable", &ioComponent.closable);
	fieldTooltip("Show a close button that hides the window (ui.set_visible shows it again).");
	prop::InputText("On Close Callback", &ioComponent.onCloseCallback);
	fieldTooltip("Lua function called when the close button is pressed.");
}

void renderProps(UiLayout& ioComponent) {
	enumCombo("Arrangement", ioComponent.arrangement);
	fieldTooltip("Row: left to right. Column: top to bottom. Grid: Columns per line, then the next line.");
	enumCombo("Cross Align", ioComponent.crossAlign);
	fieldTooltip("Placement of a child across the arrangement direction (in its cell for a grid).");
	prop::DragFloat("Spacing", &ioComponent.spacing, 0.5f, 0.0f, 200.0f);
	prop::DragFloat("Padding", &ioComponent.padding, 0.5f, 0.0f, 200.0f);
	if (ioComponent.arrangement == UiLayout::Arrangement::Grid) {
		auto columns = static_cast<int>(ioComponent.columns);
		if (prop::DragInt("Columns", &columns, 1.0f, 1, 64))
			ioComponent.columns = static_cast<uint32_t>(std::max(columns, 1));
		prop::DragFloat2("Cell Size", ioComponent.cellSize.data(), 1.0f, 0.0f, 10000.0f);
		fieldTooltip("Size of a cell in reference pixels; 0 on an axis takes the largest child.");
	}
}

void renderProps(UiScrollView& ioComponent) {
	prop::Checkbox("Vertical", &ioComponent.vertical);
	fieldTooltip("The content scrolls up and down.");
	prop::Checkbox("Horizontal", &ioComponent.horizontal);
	fieldTooltip("The content scrolls left and right.");
	prop::DragFloat2("Content Size", ioComponent.contentSize.data(), 1.0f, 0.0f, 100000.0f);
	fieldTooltip("Size of the content in reference pixels; 0 on an axis takes the extent of the children a UiLayout "
				 "of this widget places, else the view (nothing to scroll).");
	prop::DragFloat2("Scroll Offset", ioComponent.scrollOffset.data(), 1.0f, 0.0f, 100000.0f);
	fieldTooltip("Distance the content is scrolled by, in reference pixels; kept inside the content.");
	prop::DragFloat("Wheel Step", &ioComponent.wheelStep, 0.5f, 1.0f, 1000.0f);
	fieldTooltip("Scroll distance of one mouse wheel notch, in reference pixels.");
	prop::Checkbox("Show Scroll Bar", &ioComponent.showScrollBar);
	fieldTooltip("Draw the bars (they take room) when the content overflows.");
	prop::DragFloat("Scroll Bar Width", &ioComponent.scrollBarWidth, 0.5f, 1.0f, 64.0f);
	prop::ColorEdit4("Track Color", reinterpret_cast<float*>(&ioComponent.trackColor));
	prop::ColorEdit4("Thumb Color", reinterpret_cast<float*>(&ioComponent.thumbColor));
}

void renderProps(UiList& ioComponent) {
	auto count = static_cast<int>(ioComponent.itemCount);
	if (prop::DragInt("Item Count", &count, 1.0f, 0, 1000000))
		ioComponent.setCount(static_cast<uint32_t>(std::max(count, 0)));
	fieldTooltip("Number of rows of the model; a script sets it with ui.list_set_count.");
	prop::DragFloat("Row Height", &ioComponent.rowHeight, 0.5f, 1.0f, 500.0f);
	prop::DragFloat("Row Spacing", &ioComponent.rowSpacing, 0.5f, 0.0f, 100.0f);
	auto selected = static_cast<int>(ioComponent.selectedIndex);
	if (prop::DragInt("Selected Index", &selected, 1.0f, -1, std::max(static_cast<int>(ioComponent.itemCount) - 1, -1)))
		ioComponent.select(selected);
	fieldTooltip("Selected row, 0 based (-1 for none).");
	prop::DragFloat("Scroll Offset", &ioComponent.scrollOffset, 1.0f, 0.0f, 10000000.0f);
	fieldTooltip("Distance the rows are scrolled by, in reference pixels; kept inside the content.");
	prop::DragFloat("Wheel Step", &ioComponent.wheelStep, 0.5f, 1.0f, 1000.0f);
	prop::DragFloat("Font Size", &ioComponent.fontSize, 0.5f, 1.0f, 200.0f);
	prop::ColorEdit4("Row Color", reinterpret_cast<float*>(&ioComponent.rowColor));
	prop::ColorEdit4("Alternate Row Color", reinterpret_cast<float*>(&ioComponent.rowAltColor));
	prop::ColorEdit4("Hover Color", reinterpret_cast<float*>(&ioComponent.hoverColor));
	prop::ColorEdit4("Selected Color", reinterpret_cast<float*>(&ioComponent.selectedColor));
	prop::ColorEdit4("Text Color", reinterpret_cast<float*>(&ioComponent.textColor));
	prop::Checkbox("Show Scroll Bar", &ioComponent.showScrollBar);
	prop::DragFloat("Scroll Bar Width", &ioComponent.scrollBarWidth, 0.5f, 1.0f, 64.0f);
	prop::ColorEdit4("Track Color", reinterpret_cast<float*>(&ioComponent.trackColor));
	prop::ColorEdit4("Thumb Color", reinterpret_cast<float*>(&ioComponent.thumbColor));
	prop::InputText("On Select", &ioComponent.onSelectCallback);
	fieldTooltip("Lua function called when the selection changes. Reads _list_entity and _selected_index (1-based).");
	prop::InputText("On Bind", &ioComponent.onBindCallback);
	fieldTooltip("Lua function called once for a row in view nobody filled. Reads _list_entity and _row_index "
				 "(1-based); it fills the row with ui.list_set_row. A row not filled shows Item N.");
}

void renderProps(UiTooltip& ioComponent) {
	prop::InputTextMultiline("Text", &ioComponent.text, ImVec2(0, 60));
	fieldTooltip("Text shown near the mouse once the widget has been hovered for the delay.");
	prop::DragFloat("Delay", &ioComponent.delay, 0.01f, 0.0f, 10.0f);
	fieldTooltip("Hover time before the tooltip shows, in seconds.");
	prop::DragFloat("Font Size", &ioComponent.fontSize, 0.5f, 1.0f, 200.0f);
	prop::DragFloat("Padding", &ioComponent.padding, 0.5f, 0.0f, 100.0f);
	prop::DragFloat2("Offset", ioComponent.offset.data(), 0.5f, -500.0f, 500.0f);
	fieldTooltip("Offset of the box from the mouse, in reference pixels.");
	prop::ColorEdit4("Background", reinterpret_cast<float*>(&ioComponent.backgroundColor));
	prop::ColorEdit4("Text Color", reinterpret_cast<float*>(&ioComponent.textColor));
}

void renderProps(UiToolCursor& ioComponent) {
	widgets::textureField("Texture", ioComponent.texture);
	prop::ColorEdit4("Tint", reinterpret_cast<float*>(&ioComponent.tint));
	prop::DragFloat2("Size", ioComponent.size.data(), 1.0f, 1.0f, 1024.0f);
	fieldTooltip("Size of the sprite in reference pixels.");
	prop::DragFloat2("Hotspot", ioComponent.hotspot.data(), 0.01f, 0.0f, 1.0f);
	fieldTooltip("Point of the sprite put on the mouse: (0,0) = top-left, (1,1) = bottom-right.");
	prop::Checkbox("Active", &ioComponent.active);
	fieldTooltip("Drawn over the HUD in play; a script switches it with ui.set_tool_cursor.");
}

namespace {
// A combo listing the UI entities of the scene by name, with a filter and a "none" entry.
void uiEntityPicker(const char* iLabel, core::UUID& ioTarget, const Entity& iOwner) {
	Scene* const scene = iOwner.getScene();
	const Entity current = scene != nullptr && ioTarget != core::UUID{0} ? scene->findEntityByUUID(ioTarget) : Entity{};
	std::string preview = ioTarget != core::UUID{0} ? "(missing)" : "(nearest)";
	if (current)
		preview = current.getName();
	if (!prop::BeginCombo(iLabel, preview.c_str()))
		return;
	static ImGuiTextFilter s_filter;
	s_filter.Draw("##filter", -FLT_MIN);
	if (ImGui::Selectable("(nearest)", ioTarget == core::UUID{0}))
		ioTarget = core::UUID{0};
	if (scene != nullptr) {
		for (const auto view = scene->registry.view<UiRect, ID>(); const auto entity: view) {
			const Entity candidate{entity, scene};
			if (candidate == iOwner || !s_filter.PassFilter(candidate.getName().c_str()))
				continue;
			const auto uuid = view.get<ID>(entity).id;
			ImGui::PushID(static_cast<int>(static_cast<uint64_t>(uuid) & 0x7fffffff));
			if (ImGui::Selectable(candidate.getName().c_str(), uuid == ioTarget))
				ioTarget = uuid;
			ImGui::PopID();
		}
	}
	ImGui::EndCombo();
}

}// namespace

void renderProps(UiFocus& ioComponent, const Entity& iOwner) {
	prop::Checkbox("Focusable", &ioComponent.focusable);
	fieldTooltip("Whether the keyboard and the gamepad can reach the widget.");
	prop::Checkbox("Initial", &ioComponent.initial);
	fieldTooltip("Takes the focus, shown, as soon as the scene plays; a lost focus restarts from it.");
	uiEntityPicker("Up", ioComponent.up, iOwner);
	fieldTooltip("Widget the focus goes to on ui_up. (nearest): the nearest one above.");
	uiEntityPicker("Down", ioComponent.down, iOwner);
	fieldTooltip("Widget the focus goes to on ui_down. (nearest): the nearest one below.");
	uiEntityPicker("Left", ioComponent.left, iOwner);
	fieldTooltip("Widget the focus goes to on ui_left. (nearest): the nearest one on the left.");
	uiEntityPicker("Right", ioComponent.right, iOwner);
	fieldTooltip("Widget the focus goes to on ui_right. (nearest): the nearest one on the right.");
	prop::InputText("On Focus", &ioComponent.onFocusCallback);
	fieldTooltip("Lua function called when the widget takes the focus. Receives the widget id in _focused_entity.");
	prop::ColorEdit4("Highlight", reinterpret_cast<float*>(&ioComponent.highlightColor));
	prop::DragFloat("Thickness", &ioComponent.highlightThickness, 0.1f, 0.5f, 32.0f);
	fieldTooltip("Thickness of the frame around the focused widget, in reference pixels.");
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
	if (prop::BeginCombo("Layer", preview)) {
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
			ImGui::TextColored(vec(UiLayer::getTheme().warning), "Unknown layer — entity will be skipped.");
		}
	}
}

void renderProps(IsometricObject& ioComponent) {
	const auto& texture = ioComponent.atlas ? ioComponent.atlas->getTexture() : nullptr;
	if (assetPathRow("Sheet", ioComponent.atlasPath.generic_string(), "drop an object sheet .json",
					 "Object sheets are written by tools/iso_atlas.", texture)) {
		ioComponent.atlasPath.clear();
		ioComponent.atlas.reset();
		ioComponent.building.clear();
	}
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::ObjectSheet, dropped)) {
		ioComponent.atlasPath = dropped;
		ioComponent.atlas.reset();
	}
	prop::ColorEdit4("Color", ioComponent.color.data());
	fieldTooltip("Tint multiplied with the sprites of the object.");
	if (ioComponent.atlasPath.empty()) {
		widgets::notice("No sheet: drop the .json of an object sheet from the Content Browser on the field.");
		return;
	}
	if (!ioComponent.atlas) {
		ImGui::TextDisabled("(sheet will be loaded on the next scene resolve)");
		return;
	}
	const auto& objects = ioComponent.atlas->getObjects();
	if (objects.empty()) {
		widgets::notice("The sheet could not be read: check the path and the file.");
		return;
	}
	if (prop::BeginCombo("Building", ioComponent.building.empty() ? "(none)" : ioComponent.building.c_str())) {
		for (const auto& object: objects) {
			const bool isSelected = object.name == ioComponent.building;
			if (ImGui::Selectable(object.name.c_str(), isSelected))
				ioComponent.building = object.name;
			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	fieldTooltip("Object of the sheet. The entity position is the back corner of its footprint.");
	if (const auto* object = ioComponent.getObject(); object != nullptr) {
		ImGui::Text("Footprint: %u x %u tiles", object->footprint.x(), object->footprint.y());
		ImGui::Text("Slices: %zu, height %.2f", object->slices.size(), static_cast<double>(object->getHeight()));
	} else {
		widgets::notice("No such building in the sheet: pick one in the list.");
	}
}

namespace {

NavigationGraphSwitch g_navigationGraphSwitch;

// Name of a tile source in plain words.
auto navSourceLabel(const scene::nav::NavSource iSource) -> const char* {
	return iSource == scene::nav::NavSource::Overlay ? "Overlay (roads, tracks)" : "Terrain (the ground itself)";
}

// Name of a graph reading mode in plain words.
auto navModeLabel(const scene::nav::NavMode iMode) -> const char* {
	switch (iMode) {
		case scene::nav::NavMode::Rules:
			return "Explicit rules (legacy)";
		case scene::nav::NavMode::Connections:
			return "Tileset connections";
		case scene::nav::NavMode::AnyCell:
			return "Any non-empty cell";
	}
	return "";
}

void renderNavigation(scene::nav::NavProfile& ioProfile, const scene::TilemapAsset* const iAsset) {
	ImGui::Separator();
	if (!ImGui::TreeNodeEx("Navigation", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
		return;
	using scene::nav::NavMode;
	const bool legacy = ioProfile.mode == NavMode::Rules;
	if (legacy && ioProfile.rules.empty()) {
		ImGui::TextDisabled("No navigation graph: vehicles and characters cannot look for a path on this tilemap.");
		if (ImGui::Button("Enable navigation"))
			ioProfile.mode = NavMode::Connections;
		fieldTooltip("Build a graph from the roads of the tilemap, as the tileset describes them (Tileset Editor, "
					 "tile connections).");
		ImGui::TreePop();
		return;
	}
	// --- Where the graph is read from ------------------------------------------------------
	const char* layerName = "(no such layer)";
	if (iAsset != nullptr && ioProfile.layer < iAsset->layers.size())
		layerName = iAsset->layers[ioProfile.layer].name.c_str();
	if (prop::BeginCombo("Graph read from layer", layerName)) {
		if (iAsset != nullptr) {
			for (uint32_t i = 0; i < iAsset->layers.size(); ++i) {
				const auto& name = iAsset->layers[i].name;
				ImGui::PushID(static_cast<int>(i));
				if (ImGui::Selectable(name.empty() ? "(unnamed)" : name.c_str(), i == ioProfile.layer))
					ioProfile.layer = i;
				ImGui::PopID();
			}
		}
		ImGui::EndCombo();
	}
	fieldTooltip("The layer of the tilemap whose cells make the graph (the roads).");
	if (prop::BeginCombo("Tile read", navSourceLabel(ioProfile.source))) {
		for (const auto source: {scene::nav::NavSource::Overlay, scene::nav::NavSource::Terrain}) {
			if (ImGui::Selectable(navSourceLabel(source), ioProfile.source == source))
				ioProfile.source = source;
		}
		ImGui::EndCombo();
	}
	fieldTooltip("Which tile of a cell the graph reads: the overlay (roads, tracks) or the ground tile.");
	if (prop::BeginCombo("Read mode", navModeLabel(ioProfile.mode))) {
		for (const auto mode: {NavMode::Connections, NavMode::AnyCell}) {
			if (ImGui::Selectable(navModeLabel(mode), ioProfile.mode == mode))
				ioProfile.mode = mode;
		}
		ImGui::EndCombo();
	}
	fieldTooltip("Tileset connections: a tile is a road when the tileset gives it connections. Any non-empty cell: "
				 "every cell with a tile can be walked on, in the four directions.");
	if (legacy) {
		ImGui::TextWrapped("This profile still lists explicit rules for %zu tile(s).", ioProfile.rules.size());
		if (ImGui::Button("Use the tileset connections instead")) {
			ioProfile.mode = NavMode::Connections;
			ioProfile.rules.clear();
		}
		fieldTooltip("Drops the rules: the roads are read from the connections of the tileset.");
	}
	// --- Slopes ---------------------------------------------------------------------------
	auto rise = static_cast<int>(ioProfile.maxRise);
	if (prop::DragInt("Max slope rise", &rise, 1.f, 0, scene::nav::NavProfile::maxRiseLimit))
		ioProfile.maxRise = static_cast<uint8_t>(std::clamp(rise, 0, int{scene::nav::NavProfile::maxRiseLimit}));
	fieldTooltip("Highest rise, in height steps, a move may cross over one cell.");
	prop::DragFloat("Climb cost", &ioProfile.climbCost, 0.05f, 0.f, 20.f, "%.2f");
	fieldTooltip("Extra cost, in cells, of climbing one height step.");
	prop::Checkbox("Allow steep slopes", &ioProfile.allowSteep);
	fieldTooltip("Whether a cell on a steep slope (a corner two steps above the lowest) can be crossed.");
	// --- Overlay and exceptions -----------------------------------------------------------
	if (g_navigationGraphSwitch.isShown && g_navigationGraphSwitch.setShown) {
		const bool shown = g_navigationGraphSwitch.isShown();
		if (ImGui::Button(shown ? "Hide graph" : "Show graph"))
			g_navigationGraphSwitch.setShown(!shown);
		fieldTooltip("Draw the graph on the viewport: green two-way roads, orange one-way roads.");
	}
	if (!ioProfile.overrides.empty()) {
		ImGui::TextDisabled("%zu cell(s) with an exception (one-way, closed side or own cost).",
							ioProfile.overrides.size());
		ImGui::SameLine();
		if (ImGui::SmallButton("Clear exceptions"))
			ioProfile.overrides.clear();
	}
	ImGui::TextDisabled("One-way roads and slower cells: Navigation tool of the isometric view.");
	ImGui::TreePop();
}

}// namespace

void setNavigationGraphSwitch(NavigationGraphSwitch iSwitch) { g_navigationGraphSwitch = std::move(iSwitch); }

void renderProps(Tilemap& ioComponent) {
	// --- Tilemap asset slot (path drop target) --------------------------------------------
	if (assetPathRow("Tilemap", ioComponent.tilemapPath.generic_string(), "drop a .owltilemap",
					 "Edit the tilemap in the Tilemap Editor.",
					 ioComponent.asset && ioComponent.asset->tileset ? ioComponent.asset->tileset->texture : nullptr)) {
		ioComponent.tilemapPath.clear();
		ioComponent.asset.reset();
	}
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
			const size_t occupied = layer.getOccupiedCount();
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
	renderNavigation(ioComponent.navigation, ioComponent.asset.get());
}

namespace {
auto interactionActionCombo(const char* iLabel, std::string& ioAction) -> bool {
	const auto& actions = app::Application::get().getEngineContext().getActionMap()->getActions();
	const char* preview = ioAction.empty() ? "(disabled)" : ioAction.c_str();
	bool changed = false;
	if (prop::BeginCombo(iLabel, preview)) {
		if (ImGui::Selectable("(disabled)", ioAction.empty())) {
			ioAction.clear();
			changed = true;
		}
		for (const auto& action: actions) {
			if (action.kind != input::ActionKind::Button)
				continue;
			const bool selected = action.name == ioAction;
			if (ImGui::Selectable(action.name.c_str(), selected)) {
				ioAction = action.name;
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
	if (assetPathRow("Tileset", ioComponent.tilesetPath, "drop a .owltileset",
					 "Edit the atlas in the Tileset Editor; use the tileset of the world tilemap to share its texture.",
					 ioComponent.tileset ? ioComponent.tileset->texture : nullptr)) {
		ioComponent.tilesetPath.clear();
		ioComponent.tileset.reset();
	}
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
	if (prop::BeginCombo("Opening Direction", kDirLabels[dirIndex])) {
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
	prop::DragFloat("Open Speed", &ioComponent.slideSpeed, 0.1f, 0.0f, 32.0f, "%.2f c/s");
	fieldTooltip("Animation speed while opening, in cells per second.");
	prop::DragFloat("Hold Time", &ioComponent.holdTime, 0.1f, 0.0f, 60.0f, "%.2f s");
	fieldTooltip("How long the door stays fully open before automatically closing. 0 = close immediately.");
	prop::DragFloat("Close Speed", &ioComponent.closeSpeed, 0.1f, 0.0f, 32.0f, "%.2f c/s");
	fieldTooltip("Animation speed while closing, in cells per second.");
	interactionActionCombo("Interaction Action", ioComponent.interactionAction);
	fieldTooltip("Button action of the project's input map the player triggers to open the door when in range. "
				 "Pick (disabled) to drive activation exclusively from Lua via door.activate(entity_id).");
	prop::DragFloat("Interaction Range", &ioComponent.interactionRange, 0.05f, 0.0f, 8.0f, "%.2f cells");
	fieldTooltip("Maximum distance between the player and the door's current centre for the built-in "
				 "interaction action to fire.");
	ImGui::Separator();
	const char* stateName = nullptr;
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
	if (assetPathRow("Tileset", ioComponent.tilesetPath, "drop a .owltileset",
					 "Edit the atlas in the Tileset Editor; use the tileset of the world tilemap to share its texture.",
					 ioComponent.tileset ? ioComponent.tileset->texture : nullptr)) {
		ioComponent.tilesetPath.clear();
		ioComponent.tileset.reset();
	}
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Tileset, dropped)) {
		ioComponent.tilesetPath = dropped.generic_string();
		ioComponent.tileset.reset();
	}
	tilePickField("Tile", ioComponent.tileset, ioComponent.tileIndex);
	fieldTooltip("Click the thumbnail to pick the block tile from the tileset atlas. Sampled on every "
				 "face of the moving cube.");
	prop::DragFloat2("Slide Direction", ioComponent.slideDirection.data(), 0.05f, -1.0f, 1.0f, "%.2f");
	fieldTooltip("Unit-length vector along which the pushwall slides when triggered.");
	prop::DragFloat("Slide Distance", &ioComponent.slideDistance, 0.05f, 0.0f, 16.0f, "%.2f cells");
	fieldTooltip("Total travel distance, in cells. Pushwalls slide once and stay at Final.");
	prop::DragFloat("Slide Speed", &ioComponent.slideSpeed, 0.1f, 0.0f, 32.0f, "%.2f c/s");
	fieldTooltip("Animation speed while moving, in cells per second.");
	interactionActionCombo("Interaction Action", ioComponent.interactionAction);
	fieldTooltip("Button action of the project's input map the player triggers to open the pushwall when in range. "
				 "Pick (disabled) to drive activation exclusively from Lua via pushwall.activate(entity_id).");
	prop::DragFloat("Interaction Range", &ioComponent.interactionRange, 0.05f, 0.0f, 8.0f, "%.2f cells");
	fieldTooltip("Maximum distance between the player and the pushwall's current centre for the built-in "
				 "interaction action to fire.");
	ImGui::Separator();
	const char* stateName = nullptr;
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
	if (assetPathRow("Tileset", ioComponent.tilesetPath.generic_string(), "drop a .owltileset",
					 "Edit the atlas of the blocks in the Tileset Editor; face indices are its tile indices.",
					 ioComponent.tileset ? ioComponent.tileset->texture : nullptr)) {
		ioComponent.tilesetPath.clear();
		ioComponent.tileset.reset();
	}
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Tileset, dropped)) {
		ioComponent.tilesetPath = dropped.generic_string();
		ioComponent.tileset.reset();// force lazy reload at next scene resolve
	}
	ImGui::Separator();
	drawVec3Control("Sun Direction", ioComponent.sunDirection);
	prop::ColorEdit3("Ambient", ioComponent.ambient.data());
	if (prop::Checkbox("Ambient Occlusion", &ioComponent.ambientOcclusion)) {
		for (const auto& coord: ioComponent.world.chunkCoordinates())
			if (const auto chunk = ioComponent.world.getChunk(coord))
				chunk->markDirty();
	}
	fieldTooltip("Bake per-vertex ambient occlusion that darkens concave block edges; off gives flat lighting.");
	ImGui::Separator();
	prop::Checkbox("Procedural Terrain", &ioComponent.proceduralTerrain);
	fieldTooltip("Stream chunks in/out around the camera from the seed below, instead of authored chunks.");
	if (ioComponent.proceduralTerrain) {
		auto& t = ioComponent.terrain;
		auto dragU32 = [](const char* iLabel, uint32_t& ioValue, const int iMin, const int iMax) -> void {
			int v = static_cast<int>(ioValue);
			if (prop::DragInt(iLabel, &v, 1.f, iMin, iMax))
				ioValue = static_cast<uint32_t>(std::clamp(v, iMin, iMax));
		};
		auto dragId = [](const char* iLabel, data::voxel::BlockId& ioValue) -> void {
			int v = static_cast<int>(ioValue);
			if (prop::DragInt(iLabel, &v, 1.f, 0, 65535))
				ioValue = static_cast<data::voxel::BlockId>(std::clamp(v, 0, 65535));
		};
		dragU32("Seed", t.seed, 0, 1000000);
		prop::DragFloat("Frequency", &t.frequency, 0.001f, 0.001f, 0.5f, "%.4f");
		dragU32("Octaves", t.octaves, 1, 8);
		prop::DragFloat("Lacunarity", &t.lacunarity, 0.05f, 1.f, 4.f);
		prop::DragFloat("Persistence", &t.persistence, 0.02f, 0.f, 1.f);
		prop::DragInt("Base Height", &t.baseHeight);
		prop::DragInt("Amplitude", &t.amplitude, 1.f, 0, 256);
		prop::DragInt("Sea Level", &t.seaLevel);
		prop::DragInt("Dirt Depth", &t.dirtDepth, 1.f, 0, 32);
		prop::DragFloat("Cave Frequency", &t.caveFrequency, 0.005f, 0.f, 0.5f, "%.3f");
		prop::DragFloat("Cave Threshold", &t.caveThreshold, 0.02f, 0.f, 2.f);
		fieldTooltip("Cave noise above this carves rock to air; >= 1 disables caves.");
		prop::Checkbox("Biomes", &t.biomes);
		fieldTooltip("Vary the surface block by a low-frequency biome field (desert / plains / snow / mountain).");
		if (t.biomes)
			prop::DragFloat("Biome Frequency", &t.biomeFrequency, 0.001f, 0.001f, 0.1f, "%.4f");
		prop::DragInt("Stream Radius", &ioComponent.streamRadius, 1.f, 0, 32);
		prop::DragInt("Stream Height", &ioComponent.streamHeight, 1.f, 0, 16);
		fieldTooltip("Runtime view distance in chunks (horizontal radius / vertical half-extent), around the player.");
		prop::DragInt("Editor Stream Radius", &ioComponent.editorStreamRadius, 1.f, 0, 32);
		prop::DragInt("Editor Stream Height", &ioComponent.editorStreamHeight, 1.f, 0, 16);
		fieldTooltip("View distance used while editing (usually larger, to see further); applies in the editor only.");
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

void renderProps(VoxelPlayer& ioComponent) {
	prop::DragFloat("Walk Speed", &ioComponent.walkSpeed, 0.1f, 0.f, 50.f, "%.2f");
	prop::DragFloat("Run Speed", &ioComponent.runSpeed, 0.1f, 0.f, 50.f, "%.2f");
	prop::DragFloat("Jump Speed", &ioComponent.jumpSpeed, 0.1f, 0.f, 50.f, "%.2f");
	prop::DragFloat("Gravity", &ioComponent.gravity, 0.5f, 0.f, 100.f, "%.2f");
	prop::DragFloat("Look Speed", &ioComponent.lookSpeed, 0.05f, 0.05f, 10.f, "%.2f");
	prop::DragFloat("Mouse Sensitivity", &ioComponent.mouseSensitivity, 0.0005f, 0.0001f, 0.05f, "%.4f");
	fieldTooltip(
			"Radians per pixel while the cursor is captured (left-click the viewport to capture, Esc to release).");
	prop::DragFloat("Fly Speed", &ioComponent.flySpeed, 0.5f, 0.f, 100.f, "%.2f");
	prop::DragFloat("Super Speed x", &ioComponent.superSpeedMultiplier, 0.1f, 1.f, 20.f, "%.2f");
	fieldTooltip("Fly mode: double-tap Space to toggle, J toggles super-speed; WASD horizontal, Space up, Shift down.");
	drawVec3Control("Half Extents", ioComponent.halfExtents, 0.4f);
	fieldTooltip("Half the player collision box on each axis (the transform is its centre / eye).");
	prop::DragFloat("Reach", &ioComponent.reach, 0.25f, 1.f, 32.f, "%.2f");
	fieldTooltip("Targeting-ray length in blocks. Left-click breaks the targeted block, right-click places one.");
	int placeBlock = static_cast<int>(ioComponent.placeBlock);
	if (prop::DragInt("Place Block", &placeBlock, 0.1f, 1, 65535))
		ioComponent.placeBlock = static_cast<data::voxel::BlockId>(std::max(1, placeBlock));
	fieldTooltip("Block id placed on right-click (an index into the targeted world's block registry).");
	prop::Checkbox("Capture Cursor", &ioComponent.captureCursor);
	fieldTooltip("Let clicking the viewport hide/lock the cursor for mouse-look (Esc frees it). Off keeps a normal "
				 "cursor and disables block break / place.");
}

void renderProps(FlyCamera& ioComponent) {
	prop::DragFloat("Move Speed", &ioComponent.moveSpeed, 0.1f, 0.1f, 200.f, "%.2f");
	fieldTooltip("Translation speed in world units per second (WASD move, Space/E up, Left-Shift/Q down).");
	prop::DragFloat("Look Speed", &ioComponent.lookSpeed, 0.05f, 0.05f, 10.f, "%.2f");
	fieldTooltip("Rotation speed in radians per second for the arrow-key look controls.");
}

void setOpenAssetHandler(std::function<void(const std::filesystem::path&)> iHandler) {
	openAssetHandler() = std::move(iHandler);
}

auto openAsset(const std::filesystem::path& iPath) -> bool {
	if (iPath.empty() || !openAssetHandler())
		return false;
	openAssetHandler()(iPath);
	return true;
}

}// namespace owl::gui::component
