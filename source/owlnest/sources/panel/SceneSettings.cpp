/**
 * @file SceneSettings.cpp
 * @author Silmaen
 * @date 06/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneSettings.h"

#include "../DockLayout.h"

#include "../commands/SceneSettingsCommands.h"

#include <gui/IconFont.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Toggle.h>
#include <gui/widgets/Tooltip.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

}// namespace

auto SceneSettings::renderLayerOverridesEditor(const std::string& iTypeKey, YAML::Node& ioOverrides) -> bool {
	bool changed = false;
	if (!ioOverrides || !ioOverrides.IsMap())
		ioOverrides = YAML::Node{YAML::NodeType::Map};
	if (iTypeKey != "Renderer2D" && iTypeKey != "RendererRaycast") {
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
	} else {
		float fov = ioOverrides["Fov"] ? ioOverrides["Fov"].as<float>(75.f) : 75.f;
		if (gui::widgets::propertyFloat("Field of View", fov, 75.f, 0.5f, "%.1f deg")) {
			ioOverrides["Fov"] = std::clamp(fov, 30.f, 120.f);
			changed = true;
		}
		float maxDist = ioOverrides["MaxDistance"] ? ioOverrides["MaxDistance"].as<float>(32.f) : 32.f;
		if (gui::widgets::propertyFloat("Max Distance", maxDist, 32.f, 0.5f, "%.1f cells")) {
			ioOverrides["MaxDistance"] = std::clamp(maxDist, 1.f, 256.f);
			changed = true;
		}
		int numRays = ioOverrides["NumRays"] ? ioOverrides["NumRays"].as<int>(0) : 0;
		if (gui::widgets::propertyInt("Rays", numRays, 0, 0, 4096, numRays == 0 ? "viewport width" : "%d",
									  "Rays cast per frame; 0 casts one per viewport column.")) {
			ioOverrides["NumRays"] = numRays;
			changed = true;
		}
		for (const auto& [key, label, fallback]:
			 {std::tuple{"CeilingColor", "Ceiling", math::vec4{0.18f, 0.20f, 0.30f, 1.f}},
			  std::tuple{"FloorColor", "Floor", math::vec4{0.20f, 0.16f, 0.12f, 1.f}}}) {
			const math::vec4 color = readColor(ioOverrides, key, fallback);
			std::array<float, 4> values{color.r(), color.g(), color.b(), color.a()};
			gui::widgets::propertyLabel(label);
			if (ImGui::ColorEdit4("##color", values.data())) {
				writeColor(ioOverrides, key, {values[0], values[1], values[2], values[3]});
				changed = true;
			}
			std::ignore = gui::widgets::propertyReset(false);
		}
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
		std::ignore = gui::widgets::propertyFloat("Tick Rate", edited.tickRate, defaults.tickRate, 1.f, "%.0f Hz",
												  "Fixed physics steps per second, whatever the frame rate.");
		auto maxSteps = static_cast<int>(edited.maxStepsPerFrame);
		if (gui::widgets::propertyInt("Max Steps per Frame", maxSteps, static_cast<int>(defaults.maxStepsPerFrame), 1,
									  static_cast<int>(scene::PhysicsSettings::maxStepsLimit), "%d",
									  "Time beyond this many steps in one frame is dropped: the game slows down "
									  "instead of stalling."))
			edited.maxStepsPerFrame = static_cast<uint32_t>(maxSteps);
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
	ImGui::TextDisabled("Applied when Play starts.");
	gui::widgets::endSection();
	edited = edited.clamped();
	if (edited == before)
		return;
	m_scene->getPhysicsSettings() = edited;
	if (mp_undoManager != nullptr)
		mp_undoManager->push(mkUniq<commands::ModifyPhysicsSettingsCommand>(before, edited));
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
			if (mp_undoManager != nullptr) {
				auto cmd = mkUniq<commands::ModifyEnabledRenderersCommand>(m_frameSnapshot, afterYaml);
				mp_undoManager->push(std::move(cmd));
			}
			m_frameSnapshot = afterYaml;
			m_dirty = true;
		}
	}

	ImGui::End();
}

}// namespace owl::nest::panel
