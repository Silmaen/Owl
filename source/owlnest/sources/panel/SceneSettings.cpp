/**
 * @file SceneSettings.cpp
 * @author Silmaen
 * @date 06/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneSettings.h"

#include "../commands/SceneSettingsCommands.h"

#include <imgui.h>

#include <algorithm>
#include <sstream>

namespace owl::nest::panel {

namespace {

constexpr auto g_windowName = "Scene Settings";

/// Serialise the scene's `EnabledRenderersConfig` to a YAML string. An empty
/// config yields an empty string so undo / merge comparisons stay trivial.
auto serializeConfig(const renderer::EnabledRenderersConfig& iConfig) -> std::string {
	if (iConfig.isEmpty())
		return {};
	std::stringstream ss;
	ss << iConfig.toYaml();
	return ss.str();
}

/// Index of the entry with the given name, or `entries.size()` when absent.
auto indexOfEntry(const renderer::EnabledRenderersConfig& iConfig, const std::string& iName) -> size_t {
	for (size_t i = 0; i < iConfig.entries.size(); ++i)
		if (iConfig.entries[i].name == iName)
			return i;
	return iConfig.entries.size();
}

/// Read a `vec4` colour out of a YAML override, or return the supplied default
/// when the key is absent or malformed.
auto readColor(const YAML::Node& iNode, const char* iKey, const math::vec4& iDefault) -> math::vec4 {
	if (!iNode || !iNode.IsMap())
		return iDefault;
	const auto v = iNode[iKey];
	if (!v || !v.IsSequence() || v.size() != 4)
		return iDefault;
	try {
		return {v[0].as<float>(), v[1].as<float>(), v[2].as<float>(), v[3].as<float>()};
	} catch (const YAML::Exception&) {
		return iDefault;
	}
}

/// Write a `vec4` colour into a YAML override.
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

	if (iTypeKey == "Renderer2D") {
		const std::string space =
				ioOverrides["Space"] ? ioOverrides["Space"].as<std::string>("World") : std::string{"World"};
		const char* preview = space.c_str();
		ImGui::SetNextItemWidth(160.f);
		if (ImGui::BeginCombo("Space", preview)) {
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
	} else if (iTypeKey == "RendererRaycast") {
		float fov = ioOverrides["Fov"] ? ioOverrides["Fov"].as<float>(75.f) : 75.f;
		ImGui::SetNextItemWidth(160.f);
		if (ImGui::DragFloat("Fov", &fov, 0.5f, 30.f, 120.f, "%.1f deg")) {
			ioOverrides["Fov"] = fov;
			changed = true;
		}
		float maxDist = ioOverrides["MaxDistance"] ? ioOverrides["MaxDistance"].as<float>(32.f) : 32.f;
		ImGui::SetNextItemWidth(160.f);
		if (ImGui::DragFloat("MaxDistance", &maxDist, 0.5f, 1.f, 256.f, "%.1f cells")) {
			ioOverrides["MaxDistance"] = maxDist;
			changed = true;
		}
		int numRays = ioOverrides["NumRays"] ? ioOverrides["NumRays"].as<int>(0) : 0;
		ImGui::SetNextItemWidth(160.f);
		if (ImGui::DragInt("NumRays", &numRays, 1.f, 0, 4096, "%d (0 = viewport width)")) {
			ioOverrides["NumRays"] = numRays;
			changed = true;
		}
		math::vec4 ceiling = readColor(ioOverrides, "CeilingColor", {0.18f, 0.20f, 0.30f, 1.f});
		std::array<float, 4> ceilingArr{ceiling.r(), ceiling.g(), ceiling.b(), ceiling.a()};
		ImGui::SetNextItemWidth(220.f);
		if (ImGui::ColorEdit4("CeilingColor", ceilingArr.data())) {
			writeColor(ioOverrides, "CeilingColor", {ceilingArr[0], ceilingArr[1], ceilingArr[2], ceilingArr[3]});
			changed = true;
		}
		math::vec4 floor = readColor(ioOverrides, "FloorColor", {0.20f, 0.16f, 0.12f, 1.f});
		std::array<float, 4> floorArr{floor.r(), floor.g(), floor.b(), floor.a()};
		ImGui::SetNextItemWidth(220.f);
		if (ImGui::ColorEdit4("FloorColor", floorArr.data())) {
			writeColor(ioOverrides, "FloorColor", {floorArr[0], floorArr[1], floorArr[2], floorArr[3]});
			changed = true;
		}
	} else {
		ImGui::TextDisabled("(no editable overrides for type '%s')", iTypeKey.c_str());
	}
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
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%s", entry.name.c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("[%s]", projectEntry != nullptr ? projectEntry->typeKey.c_str() : "?");
		ImGui::SameLine();
		bool enabled = entry.enabled;
		if (ImGui::Checkbox("Enabled", &enabled)) {
			entry.enabled = enabled;
			changed = true;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(i == 0);
		if (ImGui::ArrowButton("##up", ImGuiDir_Up))
			moveUp = i;
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(i + 1 == static_cast<int>(sceneCfg.entries.size()));
		if (ImGui::ArrowButton("##down", ImGuiDir_Down))
			moveDown = i;
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::SmallButton("Detach"))
			detachAt = i;
		if (projectEntry != nullptr) {
			ImGui::Indent();
			if (ImGui::CollapsingHeader("Overrides")) {
				if (renderLayerOverridesEditor(projectEntry->typeKey, entry.overrides))
					changed = true;
			}
			ImGui::Unindent();
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

	ImGui::Separator();
	ImGui::TextDisabled("Add scene-level override for:");
	bool added = false;
	for (const auto* entry: missing) {
		ImGui::PushID(entry->name.c_str());
		if (ImGui::SmallButton("+")) {
			renderer::EnabledRenderersConfig::Entry newEntry;
			newEntry.name = entry->name;
			newEntry.enabled = true;
			newEntry.overrides = YAML::Node{YAML::NodeType::Map};
			sceneCfg.entries.push_back(std::move(newEntry));
			added = true;
		}
		ImGui::SameLine();
		ImGui::Text("%s", entry->name.c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("[%s]", entry->typeKey.c_str());
		ImGui::PopID();
	}
	return added;
}

void SceneSettings::onImGuiRender() {
	if (!m_visible)
		return;

	if (!ImGui::Begin(g_windowName, &m_visible)) {
		ImGui::End();
		return;
	}

	if (mp_project == nullptr || !m_scene) {
		ImGui::TextDisabled("Open a scene to edit its settings.");
		ImGui::End();
		return;
	}

	// Refresh the snapshot every frame so external mutations (Ctrl+Z, hot-reload,
	// other panels) don't leave it stale. Within a single frame the snapshot
	// stays the "before" reference for any widget edit committed below.
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

	if (mp_project->rendererStack.isEmpty()) {
		ImGui::TextWrapped("The active project declares no renderer stack — only an "
						   "implicit `Renderer2D` is active. Edit the project settings to "
						   "compose a stack first.");
		ImGui::End();
		return;
	}

	ImGui::Text("Renderer Stack");
	ImGui::TextDisabled("Layers listed here are emitted in this order; project layers "
						"omitted from the list still run after, in project order.");
	ImGui::Spacing();

	bool changed = renderRendererStackSection();
	if (renderAddLayerSection())
		changed = true;

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
