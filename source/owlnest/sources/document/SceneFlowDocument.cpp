/**
 * @file SceneFlowDocument.cpp
 * @author Silmaen
 * @date 24/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneFlowDocument.h"

#include "../Project.h"
#include "EditorLayer.h"

#include <gui/widgets/NodeCanvasSerializer.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <imgui.h>
#include <imgui_internal.h>
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <fstream>
#include <queue>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace owl::nest {

namespace {

// Inter-cell padding only — actual node sizes drive the column/row dimensions.
constexpr float g_hSpacing = 60.0f;
constexpr float g_vSpacing = 40.0f;
constexpr math::vec4 g_normalTitleColor{1.0f, 1.0f, 1.0f, 1.0f};
constexpr math::vec4 g_orphanTitleColor{1.0f, 0.35f, 0.35f, 1.0f};

/// @brief Kind of scene transition for label / styling purposes.
enum struct TransitionKind : uint8_t {
	Teleport,///< `Trigger { Type: Teleport }` — explicit player teleport.
	Death,///< `Trigger { Type: Death }` — death zone reloading the player into another scene.
	Victory,///< `Trigger { Type: Victory }` — victory zone leading to the win scene.
	Lua,///< Heuristically detected `scene.load_scene("...")` call inside an attached Lua script.
};

/// @brief One transition found in a scene (Trigger of type Teleport/Death/Victory, or Lua call).
struct Transition {
	std::string sourceName;///< Source entity name (Tag) or stripped script filename for Lua.
	std::string levelName;///< Destination scene reference as written in the YAML / script.
	std::string targetName;///< Target entity name inside the destination (Teleport only).
	TransitionKind kind{TransitionKind::Teleport};
};

/// @brief Summary of one `.owl` scene file.
struct SceneSummary {
	std::filesystem::path absolutePath;
	std::string relativePath;///< Forward-slash relative path (canonical key + node title input).
	std::vector<Transition> transitions;
};

/// @brief Walk the project directory and collect every `.owl` scene file.
auto listAllScenes(const std::filesystem::path& iProjectDir) -> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> out;
	if (iProjectDir.empty() || !std::filesystem::exists(iProjectDir))
		return out;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(iProjectDir)) {
		if (entry.is_regular_file() && entry.path().extension() == ".owl")
			out.push_back(entry.path());
	}
	std::ranges::sort(out);
	return out;
}

/// @brief Resolve a scriptPath (may be relative to a project asset dir or absolute) to disk.
auto resolveScriptPath(const std::filesystem::path& iProjectDir, const std::string& iScriptPath)
		-> std::filesystem::path {
	std::filesystem::path raw{iScriptPath};
	if (raw.is_absolute() && std::filesystem::exists(raw))
		return raw;
	for (const auto& candidate: {iProjectDir / raw, iProjectDir / "scripts" / raw, iProjectDir / "assets" / raw}) {
		if (std::filesystem::exists(candidate))
			return candidate;
	}
	return {};
}

/// @brief Best-effort regex scan of a `.lua` file for `scene.load_scene("X")` calls.
auto scanLuaForLoadScene(const std::filesystem::path& iLuaPath) -> std::vector<std::string> {
	std::vector<std::string> destinations;
	const std::ifstream in(iLuaPath);
	if (!in)
		return destinations;
	std::stringstream buffer;
	buffer << in.rdbuf();
	const std::string src = buffer.str();
	// Match `scene.load_scene("<arg>")` allowing arbitrary whitespace.
	const std::regex pattern{R"(scene\.load_scene\s*\(\s*["']([^"']+)["'])"};
	for (auto it = std::sregex_iterator{src.begin(), src.end(), pattern}; it != std::sregex_iterator{}; ++it) {
		destinations.push_back((*it)[1].str());
	}
	return destinations;
}

/// @brief Parse a scene YAML and return both Teleport triggers and any Lua-driven destinations.
auto extractTransitions(const std::filesystem::path& iScenePath, const std::filesystem::path& iProjectDir)
		-> std::vector<Transition> {
	std::vector<Transition> out;
	YAML::Node root;
	try {
		root = YAML::LoadFile(iScenePath.string());
	} catch (const YAML::Exception& ex) {
		OWL_CORE_WARN("SceneFlow: failed to parse '{}': {}", iScenePath.string(), ex.what())
		return out;
	}
	const auto entities = root["Entities"];
	if (!entities || !entities.IsSequence())
		return out;

	std::vector<std::filesystem::path> luaScriptsToScan;
	std::unordered_set<std::string> seenLuaPaths;

	for (const auto& ent: entities) {
		// Pull the Tag.tag once per entity for source-name display on both Teleport pins and Lua sources.
		const auto tagNode = ent["Tag"];
		std::string entityName;
		if (tagNode && tagNode["tag"])
			entityName = tagNode["tag"].as<std::string>();

		// Inter-scene triggers — Teleport / Death / Victory all serialize a `LevelName`.
		if (const auto trig = ent["Trigger"]; trig) {
			if (const auto type = trig["Type"]; type) {
				const auto typeStr = type.as<std::string>();
				std::optional<TransitionKind> kind;
				if (typeStr == "Teleport")
					kind = TransitionKind::Teleport;
				else if (typeStr == "Death")
					kind = TransitionKind::Death;
				else if (typeStr == "Victory")
					kind = TransitionKind::Victory;
				if (kind && trig["LevelName"]) {
					Transition tr;
					tr.kind = *kind;
					tr.sourceName = !entityName.empty() ? entityName : std::string{typeStr};
					tr.levelName = trig["LevelName"].as<std::string>();
					tr.targetName = trig["TargetName"] ? trig["TargetName"].as<std::string>() : std::string{};
					out.push_back(std::move(tr));
				}
			}
		}
		// Lua scripts attached to an entity may call scene.load_scene at runtime.
		if (const auto lua = ent["LuaScript"]; lua && lua["scriptPath"]) {
			auto scriptPath = lua["scriptPath"].as<std::string>();
			if (seenLuaPaths.insert(scriptPath).second) {
				if (const auto resolved = resolveScriptPath(iProjectDir, scriptPath); !resolved.empty())
					luaScriptsToScan.push_back(resolved);
			}
		}
	}

	// Walk the (deduplicated) script list once to add Lua-driven outputs.
	for (const auto& scriptPath: luaScriptsToScan) {
		const auto destinations = scanLuaForLoadScene(scriptPath);
		for (const auto& dst: destinations) {
			Transition tr;
			tr.sourceName = scriptPath.stem().string();
			tr.levelName = dst;
			tr.kind = TransitionKind::Lua;
			out.push_back(std::move(tr));
		}
	}
	return out;
}

/// @brief Trim the `.owl` extension from a relative path for display use.
auto stripOwlExtension(std::string_view iPath) -> std::string {
	std::string out{iPath};
	if (out.ends_with(".owl"))
		out.erase(out.size() - 4);
	return out;
}

/// @brief Normalise a user-typed `LevelName` to the canonical relative path used as scene key.
/// Accepts `foo`, `foo.owl`, `scenes/foo`, `scenes/foo.owl` — picks whichever matches a known scene.
auto resolveLevelName(std::string_view iLevelName, const std::unordered_map<std::string, size_t>& iKnownScenes)
		-> std::string {
	if (iLevelName.empty())
		return {};
	std::string candidate{iLevelName};
	std::ranges::replace(candidate, '\\', '/');
	if (iKnownScenes.contains(candidate))
		return candidate;
	if (!candidate.ends_with(".owl")) {
		const auto withExt = candidate + ".owl";
		if (iKnownScenes.contains(withExt))
			return withExt;
	}
	const std::array<std::string_view, 2> prefixes{"scenes/", ""};
	for (const auto& prefix: prefixes) {
		auto withPrefix = std::string{prefix} + candidate;
		if (iKnownScenes.contains(withPrefix))
			return withPrefix;
		if (!withPrefix.ends_with(".owl")) {
			auto withExt = withPrefix + ".owl";
			if (iKnownScenes.contains(withExt))
				return withExt;
		}
	}
	return {};
}

/// @brief Compact pin label. Destination is implicit from the link, so just show the source
///        identifier prefixed by a single-glyph kind indicator.
auto pinLabel(const Transition& iTr) -> std::string {
	const auto& src = iTr.sourceName;
	switch (iTr.kind) {
		case TransitionKind::Lua:
			return std::format("λ {}", src);
		case TransitionKind::Death:
			return std::format("† {}", src);
		case TransitionKind::Victory:
			return std::format("★ {}", src);
		case TransitionKind::Teleport:
			break;
	}
	if (src.empty()) {
		const auto destShort = iTr.levelName.empty() ? std::string{"(self)"} : stripOwlExtension(iTr.levelName);
		return std::format("→ {}", destShort);
	}
	return src;
}

/// @brief Pin typeTag for a given transition kind — drives styling on the Properties panel.
auto pinTypeTag(TransitionKind iKind) -> std::string_view {
	switch (iKind) {
		case TransitionKind::Lua:
			return "scene_lua_exit";
		case TransitionKind::Death:
			return "scene_death_exit";
		case TransitionKind::Victory:
			return "scene_victory_exit";
		case TransitionKind::Teleport:
			break;
	}
	return "scene_exit";
}

/// @brief Per-kind label color used by `NodeCanvas::CustomDraw` for the in-node text.
auto pinLabelColor(TransitionKind iKind) -> math::vec4 {
	switch (iKind) {
		case TransitionKind::Lua:
			return {0.55f, 0.78f, 1.0f, 1.0f};// blue
		case TransitionKind::Death:
			return {1.0f, 0.51f, 0.51f, 1.0f};// red
		case TransitionKind::Victory:
			return {0.71f, 1.0f, 0.63f, 1.0f};// green
		case TransitionKind::Teleport:
			break;
	}
	return {1.0f, 1.0f, 1.0f, 1.0f};
}

/// @brief Write a minimal but well-formed empty scene YAML to disk.
auto writeEmptyScene(const std::filesystem::path& iAbsolutePath) -> bool {
	std::ofstream out(iAbsolutePath);
	if (!out)
		return false;
	const auto stem = iAbsolutePath.stem().string();
	// The serializer accepts an empty Entities list; the scene's name is taken from the file name
	// when loaded.
	out << "Scene: " << stem << '\n';
	out << "Entities: []\n";
	return out.good();
}

}// namespace

SceneFlowDocument::SceneFlowDocument() = default;
SceneFlowDocument::~SceneFlowDocument() = default;

void SceneFlowDocument::onCanvasReady() {
	m_canvas.setOnNodeDoubleClicked([this](core::UUID iNodeId) -> void {
		if (mp_editorLayer == nullptr)
			return;
		if (const auto absPath = absolutePathFor(iNodeId); !absPath.empty() && std::filesystem::exists(absPath))
			mp_editorLayer->openScene(absPath);
	});
	m_canvas.setOnNodeSelected([this](core::UUID iNodeId) -> void { m_selectedNodeId = iNodeId; });
	m_canvas.setOnContextMenu([this](std::optional<core::UUID> iNodeUnderCursor) -> void {
		m_openContextMenu = true;
		m_contextNodeId = iNodeUnderCursor.value_or(core::UUID{0});
	});
}

void SceneFlowDocument::refreshFromProject(const Project& iProject) {
	m_canvas.clear();
	m_selectedNodeId = core::UUID{0};
	if (!iProject.isLoaded())
		return;
	m_projectDirectory = iProject.projectDirectory;
	m_projectName = iProject.name;
	m_projectFirstScene = iProject.firstScene;

	const auto scenePaths = listAllScenes(iProject.projectDirectory);
	if (scenePaths.empty())
		return;

	std::vector<SceneSummary> summaries;
	summaries.reserve(scenePaths.size());
	std::unordered_map<std::string, size_t> indexByRel;

	for (const auto& abs: scenePaths) {
		SceneSummary summary;
		summary.absolutePath = abs;
		const auto rel = std::filesystem::relative(abs, iProject.projectDirectory);
		summary.relativePath = rel.generic_string();
		summary.transitions = extractTransitions(abs, iProject.projectDirectory);
		indexByRel.emplace(summary.relativePath, summaries.size());
		summaries.push_back(std::move(summary));
	}

	std::vector<core::UUID> nodeIdsByIndex(summaries.size());
	std::vector<core::UUID> entryPinByIndex(summaries.size());
	std::vector<std::vector<core::UUID>> outputPinsByIndex(summaries.size());
	std::vector<gui::widgets::Node> stagedNodes(summaries.size());
	std::vector<math::vec2f> nodeSizes(summaries.size());

	// First pass: build every Node fully (title + pins + customData) and measure its actual size.
	for (size_t i = 0; i < summaries.size(); ++i) {
		const auto& summary = summaries[i];
		auto& node = stagedNodes[i];
		node.title = stripOwlExtension(summary.relativePath);
		node.customData = std::format("scenePath: {}\n", summary.relativePath);

		gui::widgets::NodePin entryPin;
		entryPin.label = "entry";
		entryPin.typeTag = "scene_entry";
		entryPin.kind = gui::widgets::PinKind::Input;
		entryPinByIndex[i] = entryPin.id;
		node.inputs.push_back(std::move(entryPin));

		outputPinsByIndex[i].reserve(summary.transitions.size());
		for (const auto& tr: summary.transitions) {
			gui::widgets::NodePin outPin;
			outPin.label = pinLabel(tr);
			outPin.typeTag = std::string{pinTypeTag(tr.kind)};
			outPin.labelColor = pinLabelColor(tr.kind);
			outPin.kind = gui::widgets::PinKind::Output;
			outputPinsByIndex[i].push_back(outPin.id);
			node.outputs.push_back(std::move(outPin));
		}

		nodeSizes[i] = gui::widgets::NodeCanvas::measureNode(node);
	}

	// --- Layered layout (BFS from `firstScene`) -----------------------------
	// Build a directed adjacency list so we can compute reachability layers and assign each scene
	// a column index based on its BFS depth. Cycles (e.g. main_menu ↔ settings_menu ↔ main_menu)
	// are handled by `firstSeen`: each node lands in the layer where it is first visited, so back
	// edges don't push it further right.
	std::vector<std::vector<size_t>> adjacency(summaries.size());
	for (size_t i = 0; i < summaries.size(); ++i) {
		for (const auto& tr: summaries[i].transitions) {
			const auto resolved = resolveLevelName(tr.levelName, indexByRel);
			if (!resolved.empty())
				adjacency[i].push_back(indexByRel[resolved]);
		}
	}

	constexpr size_t kInvalidLayer = std::numeric_limits<size_t>::max();
	std::vector<size_t> layer(summaries.size(), kInvalidLayer);

	const auto firstResolved = resolveLevelName(iProject.firstScene, indexByRel);
	if (!firstResolved.empty()) {
		std::queue<size_t> bfs;
		bfs.push(indexByRel[firstResolved]);
		layer[indexByRel[firstResolved]] = 0;
		while (!bfs.empty()) {
			const auto idx = bfs.front();
			bfs.pop();
			for (const auto next: adjacency[idx]) {
				if (layer[next] == kInvalidLayer) {
					layer[next] = layer[idx] + 1;
					bfs.push(next);
				}
			}
		}
	}
	// Orphans land in a "limbo" layer pushed to the right of the reachable graph.
	size_t maxLayer = 0;
	for (const auto l: layer)
		if (l != kInvalidLayer)
			maxLayer = std::max(maxLayer, l);
	const size_t orphanLayer = layer.empty() ? 0 : maxLayer + 2;
	for (auto& l: layer)
		if (l == kInvalidLayer)
			l = orphanLayer;

	// Group node indices by layer, sort within each layer by relative path for deterministic order.
	const size_t layerCount = orphanLayer + 1;
	std::vector<std::vector<size_t>> nodesPerLayer(layerCount);
	for (size_t i = 0; i < summaries.size(); ++i)
		nodesPerLayer[layer[i]].push_back(i);
	for (auto& bucket: nodesPerLayer) {
		std::ranges::sort(bucket, [&](size_t a, size_t b) -> bool {
			return summaries[a].relativePath < summaries[b].relativePath;
		});
	}

	// Compute per-layer max width and per-row max height — consistent placement regardless of how
	// many nodes a layer holds.
	std::vector<float> layerMaxWidth(layerCount, 0.0f);
	float globalRowHeight = 0.0f;
	for (size_t l = 0; l < layerCount; ++l) {
		for (const auto idx: nodesPerLayer[l]) {
			layerMaxWidth[l] = std::max(layerMaxWidth[l], nodeSizes[idx].x());
			globalRowHeight = std::max(globalRowHeight, nodeSizes[idx].y());
		}
	}
	std::vector<float> layerX(layerCount + 1, 0.0f);
	for (size_t l = 0; l < layerCount; ++l)
		layerX[l + 1] = layerX[l] + layerMaxWidth[l] + g_hSpacing;

	for (size_t l = 0; l < layerCount; ++l) {
		const auto& bucket = nodesPerLayer[l];
		for (size_t r = 0; r < bucket.size(); ++r) {
			const auto idx = bucket[r];
			stagedNodes[idx].position = {layerX[l], static_cast<float>(r) * (globalRowHeight + g_vSpacing)};
			nodeIdsByIndex[idx] = m_canvas.addNode(std::move(stagedNodes[idx]));
		}
	}

	// Resolve link targets — every transition (Trigger or Lua) follows the same name resolution.
	for (size_t i = 0; i < summaries.size(); ++i) {
		const auto& summary = summaries[i];
		for (size_t t = 0; t < summary.transitions.size(); ++t) {
			const auto resolved = resolveLevelName(summary.transitions[t].levelName, indexByRel);
			if (resolved.empty())
				continue;
			const auto destIndex = indexByRel[resolved];
			m_canvas.addLink(outputPinsByIndex[i][t], entryPinByIndex[destIndex]);
		}
	}

	// Reachable / orphan info already produced by the layout BFS above — anything in `orphanLayer`
	// was unreachable from `firstScene`. Reuse it instead of running a second BFS.
	for (size_t i = 0; i < summaries.size(); ++i) {
		if (auto* node = m_canvas.findNode(nodeIdsByIndex[i]))
			node->titleColor = layer[i] == orphanLayer ? g_orphanTitleColor : g_normalTitleColor;
	}

	m_savedSnapshot = gui::widgets::NodeCanvasSerializer::serializeToString(m_canvas);
}

auto SceneFlowDocument::absolutePathFor(core::UUID iNodeId) const -> std::filesystem::path {
	const auto* node = m_canvas.findNode(iNodeId);
	if (node == nullptr || node->customData.empty() || m_projectDirectory.empty())
		return {};
	try {
		const auto data = YAML::Load(node->customData);
		if (!data["scenePath"])
			return {};
		return m_projectDirectory / data["scenePath"].as<std::string>();
	} catch (const YAML::Exception&) {
		OWL_CORE_TRACE("SceneFlow: ignored malformed customData on node {}", static_cast<uint64_t>(iNodeId))
		return {};
	}
}

auto SceneFlowDocument::createSceneFile(const std::filesystem::path& iAbsolutePath) -> bool {
	if (std::filesystem::exists(iAbsolutePath)) {
		OWL_CORE_WARN("SceneFlow: scene already exists at '{}'", iAbsolutePath.string())
		return false;
	}
	std::error_code ec;
	std::filesystem::create_directories(iAbsolutePath.parent_path(), ec);
	if (ec) {
		OWL_CORE_ERROR("SceneFlow: failed to create directories for '{}': {}", iAbsolutePath.string(), ec.message())
		return false;
	}
	if (!writeEmptyScene(iAbsolutePath)) {
		OWL_CORE_ERROR("SceneFlow: failed to write empty scene at '{}'", iAbsolutePath.string())
		return false;
	}
	if (mp_editorLayer != nullptr)
		refreshFromProject(mp_editorLayer->getProject());
	return true;
}

void SceneFlowDocument::deleteSceneFile(const std::filesystem::path& iAbsolutePath) {
	std::error_code ec;
	std::filesystem::remove(iAbsolutePath, ec);
	if (ec) {
		OWL_CORE_ERROR("SceneFlow: failed to delete '{}': {}", iAbsolutePath.string(), ec.message())
		return;
	}
	if (mp_editorLayer != nullptr)
		refreshFromProject(mp_editorLayer->getProject());
}

void SceneFlowDocument::onImGuiRender() {
	NodeGraphDocument::onImGuiRender();
	renderPopups();
}

void SceneFlowDocument::renderHierarchyPanel() {
	// Project header.
	ImGui::TextDisabled("Project");
	ImGui::Text("%s", m_projectName.empty() ? "(unnamed)" : m_projectName.c_str());
	if (!m_projectFirstScene.empty())
		ImGui::TextDisabled("first scene: %s", m_projectFirstScene.c_str());
	ImGui::Separator();
	ImGui::TextDisabled("Scenes (%zu)", m_canvas.nodes().size());

	// Tree of scenes — single-click selects the node in the canvas, double-click opens it.
	for (const auto& node: m_canvas.nodes()) {
		ImGui::PushID(static_cast<int>(static_cast<uint64_t>(node.id) & 0x7fffffffu));
		const bool selected = node.id == m_selectedNodeId;
		const bool isOrphan = node.titleColor.x() > 0.99f && node.titleColor.y() < 0.5f;
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
								   ImGuiTreeNodeFlags_SpanAvailWidth;
		if (selected)
			flags |= ImGuiTreeNodeFlags_Selected;
		if (isOrphan)
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 90, 90, 255));
		ImGui::TreeNodeEx(node.title.c_str(), flags);
		if (isOrphan)
			ImGui::PopStyleColor();
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			m_selectedNodeId = node.id;
			std::array<core::UUID, 1> sel{node.id};
			m_canvas.setSelection(sel);
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
			mp_editorLayer != nullptr) {
			if (const auto path = absolutePathFor(node.id); !path.empty() && std::filesystem::exists(path))
				mp_editorLayer->openScene(path);
		}
		ImGui::PopID();
	}
}

void SceneFlowDocument::renderPropertiesPanel() {
	if (static_cast<uint64_t>(m_selectedNodeId) == 0) {
		ImGui::TextDisabled("No scene selected.");
		ImGui::TextWrapped(
				"Click a scene node in the canvas (or in the Scene Hierarchy panel) to inspect it here.");
		return;
	}
	const auto* node = m_canvas.findNode(m_selectedNodeId);
	if (node == nullptr)
		return;

	ImGui::TextDisabled("Scene");
	ImGui::Text("%s", node->title.c_str());
	if (const auto path = absolutePathFor(m_selectedNodeId); !path.empty()) {
		ImGui::TextDisabled("file");
		ImGui::TextWrapped("%s", path.string().c_str());
	}
	const bool isOrphan = node->titleColor.x() > 0.99f && node->titleColor.y() < 0.5f;
	if (isOrphan)
		ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "unreachable from first scene");

	ImGui::Separator();
	ImGui::TextDisabled("Outgoing transitions (%zu)", node->outputs.size());
	for (const auto& pin: node->outputs) {
		ImU32 color = 0;
		if (pin.typeTag == "scene_lua_exit")
			color = IM_COL32(140, 200, 255, 255);// blue — Lua
		else if (pin.typeTag == "scene_death_exit")
			color = IM_COL32(255, 130, 130, 255);// red — Death
		else if (pin.typeTag == "scene_victory_exit")
			color = IM_COL32(180, 255, 160, 255);// green — Victory
		if (color != 0)
			ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::BulletText("%s", pin.label.c_str());
		if (color != 0)
			ImGui::PopStyleColor();
	}

	ImGui::Separator();
	if (mp_editorLayer != nullptr && ImGui::Button("Open this scene", {-1.0f, 0.0f})) {
		if (const auto path = absolutePathFor(m_selectedNodeId); !path.empty() && std::filesystem::exists(path))
			mp_editorLayer->openScene(path);
	}
}

void SceneFlowDocument::renderPopups() {
	if (m_openContextMenu) {
		ImGui::OpenPopup("##scene_flow_ctx");
		m_openContextMenu = false;
	}

	if (ImGui::BeginPopup("##scene_flow_ctx")) {
		const auto onNode = static_cast<uint64_t>(m_contextNodeId) != 0;
		if (onNode) {
			if (const auto* node = m_canvas.findNode(m_contextNodeId))
				ImGui::TextDisabled("%s", node->title.c_str());
			ImGui::Separator();
			if (ImGui::MenuItem("Edit scene")) {
				if (mp_editorLayer != nullptr) {
					if (const auto path = absolutePathFor(m_contextNodeId);
						!path.empty() && std::filesystem::exists(path))
						mp_editorLayer->openScene(path);
				}
			}
			if (ImGui::MenuItem("Delete scene")) {
				m_pendingDeletePath = absolutePathFor(m_contextNodeId);
				ImGui::CloseCurrentPopup();
			}
		} else {
			ImGui::TextDisabled("Empty area");
			ImGui::Separator();
			if (ImGui::MenuItem("Add new scene...")) {
				m_openAddSceneModal = true;
				m_newSceneNameBuf[0] = '\0';
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

	// Add-scene modal.
	if (m_openAddSceneModal) {
		ImGui::OpenPopup("Add new scene");
		m_openAddSceneModal = false;
	}
	if (ImGui::BeginPopupModal("Add new scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextWrapped("Scene name (relative to project, '.owl' appended automatically).");
		ImGui::SetNextItemWidth(280.0f);
		ImGui::InputText("##sceneName", m_newSceneNameBuf, sizeof(m_newSceneNameBuf));
		ImGui::Separator();
		const bool hasName = m_newSceneNameBuf[0] != '\0';
		if (!hasName)
			ImGui::BeginDisabled();
		if (ImGui::Button("Create", {110.0f, 0.0f})) {
			std::string name{m_newSceneNameBuf};
			if (!name.ends_with(".owl"))
				name += ".owl";
			// Default to a `scenes/` subfolder if the name does not already include a slash.
			const std::filesystem::path rel = name.find('/') == std::string::npos
													  ? std::filesystem::path{"scenes"} / name
													  : std::filesystem::path{name};
			const auto target = m_projectDirectory / rel;
			if (createSceneFile(target)) {
				if (mp_editorLayer != nullptr)
					mp_editorLayer->openScene(target);
				ImGui::CloseCurrentPopup();
			}
		}
		if (!hasName)
			ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", {110.0f, 0.0f}))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	// Delete-scene confirmation modal.
	if (!m_pendingDeletePath.empty()) {
		ImGui::OpenPopup("Delete scene?");
	}
	if (ImGui::BeginPopupModal("Delete scene?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextWrapped("Permanently delete:");
		ImGui::Text("%s", m_pendingDeletePath.string().c_str());
		ImGui::Separator();
		if (ImGui::Button("Delete", {110.0f, 0.0f})) {
			deleteSceneFile(m_pendingDeletePath);
			m_pendingDeletePath.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", {110.0f, 0.0f})) {
			m_pendingDeletePath.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

}// namespace owl::nest
