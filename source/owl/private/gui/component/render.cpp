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
#include "gui/widgets/GradientEditor.h"
#include "gui/widgets/IconButton.h"
#include "gui/widgets/ImageTile.h"
#include "gui/widgets/PropertyGrid.h"
#include "gui/widgets/SectionHeader.h"
#include "gui/widgets/Toggle.h"

#include "input/ActionMap.h"
#include "renderer/MaterialLibrary.h"
#include "renderer/MeshCache.h"
#include "renderer/Renderer.h"
#include "renderer/RendererParticles.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"
#include "scene/ui/HudLayout.h"
#include "script/ScriptEngine.h"
#include "script/visual/VisualScript.h"
#include "sound/AudioEffects.h"
#include "sound/Bus.h"
#include "sound/SoundCommand.h"
#include "sound/SoundCue.h"
#include "sound/SoundHelper.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <initializer_list>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

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
	// SetCursorPosX, not SameLine(x): inside a group SameLine adds the group offset to a position that holds it already.
	ImGui::SameLine();
	ImGui::SetCursorPosX(startX + labelWidth);
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

auto DragFloat3(const char* iLabel, float* ioValues, const float iSpeed = 1.f, const float iMin = 0.f,
				const float iMax = 0.f, const char* iFormat = "%.3f", const ImGuiSliderFlags iFlags = 0) -> bool {
	return row(iLabel, [&](const char* iId) -> bool {
		return ImGui::DragFloat3(iId, ioValues, iSpeed, iMin, iMax, iFormat, iFlags);
	});
}

auto InputDouble3(const char* iLabel, double* ioValues, const char* iFormat = "%.4f") -> bool {
	return row(iLabel, [&](const char* iId) -> bool {
		return ImGui::InputScalarN(iId, ImGuiDataType_Double, ioValues, 3, nullptr, nullptr, iFormat);
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

auto editInGraphHandler() -> std::function<void(const std::string&)>& {
	static std::function<void(const std::string&)> handler;
	return handler;
}

// The handler is edited in the visual script of the scene: a graph, or no script yet.
void editInGraphButton(const Scene& iScene, const std::string& iName) {
	const auto& script = iScene.getSceneScript().scriptPath;
	if (!editInGraphHandler() || (!script.empty() && !script::visual::isVisualScriptPath(script)))
		return;
	ImGui::SameLine();
	if (widgets::iconButton("##editInGraph",
							{.glyph = icon::workflow,
							 .tooltip = "Edit in Graph",
							 .description = script.empty() ? "Create the visual script of this scene with this event."
														   : "Open the visual script of this scene on this event."}))
		editInGraphHandler()(iName);
}

auto openAssetAtLineHandler() -> std::function<void(const std::filesystem::path&, uint32_t)>& {
	static std::function<void(const std::filesystem::path&, uint32_t)> handler;
	return handler;
}

// Asset path field: its atlas swatch, the path (read only), then Open in its editor and Clear.
auto assetPathRow(const char* iLabel, const std::string& iPath, const char* iHint, const char* iOpenTip,
				  const shared<renderer::gpu::Texture2D>& iPreview = nullptr, const bool iCanOpen = true) -> bool {
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
										   .disabled = iPath.empty() || !iCanOpen || !openAssetHandler()}))
			std::ignore = component::openAsset(iPath);
		ImGui::SameLine();
		cleared = widgets::iconButton("##clear", {.glyph = icon::x, .tooltip = "Clear", .disabled = iPath.empty()});
		ImGui::PopID();
		return cleared;
	});
	return cleared;
}

// Script path as stored in a component, resolved against the asset folders.
auto resolveScriptPath(const std::string& iScriptPath) -> std::filesystem::path {
	std::filesystem::path resolved{iScriptPath};
	if (!app::Application::instanced())
		return resolved;
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (const auto candidate = assetsPath / iScriptPath; exists(candidate))
			return candidate;
	}
	return resolved;
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
	const auto resolved = resolveScriptPath(ioComponent.scriptPath);
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

// Functions a script defines, read again when its file changes (at most once a second).
auto scriptFunctionsOf(const std::string& iScriptPath) -> const std::vector<script::ScriptFunction>& {
	struct Cache {
		std::vector<script::ScriptFunction> functions;
		std::filesystem::file_time_type time;
		double checkedAt = -1.0;
	};
	static std::unordered_map<std::string, Cache> caches;
	auto& cache = caches[iScriptPath];
	const double now = ImGui::GetTime();
	if (cache.checkedAt >= 0.0 && now - cache.checkedAt < 1.0)
		return cache.functions;
	cache.checkedAt = now;
	const auto resolved = resolveScriptPath(iScriptPath);
	std::error_code error;
	const auto time = std::filesystem::last_write_time(resolved, error);
	if (error) {
		cache.functions.clear();
		return cache.functions;
	}
	if (time != cache.time || cache.functions.empty()) {
		cache.time = time;
		cache.functions = script::ScriptEngine::extractFunctions(resolved);
	}
	return cache.functions;
}

auto findFunction(const std::vector<script::ScriptFunction>& iFunctions, const std::string_view iName)
		-> const script::ScriptFunction* {
	const auto found = std::ranges::find(iFunctions, iName, &script::ScriptFunction::name);
	return found == iFunctions.end() ? nullptr : &*found;
}

auto inspectedScene() -> const Scene*& {
	static const Scene* scene = nullptr;
	return scene;
}

// Small Open button that reveals a function of a script in the code editor.
void openFunctionButton(const char* iId, const std::string& iScriptPath, const uint32_t iLine) {
	ImGui::PushID(iId);
	if (widgets::iconButton("##openFunction", {.glyph = icon::externalLink,
											   .tooltip = "Open",
											   .description = "Open the script at the line of the function.",
											   .disabled = !openAssetHandler() && !openAssetAtLineHandler()}))
		std::ignore = component::openAssetAtLine(iScriptPath, iLine);
	ImGui::PopID();
}

// Inspector row naming a Lua function the engine calls: the name, then the script of the scene that defines it.
void handlerRow(const char* iLabel, std::string& ioName, const char* iDefaultName, const char* iTooltip) {
	prop::InputText(iLabel, &ioName);
	if (iTooltip != nullptr)
		fieldTooltip(iTooltip);
	const std::string name = ioName.empty() && iDefaultName != nullptr ? std::string{iDefaultName} : ioName;
	if (name.empty())
		return;
	const Scene* scene = inspectedScene();
	if (scene == nullptr)
		return;
	ImGui::PushID(iLabel);
	std::string definedIn;
	uint32_t line = 0;
	bool anyScript = false;
	std::vector<const std::string*> paths{&scene->getSceneScript().scriptPath};
	for (const auto view = scene->registry.view<LuaScript>(); const auto entity: view)
		paths.push_back(&view.get<LuaScript>(entity).scriptPath);
	for (const auto* path: paths) {
		if (path->empty())
			continue;
		anyScript = true;
		if (const auto* function = findFunction(scriptFunctionsOf(*path), name); function != nullptr) {
			definedIn = *path;
			line = function->line;
			break;
		}
	}
	if (!definedIn.empty()) {
		if (script::visual::isVisualScriptPath(definedIn))
			ImGui::TextDisabled("%s: %s", name.c_str(), definedIn.c_str());
		else
			ImGui::TextDisabled("%s: %s, line %u", name.c_str(), definedIn.c_str(), line);
		ImGui::SameLine();
		openFunctionButton("##open", definedIn, line);
		if (definedIn == scene->getSceneScript().scriptPath)
			editInGraphButton(*scene, name);
	} else if (scene->getDocumentKind() == DocumentKind::Hud) {
		ImGui::TextDisabled("%s: in the HUD script, or a script of the scene that draws this HUD.", name.c_str());
		editInGraphButton(*scene, name);
	} else if (!anyScript) {
		widgets::notice(std::format("No script in this scene defines {}.", name).c_str());
		editInGraphButton(*scene, name);
	} else {
		widgets::notice(std::format("No script of this scene defines {}.", name).c_str());
		editInGraphButton(*scene, name);
	}
	ImGui::PopID();
}

// The hooks the engine calls on a script: defined or not, with the line and an Open button.
void renderHooks(const LuaScript& iComponent) {
	if (iComponent.scriptPath.empty() || !widgets::subHeader("Hooks", true))
		return;
	const auto& functions = scriptFunctionsOf(iComponent.scriptPath);
	for (const auto& hook: script::ScriptEngine::getHookNames()) {
		const auto* function = findFunction(functions, hook);
		ImGui::PushID(hook.c_str());
		ImGui::AlignTextToFramePadding();
		if (function != nullptr) {
			ImGui::TextUnformatted(hook.c_str());
			ImGui::SameLine();
			if (script::visual::isVisualScriptPath(iComponent.scriptPath))
				ImGui::TextDisabled("in the graph");
			else
				ImGui::TextDisabled("line %u", function->line);
			ImGui::SameLine();
			openFunctionButton("##open", iComponent.scriptPath, function->line);
		} else {
			ImGui::TextDisabled("%s (not defined)", hook.c_str());
		}
		ImGui::PopID();
	}
}

// Accept a Content Browser item whose extension is one of the given ones (lower case, with the dot).
auto extensionDropTarget(const std::initializer_list<std::string_view> iExtensions, std::filesystem::path& oPath)
		-> bool {
	bool accepted = false;
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const auto* const data = static_cast<const char*>(payload->Data);
			const std::filesystem::path candidate{
					std::string(data, ::strnlen(data, static_cast<size_t>(payload->DataSize)))};
			auto extension = candidate.extension().string();
			std::ranges::transform(extension, extension.begin(), [](const unsigned char iChar) -> char {
				return static_cast<char>(std::tolower(iChar));
			});
			if (std::ranges::find(iExtensions, extension) != iExtensions.end()) {
				oPath = candidate;
				accepted = true;
			}
		}
		ImGui::EndDragDropTarget();
	}
	return accepted;
}

// State of a material fields edit: the override mask and whether the fields carry override boxes.
struct MaterialEdit {
	renderer::MaterialParameters& values;
	uint32_t& fields;
	const renderer::MaterialParameters* inherited;
	bool boxes;
	bool changed = false;
	// Fields a reset button was pressed on this frame.
	uint32_t reset = 0;
};

auto defaultMaterial() -> const renderer::MaterialParameters& {
	static const renderer::MaterialParameters s_defaults;
	return s_defaults;
}

// The reset slot left of a field: a button when there is something to reset, else the same blank width.
void resetSlot(MaterialEdit& ioEdit, const uint32_t iBit, const bool iResettable) {
	const float size = ImGui::GetFrameHeight();
	if (!iResettable) {
		ImGui::Dummy({size, size});
	} else {
		if (ImGui::Button(icon::rotateCcw, {size, size})) {
			ioEdit.reset |= iBit;
			ioEdit.changed = true;
		}
		fieldTooltip(ioEdit.boxes ? "Stop overriding: use the material's value." : "Reset to the default value.");
	}
	ImGui::SameLine();
}

// One field: its override box (when asked), its reset button, then its widget, greyed while not overridden.
template<typename T, typename Draw>
void materialField(MaterialEdit& ioEdit, const renderer::MaterialField iField, const char* iLabel, T& ioValue,
				   const T* iInherited, const T& iDefault, Draw&& iDraw) {
	const auto bit = static_cast<uint32_t>(iField);
	bool overridden = true;
	ImGui::PushID(iLabel);
	if (ioEdit.boxes) {
		overridden = (ioEdit.fields & bit) != 0;
		bool checked = overridden;
		if (ImGui::Checkbox("##override", &checked)) {
			if (checked) {
				if (iInherited != nullptr)
					ioValue = *iInherited;
				ioEdit.fields |= bit;
			} else {
				ioEdit.fields &= ~bit;
			}
			overridden = checked;
			ioEdit.changed = true;
		}
		fieldTooltip("Override this field; unchecked, the material's value is used.");
		ImGui::SameLine();
	}
	resetSlot(ioEdit, bit, ioEdit.boxes ? overridden : !std::equal_to<T>{}(ioValue, iDefault));
	ImGui::PopID();
	T shown = (overridden || iInherited == nullptr) ? ioValue : *iInherited;
	ImGui::BeginDisabled(!overridden);
	if (std::forward<Draw>(iDraw)(iLabel, shown) && overridden) {
		ioValue = shown;
		ioEdit.changed = true;
	}
	ImGui::EndDisabled();
}

// A field named by its member of `MaterialParameters`.
template<typename T, typename Draw>
void memberField(MaterialEdit& ioEdit, const renderer::MaterialField iField, const char* iLabel,
				 T renderer::MaterialParameters::* iMember, Draw&& iDraw) {
	materialField(ioEdit, iField, iLabel, ioEdit.values.*iMember,
				  ioEdit.inherited != nullptr ? &(ioEdit.inherited->*iMember) : nullptr, defaultMaterial().*iMember,
				  std::forward<Draw>(iDraw));
}

auto drawUnitFloat(const char* iLabel, float& ioValue, const float iMin = 0.f, const float iMax = 1.f) -> bool {
	return prop::DragFloat(iLabel, &ioValue, 0.005f, iMin, iMax);
}

// A field of a component with the reset slot left of it: a button bringing back the default while the value differs.
template<typename T, typename Draw>
auto resettable(const char* iLabel, T& ioValue, const T& iDefault, Draw&& iDraw) -> bool {
	const float size = ImGui::GetFrameHeight();
	bool changed = false;
	ImGui::PushID(iLabel);
	if (std::equal_to<T>{}(ioValue, iDefault)) {
		ImGui::Dummy({size, size});
	} else {
		if (ImGui::Button(icon::rotateCcw, {size, size})) {
			ioValue = iDefault;
			changed = true;
		}
		fieldTooltip("Reset to the default value.");
	}
	ImGui::PopID();
	ImGui::SameLine();
	changed |= std::forward<Draw>(iDraw)(iLabel, ioValue);
	return changed;
}

// A float field with its reset slot.
auto resetFloat(const char* iLabel, float& ioValue, const float iDefault, const float iSpeed, const float iMin,
				const float iMax, const char* iFormat = "%.3f", const ImGuiSliderFlags iFlags = 0) -> bool {
	return resettable(iLabel, ioValue, iDefault, [&](const char* iId, float& ioField) -> bool {
		return prop::DragFloat(iId, &ioField, iSpeed, iMin, iMax, iFormat, iFlags);
	});
}

// An unsigned integer field with its reset slot, clamped to a range.
auto resetUint(const char* iLabel, uint32_t& ioValue, const uint32_t iDefault, const float iSpeed, const uint32_t iMin,
			   const uint32_t iMax) -> bool {
	return resettable(iLabel, ioValue, iDefault, [&](const char* iId, uint32_t& ioField) -> bool {
		int value = static_cast<int>(ioField);
		if (!prop::DragInt(iId, &value, iSpeed, static_cast<int>(iMin), static_cast<int>(iMax)))
			return false;
		ioField = static_cast<uint32_t>(std::clamp(value, static_cast<int>(iMin), static_cast<int>(iMax)));
		return true;
	});
}

// A boolean field with its reset slot.
auto resetCheckbox(const char* iLabel, bool& ioValue, const bool iDefault) -> bool {
	return resettable(iLabel, ioValue, iDefault,
					  [](const char* iId, bool& ioField) -> bool { return prop::Checkbox(iId, &ioField); });
}

// An sRGB colour field with its reset slot.
auto resetColor(const char* iLabel, math::vec3& ioValue, const math::vec3& iDefault) -> bool {
	return resettable(iLabel, ioValue, iDefault, [](const char* iId, math::vec3& ioField) -> bool {
		return prop::ColorEdit3(iId, ioField.data());
	});
}

// An angle stored in radians, edited in degrees, with its reset slot.
auto resetDegrees(const char* iLabel, float& ioRadians, const float iDefault, const float iMin, const float iMax)
		-> bool {
	return resettable(iLabel, ioRadians, iDefault, [&](const char* iId, float& ioField) -> bool {
		float degrees = math::degrees(ioField);
		if (!prop::DragFloat(iId, &degrees, 0.5f, iMin, iMax, "%.1f deg"))
			return false;
		ioField = math::radians(std::clamp(degrees, iMin, iMax));
		return true;
	});
}

auto drawDegrees(const char* iLabel, float& ioRadians) -> bool {
	float degrees = math::degrees(ioRadians);
	if (!prop::DragFloat(iLabel, &degrees, 0.5f, -180.f, 180.f, "%.1f deg"))
		return false;
	ioRadians = math::radians(degrees);
	return true;
}

template<typename Enum>
auto drawEnum(const char* iLabel, Enum& ioValue) -> bool {
	bool changed = false;
	const std::string current{magic_enum::enum_name(ioValue)};
	if (!prop::BeginCombo(iLabel, current.c_str()))
		return false;
	for (const auto value: magic_enum::enum_values<Enum>()) {
		const std::string name{magic_enum::enum_name(value)};
		if (ImGui::Selectable(name.c_str(), value == ioValue)) {
			ioValue = value;
			changed = true;
		}
	}
	ImGui::EndCombo();
	return changed;
}

// Texture map of a material, stored as a serialized texture name (`nam:` relative to the assets, `pat:` absolute).
auto drawMapField(const char* iLabel, std::string& ioName) -> bool {
	const bool relative = ioName.starts_with("nam:");
	const bool hasPrefix = relative || ioName.starts_with("pat:");
	const std::string shown = hasPrefix ? ioName.substr(4) : ioName;
	bool changed = false;
	if (assetPathRow(iLabel, shown, "drop an image", "Open the texture.")) {
		ioName.clear();
		changed = true;
	}
	if (std::filesystem::path dropped; extensionDropTarget({".png", ".jpg", ".jpeg", ".bmp", ".tga"}, dropped)) {
		ioName = std::string{dropped.is_absolute() ? "pat:" : "nam:"} + dropped.generic_string();
		changed = true;
	}
	return changed;
}

constexpr std::array<const char*, renderer::g_materialMapCount> g_materialMapLabels{
		"Base colour map", "Metallic map", "Normal map", "Occlusion map", "Emissive map", "Roughness map"};

// Colour of a field that is not a number: sRGB colour edit.
template<typename Vec>
auto drawColor(const char* iLabel, Vec& ioColor) -> bool {
	if constexpr (std::is_same_v<Vec, math::vec4>)
		return prop::ColorEdit4(iLabel, ioColor.data());
	else
		return prop::ColorEdit3(iLabel, ioColor.data());
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

void renderProps(WorldPosition& ioComponent) {
	prop::InputDouble3("Position", &ioComponent.position.x());
	fieldTooltip("Position in the world, in double precision. The Transform translation is this position minus the "
				 "world origin of the scene; only a root entity uses it.");
}

namespace {

// "Post process" section of a camera: exposure, bloom, vignette, colour-grading LUT and the lens effects.
void renderPostProcess(renderer::Camera& ioCamera) {
	if (!ImGui::TreeNodeEx("Post process", ImGuiTreeNodeFlags_SpanAvailWidth))
		return;
	auto settings = ioCamera.getPostProcess();
	const renderer::PostProcessSettings defaults;
	bool changed = false;
	changed |= resetFloat("EV100", settings.ev100, defaults.ev100, 0.05f, -6.f, 20.f, "%.2f");
	fieldTooltip("Exposure value of the camera at ISO 100: 15 sunny day, 12 overcast, 8 bright interior, 5 dim room, "
				 "3 candle light. One step up halves the brightness.");
	changed |= resetCheckbox("Auto exposure", settings.autoExposure, defaults.autoExposure);
	fieldTooltip("Follow the average brightness of the frame, like the eye, instead of the EV100 above.");
	if (settings.autoExposure) {
		changed |= resetFloat("Darkest (EV100)", settings.autoExposureMin, defaults.autoExposureMin, 0.05f, -6.f, 20.f,
							  "%.2f");
		fieldTooltip("Darkest scene the exposure adapts to; darker ones look dark.");
		changed |= resetFloat("Brightest (EV100)", settings.autoExposureMax, defaults.autoExposureMax, 0.05f, -6.f,
							  20.f, "%.2f");
		fieldTooltip("Brightest scene the exposure adapts to; brighter ones look bright.");
		changed |= resetFloat("Adaptation (EV/s)", settings.autoExposureSpeed, defaults.autoExposureSpeed, 0.02f, 0.f,
							  20.f, "%.2f");
		fieldTooltip("Speed the exposure follows a change of brightness, in stops per second.");
	}
	changed |= resetFloat("Compensation", settings.exposure, defaults.exposure, 0.02f, -10.f, 10.f, "%.2f EV");
	fieldTooltip("Exposure compensation in stops on top of the exposure: +1 doubles the brightness.");
	changed |= resetCheckbox("Bloom", settings.bloom, defaults.bloom);
	fieldTooltip("The bright parts of the image glow.");
	if (settings.bloom) {
		changed |= resetFloat("Bloom threshold", settings.bloomThreshold, defaults.bloomThreshold, 0.01f, 0.f, 100.f);
		fieldTooltip("Displayed brightness above which the colour blooms (1 is white).");
		changed |= resetFloat("Bloom knee", settings.bloomKnee, defaults.bloomKnee, 0.005f, 0.f, 1.f);
		fieldTooltip("Softness of the threshold, 0 hard to 1 soft.");
		changed |= resetFloat("Bloom intensity", settings.bloomIntensity, defaults.bloomIntensity, 0.005f, 0.f, 10.f);
		fieldTooltip("Strength of the glow added to the scene.");
		changed |= resetFloat("Bloom scatter", settings.bloomScatter, defaults.bloomScatter, 0.005f, 0.f, 1.f);
		fieldTooltip("Spread of the glow, 0 tight to 1 wide.");
	}
	changed |= resetCheckbox("Vignette", settings.vignette, defaults.vignette);
	fieldTooltip("The edges of the image darken.");
	if (settings.vignette) {
		changed |= resetFloat("Vignette intensity", settings.vignetteIntensity, defaults.vignetteIntensity, 0.005f, 0.f,
							  1.f);
		fieldTooltip("Strength of the vignette.");
		changed |= resetFloat("Vignette smoothness", settings.vignetteSmoothness, defaults.vignetteSmoothness, 0.005f,
							  0.f, 1.f);
		fieldTooltip("Softness of the vignette edge.");
		changed |= resetColor("Vignette colour", settings.vignetteColor, defaults.vignetteColor);
		fieldTooltip("Colour the edges fade to.");
	}
	if (assetPathRow("Colour LUT", settings.lut, "drop a .cube", "Colour-grading look-up table.", nullptr, false)) {
		settings.lut.clear();
		changed = true;
	}
	if (std::filesystem::path dropped; extensionDropTarget({".cube"}, dropped)) {
		settings.lut = dropped.generic_string();
		changed = true;
	}
	if (!settings.lut.empty()) {
		changed |= resetFloat("LUT contribution", settings.lutContribution, defaults.lutContribution, 0.005f, 0.f, 1.f);
		fieldTooltip("Weight of the look-up table, 0 none to 1 fully graded.");
	}
	changed |= resetCheckbox("Chromatic aberration", settings.chromaticAberration, defaults.chromaticAberration);
	fieldTooltip("The red and blue channels split towards the edges of the image, like a simple lens.");
	if (settings.chromaticAberration) {
		changed |= resetFloat("Aberration intensity", settings.chromaticAberrationIntensity,
							  defaults.chromaticAberrationIntensity, 0.005f, 0.f, 1.f);
		fieldTooltip("Strength of the split: 1 puts the channels 1 % of the frame width apart at the corners.");
	}
	changed |= resetCheckbox("Film grain", settings.filmGrain, defaults.filmGrain);
	fieldTooltip("A grain over the image, a new one each frame, strongest in the mid-tones.");
	if (settings.filmGrain) {
		changed |= resetFloat("Grain intensity", settings.filmGrainIntensity, defaults.filmGrainIntensity, 0.005f, 0.f,
							  1.f);
		fieldTooltip("Strength of the grain.");
		changed |= resetFloat("Grain size", settings.filmGrainSize, defaults.filmGrainSize, 0.01f, 1.f, 4.f, "%.2f px");
		fieldTooltip("Size of a grain, in pixels.");
	}
	changed |= resetCheckbox("Depth of field", settings.depthOfField, defaults.depthOfField);
	fieldTooltip("What is away from the focus distance blurs, the 3D layers by their depth.");
	if (settings.depthOfField) {
		changed |= resetFloat("Focus distance", settings.focusDistance, defaults.focusDistance, 0.05f, 0.f, 10000.f);
		fieldTooltip("Distance from the camera that stays sharp, in world units.");
		changed |= resetFloat("Focus range", settings.focusRange, defaults.focusRange, 0.05f, 0.001f, 10000.f);
		fieldTooltip("Distance from the focus over which the blur grows to its largest, in world units.");
		changed |= resetFloat("Bokeh radius", settings.bokehRadius, defaults.bokehRadius, 0.05f, 0.f, 32.f, "%.1f px");
		fieldTooltip("Largest blur radius, in pixels of a frame 1080 pixels high.");
	}
	changed |= resetCheckbox("Motion blur", settings.motionBlur, defaults.motionBlur);
	fieldTooltip("The image blurs along the motion of the camera, in Play only.");
	if (settings.motionBlur) {
		changed |= resetFloat("Shutter", settings.motionBlurIntensity, defaults.motionBlurIntensity, 0.005f, 0.f, 1.f);
		fieldTooltip("Fraction of the frame the shutter stays open: 1 blurs over the whole motion of a frame.");
	}
	changed |= resetCheckbox("Lens flares", settings.lensFlare, defaults.lensFlare);
	fieldTooltip("The lights with a flare strength (Light inspector) flare in the lens: a glow, a halo and ghosts, "
				 "hidden behind the meshes and voxels that cover them.");
	if (settings.lensFlare) {
		changed |= resetFloat("Flare intensity", settings.lensFlareIntensity, defaults.lensFlareIntensity, 0.01f, 0.f,
							  100.f);
		fieldTooltip("Strength of every flare.");
		changed |= resettable("Flare ghosts", settings.lensFlareGhosts, defaults.lensFlareGhosts,
							  [](const char* iId, uint32_t& ioField) -> bool {
								  int ghosts = static_cast<int>(ioField);
								  if (!prop::DragInt(iId, &ghosts, 0.05f, 0, 8))
									  return false;
								  ioField = static_cast<uint32_t>(std::clamp(ghosts, 0, 8));
								  return true;
							  });
		fieldTooltip("Discs along the line from a light through the centre of the image.");
		changed |= resetFloat("Ghost spacing", settings.lensFlareSpacing, defaults.lensFlareSpacing, 0.005f, 0.f, 2.f);
		fieldTooltip("Distance between two ghosts, as a fraction of the line from the centre to the light.");
		changed |= resetFloat("Halo radius", settings.lensFlareHalo, defaults.lensFlareHalo, 0.005f, 0.f, 2.f);
		fieldTooltip("Radius of the ring around the centre of the image, as a fraction of its height.");
		changed |= resetFloat("Glow size", settings.lensFlareSize, defaults.lensFlareSize, 0.002f, 0.f, 1.f);
		fieldTooltip("Size of the glow and streaks at a light, as a fraction of the image height.");
	}
	ImGui::TreePop();
	if (changed)
		ioCamera.setPostProcess(settings);
}

}// namespace

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
	if (prop::BeginCombo("Tone mapping", std::string(magic_enum::enum_name(camera.getToneMapping())).c_str())) {
		for (const renderer::ToneMapping& tone: magic_enum::enum_values<renderer::ToneMapping>()) {
			const bool isSelected = camera.getToneMapping() == tone;
			if (ImGui::Selectable(std::string(magic_enum::enum_name(tone)).c_str(), isSelected))
				camera.setToneMapping(tone);
			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	fieldTooltip("Operator mapping the HDR scene colour to the display range. None keeps the colour unchanged; "
				 "PbrNeutral preserves base colours; Aces gives a filmic look.");
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
	renderPostProcess(camera);
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
	if (widgets::subHeader("2D Lighting")) {
		resetCheckbox("Lit", ioComponent.lit, true);
		fieldTooltip("Whether the Light 2D entities of the scene light the sprite; an unlit sprite keeps its colour "
					 "(glows, markers).");
		widgets::textureField("Normal Map", ioComponent.normalMap);
		fieldTooltip("Tangent-space normal map (OpenGL convention, green up), sampled like the texture: the 2D lights "
					 "slant on its bumps.");
		resetCheckbox("Cast Shadows", ioComponent.castShadows, false);
		fieldTooltip("The sprite's quad blocks the Light 2D entities casting shadows; the sprite itself stays lit.");
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
	prop::Checkbox("Lit", &ioComponent.lit);
	fieldTooltip("Lit by the 2D lights; off keeps the sprite as drawn (badges, markers).");
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

namespace {

// Name of a map or empty text for a file material slot.
auto fileMaterialOf(const MeshRenderer& iComponent, const size_t iSlot) -> shared<const renderer::Material> {
	if (!iComponent.mesh || iSlot >= iComponent.mesh->getSubmeshes().size())
		return nullptr;
	return iComponent.mesh->getFileMaterial(iSlot);
}

// Values a slot's overrides replace: its resolved material, else the material it names, else the file's.
auto inheritedValuesOf(const MeshRenderer& iComponent, const MeshRenderer::MaterialSlot& iSlot, const size_t iIndex)
		-> std::optional<renderer::MaterialParameters> {
	if (iSlot.resolved)
		return iSlot.resolved->getParameters();
	if (!iSlot.material.empty() && app::Application::instanced()) {
		if (const auto material = app::Application::get().getEngineContext().getMaterialLibrary().get(iSlot.material))
			return material->getParameters();
		return std::nullopt;
	}
	if (const auto material = fileMaterialOf(iComponent, iIndex))
		return material->getParameters();
	return std::nullopt;
}

// One material slot: the material asset, then the fields it overrides for this entity.
auto renderMaterialSlot(const MeshRenderer& iComponent, MeshRenderer::MaterialSlot& ioSlot, const size_t iIndex)
		-> bool {
	bool changed = false;
	ImGui::PushID(static_cast<int>(iIndex));
	const auto fileMaterial = fileMaterialOf(iComponent, iIndex);
	if (ioSlot.material.empty() && fileMaterial) {
		ImGui::ColorButton("##fileColor", vec(fileMaterial->getParameters().baseColor),
						   ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip);
		ImGui::SameLine();
		ImGui::TextDisabled("File material");
	}
	if (assetPathRow("Material", ioSlot.material.generic_string(), "File material", "Open the material.")) {
		ioSlot.material.clear();
		changed = true;
	}
	if (std::filesystem::path dropped; extensionDropTarget({".owlmat"}, dropped)) {
		ioSlot.material = dropped;
		changed = true;
	}
	if (ImGui::TreeNodeEx("Overrides", ImGuiTreeNodeFlags_SpanAvailWidth)) {
		const auto inherited = inheritedValuesOf(iComponent, ioSlot, iIndex);
		if (renderMaterialFields(ioSlot.overrides.values, ioSlot.overrides.fields, true,
								 inherited ? &*inherited : nullptr))
			changed = true;
		ImGui::TreePop();
	}
	ImGui::PopID();
	if (changed)
		ioSlot.invalidate();
	return changed;
}

// The "Materials" section of a mesh renderer: one slot per submesh of the loaded mesh.
void renderMaterialSlots(MeshRenderer& ioComponent) {
	const size_t count = ioComponent.mesh ? ioComponent.mesh->getSubmeshes().size() : ioComponent.materials.size();
	if (count == 0 ||
		!ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
		return;
	for (size_t index = 0; index < count; ++index) {
		const auto label = std::format("Slot {}", index);
		if (!ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
			continue;
		if (index < ioComponent.materials.size()) {
			std::ignore = renderMaterialSlot(ioComponent, ioComponent.materials[index], index);
		} else {
			MeshRenderer::MaterialSlot scratch;
			if (renderMaterialSlot(ioComponent, scratch, index)) {
				ioComponent.materials.resize(index + 1);
				ioComponent.materials[index] = std::move(scratch);
			}
		}
		ImGui::TreePop();
	}
	ImGui::TreePop();
}

}// namespace

auto renderMaterialFields(renderer::MaterialParameters& ioValues, uint32_t& ioFields, const bool iShowOverrides,
						  const renderer::MaterialParameters* iInherited) -> bool {
	using Field = renderer::MaterialField;
	using P = renderer::MaterialParameters;
	MaterialEdit edit{.values = ioValues, .fields = ioFields, .inherited = iInherited, .boxes = iShowOverrides};
	const auto unit = [](const char* iLabel, float& ioValue) -> bool { return drawUnitFloat(iLabel, ioValue); };
	const auto channel = drawEnum<renderer::TextureChannel>;

	const bool anything = iShowOverrides ? ioFields != 0 : ioValues.differingFields(defaultMaterial()) != 0;
	ImGui::BeginDisabled(!anything);
	if (ImGui::Button(std::format("{} Reset all", icon::rotateCcw).c_str()))
		edit.reset = renderer::g_allMaterialFields;
	fieldTooltip(iShowOverrides ? "Stop overriding every field." : "Give every field its default value.");
	ImGui::EndDisabled();

	ImGui::SeparatorText("Surface");
	memberField(edit, Field::ShadingModel, "Shading", &P::shadingModel, drawEnum<renderer::ShadingModel>);
	memberField(edit, Field::BaseColor, "Base colour", &P::baseColor, drawColor<math::vec4>);
	memberField(edit, Field::Metallic, "Metallic", &P::metallic, unit);
	memberField(edit, Field::Ior, "IOR", &P::ior, [](const char* iLabel, float& ioValue) -> bool {
		return prop::DragFloat(iLabel, &ioValue, 0.005f, 1.f, 3.f, "%.3f");
	});
	fieldTooltip("Index of refraction of a dielectric: water 1.33, plastic and glass 1.5, diamond 2.42. Sets the "
				 "reflection at normal incidence (4 % at 1.5); metals ignore it.");
	memberField(edit, Field::Roughness, "Roughness", &P::roughness, unit);
	memberField(edit, Field::NormalScale, "Normal scale", &P::normalScale,
				[](const char* iLabel, float& ioValue) -> bool { return drawUnitFloat(iLabel, ioValue, 0.f, 4.f); });
	memberField(edit, Field::NormalConvention, "Normal convention", &P::normalConvention,
				drawEnum<renderer::NormalConvention>);
	fieldTooltip("OpenGl when the green of the normal map points up the image (glTF, Blender, nor_gl), DirectX when "
				 "it points down (Unreal, nor_dx).");
	memberField(edit, Field::OcclusionStrength, "Occlusion", &P::occlusionStrength, unit);
	memberField(edit, Field::DoubleSided, "Double sided", &P::doubleSided,
				[](const char* iLabel, bool& ioValue) -> bool { return prop::Checkbox(iLabel, &ioValue); });

	ImGui::SeparatorText("Maps");
	const auto mapRow = [&](const renderer::MaterialMap iMap) -> void {
		const auto index = static_cast<size_t>(iMap);
		materialField(edit, renderer::mapField(iMap), g_materialMapLabels[index], ioValues.maps[index],
					  iInherited != nullptr ? &iInherited->maps[index] : nullptr, defaultMaterial().maps[index],
					  drawMapField);
	};
	mapRow(renderer::MaterialMap::BaseColor);
	mapRow(renderer::MaterialMap::Metallic);
	memberField(edit, Field::MetallicChannel, "Metallic channel", &P::metallicChannel, channel);
	mapRow(renderer::MaterialMap::Roughness);
	memberField(edit, Field::RoughnessChannel, "Roughness channel", &P::roughnessChannel, channel);
	mapRow(renderer::MaterialMap::Occlusion);
	memberField(edit, Field::OcclusionChannel, "Occlusion channel", &P::occlusionChannel, channel);
	mapRow(renderer::MaterialMap::Normal);
	mapRow(renderer::MaterialMap::Emissive);
	const auto& occlusion = ioValues.maps[static_cast<size_t>(renderer::MaterialMap::Occlusion)];
	ImGui::BeginDisabled(occlusion.empty());
	if (ImGui::Button("Use the occlusion map as ARM")) {
		ioValues.maps[static_cast<size_t>(renderer::MaterialMap::Metallic)] = occlusion;
		ioValues.maps[static_cast<size_t>(renderer::MaterialMap::Roughness)] = occlusion;
		ioValues.occlusionChannel = renderer::TextureChannel::R;
		ioValues.roughnessChannel = renderer::TextureChannel::G;
		ioValues.metallicChannel = renderer::TextureChannel::B;
		if (iShowOverrides)
			ioFields |= static_cast<uint32_t>(Field::MetallicMap) | static_cast<uint32_t>(Field::RoughnessMap) |
						static_cast<uint32_t>(Field::MetallicChannel) | static_cast<uint32_t>(Field::RoughnessChannel) |
						static_cast<uint32_t>(Field::OcclusionChannel);
		edit.changed = true;
	}
	fieldTooltip("One image packing occlusion in R, roughness in G and metallic in B: it becomes the metallic and "
				 "roughness maps too, each reading its channel.");
	ImGui::EndDisabled();

	ImGui::SeparatorText("Emission");
	memberField(edit, Field::Emissive, "Emissive", &P::emissive, drawColor<math::vec3>);
	memberField(edit, Field::EmissiveLuminance, "Luminance (cd/m2)", &P::emissiveLuminance,
				[](const char* iLabel, float& ioValue) -> bool {
					return prop::DragFloat(iLabel, &ioValue, 10.f, 0.f, 1.e7f, "%.0f", ImGuiSliderFlags_Logarithmic);
				});

	ImGui::SeparatorText("Layers");
	memberField(edit, Field::Clearcoat, "Clearcoat", &P::clearcoat, unit);
	memberField(edit, Field::ClearcoatRoughness, "Clearcoat roughness", &P::clearcoatRoughness, unit);
	memberField(edit, Field::ClearcoatIor, "Clearcoat IOR", &P::clearcoatIor,
				[](const char* iLabel, float& ioValue) -> bool {
					return prop::DragFloat(iLabel, &ioValue, 0.005f, 1.f, 3.f, "%.3f");
				});
	memberField(edit, Field::SheenColor, "Sheen colour", &P::sheenColor, drawColor<math::vec3>);
	memberField(edit, Field::SheenRoughness, "Sheen roughness", &P::sheenRoughness, unit);
	memberField(edit, Field::Anisotropy, "Anisotropy", &P::anisotropy,
				[](const char* iLabel, float& ioValue) -> bool { return drawUnitFloat(iLabel, ioValue, -1.f, 1.f); });
	memberField(edit, Field::AnisotropyRotation, "Anisotropy rotation", &P::anisotropyRotation, drawDegrees);

	const auto shading =
			(iShowOverrides && iInherited != nullptr && (ioFields & static_cast<uint32_t>(Field::ShadingModel)) == 0)
					? iInherited->shadingModel
					: ioValues.shadingModel;
	if (shading == renderer::ShadingModel::Toon) {
		ImGui::SeparatorText("Toon");
		memberField(edit, Field::ToonBands, "Bands", &P::toonBands, [](const char* iLabel, uint32_t& ioValue) -> bool {
			int bands = static_cast<int>(ioValue);
			if (!prop::DragInt(iLabel, &bands, 0.1f, 1, 16))
				return false;
			ioValue = static_cast<uint32_t>(std::clamp(bands, 1, 16));
			return true;
		});
		memberField(edit, Field::ToonRim, "Rim", &P::toonRim, unit);
	}

	ImGui::SeparatorText("Alpha");
	memberField(edit, Field::AlphaMode, "Alpha mode", &P::alphaMode, drawEnum<renderer::AlphaMode>);
	memberField(edit, Field::AlphaCutoff, "Alpha cutoff", &P::alphaCutoff, unit);

	if (edit.reset != 0) {
		if (iShowOverrides)
			ioFields &= ~edit.reset;
		else
			ioValues.copyFields(defaultMaterial(), edit.reset);
	}
	return edit.changed;
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
	widgets::textureField("Texture", ioComponent.texture, {100.f, 100.f},
						  mesh ? mesh->getBaseColorTexture() : shared<renderer::gpu::Texture2D>{});
	fieldTooltip("Replaces the base-colour texture of the mesh material; empty keeps the material's (shown faded).");
	prop::Checkbox("Transparent", &ioComponent.transparent);
	fieldTooltip("Draw with alpha blending (the colour alpha), after the opaque meshes and sorted back-to-front.");
	renderMaterialSlots(ioComponent);
	if (mesh) {
		const auto& geometry = mesh->getGeometry();
		ImGui::Text("Vertices: %zu, triangles: %zu", geometry.vertices.size(), geometry.indices.size() / 3);
	} else if (!ioComponent.meshPath.empty()) {
		ImGui::TextDisabled("(loaded on the next frame, or the file cannot be read: see the log)");
	} else {
		widgets::notice("No mesh: drop a .gltf, .glb or .obj file on the field.");
	}
}

void renderProps(Environment& ioComponent) {
	const Environment defaults;
	if (assetPathRow("HDRI", ioComponent.hdri.generic_string(), "drop a .hdr", "Open the image.", nullptr, false))
		ioComponent.hdri.clear();
	if (std::filesystem::path dropped; extensionDropTarget({".hdr"}, dropped))
		ioComponent.hdri = dropped;
	fieldTooltip("Equirectangular HDR image lighting the 3D layers; empty lights them with the ambient colour.");
	resetFloat("Luminance (cd/m2)", ioComponent.intensity, defaults.intensity, 10.f, 0.f, 1.e6f, "%.1f",
			   ImGuiSliderFlags_Logarithmic);
	fieldTooltip("Luminance of the sky: a pixel value of 1 of the image, or a white ambient, shines this bright. Clear "
				 "day 10 000, overcast 2 000, dusk 100, night 0.01; 0 leaves only the lights.");
	if (ioComponent.hdri.empty()) {
		resetColor("Ambient", ioComponent.ambient, defaults.ambient);
		fieldTooltip("Colour of the constant sky lighting the scene without an image.");
	}
	resetDegrees("Rotation", ioComponent.rotation, defaults.rotation, -360.f, 360.f);
	fieldTooltip("Rotation of the image about the vertical axis, in degrees.");
	resetCheckbox("Show Sky", ioComponent.showSky, defaults.showSky);
	fieldTooltip("Draw the image behind the scene as its sky.");
}

void renderProps(Light2D& ioComponent) {
	const Light2D defaults;
	resetCheckbox("Enabled", ioComponent.enabled, defaults.enabled);
	fieldTooltip("A light switched off keeps its settings and stops shining.");
	resettable("Type", ioComponent.type, defaults.type, drawEnum<Light2D::Type>);
	fieldTooltip("Ambient: the same light everywhere (day and night tint; without one the ambient is white). Point: "
				 "radiates from its position. Spot: a point light restricted to a cone along the +X axis of the "
				 "entity.");
	resetColor("Color", ioComponent.color, defaults.color);
	fieldTooltip("Colour of the light, as authored (sRGB).");
	resetFloat("Intensity", ioComponent.intensity, defaults.intensity, 0.01f, 0.f, 100.f);
	fieldTooltip("Multiplier of the colour: 1 lights a sprite with its own colour at the centre of the light.");
	if (ioComponent.type == Light2D::Type::Ambient)
		return;
	resetFloat("Radius", ioComponent.radius, defaults.radius, 0.05f, 0.01f, 10000.f);
	fieldTooltip("Distance where the light reaches zero, in world units.");
	resetFloat("Falloff", ioComponent.falloff, defaults.falloff, 0.01f, 0.1f, 8.f);
	fieldTooltip(
			"Exponent of the fade towards the radius: 1 linear, 2 brighter near the light and softer at the edge.");
	resetFloat("Height", ioComponent.height, defaults.height, 0.01f, 0.f, 100.f);
	fieldTooltip(
			"Height of the light above the sprites: the slant of its light on the normal-mapped sprites and on the "
			"isometric atlases.");
	if (ioComponent.type == Light2D::Type::Spot) {
		constexpr float maxAngle = 179.f;
		if (resetDegrees("Inner Angle", ioComponent.innerAngle, defaults.innerAngle, 0.f, maxAngle))
			ioComponent.outerAngle = std::max(ioComponent.outerAngle, ioComponent.innerAngle);
		fieldTooltip("Half angle of the cone at full intensity, in degrees.");
		if (resetDegrees("Outer Angle", ioComponent.outerAngle, defaults.outerAngle, 0.f, maxAngle))
			ioComponent.innerAngle = std::min(ioComponent.innerAngle, ioComponent.outerAngle);
		fieldTooltip("Half angle of the cone where the light reaches zero, in degrees.");
	}
	resetCheckbox("Cast Shadows", ioComponent.castShadows, defaults.castShadows);
	fieldTooltip(
			"The sprites flagged Cast Shadows block the light (2D layers; the isometric layer has no 2D shadows).");
	if (!ioComponent.castShadows)
		return;
	resetFloat("Shadow Softness", ioComponent.shadowSoftness, defaults.shadowSoftness, 0.01f, 0.f, 4.f);
	fieldTooltip("Width of the penumbra: 0 hard, 1 to 3 soft.");
}

void renderProps(Light& ioComponent) {
	const Light defaults;
	resetCheckbox("Enabled", ioComponent.enabled, defaults.enabled);
	fieldTooltip("A light switched off keeps its settings and stops shining.");
	const auto previousType = ioComponent.type;
	resettable("Type", ioComponent.type, defaults.type, drawEnum<Light::Type>);
	fieldTooltip("Directional: parallel rays like the sun. Point: radiates from its position. Spot: a point light "
				 "restricted to a cone. Rectangle and Disc: a panel emitting from its -Z face, soft highlights and "
				 "shadows. Sphere: a bulb of a given radius. It shines along -Z of the entity.");
	const bool directional = ioComponent.type == Light::Type::Directional;
	const float defaultIntensity = directional ? g_defaultSunIlluminance : g_defaultBulbPower;
	// Switching between lux and lumens keeps a default value a default value of the new unit.
	if (previousType != ioComponent.type &&
		std::equal_to<float>{}(ioComponent.intensity,
							   previousType == Light::Type::Directional ? g_defaultSunIlluminance : g_defaultBulbPower))
		ioComponent.intensity = defaultIntensity;
	resetColor("Color", ioComponent.color, defaults.color);
	fieldTooltip("Colour of the light, as authored (sRGB): candle and fire about [1, 0.55, 0.25], tungsten bulb "
				 "[1, 0.8, 0.6], noon sun white, overcast sky [0.85, 0.9, 1].");
	if (directional) {
		resetFloat("Illuminance (lx)", ioComponent.intensity, defaultIntensity, 100.f, 0.f, 200000.f, "%.1f",
				   ImGuiSliderFlags_Logarithmic);
		fieldTooltip("Light received by a surface facing it: noon sun 100 000 lx, overcast 10 000, sunrise 400, full "
					 "moon 0.25.");
	} else {
		resetFloat("Power (lm)", ioComponent.intensity, defaultIntensity, 1.f, 0.f, 1.e6f, "%.1f",
				   ImGuiSliderFlags_Logarithmic);
		fieldTooltip("Luminous power: candle 12 lm, 60 W bulb 800, fireplace 2 000, street lamp 10 000. It falls off "
					 "with the square of the distance.");
		resetFloat("Range", ioComponent.range, defaults.range, 0.1f, 0.01f, 100000.0f);
		fieldTooltip("Distance where the light is cut off smoothly, in world units (metres).");
	}
	if (ioComponent.type == Light::Type::Rectangle) {
		resettable("Size", ioComponent.size, defaults.size, [](const char* iId, math::vec2& ioField) -> bool {
			if (!prop::DragFloat2(iId, &ioField.x(), 0.01f, g_minAreaSize, 1000.f))
				return false;
			ioField = {std::max(ioField.x(), g_minAreaSize), std::max(ioField.y(), g_minAreaSize)};
			return true;
		});
		fieldTooltip(
				"Width (along X) and height (along Y) of the emitting rectangle, in world units: the power spreads "
				"over it, a larger panel gives softer highlights and shadows.");
	}
	if (ioComponent.type == Light::Type::Disc || ioComponent.type == Light::Type::Sphere) {
		resetFloat("Radius", ioComponent.radius, defaults.radius, 0.005f, g_minAreaSize, 1000.f);
		fieldTooltip("Radius of the emitting disc or sphere, in world units: larger gives softer highlights and "
					 "shadows.");
	}
	if (ioComponent.type == Light::Type::Spot) {
		constexpr float maxAngle = 89.f;
		if (resetDegrees("Inner Angle", ioComponent.innerAngle, defaults.innerAngle, 0.f, maxAngle))
			ioComponent.outerAngle = std::max(ioComponent.outerAngle, ioComponent.innerAngle);
		fieldTooltip("Half angle of the cone at full intensity, in degrees.");
		if (resetDegrees("Outer Angle", ioComponent.outerAngle, defaults.outerAngle, 0.f, maxAngle))
			ioComponent.innerAngle = std::min(ioComponent.innerAngle, ioComponent.outerAngle);
		fieldTooltip("Half angle of the cone where the light reaches zero, in degrees.");
	}
	if (directional) {
		resettable("Angular Radius", ioComponent.angularRadius, defaults.angularRadius,
				   [](const char* iId, float& ioField) -> bool {
					   const float maxDegrees = math::degrees(g_maxAngularRadius);
					   float degrees = math::degrees(ioField);
					   if (!prop::DragFloat(iId, &degrees, 0.01f, 0.f, maxDegrees, "%.2f deg"))
						   return false;
					   ioField = math::radians(std::clamp(degrees, 0.f, maxDegrees));
					   return true;
				   });
		fieldTooltip("Apparent radius of the light, in degrees: the sun about 0.27. Softens the shadows with the "
					 "distance to their caster and widens the highlight; 0 for hard shadows.");
	}
	resetFloat("Lens Flare", ioComponent.flare, defaults.flare, 0.01f, 0.f, 1000.f);
	fieldTooltip("Strength of the lens flare of the light for a camera with lens flares (post process); 0 never "
				 "flares. A point light flares with the illuminance it gives at the camera.");
	resetCheckbox("Cast Shadows", ioComponent.castShadows, defaults.castShadows);
	fieldTooltip(directional ? "The first shadow-casting directional light of the scene shadows the 3D layers."
							 : "Point, spot and area lights nearest to the camera cast shadows, within a budget per "
							   "frame (4 spots and 4 cubes on OpenGL, 16 of each on Vulkan); the others light without "
							   "shadow. An area light casts through a cube, softened by its size.");
	if (!ioComponent.castShadows)
		return;
	if (directional) {
		resetFloat("Shadow Distance", ioComponent.shadowDistance, defaults.shadowDistance, 0.5f, 1.0f, 100000.0f);
		fieldTooltip("Distance from the camera the shadows cover, in world units.");
		resettable("Cascades", ioComponent.cascadeCount, defaults.cascadeCount,
				   [](const char* iId, uint32_t& ioField) -> bool {
					   int cascades = static_cast<int>(ioField);
					   if (!prop::DragInt(iId, &cascades, 0.1f, 1, 4))
						   return false;
					   ioField = static_cast<uint32_t>(std::clamp(cascades, 1, 4));
					   return true;
				   });
		fieldTooltip("Number of shadow cascades, from 1 to 4: more give sharper shadows near the camera.");
	}
	resettable("Resolution", ioComponent.shadowResolution, defaults.shadowResolution,
			   [](const char* iId, uint32_t& ioField) -> bool {
				   bool changed = false;
				   if (prop::BeginCombo(iId, std::to_string(ioField).c_str())) {
					   for (const uint32_t resolution: {512u, 1024u, 2048u, 4096u}) {
						   const bool isSelected = ioField == resolution;
						   if (ImGui::Selectable(std::to_string(resolution).c_str(), isSelected)) {
							   ioField = resolution;
							   changed = true;
						   }
						   if (isSelected)
							   ImGui::SetItemDefaultFocus();
					   }
					   ImGui::EndCombo();
				   }
				   return changed;
			   });
	fieldTooltip(directional ? "Size of each cascade's shadow map, in texels."
							 : "Size of the shadow map (spot) or of each face of the shadow cube (point and area "
							   "lights), in texels.");
	resetFloat("Bias", ioComponent.shadowBias, defaults.shadowBias, 0.05f, 0.0f, 20.0f);
	fieldTooltip("Depth bias against self-shadowing, in shadow-map texels along the surface normal.");
}

namespace {

// Font combo (every font found), a drop target for a font file and a preview of the chosen font; a null font is the default.
void fontPicker(shared<data::fonts::Font>& ioFont) {
	if (!app::Application::instanced())
		return;
	auto& fontLib = app::Application::get().getFontLibrary();
	const bool isDefault = !ioFont || ioFont->isDefault();
	const std::string display = isDefault ? "(default)" : ioFont->getName();
	if (prop::BeginCombo("Font", display.c_str())) {
		for (const auto& font: data::fonts::FontLibrary::getFoundFontNames()) {
			if (ImGui::Selectable(font.c_str(), !isDefault && ioFont->getName() == font))
				ioFont = fontLib.getFont(font);
		}
		ImGui::EndCombo();
	}
	std::filesystem::path dropped;
	if (widgets::assetDropTarget(widgets::AssetKind::Font, dropped))
		ioFont = fontLib.getFont(dropped.stem().string());
	if (ioFont && !ioFont->isDefault()) {
		constexpr ImVec2 previewSize{220.f, 44.f};
		if (const auto previewFb = FontPreviewCache::get().request(ioFont); previewFb) {
			if (const auto preview = imTexture(previewFb, 0); preview.has_value())
				ImGui::Image(preview.value(), previewSize, gui::vec(previewFb->getLowerData()),
							 gui::vec(previewFb->getUpperData()));
		} else if (const auto atlas = imTexture(ioFont->getAtlasTexture()); atlas.has_value()) {
			// First-frame fallback: atlas thumbnail until pumpPending() renders the sample-string preview.
			ImGui::Image(atlas.value(), previewSize, {0, 1}, {1, 0});
		}
	}
}

}// namespace

void renderProps(Text& ioComponent) {
	prop::InputTextMultiline("Text String", &ioComponent.text, {0, 70});
	fontPicker(ioComponent.font);

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

		handlerRow("Callback", ioComponent.trigger.callbackName, "on_timer",
				   "Lua function name to call. Defaults to on_timer if empty.");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::Interaction) {
		prop::DragFloat("Interaction Range", &ioComponent.trigger.interactionRange, 0.05f, 0.1f, 20.0f);

		fieldTooltip("Distance within which the player can press E to interact.");

		handlerRow("Callback", ioComponent.trigger.callbackName, "on_interact",
				   "Lua function name to call. Defaults to on_interact if empty.");
	}
	if (ioComponent.trigger.type == SceneTrigger::TriggerType::LuaCallback) {
		handlerRow("Callback", ioComponent.trigger.callbackName, "on_triggered",
				   "Lua function name to call when the player overlaps. Defaults to on_triggered if empty.");
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

namespace {

auto drawBus(const char* iLabel, sound::Bus& ioBus) -> bool {
	bool changed = false;
	const std::string current{sound::getBusName(ioBus)};
	if (!prop::BeginCombo(iLabel, current.c_str()))
		return false;
	for (const sound::Bus bus: sound::g_allBuses) {
		const std::string name{sound::getBusName(bus)};
		if (ImGui::Selectable(name.c_str(), bus == ioBus)) {
			ioBus = bus;
			changed = true;
		}
	}
	ImGui::EndCombo();
	return changed;
}

enum struct FilterKind : uint8_t { None, LowPass, HighPass, BandPass };

auto getFilterKind(const sound::Filter& iFilter) -> FilterKind {
	const bool low = iFilter.gainLow < 1.f;
	const bool high = iFilter.gainHigh < 1.f;
	if (low && high)
		return FilterKind::BandPass;
	if (high)
		return FilterKind::LowPass;
	if (low)
		return FilterKind::HighPass;
	return FilterKind::None;
}

// The filter as a kind (none, low-pass, high-pass, band-pass) and the gains that kind uses.
void drawFilter(sound::Filter& ioFilter) {
	constexpr std::array<const char*, 4> kinds{"None", "Low-pass", "High-pass", "Band-pass"};
	int kind = static_cast<int>(getFilterKind(ioFilter));
	if (prop::Combo("Filter", &kind, std::span<const char* const>{kinds})) {
		switch (static_cast<FilterKind>(kind)) {
			case FilterKind::None:
				ioFilter = {};
				break;
			case FilterKind::LowPass:
				ioFilter = {.gain = ioFilter.gain, .gainLow = 1.f, .gainHigh = 0.2f};
				break;
			case FilterKind::HighPass:
				ioFilter = {.gain = ioFilter.gain, .gainLow = 0.2f, .gainHigh = 1.f};
				break;
			case FilterKind::BandPass:
				ioFilter = {.gain = ioFilter.gain, .gainLow = 0.5f, .gainHigh = 0.5f};
				break;
		}
	}
	fieldTooltip("Low-pass muffles the sound (underwater, behind a wall), high-pass thins it (a radio), band-pass "
				 "does both.");
	const auto current = getFilterKind(ioFilter);
	if (current == FilterKind::None)
		return;
	drawUnitFloat("Filter Gain", ioFilter.gain);
	fieldTooltip("Gain of the whole filtered sound.");
	if (current == FilterKind::LowPass || current == FilterKind::BandPass) {
		drawUnitFloat("High Frequencies", ioFilter.gainHigh, 0.f, 0.999f);
		fieldTooltip("Gain left to the frequencies above 5 kHz: lower is more muffled.");
	}
	if (current == FilterKind::HighPass || current == FilterKind::BandPass) {
		drawUnitFloat("Low Frequencies", ioFilter.gainLow, 0.f, 0.999f);
		fieldTooltip("Gain left to the frequencies below 250 Hz: lower is thinner.");
	}
}

}// namespace

void renderProps(SoundSource& ioComponent) {
	prop::InputText("Sound Asset", &ioComponent.sound.soundAsset);
	fieldTooltip("Audio file or sound cue relative to the project (e.g. sounds/explosion.wav, sounds/steps.owlsound).");
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Sound, dropped)) {
		ioComponent.sound.soundAsset = dropped.string();
		// A dropped cue brings its default bus.
		if (sound::SoundCue::isCue(dropped))
			if (const auto cue = sound::SoundCue::load(dropped); cue)
				ioComponent.sound.category = cue->bus;
	}
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
			sound::PlayParams params;
			params.volume = ioComponent.sound.volume;
			params.pitch = ioComponent.sound.pitch;
			params.bus = ioComponent.sound.category;
			params.filter = ioComponent.sound.filter;
			s_previewHandle = sound::SoundHelper::playAsset(ioComponent.sound.soundAsset, params);
		}
	}
	ImGui::EndDisabled();
	fieldTooltip("Preview the sound with current volume and pitch. Not spatialized.");
	std::ignore = drawBus("Bus", ioComponent.sound.category);
	fieldTooltip("Mixer bus of the sound: Music is dry, SFX, Ambient and Voice are heard through the reverb zones.");
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
	prop::InputText("Subtitle", &ioComponent.sound.subtitle);
	fieldTooltip(
			"Shown at the bottom of the screen while the sound plays within hearing (Max Distance of a spatial "
			"sound), when the player keeps the subtitles on: a localisation key or the text itself. Empty for none.");
	if (ioComponent.sound.spatial) {
		prop::DragFloat("Max Distance", &ioComponent.sound.maxDistance, 0.5f, 1.0f, 500.0f);
		fieldTooltip("Maximum audible distance in world units.");
		prop::DragFloat("Rolloff", &ioComponent.sound.rolloff, 0.01f, 0.0f, 10.0f);
		fieldTooltip("Distance attenuation curve. Higher = faster falloff.");
		prop::Checkbox("Doppler", &ioComponent.sound.doppler);
		fieldTooltip("Shift the pitch with the motion of the entity relative to the listener (Doppler effect).");
	}
	drawFilter(ioComponent.sound.filter);
}

void renderProps(SoundListener& ioComponent) {
	prop::Checkbox("Primary", &ioComponent.primary);
	fieldTooltip("Mark as the active listener (the 'ear' for 3D audio). Only one primary per scene.");
}

void renderProps(ReverbZone& ioComponent) {
	const ReverbZone defaults;
	resettable("Shape", ioComponent.shape, defaults.shape, drawEnum<ReverbZone::Shape>);
	fieldTooltip("Volume of the zone, centred on the entity and turned and scaled with it: the Scale gizmo resizes "
				 "it in the viewport.");
	if (ioComponent.shape == ReverbZone::Shape::Box) {
		resettable("Extents", ioComponent.extents, defaults.extents, [](const char* iId, math::vec3& ioField) -> bool {
			if (!prop::DragFloat3(iId, &ioField.x(), 0.01f, 0.01f, 10000.f))
				return false;
			ioField = {std::max(ioField.x(), 0.01f), std::max(ioField.y(), 0.01f), std::max(ioField.z(), 0.01f)};
			return true;
		});
		fieldTooltip("Half size of the box along the local X, Y and Z axes of the entity.");
	} else {
		resetFloat("Radius", ioComponent.radius, defaults.radius, 0.01f, 0.01f, 10000.f);
		fieldTooltip("Radius of the sphere, scaled by the largest axis of the entity.");
	}
	resetFloat("Blend Distance", ioComponent.blendDistance, defaults.blendDistance, 0.01f, 0.f, 1000.f);
	fieldTooltip("Distance outside the volume over which the zone fades out, in world units; 0 is a hard edge.");
	const std::string current{sound::getReverbPresetName(ioComponent.preset)};
	if (prop::BeginCombo("Preset", current.c_str())) {
		for (const auto preset: magic_enum::enum_values<sound::ReverbPreset>()) {
			const std::string name{sound::getReverbPresetName(preset)};
			if (ImGui::Selectable(name.c_str(), preset == ioComponent.preset)) {
				// Custom starts from the values of the preset left.
				if (preset == sound::ReverbPreset::Custom)
					ioComponent.reverb = ioComponent.getReverb();
				ioComponent.preset = preset;
			}
		}
		ImGui::EndCombo();
	}
	fieldTooltip("Reverb of the zone: a preset of the EFX table (Cave, Hall, Forest, Underwater...), or Custom.");
	if (ioComponent.preset == sound::ReverbPreset::Custom) {
		auto& reverb = ioComponent.reverb;
		drawUnitFloat("Density", reverb.density);
		drawUnitFloat("Diffusion", reverb.diffusion);
		drawUnitFloat("Gain", reverb.gain);
		drawUnitFloat("High Gain", reverb.gainHigh);
		prop::DragFloat("Decay Time", &reverb.decayTime, 0.01f, 0.1f, 20.f, "%.2f s");
		fieldTooltip("Time the reverberation takes to die out.");
		prop::DragFloat("Decay High Ratio", &reverb.decayHighRatio, 0.01f, 0.1f, 2.f);
		prop::DragFloat("Reflections Gain", &reverb.reflectionsGain, 0.01f, 0.f, 3.16f);
		prop::DragFloat("Reflections Delay", &reverb.reflectionsDelay, 0.001f, 0.f, 0.3f, "%.3f s");
		prop::DragFloat("Late Gain", &reverb.lateGain, 0.01f, 0.f, 10.f);
		prop::DragFloat("Late Delay", &reverb.lateDelay, 0.001f, 0.f, 0.1f, "%.3f s");
	}
	drawFilter(ioComponent.filter);
}

namespace {

// A path to a sound asset, with a drop target for the Sound assets of the browser.
auto drawSoundAsset(const char* iLabel, std::string& ioAsset) -> bool {
	bool changed = prop::InputText(iLabel, &ioAsset);
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Sound, dropped)) {
		ioAsset = dropped.string();
		changed = true;
	}
	return changed;
}

void drawMusicLayers(sound::AdaptiveMusic& ioMusic) {
	std::optional<size_t> removed;
	for (size_t index = 0; index < ioMusic.layers.size(); ++index) {
		auto& layer = ioMusic.layers[index];
		ImGui::PushID(static_cast<int>(index));
		ImGui::SeparatorText(std::format("Layer {}", index + 1).c_str());
		prop::InputText("Name", &layer.name);
		std::ignore = drawSoundAsset("Asset", layer.asset);
		fieldTooltip("Looping track of the layer; the layers start together and stay in sync.");
		drawUnitFloat("Volume", layer.volume);
		if (ImGui::Button(std::format("{} Remove layer", icon::trash2).c_str()))
			removed = index;
		ImGui::PopID();
	}
	if (removed) {
		const auto name = ioMusic.layers[*removed].name;
		ioMusic.layers.erase(ioMusic.layers.begin() + static_cast<std::ptrdiff_t>(*removed));
		for (auto& state: ioMusic.states) std::erase(state.layers, name);
	}
	if (ImGui::Button(std::format("{} Add layer", icon::plus).c_str()))
		ioMusic.layers.push_back(
				{.name = std::format("layer{}", ioMusic.layers.size() + 1), .asset = {}, .volume = 1.f});
}

void drawMusicStates(sound::AdaptiveMusic& ioMusic) {
	std::optional<size_t> removed;
	for (size_t index = 0; index < ioMusic.states.size(); ++index) {
		auto& state = ioMusic.states[index];
		ImGui::PushID(static_cast<int>(1000 + index));
		ImGui::SeparatorText(std::format("State {}", index + 1).c_str());
		prop::InputText("Name", &state.name);
		fieldTooltip("Name the game sets: `sound.set_music_state(name)` or the value of the state key.");
		for (const auto& layer: ioMusic.layers) {
			bool heard = state.hasLayer(layer.name);
			if (prop::Checkbox(layer.name.c_str(), &heard)) {
				if (heard)
					state.layers.push_back(layer.name);
				else
					std::erase(state.layers, layer.name);
			}
		}
		std::ignore = drawSoundAsset("Stinger", state.stinger);
		fieldTooltip("One-shot sound played on the Music bus when the state is entered (empty: none).");
		if (ImGui::Button(std::format("{} Remove state", icon::trash2).c_str()))
			removed = index;
		ImGui::PopID();
	}
	if (removed)
		ioMusic.states.erase(ioMusic.states.begin() + static_cast<std::ptrdiff_t>(*removed));
	if (ImGui::Button(std::format("{} Add state", icon::plus).c_str()))
		ioMusic.states.push_back(
				{.name = std::format("state{}", ioMusic.states.size() + 1), .layers = {}, .stinger = {}});
}

}// namespace

void renderProps(AdaptiveMusic& ioComponent) {
	prop::InputText("Initial State", &ioComponent.initialState);
	fieldTooltip("State the music starts in; empty: the first state.");
	prop::InputText("State Key", &ioComponent.stateKey);
	fieldTooltip("Game state key whose text value selects the state (`gamestate.set(key, \"combat\")`); "
				 "empty: Lua only.");
	prop::Checkbox("Play On Start", &ioComponent.playOnStart);
	fieldTooltip("Start the music when the scene enters Play mode.");
	prop::DragFloat("Fade", &ioComponent.music.fadeSeconds, 0.05f, 0.f, 30.f, "%.2f s");
	fieldTooltip("Length of the equal-power fades between two states.");
	drawMusicLayers(ioComponent.music);
	drawMusicStates(ioComponent.music);
}

namespace {

// A value as the reset tooltip shows it.
auto describeValue(const script::ScriptValue& iValue) -> std::string {
	return std::visit(
			[](const auto& iAlternative) -> std::string {
				using Alternative = std::decay_t<decltype(iAlternative)>;
				if constexpr (std::is_same_v<Alternative, bool>) {
					return iAlternative ? "true" : "false";
				} else if constexpr (std::is_same_v<Alternative, std::string>) {
					return "\"" + iAlternative + "\"";
				} else if constexpr (std::is_same_v<Alternative, std::vector<double>> ||
									 std::is_same_v<Alternative, std::vector<std::string>>) {
					return std::format("{} items", iAlternative.size());
				} else if constexpr (std::is_same_v<Alternative, std::array<float, 2>> ||
									 std::is_same_v<Alternative, std::array<float, 3>> ||
									 std::is_same_v<Alternative, std::array<float, 4>>) {
					std::string text;
					for (const auto component: iAlternative)
						text += std::format("{}{:g}", text.empty() ? "" : ", ", component);
					return text;
				} else {
					return std::format("{}", iAlternative);
				}
			},
			iValue);
}

// Combo over the entities of the inspected scene by name, with a filter and a "none" entry.
auto scriptEntityPicker(uint64_t& ioTarget, const float iWidth) -> bool {
	const Scene* const scene = inspectedScene();
	const core::UUID current{ioTarget};
	std::string preview = ioTarget != 0 ? "(missing)" : "(none)";
	if (scene != nullptr && ioTarget != 0) {
		if (const Entity found = scene->findEntityByUUID(current); found)
			preview = found.getName();
	}
	ImGui::SetNextItemWidth(iWidth);
	bool changed = false;
	if (widgets::beginCombo("##entity", preview.c_str())) {
		static ImGuiTextFilter s_filter;
		s_filter.Draw("##filter", -FLT_MIN);
		if (ImGui::Selectable("(none)", ioTarget == 0)) {
			ioTarget = 0;
			changed = true;
		}
		if (scene != nullptr) {
			for (const auto view = scene->registry.view<ID>(); const auto entity: view) {
				const Entity candidate{entity, const_cast<Scene*>(scene)};
				if (!s_filter.PassFilter(candidate.getName().c_str()))
					continue;
				const auto uuid = view.get<ID>(entity).id;
				ImGui::PushID(static_cast<int>(static_cast<uint64_t>(uuid) & 0x7fffffff));
				if (ImGui::Selectable(candidate.getName().c_str(), static_cast<uint64_t>(uuid) == ioTarget)) {
					ioTarget = static_cast<uint64_t>(uuid);
					changed = true;
				}
				ImGui::PopID();
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}

// Clamp a component to the range the script declares.
auto clampToRange(const float iValue, const script::ScriptProperty& iProperty) -> float {
	return std::clamp(iValue, iProperty.min.value_or(-FLT_MAX), iProperty.max.value_or(FLT_MAX));
}

// One element row of a list property: the value and a remove button; true when the element is to be removed.
template<typename Draw>
auto listElementRow(const size_t iIndex, Draw&& iDraw) -> bool {
	ImGui::PushID(static_cast<int>(iIndex));
	const float side = ImGui::GetFrameHeight();
	ImGui::Indent();
	ImGui::SetNextItemWidth(std::max(side, ImGui::GetContentRegionAvail().x - side - ImGui::GetStyle().ItemSpacing.x));
	std::forward<Draw>(iDraw)();
	ImGui::SameLine();
	const bool removed = widgets::iconButton("##remove", {.glyph = icon::x, .tooltip = "Remove"});
	ImGui::Unindent();
	ImGui::PopID();
	return removed;
}

// The editor of a list property, under its row: one row per element and the add button on the property row.
template<typename Element, typename DrawElement>
auto editList(std::vector<Element>& ioList, const bool iAdd, DrawElement&& iDrawElement) -> bool {
	bool changed = false;
	if (iAdd) {
		ioList.emplace_back();
		changed = true;
	}
	std::optional<size_t> removed;
	for (size_t index = 0; index < ioList.size(); ++index) {
		if (listElementRow(index, [&]() -> void { changed |= iDrawElement(ioList[index]); }))
			removed = index;
	}
	if (removed) {
		ioList.erase(ioList.begin() + static_cast<std::ptrdiff_t>(*removed));
		changed = true;
	}
	return changed;
}

// Height of a string property field: its wrapped text, from one line up to eight, then it scrolls.
auto stringFieldHeight(const std::string& iText, const float iWidth) -> float {
	constexpr float maxLines = 8.f;
	const auto& style = ImGui::GetStyle();
	const float lineHeight = ImGui::GetTextLineHeight();
	const float wrapWidth = std::max(1.f, iWidth - style.FramePadding.x * 2.f - style.ScrollbarSize);
	const float textHeight = ImGui::CalcTextSize(iText.c_str(), nullptr, false, wrapWidth).y;
	return std::clamp(textHeight, lineHeight, lineHeight * maxLines) + style.FramePadding.y * 2.f;
}

// Inspector row of one script property: the editor of its type, then a reset button to the value the script declares.
void drawScriptProperty(script::ScriptProperty& ioProperty) {
	using Type = script::ScriptPropertyType;
	const bool modified = ioProperty.value != ioProperty.defaultValue;
	const float side = ImGui::GetFrameHeight();
	const float step = ioProperty.step.value_or(0.1f);
	const bool ranged = ioProperty.min || ioProperty.max;
	const float rangeMin = ranged ? ioProperty.min.value_or(-FLT_MAX) : 0.f;
	const float rangeMax = ranged ? ioProperty.max.value_or(FLT_MAX) : 0.f;
	bool addRequested = false;
	prop::row(ioProperty.name.c_str(), [&](const char* iId) -> bool {
		// Measured after the label: the field takes what is left of the row but the reset button.
		const float fieldWidth =
				std::max(side, std::floor(ImGui::GetContentRegionAvail().x) - side - ImGui::GetStyle().ItemSpacing.x);
		ImGui::SetNextItemWidth(fieldWidth);
		switch (ioProperty.type) {
			case Type::Float:
				{
					auto value = std::get<float>(ioProperty.value);
					if (ImGui::DragFloat(iId, &value, step, rangeMin, rangeMax))
						ioProperty.value = value;
					break;
				}
			case Type::Int:
				{
					auto value = static_cast<int>(std::get<int64_t>(ioProperty.value));
					const auto minInt = ranged ? static_cast<int>(ioProperty.min.value_or(-1e9f)) : 0;
					const auto maxInt = ranged ? static_cast<int>(ioProperty.max.value_or(1e9f)) : 0;
					if (ImGui::DragInt(iId, &value, ioProperty.step.value_or(1.f), minInt, maxInt))
						ioProperty.value = static_cast<int64_t>(value);
					break;
				}
			case Type::String:
				{
					auto value = std::get<std::string>(ioProperty.value);
					if (ImGui::InputTextMultiline(iId, &value, {fieldWidth, stringFieldHeight(value, fieldWidth)},
												  ImGuiInputTextFlags_WordWrap))
						ioProperty.value = value;
					break;
				}
			case Type::Bool:
				{
					auto value = std::get<bool>(ioProperty.value);
					if (widgets::toggle(iId, &value))
						ioProperty.value = value;
					break;
				}
			case Type::Color:
				{
					auto value = std::get<std::array<float, 4>>(ioProperty.value);
					if (ImGui::ColorEdit4(iId, value.data()))
						ioProperty.value = value;
					break;
				}
			case Type::Vec2:
				{
					auto value = std::get<std::array<float, 2>>(ioProperty.value);
					if (widgets::vectorField(iId, value, step, "%.2f", fieldWidth)) {
						for (auto& component: value) component = clampToRange(component, ioProperty);
						ioProperty.value = value;
					}
					break;
				}
			case Type::Vec3:
				{
					auto value = std::get<std::array<float, 3>>(ioProperty.value);
					if (widgets::vectorField(iId, value, step, "%.2f", fieldWidth)) {
						for (auto& component: value) component = clampToRange(component, ioProperty);
						ioProperty.value = value;
					}
					break;
				}
			case Type::Entity:
				{
					auto value = std::get<uint64_t>(ioProperty.value);
					if (scriptEntityPicker(value, fieldWidth))
						ioProperty.value = value;
					break;
				}
			case Type::Numbers:
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("%zu items", std::get<std::vector<double>>(ioProperty.value).size());
				ImGui::SameLine();
				addRequested = widgets::iconButton("##add", {.glyph = icon::plus, .tooltip = "Add an item"});
				break;
			case Type::Strings:
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("%zu items", std::get<std::vector<std::string>>(ioProperty.value).size());
				ImGui::SameLine();
				addRequested = widgets::iconButton("##add", {.glyph = icon::plus, .tooltip = "Add an item"});
				break;
		}
		ImGui::SameLine();
		const auto description =
				std::format("Back to {}, the default of the script.", describeValue(ioProperty.defaultValue));
		if (widgets::iconButton("##reset", {.glyph = icon::rotateCcw,
											.tooltip = "Reset to default",
											.description = description.c_str(),
											.disabled = !modified}))
			ioProperty.value = ioProperty.defaultValue;
		return false;
	});
	if (ioProperty.type == Type::Numbers) {
		auto list = std::get<std::vector<double>>(ioProperty.value);
		if (editList(list, addRequested, [&](double& ioElement) -> bool {
				auto value = static_cast<float>(ioElement);
				const bool edited = ImGui::DragFloat("##value", &value, step, rangeMin, rangeMax);
				if (edited)
					ioElement = static_cast<double>(value);
				return edited;
			}))
			ioProperty.value = std::move(list);
	} else if (ioProperty.type == Type::Strings) {
		auto list = std::get<std::vector<std::string>>(ioProperty.value);
		if (editList(list, addRequested,
					 [](std::string& ioElement) -> bool { return ImGui::InputText("##value", &ioElement); }))
			ioProperty.value = std::move(list);
	}
}

}// namespace

void renderProps(LuaScript& ioComponent) {
	if (assetPathRow("Script", ioComponent.scriptPath, "drop a .lua or .owlvs file",
					 "Open the script in its editor.")) {
		ioComponent.scriptPath.clear();
		ioComponent.properties.clear();
	}
	if (ioComponent.scriptPath.empty())
		widgets::notice("No script: drop a .lua file or a visual script from the Content Browser on the field.");
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::LuaScript, dropped))
		ioComponent.scriptPath = dropped.string();
	syncScriptProperties(ioComponent);
	renderHooks(ioComponent);
	if (!ioComponent.properties.empty() && widgets::subHeader("Properties", true)) {
		for (size_t idx = 0; idx < ioComponent.properties.size(); ++idx) {
			// Index-based PushID prevents collisions if two properties share a name.
			ImGui::PushID(static_cast<int>(idx));
			drawScriptProperty(ioComponent.properties[idx]);
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

namespace {

void textKeyPicker(std::string& ioKey) {
	const auto* context = app::Application::instanced() ? &app::Application::get().getEngineContext() : nullptr;
	if (prop::BeginCombo("Text Key", ioKey.empty() ? "(none)" : ioKey.c_str())) {
		if (ImGui::Selectable("(none)", ioKey.empty()))
			ioKey.clear();
		if (context != nullptr)
			for (const auto& entry: context->getLocalisation().strings) {
				const auto label = entry.table.empty() ? entry.key : std::format("{}  ({})", entry.key, entry.table);
				if (ImGui::Selectable(label.c_str(), entry.key == ioKey))
					ioKey = entry.key;
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", std::string{context->translate(entry.key)}.c_str());
			}
		ImGui::EndCombo();
	}
	fieldTooltip("Localised text shown in place of Text, in the language of the game (strings of localisation.yml).");
	if (!ioKey.empty() && context != nullptr)
		ImGui::TextDisabled("%s: %s", context->getLanguage().c_str(), std::string{context->translate(ioKey)}.c_str());
}

}// namespace

void renderProps(UiText& ioComponent) {
	textKeyPicker(ioComponent.textKey);
	prop::InputTextMultiline("Text", &ioComponent.text, ImVec2(0, 60));
	prop::ColorEdit4("Color", reinterpret_cast<float*>(&ioComponent.color));
	fontPicker(ioComponent.font);
	fieldTooltip("Font of the text; the default font when none is chosen.");
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
	prop::InputText("Render Target", &ioComponent.renderTarget);
	fieldTooltip("Name of a Render Target of the scene: the image shows what its camera sees (a minimap), in place of "
				 "the texture. Empty to show the texture.");
	prop::ColorEdit4("Tint", reinterpret_cast<float*>(&ioComponent.tint));
	auto columns = static_cast<int>(ioComponent.columns);
	if (prop::DragInt("Columns", &columns, 1.f, 1, 256))
		ioComponent.columns = static_cast<uint32_t>(std::max(columns, 1));
	fieldTooltip("Columns of the sprite sheet grid of the texture (1: the whole texture).");
	auto rows = static_cast<int>(ioComponent.rows);
	if (prop::DragInt("Rows", &rows, 1.f, 1, 256))
		ioComponent.rows = static_cast<uint32_t>(std::max(rows, 1));
	fieldTooltip("Rows of the sprite sheet grid of the texture.");
	const auto lastFrame = static_cast<int>(ioComponent.columns * ioComponent.rows) - 1;
	auto frame = static_cast<int>(ioComponent.frame);
	if (prop::DragInt("Frame", &frame, 1.f, 0, lastFrame))
		ioComponent.frame = static_cast<uint32_t>(std::clamp(frame, 0, lastFrame));
	fieldTooltip("Frame of the grid shown, row-major from the top left (Lua: ui.set_image_frame).");
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
	handlerRow("On Click Callback", ioComponent.onClickCallback, nullptr,
			   "Lua function name to call when the button is clicked (e.g. on_play_clicked).");
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
	handlerRow("On Value Changed", ioComponent.onValueChangedCallback, nullptr,
			   "Lua function called when the value changes. Receives the new value in _slider_value.");
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
	prop::Checkbox("Modal", &ioComponent.modal);
	fieldTooltip("While shown, the window keeps the focus and the mouse reaches nothing outside it (a dialog).");
	handlerRow("On Close Callback", ioComponent.onCloseCallback, nullptr,
			   "Lua function called when the close button is pressed.");
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
	handlerRow("On Select", ioComponent.onSelectCallback, nullptr,
			   "Lua function called when the selection changes. Reads _list_entity and _selected_index (1-based).");
	handlerRow("On Bind", ioComponent.onBindCallback, nullptr,
			   "Lua function called once for a row in view nobody filled. Reads _list_entity and _row_index "
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

void renderProps(HudMarker& ioComponent) {
	prop::Checkbox("Enabled", &ioComponent.enabled);
	fieldTooltip("Draw the marker; a script switches it with entity.set_hud_marker_enabled.");
	widgets::textureField("Icon", ioComponent.icon);
	fieldTooltip("Icon drawn over the entity; a diamond of the colour without one.");
	prop::ColorEdit4("Color", reinterpret_cast<float*>(&ioComponent.color));
	fieldTooltip("Colour multiplied with the icon, also the colour of the edge arrow.");
	prop::DragFloat2("Size", ioComponent.size.data(), 1.0f, 1.0f, 512.0f, "%.0f");
	fieldTooltip("Size of the icon in reference pixels (1280x720, scaled with the screen height).");
	drawVec3Control("Offset", ioComponent.offset);
	fieldTooltip("Anchor relative to the entity origin, in world units: above the head of a character.");
	prop::InputText("Label", &ioComponent.label);
	fieldTooltip("Text drawn under the icon; empty draws none.");
	prop::ColorEdit4("Label Color", reinterpret_cast<float*>(&ioComponent.labelColor));
	prop::DragFloat("Font Size", &ioComponent.fontSize, 0.5f, 4.0f, 128.0f, "%.1f");
	fieldTooltip("Font size of the label, in reference pixels.");
	prop::Checkbox("Show Distance", &ioComponent.showDistance);
	fieldTooltip("Append the distance to the player (the camera without a Player) to the label.");
	prop::DragFloat("Max Distance", &ioComponent.maxDistance, 0.5f, 0.0f, 100000.f, "%.1f");
	fieldTooltip("Hide the marker farther than this distance from the player; 0 never hides it.");
	prop::Checkbox("Off-screen Indicator", &ioComponent.offscreenIndicator);
	fieldTooltip("Keep the marker on the screen edge with an arrow when the entity is out of view.");
	ImGui::BeginDisabled(!ioComponent.offscreenIndicator);
	prop::DragFloat("Edge Margin", &ioComponent.edgeMargin, 0.5f, 0.0f, 512.0f, "%.0f");
	fieldTooltip("Distance between an edge marker and the screen border, in reference pixels.");
	widgets::textureField("Arrow", ioComponent.arrow);
	fieldTooltip("Sprite of the edge arrow, pointing right in the texture; a chevron of the colour without one.");
	ImGui::EndDisabled();
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
	handlerRow("On Focus", ioComponent.onFocusCallback, nullptr,
			   "Lua function called when the widget takes the focus. Receives the widget id in _focused_entity.");
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

namespace {
void remeshVoxelWorld(VoxelWorld& ioComponent) {
	for (const auto& coord: ioComponent.world.chunkCoordinates())
		if (const auto chunk = ioComponent.world.getChunk(coord))
			chunk->markDirty();
}

auto drawBlockSurface(data::voxel::BlockSurface& ioSurface) -> bool {
	bool changed = drawUnitFloat("Metallic", ioSurface.metallic);
	changed |= drawUnitFloat("Roughness", ioSurface.roughness);
	changed |= prop::DragFloat("Luminance (cd/m2)", &ioSurface.emissive, 10.f, 0.f, data::voxel::g_MaxBlockLuminance,
							   "%.0f", ImGuiSliderFlags_Logarithmic);
	fieldTooltip("Luminance of the tile colour glowing without light: lava 5 000, glowstone 2 000, 0 for none.");
	changed |= drawUnitFloat("Clearcoat", ioSurface.clearcoat);
	fieldTooltip("A smooth clear layer over the block: wet stone, ice, varnish.");
	return changed;
}

void renderBlockTypes(VoxelWorld& ioComponent) {
	if (ioComponent.registry.count() <= 1 || !ImGui::TreeNodeEx("Blocks", ImGuiTreeNodeFlags_SpanAvailWidth))
		return;
	bool changed = false;
	for (data::voxel::BlockId id = 1; id < static_cast<data::voxel::BlockId>(ioComponent.registry.count()); ++id) {
		auto block = ioComponent.registry.get(id);
		ImGui::PushID(static_cast<int>(id));
		if (ImGui::TreeNodeEx("block", ImGuiTreeNodeFlags_SpanAvailWidth, "%u  %s", static_cast<uint32_t>(id),
							  block.name.c_str())) {
			if (drawBlockSurface(block.surface))
				changed |= ioComponent.registry.setBlock(id, block);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	ImGui::TreePop();
	if (changed)
		remeshVoxelWorld(ioComponent);
}
}// namespace

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
	if (prop::Checkbox("Ambient Occlusion", &ioComponent.ambientOcclusion))
		remeshVoxelWorld(ioComponent);
	fieldTooltip("Bake per-vertex ambient occlusion that darkens concave block edges; off gives flat lighting.");
	renderBlockTypes(ioComponent);
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

void renderProps(LightFlicker& ioComponent) {
	prop::Checkbox("Enabled", &ioComponent.enabled);
	fieldTooltip("Switch the flicker off to keep the light at its authored values.");
	enumCombo("Style", ioComponent.style);
	fieldTooltip("Fire: layered noise. Candle: a wavering drawn from the seed. Pulse: two sines of unrelated rates.");
	prop::DragFloat("Amplitude", &ioComponent.amplitude, 0.005f, 0.f, 1.f, "%.3f");
	fieldTooltip("Largest relative swing of the intensity: 0.28 goes from 72 % to 128 % of the authored value.");
	prop::DragFloat("Speed", &ioComponent.speed, 0.01f, 0.01f, 20.f, "%.2f");
	fieldTooltip("Time scale of the flicker.");
	prop::DragFloat("Seed", &ioComponent.seed, 0.5f, -100000.f, 100000.f, "%.1f");
	fieldTooltip("Offset in the noise: lights with different seeds never flicker together.");
	prop::DragFloat3("Color Shift", &ioComponent.colorShift.x(), 0.005f, -1.f, 1.f, "%.3f");
	fieldTooltip("Added to the colour at the brightest moment and taken off at the dimmest. It tints the sprite of an "
				 "entity without a light.");
	prop::DragFloat3("Position Jitter", &ioComponent.positionJitter.x(), 0.001f, 0.f, 10.f, "%.4f");
	fieldTooltip("Largest displacement of the position along each axis, in world units.");
	prop::DragFloat("Dip Rate", &ioComponent.dipRate, 0.01f, 0.f, 10.f, "%.2f");
	fieldTooltip("Dips per second, when a draught bends the flame; 0 for none.");
	prop::DragFloat("Dip Depth", &ioComponent.dipDepth, 0.01f, 0.f, 1.f, "%.2f");
	fieldTooltip("Share of the intensity a full dip takes off.");
	auto follow = static_cast<uint64_t>(ioComponent.follow);
	prop::row("Follow", [&](const char*) -> bool {
		if (scriptEntityPicker(follow, ImGui::GetContentRegionAvail().x)) {
			ioComponent.follow = core::UUID{follow};
			return true;
		}
		return false;
	});
	fieldTooltip("Entity whose flicker this one follows: its scale breathes with the intensity of that entity.");
	prop::DragFloat3("Scale Response", &ioComponent.scaleResponse.x(), 0.005f, -2.f, 2.f, "%.3f");
	fieldTooltip("Relative change of the scale along each axis for a relative change of 1 of the followed intensity.");
}

void renderProps(Oscillator& ioComponent) {
	prop::Checkbox("Enabled", &ioComponent.enabled);
	fieldTooltip("Switch the movement off to keep the entity at its rest position.");
	prop::DragFloat3("Offset", &ioComponent.offset.x(), 0.05f, -1000.f, 1000.f, "%.2f");
	fieldTooltip("Farthest displacement from the rest position, in world units; the viewport draws the range.");
	prop::DragFloat("Period", &ioComponent.period, 0.05f, 0.05f, 600.f, "%.2f s");
	fieldTooltip("Duration of a round trip.");
	prop::DragFloat("Phase", &ioComponent.phase, 0.005f, 0.f, 1.f, "%.3f");
	fieldTooltip("Share of the cycle already done when Play starts.");
	enumCombo("Easing", ioComponent.easing);
	fieldTooltip("Linear: constant speed. Smooth: slowing down towards each end.");
	ImGui::TextDisabled("A body moves through the physics step: a kinematic body carries what stands on it.");
}

void renderProps(Patrol& ioComponent) {
	prop::Checkbox("Enabled", &ioComponent.enabled);
	fieldTooltip("Switch the patrol off to keep the entity where it is.");
	enumCombo("Mode", ioComponent.mode);
	fieldTooltip("At the last waypoint: Loop goes back to the first, PingPong walks the route in reverse.");
	prop::DragFloat("Speed", &ioComponent.speed, 0.05f, 0.f, 100.f, "%.2f");
	fieldTooltip("Walking speed, in world units per second.");
	prop::DragFloat("Wait Time", &ioComponent.waitTime, 0.05f, 0.f, 600.f, "%.2f s");
	fieldTooltip("Seconds spent at each waypoint.");
	prop::DragFloat("Wander Radius", &ioComponent.wanderRadius, 0.05f, 0.f, 1000.f, "%.2f");
	fieldTooltip("Without waypoints: radius of the random wander around the rest position; 0 for none.");
	prop::DragFloat("Seed", &ioComponent.seed, 0.5f, -100000.f, 100000.f, "%.1f");
	fieldTooltip("Seed of the random wander.");
	prop::Checkbox("Face Direction", &ioComponent.faceDirection);
	fieldTooltip("Mirror the sprite (scale x) towards the direction it walks.");
	prop::DragFloat("Hop Height", &ioComponent.hopHeight, 0.005f, 0.f, 10.f, "%.3f");
	fieldTooltip("Height of the hop while walking; 0 for none.");
	prop::DragFloat("Hop Rate", &ioComponent.hopRate, 0.1f, 0.f, 100.f, "%.1f");
	fieldTooltip("Rate of the hop, in radians per second.");
	if (!widgets::subHeader("Waypoints", true))
		return;
	std::optional<std::size_t> removed;
	for (std::size_t index = 0; index < ioComponent.waypoints.size(); ++index) {
		ImGui::PushID(static_cast<int>(index));
		const float side = ImGui::GetFrameHeight();
		ImGui::SetNextItemWidth(
				std::max(side, ImGui::GetContentRegionAvail().x - side - ImGui::GetStyle().ItemSpacing.x));
		ImGui::DragFloat3("##waypoint", &ioComponent.waypoints[index].x(), 0.05f, 0.f, 0.f, "%.2f");
		ImGui::SameLine();
		if (widgets::iconButton("##remove", {.glyph = icon::x, .tooltip = "Remove the waypoint"}))
			removed = index;
		ImGui::PopID();
	}
	if (removed)
		ioComponent.waypoints.erase(ioComponent.waypoints.begin() + static_cast<std::ptrdiff_t>(*removed));
	if (ImGui::Button("Add Waypoint", {-FLT_MIN, 0.f})) {
		const auto last = ioComponent.waypoints.empty() ? math::vec3{0.f, 0.f, 0.f} : ioComponent.waypoints.back();
		ioComponent.waypoints.emplace_back(last.x() + 1.f, last.y(), last.z());
	}
	fieldTooltip("Add a waypoint one unit after the last one; drag its handle in the viewport to place it.");
}

void renderProps(Drift& ioComponent) {
	prop::Checkbox("Enabled", &ioComponent.enabled);
	fieldTooltip("Switch the drift off to keep the entity at its rest position.");
	prop::DragFloat3("Velocity", &ioComponent.velocity.x(), 0.01f, -1000.f, 1000.f, "%.3f");
	fieldTooltip("Drift velocity, in world units per second.");
	prop::DragFloat3("Range", &ioComponent.range.x(), 0.1f, 0.f, 100000.f, "%.1f");
	fieldTooltip(
			"Half extent of the drift along each axis: the entity jumps from +range back to -range; 0 for no wrap.");
	prop::DragFloat("Camera Follow", &ioComponent.cameraFollow, 0.005f, 0.f, 1.f, "%.3f");
	fieldTooltip("Share of the camera movement followed: 0 pins the entity to the world, 1 to the screen (parallax).");
}

void renderProps(FollowTarget& ioComponent) {
	prop::Checkbox("Enabled", &ioComponent.enabled);
	fieldTooltip("Switch the follow off to leave the entity where it is.");
	auto target = static_cast<uint64_t>(ioComponent.target);
	prop::row("Target", [&](const char*) -> bool {
		if (scriptEntityPicker(target, ImGui::GetContentRegionAvail().x)) {
			ioComponent.target = core::UUID{target};
			return true;
		}
		return false;
	});
	fieldTooltip("Entity followed: this one stays at the offset from its world position.");
	prop::DragFloat3("Offset", &ioComponent.offset.x(), 0.1f, -100000.f, 100000.f, "%.2f");
	fieldTooltip("Offset from the target along the world axes, in world units.");
	prop::Checkbox("Follow Yaw", &ioComponent.followYaw);
	fieldTooltip("Turn with the heading of the target (its rotation about Y), pitch and roll kept: a minimap camera "
				 "looking down keeps the player's forward at the top.");
}

void renderProps(RenderTarget& ioComponent) {
	prop::Checkbox("Enabled", &ioComponent.enabled);
	fieldTooltip("Draw the image each frame; off keeps the last one.");
	prop::InputText("Target Name", &ioComponent.targetName);
	fieldTooltip("Name a HUD UI Image gives in its Render Target field to show this image.");
	std::array size{static_cast<int>(ioComponent.size.x()), static_cast<int>(ioComponent.size.y())};
	if (prop::row("Size", [&](const char* iId) -> bool { return ImGui::DragInt2(iId, size.data(), 1.f, 1, 4096); }))
		ioComponent.size = {static_cast<uint32_t>(std::clamp(size[0], 1, 4096)),
							static_cast<uint32_t>(std::clamp(size[1], 1, 4096))};
	fieldTooltip("Size of the image, in pixels. The camera of the entity draws into it: give it the same aspect.");
	prop::ColorEdit4("Clear Color", &ioComponent.clearColor.x());
	fieldTooltip("Colour of the image where nothing is drawn.");
	for (const auto& layer: renderer::Renderer::getRenderStack().getLayers()) {
		const std::string& name = layer->getName();
		bool drawn = ioComponent.drawsLayer(name);
		if (prop::Checkbox(std::format("Draw {}", name).c_str(), &drawn)) {
			if (drawn)
				std::erase(ioComponent.hiddenLayers, name);
			else
				ioComponent.hiddenLayers.push_back(name);
		}
		fieldTooltip(
				"Whether the image shows this layer of the renderer stack (leave out the markers over the world).");
	}
}

void renderProps(ProceduralSky& ioComponent) {
	prop::Checkbox("Enabled", &ioComponent.enabled);
	fieldTooltip("Compute the sky from the sun; off goes back to the image or the ambient of the Environment.");
	prop::DragFloat3("Sun Direction", &ioComponent.sunDirection.x(), 0.01f, -1.f, 1.f, "%.3f");
	fieldTooltip("Direction towards the sun. A Day Night Cycle on the entity writes it from its time of day.");
	prop::DragFloat("Turbidity", &ioComponent.turbidity, 0.05f, 1.7f, 10.f, "%.2f");
	fieldTooltip("Haze of the air: 2 is a very clear sky, 10 a hazy one with a white horizon.");
	prop::ColorEdit3("Ground Color", &ioComponent.groundColor.x());
	fieldTooltip("Colour of the ground below the horizon, lit by the sky.");
	prop::ColorEdit3("Night Color", &ioComponent.nightColor.x());
	fieldTooltip("Colour of the night sky.");
	prop::DragFloat("Night Brightness", &ioComponent.nightBrightness, 0.001f, 0.f, 1.f, "%.4f");
	fieldTooltip("Brightness of the night sky against the zenith at noon (1): the light of the night.");
	prop::DragFloat("Stars", &ioComponent.stars, 0.01f, 0.f, 10.f, "%.2f");
	fieldTooltip("Brightness of the stars, 0 for none.");
	prop::DragFloat("Disc Size", &ioComponent.discSize, 0.05f, 0.f, 20.f, "%.2f");
	fieldTooltip("Angular diameter of the sun and moon discs, in degrees; 0 for none.");
	int resolution = static_cast<int>(ioComponent.resolution);
	if (prop::DragInt("Resolution", &resolution, 4.f, 64, 1024))
		ioComponent.resolution = static_cast<uint32_t>(std::clamp(resolution, 64, 1024));
	fieldTooltip("Width of the sky image in pixels, its height half of it. Sharper stars, a longer redraw.");
}

void renderProps(ReflectionProbe& ioComponent) {
	const ReflectionProbe defaults;
	resettable("Shape", ioComponent.shape, defaults.shape, drawEnum<ReflectionProbe::Shape>);
	fieldTooltip("Volume where the probe replaces the reflections of the environment, centred on the entity and "
				 "aligned on the world axes.");
	if (ioComponent.shape == ReflectionProbe::Shape::Box) {
		resettable("Extents", ioComponent.extents, defaults.extents, [](const char* iId, math::vec3& ioField) -> bool {
			if (!prop::DragFloat3(iId, &ioField.x(), 0.01f, 0.01f, 10000.f))
				return false;
			ioField = {std::max(ioField.x(), 0.01f), std::max(ioField.y(), 0.01f), std::max(ioField.z(), 0.01f)};
			return true;
		});
		fieldTooltip("Half size of the box along X, Y and Z, in world units: fit it to the walls of a room.");
	} else {
		resetFloat("Radius", ioComponent.radius, defaults.radius, 0.01f, 0.01f, 10000.f);
		fieldTooltip("Radius of the sphere of influence, in world units.");
	}
	resetFloat("Blend Distance", ioComponent.blendDistance, defaults.blendDistance, 0.01f, 0.f, 1000.f);
	fieldTooltip("Distance inside the volume over which the probe fades in from its border, blending with its "
				 "neighbour or the environment.");
	resetCheckbox("Parallax", ioComponent.parallax, defaults.parallax);
	fieldTooltip("Project the reflections on the volume, so a room reflects its walls at the right place; off sees "
				 "them as if infinitely far.");
	resetFloat("Intensity", ioComponent.intensity, defaults.intensity, 0.01f, 0.f, 100.f);
	fieldTooltip("Scale of the captured reflections.");
	resettable("Resolution", ioComponent.resolution, defaults.resolution,
			   [](const char* iId, uint32_t& ioField) -> bool {
				   constexpr std::array<const char*, 6> sizes{"16", "32", "64", "128", "256", "512"};
				   int current = std::clamp(std::countr_zero(ioField) - 4, 0, 5);
				   if (!prop::Combo(iId, &current, std::span<const char* const>{sizes}))
					   return false;
				   ioField = 16u << static_cast<uint32_t>(current);
				   return true;
			   });
	fieldTooltip("Size of each face of the captured cube, in texels: sharper mirror reflections, a longer capture.");
	resetCheckbox("Realtime", ioComponent.realtime, defaults.realtime);
	fieldTooltip("Capture the probe again while the scene runs, the realtime probes in turn, one per frame; off "
				 "captures it once, when it moves or is baked.");
	prop::row("Capture", [&](const char* /*iId*/) -> bool {
		if (!ImGui::Button("Bake", {-FLT_MIN, 0.f}))
			return false;
		++ioComponent.captureRequest;
		return true;
	});
	fieldTooltip("Capture the probe again, after the geometry or the lighting around it changed.");
}

namespace {

// A curve over a normalised time, in a collapsible row of its own.
auto curveRow(const char* iLabel, math::Curve& ioCurve, const char* iTooltip) -> bool {
	if (!ImGui::TreeNodeEx(iLabel, ImGuiTreeNodeFlags_SpanAvailWidth))
		return false;
	fieldTooltip(iTooltip);
	const bool changed = widgets::curveEditor(iLabel, ioCurve, {0.f, 120.f});
	ImGui::TreePop();
	return changed;
}

// A colour gradient, in a collapsible row of its own.
auto gradientRow(const char* iLabel, renderer::ColorGradient& ioGradient, const char* iTooltip) -> bool {
	if (!ImGui::TreeNodeEx(iLabel, ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen))
		return false;
	fieldTooltip(iTooltip);
	const bool changed = widgets::gradientEditor(iLabel, ioGradient);
	ImGui::TreePop();
	return changed;
}

void renderParticlePreview(const ParticleEmitter& iComponent) {
	const auto& system = iComponent.m_slot.system;
	prop::row("Preview", [&](const char* /*iId*/) -> bool {
		const float third = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3.f) / 4.f;
		ImGui::BeginDisabled(!system);
		if (system && system->isPlaying()) {
			if (ImGui::Button("Pause", {third, 0.f}))
				system->pause();
		} else if (ImGui::Button("Play", {third, 0.f}) && system) {
			system->play();
		}
		ImGui::SameLine();
		if (ImGui::Button("Restart", {third, 0.f}) && system)
			system->restart();
		ImGui::SameLine();
		if (ImGui::Button("Stop", {third, 0.f}) && system)
			system->stop();
		ImGui::SameLine();
		if (ImGui::Button("Burst", {third, 0.f}) && system)
			system->emit(std::max(iComponent.settings.burst, 10u));
		ImGui::EndDisabled();
		return false;
	});
	fieldTooltip("Play, pause, restart or stop the emitter in the viewport, or spawn a burst; nothing of it is saved.");
	if (system)
		ImGui::TextDisabled("%llu spawned, %u slots, %s", static_cast<unsigned long long>(system->getSpawnedCount()),
							system->getCapacity(), system->isEmitting() ? "emitting" : "not emitting");
}

void renderParticleEmission(renderer::ParticleEmitterSettings& ioSettings) {
	const renderer::ParticleEmitterSettings defaults;
	resetUint("Max Particles", ioSettings.maxParticles, defaults.maxParticles, 1.f, 1, renderer::g_maxParticles);
	fieldTooltip("Largest number of living particles: past it, the oldest is replaced (1024 at most when blended with "
				 "Alpha, the GPU sort).");
	resetFloat("Rate", ioSettings.rate, defaults.rate, 0.1f, 0.f, 100000.f, "%.1f /s");
	fieldTooltip("Particles spawned per second while emitting.");
	resetUint("Burst", ioSettings.burst, defaults.burst, 0.5f, 0, renderer::g_maxParticles);
	fieldTooltip("Particles spawned at once each time the emission starts or loops.");
	resetFloat("Duration", ioSettings.duration, defaults.duration, 0.01f, 0.f, 3600.f, "%.2f s");
	fieldTooltip("Seconds the emission lasts; 0 emits forever.");
	resetCheckbox("Loop", ioSettings.loop, defaults.loop);
	fieldTooltip("Start the emission again at the end of its duration.");
	resetFloat("Lifetime Min", ioSettings.lifetimeMin, defaults.lifetimeMin, 0.01f, 0.001f, 600.f, "%.2f s");
	resetFloat("Lifetime Max", ioSettings.lifetimeMax, defaults.lifetimeMax, 0.01f, 0.001f, 600.f, "%.2f s");
	fieldTooltip("Each particle lives a random time between the two.");
	resettable("Shape", ioSettings.shape, defaults.shape, drawEnum<renderer::ParticleShape>);
	fieldTooltip("Where particles spawn: a point, a sphere, a box, a cone (along +Y) or a circle (2D, the XY plane).");
	resettable("Shape Size", ioSettings.shapeSize, defaults.shapeSize,
			   [](const char* iId, math::vec3& ioField) -> bool {
				   return prop::DragFloat3(iId, &ioField.x(), 0.01f, 0.f, 10000.f);
			   });
	fieldTooltip("Radius in X (sphere, cone, circle) or half extents (box), in the space of the entity.");
	if (ioSettings.shape == renderer::ParticleShape::Cone) {
		resetDegrees("Cone Angle", ioSettings.coneAngle, defaults.coneAngle, 0.f, 180.f);
		fieldTooltip("Half angle of the cone the particles leave in.");
	}
	resetUint("Seed", ioSettings.seed, defaults.seed, 1.f, 0, 1000000);
	fieldTooltip("Seed of the random draws: two emitters with different seeds never look the same.");
}

void renderParticleMotion(renderer::ParticleEmitterSettings& ioSettings) {
	const renderer::ParticleEmitterSettings defaults;
	resetFloat("Speed Min", ioSettings.speedMin, defaults.speedMin, 0.01f, 0.f, 10000.f);
	resetFloat("Speed Max", ioSettings.speedMax, defaults.speedMax, 0.01f, 0.f, 10000.f);
	fieldTooltip("Start speed along the direction of the shape, in units per second.");
	resettable("Velocity", ioSettings.velocity, defaults.velocity, [](const char* iId, math::vec3& ioField) -> bool {
		return prop::DragFloat3(iId, &ioField.x(), 0.01f, -10000.f, 10000.f);
	});
	fieldTooltip("Velocity added to every particle at spawn, in the space of the entity.");
	resettable("Gravity", ioSettings.gravity, defaults.gravity, [](const char* iId, math::vec3& ioField) -> bool {
		return prop::DragFloat3(iId, &ioField.x(), 0.01f, -10000.f, 10000.f);
	});
	fieldTooltip("Acceleration of every particle, in world units per second squared (smoke rises, sparks fall).");
	resetFloat("Drag", ioSettings.drag, defaults.drag, 0.01f, 0.f, 100.f);
	fieldTooltip("Share of the velocity lost per second.");
	resetFloat("Spin Min", ioSettings.spinMin, defaults.spinMin, 0.01f, -100.f, 100.f, "%.2f rad/s");
	resetFloat("Spin Max", ioSettings.spinMax, defaults.spinMax, 0.01f, -100.f, 100.f, "%.2f rad/s");
	fieldTooltip("Each particle turns at a random speed between the two.");
	resettable("Space", ioSettings.space, defaults.space, drawEnum<renderer::ParticleSpace>);
	fieldTooltip("World: particles stay where they were spawned when the entity moves; Local: they move with it.");
}

void renderParticleLook(renderer::ParticleEmitterSettings& ioSettings) {
	const renderer::ParticleEmitterSettings defaults;
	resetFloat("Size Min", ioSettings.sizeMin, defaults.sizeMin, 0.005f, 0.f, 1000.f);
	resetFloat("Size Max", ioSettings.sizeMax, defaults.sizeMax, 0.005f, 0.f, 1000.f);
	fieldTooltip("Start size of a particle, in world units.");
	curveRow("Size over Life", ioSettings.sizeOverLife,
			 "Size multiplier over the life of a particle, from its birth (0) to its death (1); empty keeps 1.");
	prop::ColorEdit4("Color", ioSettings.color.data());
	fieldTooltip("Tint of every particle.");
	gradientRow("Color over Life", ioSettings.colorOverLife,
				"Colour and opacity over the life of a particle, multiplying the tint.");
	resetFloat("Emissive", ioSettings.emissive, defaults.emissive, 1.f, 0.f, 1000000.f, "%.0f nits");
	fieldTooltip(
			"Luminance added to the colour, pre-exposed like the lights: with the bloom of the camera the particles "
			"glow (a few thousand nits at night, more under the sun).");
	resettable("Blend", ioSettings.blend, defaults.blend, drawEnum<renderer::ParticleBlend>);
	fieldTooltip("Additive: added to the scene in any order (fire, sparks); Alpha: blended over it, sorted far to near "
				 "on the GPU (smoke, dust).");
	widgets::textureField("Texture", ioSettings.texture);
	fieldTooltip("Texture of a particle; none draws a soft disc.");
}

void renderParticleTrail(renderer::ParticleEmitterSettings& ioSettings) {
	const renderer::ParticleEmitterSettings defaults;
	resetUint("Trail Length", ioSettings.trailLength, defaults.trailLength, 0.1f, 0, renderer::g_maxParticleTrail);
	fieldTooltip("Points of the ribbon each particle leaves behind; 0 draws no trail.");
	if (ioSettings.trailLength == 0)
		return;
	resetFloat("Trail Interval", ioSettings.trailInterval, defaults.trailInterval, 0.001f, 0.001f, 10.f, "%.3f s");
	fieldTooltip("Seconds between two points of a trail: longer trails for the same number of points.");
	resetFloat("Trail Width", ioSettings.trailWidth, defaults.trailWidth, 0.01f, 0.f, 10.f);
	fieldTooltip("Width of the trail at the particle, relative to its size; it tapers to the tail.");
}

}// namespace

void renderProps(ParticleEmitter& ioComponent) {
	renderParticlePreview(ioComponent);
	resetCheckbox("Play on Start", ioComponent.playOnStart, true);
	fieldTooltip("Start the emission with the scene in Play; off waits for a script (entity.particles_play).");
	auto& settings = ioComponent.settings;
	if (widgets::subHeader("Emission", true))
		renderParticleEmission(settings);
	if (widgets::subHeader("Motion", true))
		renderParticleMotion(settings);
	if (widgets::subHeader("Look", true))
		renderParticleLook(settings);
	if (widgets::subHeader("Trails"))
		renderParticleTrail(settings);
}

void renderProps(Trail& ioComponent) {
	const Trail defaults;
	resetCheckbox("Enabled", ioComponent.enabled, defaults.enabled);
	fieldTooltip("Record and draw the trail; off clears it.");
	resetFloat("Lifetime", ioComponent.lifetime, defaults.lifetime, 0.01f, 0.f, 600.f, "%.2f s");
	fieldTooltip("Seconds a point of the trail lives: the length of the trail behind a moving entity.");
	resetFloat("Min Distance", ioComponent.minDistance, defaults.minDistance, 0.005f, 0.f, 1000.f);
	fieldTooltip("Distance the entity moves before a new point is recorded, in world units.");
	resetUint("Max Points", ioComponent.maxPoints, defaults.maxPoints, 0.2f, 2,
			  renderer::RendererParticles::g_maxTrailPoints);
	fieldTooltip("Most points of the trail.");
	resetFloat("Width", ioComponent.width, defaults.width, 0.005f, 0.f, 1000.f);
	fieldTooltip("Width of the trail at a width multiplier of 1, in world units.");
	curveRow("Width over Length", ioComponent.widthOverLength,
			 "Width multiplier from the entity (0) to the tail (1); empty tapers from 1 to 0.");
	prop::ColorEdit4("Color", ioComponent.color.data());
	fieldTooltip("Tint of the trail.");
	gradientRow("Color over Length", ioComponent.colorOverLength,
				"Colour from the entity (0) to the tail (1), multiplying the tint; empty fades from opaque to clear.");
	resetFloat("Emissive", ioComponent.emissive, defaults.emissive, 1.f, 0.f, 1000000.f, "%.0f nits");
	fieldTooltip("Luminance added to the colour: with the bloom of the camera the trail glows.");
	resettable("Blend", ioComponent.blend, defaults.blend, drawEnum<renderer::ParticleBlend>);
	fieldTooltip("Additive: added to the scene; Alpha: blended over it.");
}

void renderProps(FirstPersonController& ioComponent) {
	prop::Checkbox("Enabled", &ioComponent.enabled);
	fieldTooltip("Walk the entity on foot; off leaves it to scripts.");
	prop::DragFloat("Walk Speed", &ioComponent.walkSpeed, 0.05f, 0.f, 50.f, "%.2f");
	prop::DragFloat("Run Speed", &ioComponent.runSpeed, 0.05f, 0.f, 50.f, "%.2f");
	fieldTooltip("Speed while the sprint action is held.");
	prop::DragFloat("Look Speed", &ioComponent.lookSpeed, 0.05f, 0.f, 20.f, "%.2f");
	fieldTooltip("Radians per second of the look action (arrow keys, right stick).");
	prop::DragFloat("Mouse Sensitivity", &ioComponent.mouseSensitivity, 0.0001f, 0.0001f, 0.05f, "%.4f");
	fieldTooltip("Radians per pixel while the cursor is captured.");
	prop::Checkbox("Capture Cursor", &ioComponent.captureCursor);
	fieldTooltip("Clicking the view hides and locks the cursor for mouse look (Esc frees it).");
	prop::DragFloat("Eye Height", &ioComponent.eyeHeight, 0.01f, 0.1f, 10.f, "%.2f");
	fieldTooltip("Height of the eye (the transform of the entity) above the feet.");
	prop::DragFloat("Height", &ioComponent.height, 0.01f, 0.2f, 10.f, "%.2f");
	fieldTooltip("Height of the body box that collides with the meshes.");
	prop::DragFloat("Radius", &ioComponent.radius, 0.01f, 0.05f, 5.f, "%.2f");
	fieldTooltip("Half width of the body box.");
	prop::DragFloat("Step Height", &ioComponent.stepHeight, 0.01f, 0.f, 2.f, "%.2f");
	fieldTooltip("Highest step climbed without jumping: a box lower than this is walked onto.");
	prop::DragFloat("Gravity", &ioComponent.gravity, 0.05f, 0.f, 100.f, "%.2f");
	prop::DragFloat("Jump Speed", &ioComponent.jumpSpeed, 0.05f, 0.f, 50.f, "%.2f");
	fieldTooltip("Upward speed of a jump; 0 for no jump.");
	prop::DragFloat("Reach", &ioComponent.reach, 0.05f, 0.f, 50.f, "%.2f");
	fieldTooltip("Farthest an interaction trigger is used from: aim at it (its box is outlined) and press the interact "
				 "action.");
}

void renderProps(DayNightCycle& ioComponent) {
	prop::Checkbox("Enabled", &ioComponent.enabled);
	fieldTooltip("Advance the hours in Play; off freezes the time of day, the lighting still follows it.");
	prop::DragFloat("Time Of Day", &ioComponent.timeOfDay, 0.05f, 0.f, 24.f, "%.2f h");
	fieldTooltip("Hour of the day: sunrise at 6, noon at 12, sunset at 18. Play starts at this hour.");
	prop::DragFloat("Day Length", &ioComponent.dayLength, 1.f, 1.f, 86400.f, "%.0f s");
	fieldTooltip("Duration of a whole day, in seconds.");
	prop::DragFloat("Azimuth", &ioComponent.azimuth, 0.5f, -360.f, 360.f, "%.1f deg");
	fieldTooltip("Direction of the sunrise about the vertical axis: 0 towards -Z, 90 towards +X.");
	prop::DragFloat("Max Elevation", &ioComponent.maxElevation, 0.5f, 1.f, 90.f, "%.1f deg");
	fieldTooltip("Height of the sun at noon above the horizon.");
	auto sun = static_cast<uint64_t>(ioComponent.sun);
	prop::row("Sun", [&](const char*) -> bool {
		if (scriptEntityPicker(sun, ImGui::GetContentRegionAvail().x)) {
			ioComponent.sun = core::UUID{sun};
			return true;
		}
		return false;
	});
	fieldTooltip("Directional light turned along the path of the sun by day and of the moon by night.");
	prop::DragFloat("Sun Illuminance", &ioComponent.sunIlluminance, 100.f, 0.f, 200000.f, "%.0f lx");
	fieldTooltip("Illuminance of the sun at its highest; it fades to 0 at the horizon.");
	prop::ColorEdit3("Sun Color", &ioComponent.sunColor.x());
	fieldTooltip("Colour of the sun high in the sky.");
	prop::ColorEdit3("Sunset Color", &ioComponent.sunsetColor.x());
	fieldTooltip("Colour of the sun near the horizon, at dawn and dusk.");
	prop::DragFloat("Moon Illuminance", &ioComponent.moonIlluminance, 10.f, 0.f, 100000.f, "%.0f lx");
	fieldTooltip("Illuminance of the moon at its highest, brighter than nature for a playable night.");
	prop::ColorEdit3("Moon Color", &ioComponent.moonColor.x());
	fieldTooltip("Colour of the moonlight.");
	prop::DragFloat("Night Exposure", &ioComponent.nightExposure, 0.05f, -10.f, 10.f, "%.2f EV");
	fieldTooltip("Exposure added to the cameras at night in Play, as the eye adapts to the dark.");
}

void setOpenAssetHandler(std::function<void(const std::filesystem::path&)> iHandler) {
	openAssetHandler() = std::move(iHandler);
}

void setEditInGraphHandler(std::function<void(const std::string&)> iHandler) {
	editInGraphHandler() = std::move(iHandler);
}

void setOpenAssetAtLineHandler(std::function<void(const std::filesystem::path&, uint32_t)> iHandler) {
	openAssetAtLineHandler() = std::move(iHandler);
}

auto openAssetAtLine(const std::filesystem::path& iPath, const uint32_t iLine) -> bool {
	if (iPath.empty())
		return false;
	if (script::visual::isVisualScriptPath(iPath.string()))
		return openAsset(iPath);
	if (openAssetAtLineHandler()) {
		openAssetAtLineHandler()(iPath, iLine);
		return true;
	}
	return openAsset(iPath);
}

void setInspectedScene(const scene::Scene* iScene) { inspectedScene() = iScene; }

auto openAsset(const std::filesystem::path& iPath) -> bool {
	if (iPath.empty() || !openAssetHandler())
		return false;
	openAssetHandler()(iPath);
	return true;
}

namespace {

// Combo over a list of names; an empty list falls back to free text.
auto nameCombo(const char* iLabel, std::string& ioValue, const std::vector<std::string>& iNames,
			   const char* iNone = nullptr) -> bool {
	if (iNames.empty() && iNone == nullptr)
		return prop::InputText(iLabel, &ioValue);
	bool changed = false;
	const std::string preview = ioValue.empty() && iNone != nullptr ? std::string{iNone} : ioValue;
	if (prop::BeginCombo(iLabel, preview.c_str())) {
		if (iNone != nullptr && ImGui::Selectable(iNone, ioValue.empty())) {
			ioValue.clear();
			changed = true;
		}
		for (const auto& name: iNames) {
			if (ImGui::Selectable(name.c_str(), name == ioValue)) {
				ioValue = name;
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}

auto removeButton(const char* iTooltip) -> bool {
	return widgets::iconButton("##remove", {.glyph = icon::x, .tooltip = iTooltip});
}

template<typename T>
void eraseAt(std::vector<T>& ioItems, const std::optional<size_t> iIndex) {
	if (iIndex && *iIndex < ioItems.size())
		ioItems.erase(ioItems.begin() + static_cast<std::ptrdiff_t>(*iIndex));
}

auto meshOf(const scene::Entity& iOwner) -> shared<renderer::MeshResource> {
	if (!iOwner || !iOwner.hasComponent<MeshRenderer>())
		return nullptr;
	return iOwner.getComponent<MeshRenderer>().mesh;
}

void renderAnimatorParameters(data::animation::StateMachine& ioMachine) {
	if (!widgets::subHeader("Parameters", true))
		return;
	std::optional<size_t> removed;
	for (size_t index = 0; index < ioMachine.parameters.size(); ++index) {
		auto& parameter = ioMachine.parameters[index];
		ImGui::PushID(static_cast<int>(index));
		prop::InputText("Name", &parameter.name);
		enumCombo("Type", parameter.type);
		if (parameter.type == data::animation::ParameterType::Float)
			prop::DragFloat("Default", &parameter.value, 0.01f, -1.e6f, 1.e6f, "%.2f");
		else if (parameter.type == data::animation::ParameterType::Bool) {
			bool flag = parameter.value >= 0.5f;
			if (prop::Checkbox("Default", &flag))
				parameter.value = flag ? 1.f : 0.f;
		}
		if (removeButton("Remove the parameter"))
			removed = index;
		ImGui::Separator();
		ImGui::PopID();
	}
	eraseAt(ioMachine.parameters, removed);
	if (ImGui::Button("Add Parameter", {-FLT_MIN, 0.f}))
		ioMachine.parameters.push_back({.name = std::format("param{}", ioMachine.parameters.size()),
										.type = data::animation::ParameterType::Float,
										.value = 0.f});
	fieldTooltip("A value the scripts set (`animator.set_float`, `set_bool`, `trigger`) and the transitions test.");
}

void renderAnimatorStates(data::animation::StateMachine& ioMachine, const std::vector<std::string>& iClips) {
	if (!widgets::subHeader("States", true))
		return;
	std::vector<std::string> names;
	names.reserve(ioMachine.states.size());
	for (const auto& state: ioMachine.states) names.push_back(state.name);
	nameCombo("Entry State", ioMachine.entryState, names, "(first state)");
	fieldTooltip("State the machine starts in.");
	std::optional<size_t> removed;
	for (size_t index = 0; index < ioMachine.states.size(); ++index) {
		auto& state = ioMachine.states[index];
		ImGui::PushID(static_cast<int>(index));
		prop::InputText("Name", &state.name);
		nameCombo("Clip", state.clip, iClips, "(rest pose)");
		prop::DragFloat("Speed", &state.speed, 0.01f, -10.f, 10.f, "%.2f");
		prop::Checkbox("Loop", &state.loop);
		if (removeButton("Remove the state"))
			removed = index;
		ImGui::Separator();
		ImGui::PopID();
	}
	eraseAt(ioMachine.states, removed);
	if (ImGui::Button("Add State", {-FLT_MIN, 0.f}))
		ioMachine.states.push_back({.name = std::format("state{}", ioMachine.states.size()),
									.clip = iClips.empty() ? std::string{} : iClips.front(),
									.speed = 1.f,
									.loop = true});
	if (!iClips.empty() && ImGui::Button("Add a State per Clip", {-FLT_MIN, 0.f})) {
		for (const auto& clip: iClips) {
			if (!ioMachine.findState(clip))
				ioMachine.states.push_back({.name = clip, .clip = clip, .speed = 1.f, .loop = true});
		}
	}
	fieldTooltip("One state per clip of the mesh not yet played by a state, named after the clip.");
}

void renderTransitionConditions(data::animation::StateTransition& ioTransition,
								const std::vector<std::string>& iParameters) {
	std::optional<size_t> removed;
	for (size_t index = 0; index < ioTransition.conditions.size(); ++index) {
		auto& condition = ioTransition.conditions[index];
		ImGui::PushID(static_cast<int>(index));
		nameCombo("If", condition.parameter, iParameters);
		enumCombo("Test", condition.op);
		if (condition.op == data::animation::ConditionOp::Greater || condition.op == data::animation::ConditionOp::Less)
			prop::DragFloat("Threshold", &condition.threshold, 0.01f, -1.e6f, 1.e6f, "%.2f");
		if (removeButton("Remove the condition"))
			removed = index;
		ImGui::PopID();
	}
	eraseAt(ioTransition.conditions, removed);
	if (ImGui::Button("Add Condition", {-FLT_MIN, 0.f}))
		ioTransition.conditions.push_back({.parameter = iParameters.empty() ? std::string{} : iParameters.front(),
										   .op = data::animation::ConditionOp::Greater,
										   .threshold = 0.f});
}

void renderAnimatorTransitions(data::animation::StateMachine& ioMachine) {
	if (!widgets::subHeader("Transitions", true))
		return;
	std::vector<std::string> states{std::string{data::animation::StateMachine::g_anyState}};
	states.reserve(ioMachine.states.size() + 1);
	for (const auto& state: ioMachine.states) states.push_back(state.name);
	const std::vector<std::string> targets{states.begin() + 1, states.end()};
	std::vector<std::string> parameters;
	parameters.reserve(ioMachine.parameters.size());
	for (const auto& parameter: ioMachine.parameters) parameters.push_back(parameter.name);
	std::optional<size_t> removed;
	for (size_t index = 0; index < ioMachine.transitions.size(); ++index) {
		auto& transition = ioMachine.transitions[index];
		ImGui::PushID(static_cast<int>(index));
		nameCombo("From", transition.from, states);
		nameCombo("To", transition.to, targets);
		prop::DragFloat("Fade", &transition.duration, 0.01f, 0.f, 10.f, "%.2f s");
		fieldTooltip("Length of the cross-fade, in seconds.");
		bool waits = transition.exitTime >= 0.f;
		if (prop::Checkbox("Exit Time", &waits))
			transition.exitTime = waits ? 1.f : -1.f;
		fieldTooltip("Wait for a normalized time of the source state (1 = the end of its clip) before leaving it.");
		if (waits)
			prop::DragFloat("At", &transition.exitTime, 0.01f, 0.f, 100.f, "%.2f");
		renderTransitionConditions(transition, parameters);
		if (removeButton("Remove the transition"))
			removed = index;
		ImGui::Separator();
		ImGui::PopID();
	}
	eraseAt(ioMachine.transitions, removed);
	if (ImGui::Button("Add Transition", {-FLT_MIN, 0.f}))
		ioMachine.transitions.push_back({.from = states.size() > 1 ? states[1] : states.front(),
										 .to = states.size() > 2 ? states[2] : std::string{},
										 .duration = 0.2f,
										 .exitTime = -1.f,
										 .conditions = {}});
	fieldTooltip("Transitions are tested in order: the first one whose conditions hold fires.");
}

}// namespace

void renderProps(Animator& ioComponent, const scene::Entity& iOwner) {
	const auto mesh = meshOf(iOwner);
	std::vector<std::string> clips;
	if (mesh) {
		for (const auto& clip: mesh->getClips()) clips.push_back(clip.name);
	}
	if (!mesh || !mesh->isSkinned())
		widgets::notice("The Mesh Renderer of this entity draws no skinned mesh: the animator has nothing to move.");
	prop::Checkbox("Playing", &ioComponent.playing);
	fieldTooltip("Off holds the current pose.");
	prop::DragFloat("Speed", &ioComponent.speed, 0.01f, 0.f, 10.f, "%.2f");
	fieldTooltip("Time scale of the whole machine.");
	if (prop::Checkbox("Preview", &ioComponent.preview) && !ioComponent.preview)
		ioComponent.invalidate();
	fieldTooltip("Play the machine in Edit mode, to check the clips and the fades without starting the game.");
	if (const auto state = ioComponent.player.getCurrentState(); state && *state < ioComponent.machine.states.size())
		ImGui::Text("State: %s (%.0f %%)", ioComponent.machine.states[*state].name.c_str(),
					static_cast<double>(ioComponent.player.getBlendWeight()) * 100.0);
	if (mesh && mesh->getSkeleton())
		ImGui::Text("Bones: %zu, clips: %zu", mesh->getSkeleton()->getBoneCount(), clips.size());
	renderAnimatorParameters(ioComponent.machine);
	renderAnimatorStates(ioComponent.machine, clips);
	renderAnimatorTransitions(ioComponent.machine);
}

auto getSocketSelection() -> SocketSelection& {
	static SocketSelection s_selection;
	return s_selection;
}

void renderProps(MeshSockets& ioComponent, const scene::Entity& iOwner) {
	const auto mesh = meshOf(iOwner);
	std::vector<std::string> bones;
	if (mesh && mesh->getSkeleton()) {
		for (const auto& bone: mesh->getSkeleton()->getBones()) bones.push_back(bone.name);
	}
	if (mesh && !mesh->getSockets().empty()) {
		std::string fileSockets;
		for (const auto& socket: mesh->getSockets()) fileSockets += (fileSockets.empty() ? "" : ", ") + socket.name;
		ImGui::TextWrapped("Sockets of the mesh file: %s", fileSockets.c_str());
	}
	std::optional<size_t> removed;
	for (size_t index = 0; index < ioComponent.sockets.size(); ++index) {
		auto& socket = ioComponent.sockets[index];
		ImGui::PushID(static_cast<int>(index));
		auto& selection = getSocketSelection();
		const bool selected = iOwner && selection.is(iOwner.getUUID(), socket.name);
		if (widgets::iconButton("##moveSocket", {.glyph = icon::move,
												 .tooltip = selected ? "Moved in the viewport" : "Move in the viewport",
												 .description = "Select the socket: the viewport gizmo moves and "
																"turns it, relative to its bone.",
												 .selected = selected})) {
			if (selected)
				selection.clear();
			else if (iOwner)
				selection = {.entity = iOwner.getUUID(), .socket = socket.name};
		}
		ImGui::SameLine();
		if (selected)
			ImGui::TextColored({1.f, 0.8f, 0.25f, 1.f}, "%s (selected)", socket.name.c_str());
		else
			ImGui::TextUnformatted(socket.name.c_str());
		prop::InputText("Name", &socket.name);
		nameCombo("Bone", socket.bone, bones, "(entity origin)");
		fieldTooltip("Bone the socket follows in the animation; none for the origin of the entity.");
		prop::DragFloat3("Position", socket.offset.translation().data(), 0.005f, -1.e4f, 1.e4f, "%.3f");
		math::vec3 degrees = socket.offset.rotation() * (180.f / std::numbers::pi_v<float>);
		if (prop::DragFloat3("Rotation", degrees.data(), 0.5f, -360.f, 360.f, "%.1f"))
			socket.offset.rotation() = degrees * (std::numbers::pi_v<float> / 180.f);
		prop::DragFloat3("Scale", socket.offset.scale().data(), 0.005f, 0.001f, 1.e3f, "%.3f");
		if (removeButton("Remove the socket"))
			removed = index;
		ImGui::Separator();
		ImGui::PopID();
	}
	eraseAt(ioComponent.sockets, removed);
	if (ImGui::Button("Add Socket", {-FLT_MIN, 0.f}))
		ioComponent.sockets.push_back({.name = std::format("socket{}", ioComponent.sockets.size()),
									   .bone = bones.empty() ? std::string{} : bones.front(),
									   .offset = {}});
	fieldTooltip("A named point where an entity is snapped (`socket.attach`, the Attach to Socket menu of the "
				 "hierarchy); the viewport draws the sockets of the selected entity, a click on one selects it for "
				 "the gizmo.");
}

void renderProps(SocketAttachment& ioComponent, const scene::Entity& iOwner) {
	std::vector<std::string> names;
	if (iOwner && iOwner.getScene() != nullptr) {
		const auto parentId = iOwner.getComponent<Hierarchy>().parentId;
		if (const auto parent = iOwner.getScene()->findEntityByUUID(parentId); parent)
			names = iOwner.getScene()->getSocketNames(parent);
	}
	if (names.empty())
		widgets::notice("The parent of this entity has no socket: add a Mesh Sockets component to it.");
	nameCombo("Socket", ioComponent.socket, names);
	fieldTooltip("Socket of the parent the entity is snapped on; its transform follows the socket.");
}


auto SplineSelection::contains(const core::UUID iEntity, const size_t iIndex) const -> bool {
	return entity == iEntity && entity != core::UUID{0} && std::ranges::binary_search(points, iIndex);
}

void SplineSelection::select(const core::UUID iEntity, const size_t iIndex, const bool iAdd, const Handle iHandle) {
	if (entity != iEntity || !iAdd) {
		entity = iEntity;
		points.clear();
	}
	handle = iHandle;
	if (const auto found = std::ranges::lower_bound(points, iIndex); found != points.end() && *found == iIndex) {
		if (iAdd)
			points.erase(found);
	} else {
		points.insert(found, iIndex);
	}
	if (points.size() != 1)
		handle = Handle::Point;
}

void SplineSelection::clamp(const core::UUID iEntity, const size_t iCount) {
	if (entity != iEntity) {
		clear();
		return;
	}
	std::erase_if(points, [iCount](const size_t iIndex) -> bool { return iIndex >= iCount; });
	if (points.size() != 1)
		handle = Handle::Point;
}

auto getSplineSelection() -> SplineSelection& {
	static SplineSelection s_selection;
	return s_selection;
}

namespace {

// Point added after the last one, one unit further along the end of the curve.
auto nextPoint(const math::SplineCurve& iCurve) -> math::SplinePoint {
	const auto& points = iCurve.getPoints();
	if (points.empty())
		return {};
	const auto end = iCurve.sampleAtDistance(iCurve.getLength());
	const math::vec3 direction = end.tangent.normSq() > 0.f ? end.tangent : math::vec3{1.f, 0.f, 0.f};
	const math::vec3 position = points.back().position + direction;
	return {.position = position, .tangentIn = direction * -0.33f, .tangentOut = direction * 0.33f};
}

void renderSplinePoints(Spline& ioComponent, const scene::Entity& iOwner) {
	auto& selection = getSplineSelection();
	const core::UUID owner = iOwner ? iOwner.getUUID() : core::UUID{0};
	auto points = ioComponent.curve.getPoints();
	selection.clamp(owner, points.size());
	const bool bezier = ioComponent.curve.getInterpolation() == math::SplineInterpolation::Bezier;
	bool changed = false;
	std::optional<size_t> removed;
	for (size_t index = 0; index < points.size(); ++index) {
		auto& point = points[index];
		ImGui::PushID(static_cast<int>(index));
		const bool selected = selection.contains(owner, index);
		if (ImGui::Selectable(std::format("Point {}", index).c_str(), selected, ImGuiSelectableFlags_AllowOverlap,
							  {ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 2.f, 0.f}))
			selection.select(owner, index, ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift);
		ImGui::SameLine();
		if (bezier && widgets::iconButton("##smooth", {.glyph = icon::route,
													   .tooltip = "Smooth the point",
													   .description = "Set its two handles from its neighbours."})) {
			point.tangentOut = ioComponent.curve.getSmoothTangent(index);
			point.tangentIn = point.tangentOut * -1.f;
			changed = true;
		}
		ImGui::SameLine();
		if (removeButton("Delete the point"))
			removed = index;
		changed |= prop::DragFloat3("Position", point.position.data(), 0.05f, 0.f, 0.f, "%.2f");
		if (bezier) {
			changed |= prop::DragFloat3("Handle In", point.tangentIn.data(), 0.05f, 0.f, 0.f, "%.2f");
			changed |= prop::DragFloat3("Handle Out", point.tangentOut.data(), 0.05f, 0.f, 0.f, "%.2f");
		}
		ImGui::PopID();
	}
	if (removed) {
		eraseAt(points, removed);
		selection.clear();
		changed = true;
	}
	if (ImGui::Button("Add Point", {-FLT_MIN, 0.f})) {
		points.push_back(nextPoint(ioComponent.curve));
		selection.select(owner, points.size() - 1);
		changed = true;
	}
	fieldTooltip("Append a point one unit after the end of the curve; drag its handle in the viewport to place it. "
				 "In the viewport: click a point, Ctrl+click adds it to the selection, Alt+click on the curve inserts "
				 "a point, Delete removes the selected points.");
	if (changed)
		ioComponent.curve.setPoints(std::move(points));
	const bool canInsert = selection.points.size() == 1 && selection.entity == owner &&
						   selection.points.front() < ioComponent.curve.getSegmentCount();
	ImGui::BeginDisabled(!canInsert);
	if (ImGui::Button("Insert After Selected", {-FLT_MIN, 0.f}) && canInsert) {
		const size_t inserted =
				ioComponent.curve.insertAtParameter(static_cast<float>(selection.points.front()) + 0.5f);
		selection.select(owner, inserted);
	}
	ImGui::EndDisabled();
	fieldTooltip("Insert a point in the middle of the segment after the selected point, keeping the shape.");
	ImGui::BeginDisabled(selection.entity != owner || selection.points.empty());
	if (ImGui::Button("Delete Selected", {-FLT_MIN, 0.f}) && selection.entity == owner) {
		auto kept = ioComponent.curve.getPoints();
		for (const auto index: selection.points | std::views::reverse)
			if (index < kept.size())
				kept.erase(kept.begin() + static_cast<std::ptrdiff_t>(index));
		ioComponent.curve.setPoints(std::move(kept));
		selection.clear();
	}
	ImGui::EndDisabled();
}

void renderSplineLanes(Spline& ioComponent) {
	std::optional<size_t> removed;
	for (size_t index = 0; index < ioComponent.lanes.size(); ++index) {
		auto& lane = ioComponent.lanes[index];
		ImGui::PushID(static_cast<int>(index));
		prop::InputText("Name", &lane.name);
		ImGui::SameLine();
		if (removeButton("Remove the lane"))
			removed = index;
		prop::DragFloat("Offset", &lane.offset, 0.05f, -1.e4f, 1.e4f, "%.2f");
		fieldTooltip("Lateral offset from the curve: positive to the right when looking along it.");
		prop::DragFloat("Height", &lane.height, 0.05f, -1.e4f, 1.e4f, "%.2f");
		fieldTooltip("Offset along the up direction.");
		ImGui::Separator();
		ImGui::PopID();
	}
	eraseAt(ioComponent.lanes, removed);
	if (ImGui::Button("Add Lane", {-FLT_MIN, 0.f}))
		ioComponent.lanes.push_back({.name = std::format("lane{}", ioComponent.lanes.size()),
									 .offset = ioComponent.lanes.empty() ? 1.f : ioComponent.lanes.back().offset + 1.f,
									 .height = 0.f});
	fieldTooltip("A path beside the curve that followers and scripts name: a lane of a road, a track of a rail.");
}

// A double field with its reset slot, dragged at a speed proportional to the value.
auto resetDouble(const char* iLabel, double& ioValue, const double iDefault, const double iMin, const double iMax,
				 const char* iFormat = "%.1f") -> bool {
	return resettable(iLabel, ioValue, iDefault, [&](const char* iId, double& ioField) -> bool {
		const float speed = static_cast<float>(std::max(std::abs(ioField) * 0.002, 0.01));
		return prop::row(iId, [&](const char* iRowId) -> bool {
			if (!ImGui::DragScalar(iRowId, ImGuiDataType_Double, &ioField, speed, &iMin, &iMax, iFormat))
				return false;
			ioField = std::clamp(ioField, iMin, iMax);
			return true;
		});
	});
}

void renderPlanetShape(renderer::PlanetSettings& ioSettings) {
	const renderer::PlanetSettings defaults;
	resetDouble("Radius", ioSettings.radius, defaults.radius, 1.0, 1e12, "%.1f m");
	fieldTooltip("Radius of the sea surface, in metres (Earth: 6 371 000).");
	resetFloat("Height Scale", ioSettings.heightScale, defaults.heightScale, 0.5f, 0.f, 1e7f, "%.1f m");
	fieldTooltip("Height of the highest ground (a height value of 1) above the lowest (0), in metres.");
	resetFloat("Sea Level", ioSettings.seaLevel, defaults.seaLevel, 0.005f, 0.f, 1.f);
	fieldTooltip("Height value of the sea: lower ground is flattened to it and drawn as sea.");
	resetFloat("Detail", ioSettings.detailAmplitude, defaults.detailAmplitude, 0.002f, 0.f, 1.f);
	fieldTooltip("Amplitude of the noise added to the heights of the graph, in height values.");
	resetFloat("Detail Frequency", ioSettings.detailFrequency, defaults.detailFrequency, 0.1f, 0.f, 10000.f);
	fieldTooltip("Cycles of the detail noise around the planet.");
}

void renderPlanetLod(renderer::PlanetSettings& ioSettings) {
	const renderer::PlanetSettings defaults;
	resetUint("Max Level", ioSettings.maxLevel, defaults.maxLevel, 0.1f, 0, renderer::g_planetMaxLevel);
	fieldTooltip("Deepest level of the quadtree of a cube face: each level halves the chunks near the camera.");
	resetFloat("LOD Factor", ioSettings.lodFactor, defaults.lodFactor, 0.01f, 0.5f, 16.f);
	fieldTooltip("A chunk splits while the camera is closer than this many times its size: higher is finer.");
	resetUint("Chunk Resolution", ioSettings.chunkResolution, defaults.chunkResolution, 0.2f, 2,
			  renderer::g_planetMaxChunkResolution);
	fieldTooltip("Quads along a side of a chunk.");
	resetDouble("Scaled Space", ioSettings.scaledSpaceDistance, defaults.scaledSpaceDistance, 0.0, 1e15, "%.0f m");
	fieldTooltip("Beyond this distance from its centre the planet is drawn shrunk toward the camera, its angular size "
				 "kept, with the coarsest chunks only; 0 never.");
}

void renderPlanetLook(renderer::PlanetSettings& ioSettings) {
	const renderer::PlanetSettings defaults;
	resetColor("Sea", ioSettings.seaColor, defaults.seaColor);
	resetColor("Low Land", ioSettings.lowColor, defaults.lowColor);
	resetColor("High Land", ioSettings.highColor, defaults.highColor);
	resetColor("Peaks", ioSettings.peakColor, defaults.peakColor);
	fieldTooltip("Colours of the ground by height, from the sea floor to the peaks.");
	resetCheckbox("Atmosphere", ioSettings.atmosphere, defaults.atmosphere);
	fieldTooltip("Draw the atmosphere shell: the sky from the ground, the blue limb from orbit.");
	if (!ioSettings.atmosphere)
		return;
	resetFloat("Thickness", ioSettings.atmosphereHeight, defaults.atmosphereHeight, 0.5f, 0.f, 1e8f, "%.1f m");
	fieldTooltip("Height of the atmosphere above the sea surface, in metres (Earth: 100 000).");
	resettable("Rayleigh", ioSettings.rayleighDepth, defaults.rayleighDepth,
			   [](const char* iId, math::vec3& ioField) -> bool {
				   return prop::DragFloat3(iId, ioField.data(), 0.001f, 0.f, 10.f, "%.3f");
			   });
	fieldTooltip("Optical depth of the air at the zenith per channel (red, green, blue): the colour of the sky.");
	resetFloat("Haze", ioSettings.mieDepth, defaults.mieDepth, 0.001f, 0.f, 10.f);
	fieldTooltip("Optical depth of the haze (Mie): the glow around the sun.");
	resetFloat("Intensity", ioSettings.scatteringIntensity, defaults.scatteringIntensity, 0.01f, 0.f, 1000.f);
	fieldTooltip("Strength of the scattered sunlight.");
}

}// namespace

void renderProps(Planet& ioComponent) {
	const Planet defaults;
	if (assetPathRow("Height Graph", ioComponent.graph, "drop a .owlpcg", "Edit the graph."))
		ioComponent.graph.clear();
	if (std::filesystem::path dropped; widgets::assetDropTarget(widgets::AssetKind::Any, dropped)) {
		if (dropped.extension() == ".owlpcg")
			ioComponent.graph = dropped.generic_string();
	}
	fieldTooltip("A PCG graph whose Write Heights node gives the relief; without one the detail noise alone shapes "
				 "the ground.");
	resettable("Seed", ioComponent.seed, defaults.seed, [](const char* iId, uint64_t& ioField) -> bool {
		return prop::row(iId, [&](const char* iRowId) -> bool {
			return ImGui::InputScalar(iRowId, ImGuiDataType_U64, &ioField);
		});
	});
	fieldTooltip("Seed of the graph and of the detail noise: the same seed gives the same planet.");
	resetUint("Height Resolution", ioComponent.heightResolution, defaults.heightResolution, 1.f,
			  Planet::minHeightResolution, Planet::maxHeightResolution);
	fieldTooltip("Side of the square the graph is evaluated on, in cells.");
	auto& settings = ioComponent.settings;
	if (ImGui::TreeNodeEx("Shape", ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen)) {
		renderPlanetShape(settings);
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Detail and Distance", ImGuiTreeNodeFlags_SpanAvailWidth)) {
		renderPlanetLod(settings);
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Colours and Atmosphere", ImGuiTreeNodeFlags_SpanAvailWidth)) {
		renderPlanetLook(settings);
		ImGui::TreePop();
	}
	if (const auto& terrain = ioComponent.m_runtime.terrain; terrain) {
		const auto& stats = terrain->getStats();
		ImGui::TextDisabled("%u chunks drawn of %u selected, %u building, %u kept", stats.drawn, stats.selected,
							stats.pending, stats.cached);
	} else {
		ImGui::TextDisabled("Drawn by a RendererPlanet layer of the renderer stack.");
	}
}

void renderProps(StreamingSource& ioComponent) {
	const StreamingSource defaults;
	resetCheckbox("Enabled", ioComponent.enabled, defaults.enabled);
	fieldTooltip("Stream the world partition cells around this entity; a scene without an enabled source streams "
				 "around its primary camera.");
	resetFloat("Radius Scale", ioComponent.radiusScale, defaults.radiusScale, 0.01f, StreamingSource::minRadiusScale,
			   StreamingSource::maxRadiusScale);
	fieldTooltip("Factor on the load and unload radii of the scene (World Partition, Scene Settings) for this source.");
}

void renderProps(Spline& ioComponent, const scene::Entity& iOwner) {
	auto interpolation = ioComponent.curve.getInterpolation();
	enumCombo("Interpolation", interpolation);
	if (interpolation != ioComponent.curve.getInterpolation()) {
		if (interpolation == math::SplineInterpolation::Bezier) {
			// Bezier handles start where the Catmull-Rom curve had them: the shape stays.
			auto points = ioComponent.curve.getPoints();
			for (size_t index = 0; index < points.size(); ++index) {
				points[index].tangentOut = ioComponent.curve.getSmoothTangent(index);
				points[index].tangentIn = points[index].tangentOut * -1.f;
			}
			ioComponent.curve.setPoints(std::move(points));
		}
		ioComponent.curve.setInterpolation(interpolation);
	}
	fieldTooltip("Linear: straight segments. CatmullRom: a smooth curve through the points. Bezier: a smooth curve "
				 "shaped by two handles per point.");
	if (bool closed = ioComponent.curve.isClosed(); prop::Checkbox("Closed", &closed))
		ioComponent.curve.setClosed(closed);
	fieldTooltip("Join the last point to the first: a loop.");
	if (auto resolution = static_cast<int>(ioComponent.curve.getResolution());
		prop::DragInt("Resolution", &resolution, 0.2f, 1, 256))
		ioComponent.curve.setResolution(static_cast<uint32_t>(std::clamp(resolution, 1, 256)));
	fieldTooltip("Samples per segment of the table that measures the distances along the curve.");
	prop::DragFloat3("Up", ioComponent.up.data(), 0.01f, -1.f, 1.f, "%.2f");
	fieldTooltip("Up direction of the path, in the space of the entity: (0, 1, 0) in 3D, (0, 0, 1) for a 2D path. "
				 "It places the lanes and turns the followers.");
	ImGui::TextDisabled("Length: %.2f, %zu segments", static_cast<double>(ioComponent.curve.getLength()),
						ioComponent.curve.getSegmentCount());
	if (widgets::subHeader("Points", true))
		renderSplinePoints(ioComponent, iOwner);
	if (widgets::subHeader("Lanes", true))
		renderSplineLanes(ioComponent);
}

void renderProps(SplineFollower& ioComponent) {
	prop::Checkbox("Enabled", &ioComponent.enabled);
	fieldTooltip("Switch the follower off to leave the entity where it is.");
	auto spline = static_cast<uint64_t>(ioComponent.spline);
	prop::row("Spline", [&](const char*) -> bool {
		if (scriptEntityPicker(spline, ImGui::GetContentRegionAvail().x)) {
			ioComponent.spline = core::UUID{spline};
			return true;
		}
		return false;
	});
	fieldTooltip("Entity holding the Spline component the entity moves along.");
	prop::InputText("Lane", &ioComponent.lane);
	fieldTooltip("Name of a lane of the spline; empty for the curve itself.");
	prop::DragFloat("Offset", &ioComponent.offset, 0.05f, -1.e4f, 1.e4f, "%.2f");
	fieldTooltip("Extra lateral offset, added to the lane's.");
	prop::DragFloat("Speed", &ioComponent.speed, 0.05f, -1.e4f, 1.e4f, "%.2f");
	fieldTooltip("Speed along the curve, in units of the spline per second; negative goes backwards.");
	prop::DragFloat("Start Distance", &ioComponent.startDistance, 0.05f, -1.e6f, 1.e6f, "%.2f");
	fieldTooltip("Distance along the curve where Play starts the entity.");
	enumCombo("Mode", ioComponent.mode);
	fieldTooltip("At the end: Once stops (after a lap on a closed spline), Loop starts again, PingPong turns round.");
	prop::Checkbox("Orient", &ioComponent.orient);
	fieldTooltip("Face the direction of the curve: the -Z axis in 3D, the +X axis on a 2D path.");
}

void renderProps(SequencePlayer& ioComponent) {
	if (assetPathRow("Sequence", ioComponent.sequence, "drop a .owlseq", "Open the sequence in its editor."))
		ioComponent.sequence.clear();
	if (std::filesystem::path dropped; extensionDropTarget({".owlseq"}, dropped))
		ioComponent.sequence = dropped.generic_string();
	fieldTooltip("Cutscene played: its camera tracks drive the camera, its events call Lua functions.");
	auto camera = static_cast<uint64_t>(ioComponent.camera);
	prop::row("Camera", [&](const char*) -> bool {
		if (scriptEntityPicker(camera, ImGui::GetContentRegionAvail().x)) {
			ioComponent.camera = core::UUID{camera};
			return true;
		}
		return false;
	});
	fieldTooltip("Entity holding the Camera driven while the sequence plays; none: this entity.");
	prop::Checkbox("Play On Start", &ioComponent.playOnStart);
	fieldTooltip("Start the sequence when the scene enters Play; otherwise Lua `sequence.play`.");
	prop::Checkbox("Loop", &ioComponent.loop);
	fieldTooltip("Start again at the end instead of giving the camera back.");
	prop::DragFloat("Speed", &ioComponent.speed, 0.01f, 0.f, 10.f, "%.2f");
	fieldTooltip("Playback speed: 1 real time, 0 paused.");
	prop::InputText("On Finished", &ioComponent.onFinished);
	fieldTooltip("Lua function called when the sequence ends or is stopped, in the script of this entity, else the "
				 "scene script.");
}

}// namespace owl::gui::component
