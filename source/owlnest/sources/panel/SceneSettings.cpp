/**
 * @file SceneSettings.cpp
 * @author Silmaen
 * @date 06/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneSettings.h"

#include "../AssetKind.h"
#include "../DockLayout.h"

#include "../commands/SceneSettingsCommands.h"

#include <gui/IconFont.h>
#include <gui/widgets/AssetField.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Toggle.h>
#include <gui/widgets/Tooltip.h>
#include <renderer/RendererIsometric.h>
#include <renderer/RendererMesh.h>
#include <renderer/RendererRaycast.h>
#include <renderer/RendererVoxel.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest::panel {

namespace {

auto serializeConfig(const renderer::EnabledRenderersConfig& iConfig) -> std::string {
	if (iConfig.isEmpty())
		return {};
	return iConfig.toYaml();
}

auto indexOfEntry(const renderer::EnabledRenderersConfig& iConfig, const std::string& iName) -> size_t {
	for (size_t i = 0; i < iConfig.entries.size(); ++i)
		if (iConfig.entries[i].name == iName)
			return i;
	return iConfig.entries.size();
}

auto readColor(const YAML::Node& iNode, const char* iKey, const math::vec4& iDefault) -> math::vec4 {
	if (!iNode || !iNode.IsMap())
		return iDefault;
	const auto v = iNode[iKey];
	if (!v || !v.IsSequence() || v.size() != 4)
		return iDefault;
	try {
		return {v[0].as<float>(), v[1].as<float>(), v[2].as<float>(), v[3].as<float>()};
	} catch (const YAML::Exception&) { return iDefault; }
}

void writeColor(YAML::Node& ioNode, const char* iKey, const math::vec4& iValue) {
	YAML::Node seq{YAML::NodeType::Sequence};
	seq.push_back(iValue.r());
	seq.push_back(iValue.g());
	seq.push_back(iValue.b());
	seq.push_back(iValue.a());
	ioNode[iKey] = seq;
}

auto overrideFloat(const YAML::Node& iNode, const char* iKey, const float iDefault) -> float {
	const auto v = iNode[iKey];
	return v && v.IsScalar() ? v.as<float>(iDefault) : iDefault;
}

auto overrideVec3(const YAML::Node& iNode, const char* iKey, const math::vec3& iDefault) -> math::vec3 {
	const auto v = iNode[iKey];
	if (!v || !v.IsSequence() || v.size() != 3)
		return iDefault;
	try {
		return {v[0].as<float>(), v[1].as<float>(), v[2].as<float>()};
	} catch (const YAML::Exception&) { return iDefault; }
}

void writeVec3(YAML::Node& ioNode, const char* iKey, const math::vec3& iValue) {
	YAML::Node seq{YAML::NodeType::Sequence};
	seq.push_back(iValue.x());
	seq.push_back(iValue.y());
	seq.push_back(iValue.z());
	ioNode[iKey] = seq;
}

auto renderColorOverride(YAML::Node& ioOverrides, const char* iKey, const char* iLabel, const math::vec4& iDefault)
		-> bool {
	const math::vec4 color = readColor(ioOverrides, iKey, iDefault);
	std::array<float, 4> values{color.r(), color.g(), color.b(), color.a()};
	gui::widgets::propertyLabel(iLabel);
	bool changed = false;
	if (ImGui::ColorEdit4("##color", values.data())) {
		writeColor(ioOverrides, iKey, {values[0], values[1], values[2], values[3]});
		changed = true;
	}
	if (gui::widgets::propertyReset(ioOverrides[iKey].IsDefined())) {
		ioOverrides.remove(iKey);
		changed = true;
	}
	return changed;
}

auto renderTilesetOverride(YAML::Node& ioOverrides, const char* iKey, const char* iLabel) -> bool {
	std::string path = ioOverrides[iKey] ? ioOverrides[iKey].as<std::string>("") : std::string{};
	gui::widgets::propertyLabel(iLabel, "Tileset drawn as the texture of the plane; empty keeps the solid colour.");
	bool changed = false;
	if (ImGui::InputTextWithHint("##value", "drop a .owltileset", &path)) {
		ioOverrides[iKey] = path;
		changed = true;
	}
	if (std::filesystem::path dropped; gui::widgets::assetDropTarget(gui::widgets::AssetKind::Tileset, dropped)) {
		ioOverrides[iKey] = dropped.generic_string();
		changed = true;
	}
	if (gui::widgets::propertyReset(!path.empty())) {
		ioOverrides.remove(iKey);
		changed = true;
	}
	return changed;
}

auto renderTileIndexOverride(YAML::Node& ioOverrides, const char* iKey, const char* iLabel) -> bool {
	int index = ioOverrides[iKey] ? ioOverrides[iKey].as<int>(0) : 0;
	if (!gui::widgets::propertyInt(iLabel, index, 0, 0, 65535, "%d", "Index of the tile sampled in the tileset."))
		return false;
	ioOverrides[iKey] = std::clamp(index, 0, 65535);
	return true;
}

template<typename Config>
auto renderLightingOverrides(YAML::Node& ioOverrides) -> bool {
	const Config defaults;
	bool changed = false;
	math::vec3 sun = overrideVec3(ioOverrides, "SunDirection", defaults.sunDirection);
	if (gui::widgets::propertyVec3("Sun Direction", sun, defaults.sunDirection, 0.01f,
								   "World-space direction the sun light travels.")) {
		writeVec3(ioOverrides, "SunDirection", sun);
		changed = true;
	}
	float illuminance = overrideFloat(ioOverrides, "SunIlluminance", defaults.sunIlluminance);
	if (gui::widgets::propertyFloat("Sun Illuminance", illuminance, defaults.sunIlluminance, 100.f, "%.0f lx",
									"Illuminance of the sun, lighting a scene that has no light.")) {
		ioOverrides["SunIlluminance"] = std::max(illuminance, 0.f);
		changed = true;
	}
	math::vec3 ambient = overrideVec3(ioOverrides, "Ambient", defaults.ambient);
	if (gui::widgets::propertyVec3("Ambient", ambient, defaults.ambient, 0.01f,
								   "Linear colour of the constant sky lighting a scene without an environment.")) {
		writeVec3(ioOverrides, "Ambient", ambient);
		changed = true;
	}
	float luminance = overrideFloat(ioOverrides, "AmbientLuminance", defaults.ambientLuminance);
	if (gui::widgets::propertyFloat("Ambient Luminance", luminance, defaults.ambientLuminance, 100.f, "%.0f cd/m2",
									"Luminance of a white ambient colour.")) {
		ioOverrides["AmbientLuminance"] = std::max(luminance, 0.f);
		changed = true;
	}
	return changed;
}

auto renderRaycastOverrides(YAML::Node& ioOverrides) -> bool {
	const renderer::RaycastConfig defaults;
	bool changed = false;
	float fov = overrideFloat(ioOverrides, "Fov", defaults.fovDegrees);
	if (gui::widgets::propertyFloat("Field of View", fov, defaults.fovDegrees, 0.5f, "%.1f deg")) {
		ioOverrides["Fov"] = std::clamp(fov, 30.f, 120.f);
		changed = true;
	}
	float maxDist = overrideFloat(ioOverrides, "MaxDistance", defaults.maxDistance);
	if (gui::widgets::propertyFloat("Max Distance", maxDist, defaults.maxDistance, 0.5f, "%.1f cells")) {
		ioOverrides["MaxDistance"] = std::clamp(maxDist, 1.f, 256.f);
		changed = true;
	}
	int numRays = ioOverrides["NumRays"] ? ioOverrides["NumRays"].as<int>(0) : 0;
	if (gui::widgets::propertyInt("Rays", numRays, 0, 0, 4096, numRays == 0 ? "viewport width" : "%d",
								  "Rays cast per frame; 0 casts one per viewport column.")) {
		ioOverrides["NumRays"] = numRays;
		changed = true;
	}
	changed |= renderColorOverride(ioOverrides, "CeilingColor", "Ceiling", defaults.ceilingColor);
	changed |= renderColorOverride(ioOverrides, "FloorColor", "Floor", defaults.floorColor);
	changed |= renderTilesetOverride(ioOverrides, "CeilingTileset", "Ceiling Tileset");
	changed |= renderTileIndexOverride(ioOverrides, "CeilingTileIndex", "Ceiling Tile");
	changed |= renderTilesetOverride(ioOverrides, "FloorTileset", "Floor Tileset");
	changed |= renderTileIndexOverride(ioOverrides, "FloorTileIndex", "Floor Tile");
	changed |= renderColorOverride(ioOverrides, "FogColor", "Fog Colour", defaults.fogColor);
	float fogStart = overrideFloat(ioOverrides, "FogStart", defaults.fogStart);
	if (gui::widgets::propertyFloat("Fog Start", fogStart, defaults.fogStart, 0.5f, "%.1f cells",
									"Distance at which the fog starts mixing in.")) {
		ioOverrides["FogStart"] = std::clamp(fogStart, 0.f, 256.f);
		changed = true;
	}
	float fogEnd = overrideFloat(ioOverrides, "FogEnd", defaults.fogEnd);
	if (gui::widgets::propertyFloat("Fog End", fogEnd, defaults.fogEnd, 0.5f, "%.1f cells",
									"Distance at which the fog is full; not above Fog Start turns the fog off.")) {
		ioOverrides["FogEnd"] = std::clamp(fogEnd, 0.f, 256.f);
		changed = true;
	}
	return changed;
}

auto renderIsometricOverrides(YAML::Node& ioOverrides) -> bool {
	bool changed = false;
	const renderer::IsometricConfig defaults;
	for (const auto& [key, label, fallback, tooltip]:
		 {std::tuple{"TileWidth", "Tile Width", static_cast<int>(defaults.tileSize.x()),
					 "Width of a flat tile, in pixels."},
		  std::tuple{"TileHeight", "Tile Height", static_cast<int>(defaults.tileSize.y()),
					 "Height of a flat tile, in pixels: half the width gives the 2:1 view (30° pitch)."}}) {
		int value = ioOverrides[key] ? ioOverrides[key].as<int>(fallback) : fallback;
		if (gui::widgets::propertyInt(label, value, fallback, 2, 4096, "%d px", tooltip)) {
			ioOverrides[key] = std::clamp(value, 2, 4096);
			changed = true;
		}
	}
	float zStep = ioOverrides["ZStep"] ? ioOverrides["ZStep"].as<float>(defaults.zStep) : defaults.zStep;
	if (gui::widgets::propertyFloat("Height Step", zStep, defaults.zStep, 0.5f, "%.1f px",
									"Screen pixels of one world height unit.")) {
		ioOverrides["ZStep"] = std::clamp(zStep, 0.f, 4096.f);
		changed = true;
	}
	int zoom = ioOverrides["Zoom"] ? ioOverrides["Zoom"].as<int>(0) : 0;
	if (gui::widgets::propertyInt("Zoom", zoom, 0, renderer::IsometricCamera::minZoomLevel,
								  renderer::IsometricCamera::maxZoomLevel, "2^%d",
								  "Zoom level in Play: each step doubles or halves the sprite pixels.")) {
		ioOverrides["Zoom"] = std::clamp(zoom, int32_t{renderer::IsometricCamera::minZoomLevel},
										 int32_t{renderer::IsometricCamera::maxZoomLevel});
		changed = true;
	}
	return changed;
}

}// namespace

auto SceneSettings::renderLayerOverridesEditor(const std::string& iTypeKey, YAML::Node& ioOverrides) -> bool {
	bool changed = false;
	if (!ioOverrides || !ioOverrides.IsMap())
		ioOverrides = YAML::Node{YAML::NodeType::Map};
	if (iTypeKey != "Renderer2D" && iTypeKey != "RendererRaycast" && iTypeKey != "RendererIsometric" &&
		iTypeKey != "RendererMesh" && iTypeKey != "RendererVoxel") {
		ImGui::TextDisabled("No editable override for the type '%s'.", iTypeKey.c_str());
		return false;
	}
	if (!gui::widgets::beginProperties("##overrides"))
		return false;
	if (iTypeKey == "Renderer2D") {
		const std::string space =
				ioOverrides["Space"] ? ioOverrides["Space"].as<std::string>("World") : std::string{"World"};
		gui::widgets::propertyLabel("Space", "World draws with the scene camera, Screen in viewport pixels.");
		if (gui::widgets::beginCombo("##space", space.c_str())) {
			for (const auto* candidate: {"World", "Screen"}) {
				const bool selected = (space == candidate);
				if (ImGui::Selectable(candidate, selected) && !selected) {
					ioOverrides["Space"] = std::string{candidate};
					changed = true;
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		std::ignore = gui::widgets::propertyReset(false);
		float referenceHeight = ioOverrides["ReferenceHeight"] ? ioOverrides["ReferenceHeight"].as<float>(0.f) : 0.f;
		if (gui::widgets::propertyFloat("Reference Height", referenceHeight, 0.f, 1.f,
										referenceHeight <= 0.f ? "pixels" : "%.0f units",
										"Screen space: height of the screen in units, so the overlay scales with the "
										"resolution (720 for a 1280x720 layout); 0 keeps one unit per pixel.")) {
			ioOverrides["ReferenceHeight"] = std::clamp(referenceHeight, 0.f, 16384.f);
			changed = true;
		}
	} else if (iTypeKey == "RendererIsometric") {
		changed = renderIsometricOverrides(ioOverrides);
	} else if (iTypeKey == "RendererMesh") {
		const renderer::MeshLayerConfig defaults;
		bool occlusion = ioOverrides["OcclusionCulling"] ? ioOverrides["OcclusionCulling"].as<bool>(false) : false;
		if (gui::widgets::propertyBool("Occlusion Culling", occlusion, defaults.occlusionCulling,
									   "Skip the opaque meshes hidden behind others: a depth of the meshes and voxel "
									   "worlds is drawn first, then each mesh is tested against it on the GPU.")) {
			ioOverrides["OcclusionCulling"] = occlusion;
			changed = true;
		}
		float scale =
				ioOverrides["OcclusionScale"] ? ioOverrides["OcclusionScale"].as<float>(1.f) : defaults.occlusionScale;
		if (gui::widgets::propertyFloat("Occluder Resolution", scale, defaults.occlusionScale, 0.01f, "%.2f x frame",
										"Resolution of the occluder depth: 1 is exact, lower is cheaper but a mesh "
										"seen through a gap narrower than a texel may be skipped.")) {
			ioOverrides["OcclusionScale"] = std::clamp(scale, 0.125f, 1.f);
			changed = true;
		}
		changed |= renderLightingOverrides<renderer::MeshLayerConfig>(ioOverrides);
	} else if (iTypeKey == "RendererVoxel") {
		changed = renderLightingOverrides<renderer::VoxelConfig>(ioOverrides);
	} else {
		changed = renderRaycastOverrides(ioOverrides);
	}
	gui::widgets::endProperties();
	return changed;
}

auto SceneSettings::renderRendererStackSection() -> bool {
	auto& sceneCfg = m_scene->getEnabledRenderers();
	const auto& projectCfg = mp_project->rendererStack;

	bool changed = false;

	int moveUp = -1;
	int moveDown = -1;
	int detachAt = -1;

	for (int i = 0; i < static_cast<int>(sceneCfg.entries.size()); ++i) {
		auto& entry = sceneCfg.entries[static_cast<size_t>(i)];
		const auto* projectEntry = projectCfg.find(entry.name);
		ImGui::PushID(i);
		bool enabled = entry.enabled;
		if (gui::widgets::toggle("##enabled", &enabled)) {
			entry.enabled = enabled;
			changed = true;
		}
		gui::widgets::itemTooltip("Enabled", nullptr, "Draw this layer in the scene.");
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(entry.name.c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("%s", projectEntry != nullptr ? projectEntry->typeKey.c_str() : "?");
		const float side = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - side * 3.f - spacing * 2.f));
		if (gui::widgets::iconButton("##up", {.glyph = gui::icon::chevronUp, .tooltip = "Move up", .disabled = i == 0}))
			moveUp = i;
		ImGui::SameLine();
		if (gui::widgets::iconButton("##down", {.glyph = gui::icon::chevronDown,
												.tooltip = "Move down",
												.disabled = i + 1 == static_cast<int>(sceneCfg.entries.size())}))
			moveDown = i;
		ImGui::SameLine();
		if (gui::widgets::iconButton("##detach", {.glyph = gui::icon::unlink,
												  .tooltip = "Detach",
												  .description = "Remove the scene override: the layer follows the "
																 "project again."}))
			detachAt = i;
		if (projectEntry != nullptr && gui::widgets::subHeader("Overrides")) {
			auto overrides = entry.overrides.empty() ? YAML::Node{} : YAML::Load(entry.overrides);
			if (renderLayerOverridesEditor(projectEntry->typeKey, overrides)) {
				entry.overrides = YAML::Dump(overrides);
				changed = true;
			}
		}
		ImGui::PopID();
	}

	if (moveUp > 0) {
		const auto idx = static_cast<size_t>(moveUp);
		std::swap(sceneCfg.entries[idx], sceneCfg.entries[idx - 1]);
		changed = true;
	}
	if (moveDown >= 0 && moveDown + 1 < static_cast<int>(sceneCfg.entries.size())) {
		const auto idx = static_cast<size_t>(moveDown);
		std::swap(sceneCfg.entries[idx], sceneCfg.entries[idx + 1]);
		changed = true;
	}
	if (detachAt >= 0) {
		sceneCfg.entries.erase(sceneCfg.entries.begin() + detachAt);
		changed = true;
	}

	return changed;
}

auto SceneSettings::renderAddLayerSection() -> bool {
	auto& sceneCfg = m_scene->getEnabledRenderers();
	const auto& projectCfg = mp_project->rendererStack;

	std::vector<const renderer::RendererStackEntry*> missing;
	for (const auto& projEntry: projectCfg.entries) {
		if (indexOfEntry(sceneCfg, projEntry.name) == sceneCfg.entries.size())
			missing.push_back(&projEntry);
	}
	if (missing.empty())
		return false;

	ImGui::Spacing();
	ImGui::TextDisabled("Add a scene override for:");
	bool added = false;
	for (const auto* entry: missing) {
		ImGui::PushID(entry->name.c_str());
		if (gui::widgets::iconButton("##add", {.glyph = gui::icon::plus, .tooltip = "Add a scene override"})) {
			renderer::EnabledRenderersConfig::Entry newEntry;
			newEntry.name = entry->name;
			newEntry.enabled = true;
			sceneCfg.entries.push_back(std::move(newEntry));
			added = true;
		}
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(entry->name.c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("%s", entry->typeKey.c_str());
		ImGui::PopID();
	}
	return added;
}

void SceneSettings::renderPhysicsSection() const {
	if (!gui::widgets::sectionHeader("Physics", gui::icon::gauge, false).open)
		return;
	const scene::PhysicsSettings defaults;
	const scene::PhysicsSettings before = m_scene->getPhysicsSettings();
	scene::PhysicsSettings edited = before;
	if (gui::widgets::beginProperties("##physics")) {
		auto subSteps = static_cast<int>(edited.solverSubSteps);
		if (gui::widgets::propertyInt("Solver Sub-steps", subSteps, static_cast<int>(defaults.solverSubSteps), 1,
									  static_cast<int>(scene::PhysicsSettings::maxSolverSubSteps)))
			edited.solverSubSteps = static_cast<uint32_t>(subSteps);
		auto workers = static_cast<int>(edited.workerCount);
		if (gui::widgets::propertyInt("Solver Threads", workers, static_cast<int>(defaults.workerCount), 0,
									  static_cast<int>(scene::PhysicsSettings::maxWorkerCount),
									  workers == 0 ? "auto" : "%d",
									  "Threads of the Box2D solver: 0 picks a count from the hardware, 1 is "
									  "single-threaded."))
			edited.workerCount = static_cast<uint32_t>(workers);
		std::ignore = gui::widgets::propertyBool("Interpolate", edited.interpolate, defaults.interpolate,
												 "Blend the drawn positions between the last two physics steps "
												 "(smooth motion, one step of latency).");
		gui::widgets::endProperties();
	}
	ImGui::TextDisabled("One step per simulation tick (%.0f Hz, Project Settings). Applied when Play starts.",
						static_cast<double>(mp_project->simulation.clamped().tickRate));
	gui::widgets::endSection();
	edited = edited.clamped();
	if (edited == before)
		return;
	m_scene->getPhysicsSettings() = edited;
	if (mp_undoManager != nullptr)
		mp_undoManager->push(mkUniq<commands::ModifyPhysicsSettingsCommand>(before, edited));
}

void SceneSettings::renderHudSection() {
	if (!gui::widgets::sectionHeader("HUD", gui::icon::panelsTopLeft, false).open)
		return;
	const auto& own = m_scene->getHudPath();
	const std::string& projectDefault = mp_project->defaultHud;
	const auto choose = [this](const commands::CommandArgs& iArgs) -> void {
		const commands::CommandTarget target{.registry = mp_commands, .scene = m_scene, .undo = mp_undoManager};
		std::ignore = target.execute("scene.set_hud", iArgs);
	};
	std::string preview = std::format("Project default ({})", projectDefault.empty() ? "none" : projectDefault);
	if (own)
		preview = own->empty() ? "None" : *own;
	const bool open = gui::widgets::beginCombo("##hud", preview.c_str());
	if (open) {
		if (!m_hudChoiceOpen)
			m_hudFiles = listAssets(mp_project->projectDirectory, ".owlhud");
		if (ImGui::Selectable("Project default", !own))
			choose({{"use_default", true}});
		if (ImGui::Selectable("None", own && own->empty()))
			choose({{"path", std::string{}}});
		for (const auto& file: m_hudFiles) {
			if (ImGui::Selectable(file.c_str(), own && *own == file))
				choose({{"path", file}});
		}
		ImGui::EndCombo();
	}
	m_hudChoiceOpen = open;
	if (std::filesystem::path dropped; gui::widgets::assetDropTarget(gui::widgets::AssetKind::Hud, dropped))
		choose({{"path", dropped.generic_string()}});
	ImGui::TextDisabled("Drawn over the scene in pixels, shared by the scenes naming the same file.");
	gui::widgets::endSection();
}

void SceneSettings::onImGuiRender() {
	if (!m_visible)
		return;

	if (!ImGui::Begin(panelWindow::title(gui::icon::settings2, "Scene Settings", panelWindow::sceneSettings).c_str(),
					  &m_visible)) {
		ImGui::End();
		return;
	}

	if (mp_project == nullptr || !m_scene) {
		ImGui::TextDisabled("Open a scene to edit its settings.");
		ImGui::End();
		return;
	}

	m_frameSnapshot = serializeConfig(m_scene->getEnabledRenderers());

	switch (m_scene->status) {
		case scene::Scene::Status::Editing:
			ImGui::TextDisabled("Active scene — editing");
			break;
		case scene::Scene::Status::Playing:
			ImGui::TextDisabled("Active scene — playing");
			break;
		case scene::Scene::Status::Victory:
			ImGui::TextDisabled("Active scene — victory");
			break;
		case scene::Scene::Status::Death:
			ImGui::TextDisabled("Active scene — death");
			break;
	}
	ImGui::Separator();

	renderPhysicsSection();
	renderHudSection();

	if (!gui::widgets::sectionHeader("Renderer Stack", gui::icon::layers, false).open) {
		ImGui::End();
		return;
	}
	if (mp_project->rendererStack.isEmpty()) {
		ImGui::TextWrapped("The active project declares no renderer stack — only an "
						   "implicit `Renderer2D` is active. Edit the project settings to "
						   "compose a stack first.");
		gui::widgets::endSection();
		ImGui::End();
		return;
	}
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::TextWrapped("Layers listed here are emitted in this order; project layers "
					   "omitted from the list still run after, in project order.");
	ImGui::PopStyleColor();
	ImGui::Spacing();

	bool changed = renderRendererStackSection();
	if (renderAddLayerSection())
		changed = true;
	gui::widgets::endSection();

	if (changed) {
		const auto afterYaml = serializeConfig(m_scene->getEnabledRenderers());
		if (afterYaml != m_frameSnapshot) {
			const commands::CommandTarget target{.registry = mp_commands, .scene = m_scene, .undo = mp_undoManager};
			std::ignore = target.execute("scene.set_renderers", {{"config", afterYaml}});
			m_frameSnapshot = afterYaml;
			m_dirty = true;
		}
	}

	ImGui::End();
}

}// namespace owl::nest::panel
