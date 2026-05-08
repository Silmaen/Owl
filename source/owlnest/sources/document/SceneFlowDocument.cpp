/**
 * @file SceneFlowDocument.cpp
 * @author Silmaen
 * @date 24/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneFlowDocument.h"

#include "../Project.h"
#include "../commands/ComponentCommands.h"
#include "../commands/EntityCommands.h"
#include "../commands/SceneFlowCommands.h"
#include "../document/SceneDocument.h"
#include "EditorLayer.h"

#include <gui/widgets/NodeCanvasSerializer.h>
#include <scene/Entity.h>
#include <scene/component/Transform.h>
#include <scene/component/Trigger.h>

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
// Inter-cell padding only — actual node sizes drive the column/row dimensions. Generous
// horizontal spacing gives the curved links room to bend around without clipping the next column.
constexpr float g_hSpacing = 140.0f;
// Vertical breathing room between rows. Bumped up so curved cross-layer links don't visually fuse
// with adjacent nodes at typical zoom levels.
constexpr float g_vSpacing = 70.0f;
constexpr math::vec4 g_normalTitleColor{1.0f, 1.0f, 1.0f, 1.0f};
constexpr math::vec4 g_orphanTitleColor{1.0f, 0.35f, 0.35f, 1.0f};

/**
 * @brief
 *  Kind of scene transition for label / styling purposes.
 */
enum struct TransitionKind : uint8_t {
	Teleport,///< `Trigger { Type: Teleport }` — explicit player teleport.
	Death,///< `Trigger { Type: Death }` — death zone reloading the player into another scene.
	Victory,///< `Trigger { Type: Victory }` — victory zone leading to the win scene.
	Lua,///< Heuristically detected `scene.load_scene("...")` call inside an attached Lua script.
};

/**
 * @brief
 *  One transition found in a scene (Trigger of type Teleport/Death/Victory, or Lua call).
 */
struct Transition {
	std::string sourceName;///< Source entity name (Tag) or stripped script filename for Lua.
	std::string levelName;///< Destination scene reference as written in the YAML / script.
	std::string targetName;///< Target entity name inside the destination (Teleport only).
	core::UUID triggerEntityUuid{0};///< UUID of the Trigger-bearing entity (zero for Lua transitions).
	TransitionKind kind{TransitionKind::Teleport};
};

/**
 * @brief
 *  Pin label for the ghost "+ Add teleport" affordance shown on every scene node.
 */
constexpr const char* g_ghostPinLabel = "+ Add teleport";
/**
 * @brief
 *  Pin typeTag identifying the ghost pin (used by the validator).
 */
constexpr const char* g_ghostPinTypeTag = "scene_new_teleport";
/**
 * @brief
 *  Pin typeTag for entry pins (input on every scene node).
 */
constexpr const char* g_entryPinTypeTag = "scene_entry";
/**
 * @brief
 *  Pin typeTag for ordinary Teleport output pins.
 */
constexpr const char* g_teleportPinTypeTag = "scene_exit";

/**
 * @brief
 *  Summary of one `.owl` scene file.
 */
struct SceneSummary {
	std::filesystem::path absolutePath;
	std::string relativePath;///< Forward-slash relative path (canonical key + node title input).
	std::vector<Transition> transitions;
};

/**
 * @brief
 *  Walk the project directory and collect every `.owl` scene file.
 */
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

/**
 * @brief
 *  Resolve a scriptPath (may be relative to a project asset dir or absolute) to disk.
 */
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

/**
 * @brief
 *  Best-effort regex scan of a `.lua` file for scene transitions.
 *
 * Two patterns are matched:
 *  1. Direct literal — `scene.load_scene("scenes/X.owl")`.
 *  2. Indirect via variable — when a file uses `scene.load_scene` anywhere AND contains a
 *     `*.owl` string literal (typical pattern: `pending_scene = "scenes/X.owl"` followed by
 *     `scene.load_scene(pending_scene)` in `on_update`). The literal is treated as a possible
 *     destination. False positives in comments / non-transition code are rare and harmless on
 *     the Scene Flow visualization.
 */
auto scanLuaForLoadScene(const std::filesystem::path& iLuaPath) -> std::vector<std::string> {
	std::vector<std::string> destinations;
	const std::ifstream in(iLuaPath);
	if (!in)
		return destinations;
	std::stringstream buffer;
	buffer << in.rdbuf();
	const std::string src = buffer.str();

	// 1) Direct: `scene.load_scene("...")`.
	const std::regex direct{R"(scene\.load_scene\s*\(\s*["']([^"']+)["'])"};
	for (auto it = std::sregex_iterator{src.begin(), src.end(), direct}; it != std::sregex_iterator{}; ++it) {
		destinations.push_back((*it)[1].str());
	}
	// 2) Indirect: any `*.owl` literal in a file that ever calls `scene.load_scene`.
	if (src.find("scene.load_scene") != std::string::npos) {
		const std::regex owlLit{R"(["']([^"'\s]+\.owl)["'])"};
		for (auto it = std::sregex_iterator{src.begin(), src.end(), owlLit}; it != std::sregex_iterator{}; ++it) {
			destinations.push_back((*it)[1].str());
		}
	}
	// De-duplicate while preserving the first appearance order so determinism is unchanged.
	std::unordered_set<std::string> seen;

	std::erase_if(destinations, [&](const std::string& iEntry) -> bool { return !seen.insert(iEntry).second; });
	return destinations;
}

/**
 * @brief
 *  Parse a scene YAML and return both Teleport triggers and any Lua-driven destinations.
 */
auto extractTransitions(const std::filesystem::path& iScenePath, const std::filesystem::path& iProjectDir)
		-> std::vector<Transition> {
	std::vector<Transition> out;
	YAML::Node root;
	try {
		root = YAML::LoadFile(iScenePath.string());
	} catch (const YAML::Exception& ex) {
		OWL_CORE_WARN("SceneFlow: failed to parse '{}': {}.", iScenePath.string(), ex.what())
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

		// Entity UUID is serialized as the `Entity:` key value at the entity dict head.
		core::UUID entityUuid{0};
		if (const auto idNode = ent["Entity"]; idNode)
			entityUuid = core::UUID{idNode.as<uint64_t>()};

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
					tr.triggerEntityUuid = entityUuid;
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

/**
 * @brief
 *  Trim the `.owl` extension from a relative path for display use.
 */
auto stripOwlExtension(std::string_view iPath) -> std::string {
	std::string out{iPath};
	if (out.ends_with(".owl"))
		out.erase(out.size() - 4);
	return out;
}

/**
 * @brief
 *  Normalise a user-typed `LevelName` to the canonical relative path used as scene key.
 * Accepts `foo`, `foo.owl`, `scenes/foo`, `scenes/foo.owl` — picks whichever matches a known scene.
 */
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

/**
 * @brief
 *  Compact pin label. Destination is implicit from the link, so just show the source
 *        identifier; Death and Victory rely on the label colour for the kind hint, while Lua
 *        keeps a visible `λ` prefix because it is the only kind that maps to a script-driven
 *        transition (worth flagging at a glance even when colours are off).
 */
auto pinLabel(const Transition& iTr) -> std::string {
	const auto& src = iTr.sourceName;
	switch (iTr.kind) {
		case TransitionKind::Lua:
			return std::format("λ {}", src);
		case TransitionKind::Death:
		case TransitionKind::Victory:
			return src;
		case TransitionKind::Teleport:
			break;
	}
	if (src.empty()) {
		const auto destShort = iTr.levelName.empty() ? std::string{"(self)"} : stripOwlExtension(iTr.levelName);
		return std::format("→ {}", destShort);
	}
	return src;
}

/**
 * @brief
 *  Pin typeTag for a given transition kind — drives styling on the Properties panel.
 */
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

/**
 * @brief
 *  Per-kind label colour used by `NodeCanvas::CustomDraw` for the in-node text.
 */
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

/**
 * @brief
 *  Result of the layered Sugiyama-ish layout computation.
 */
struct LayeringResult {
	std::vector<size_t> layer;///< layer[i] = column index of summaries[i]
	std::vector<std::vector<size_t>> nodesPerLayer;///< sorted (stable) lists of node indices per column
	size_t orphanLayer{0};///< column reserved for unreachable scenes
	size_t layerCount{0};///< total column count (orphan layer + 1)
};

/**
 * @brief
 *  Compute per-node column index via BFS from the first scene + DFS back-edge detection +
 *        monotone promotion, then group nodes per column with a deterministic alphabetic sort.
 */
[[nodiscard]] auto computeLayering(const std::vector<SceneSummary>& iSummaries,
								   const std::vector<std::vector<size_t>>& iAdjacency,
								   const std::string& iFirstResolved,
								   const std::unordered_map<std::string, size_t>& iIndexByRel) -> LayeringResult {
	constexpr size_t kInvalidLayer = std::numeric_limits<size_t>::max();
	LayeringResult result;
	result.layer.assign(iSummaries.size(), kInvalidLayer);
	if (!iFirstResolved.empty()) {
		std::queue<size_t> bfs;
		const auto rootIdx = iIndexByRel.at(iFirstResolved);
		bfs.push(rootIdx);
		result.layer[rootIdx] = 0;
		while (!bfs.empty()) {
			const auto idx = bfs.front();
			bfs.pop();
			for (const auto next: iAdjacency[idx]) {
				if (result.layer[next] == kInvalidLayer) {
					result.layer[next] = result.layer[idx] + 1;
					bfs.push(next);
				}
			}
		}
	}
	size_t maxLayer = 0;
	for (const auto l: result.layer)
		if (l != kInvalidLayer)
			maxLayer = std::max(maxLayer, l);
	const size_t initialOrphanLayer = result.layer.empty() ? 0 : maxLayer + 2;
	for (auto& l: result.layer)
		if (l == kInvalidLayer)
			l = initialOrphanLayer;
	// DFS back-edge detection — start from the first scene so loops back to it are correctly
	// classified as back edges (and don't push it rightward in the promotion step).
	std::vector<uint8_t> dfsState(iSummaries.size(), 0);
	std::unordered_set<uint64_t> backEdges;
	const auto encode = [](size_t iSrc, size_t iDst) -> uint64_t {
		return (static_cast<uint64_t>(iSrc) << 32) | static_cast<uint64_t>(iDst);
	};
	const auto runDfs = [&](size_t iStart) -> void {
		std::vector<std::pair<size_t, size_t>> stack;
		stack.emplace_back(iStart, 0);
		dfsState[iStart] = 1;
		while (!stack.empty()) {
			auto& [node, childIdx] = stack.back();
			if (childIdx >= iAdjacency[node].size()) {
				dfsState[node] = 2;
				stack.pop_back();
				continue;
			}
			const auto next = iAdjacency[node][childIdx++];
			if (dfsState[next] == 1)
				backEdges.insert(encode(node, next));
			else if (dfsState[next] == 0) {
				dfsState[next] = 1;
				stack.emplace_back(next, 0);
			}
		}
	};
	if (!iFirstResolved.empty() && dfsState[iIndexByRel.at(iFirstResolved)] == 0)

		runDfs(iIndexByRel.at(iFirstResolved));
	for (size_t i = 0; i < iSummaries.size(); ++i)
		if (dfsState[i] == 0)

			runDfs(i);

	// Monotone promotion — every forward edge ends at a strictly greater layer than its source.
	bool changed = true;
	for (size_t guard = 0; changed && guard < iSummaries.size() + 1; ++guard) {
		changed = false;
		for (size_t src = 0; src < iSummaries.size(); ++src) {
			for (const auto dst: iAdjacency[src]) {
				if (backEdges.contains(encode(src, dst)))
					continue;
				if (result.layer[dst] <= result.layer[src]) {
					result.layer[dst] = result.layer[src] + 1;
					changed = true;
				}
			}
		}
	}
	// Re-derive orphan column AFTER promotion so it always sits beyond the reachable graph.
	maxLayer = 0;
	for (size_t i = 0; i < iSummaries.size(); ++i)
		if (result.layer[i] != initialOrphanLayer)
			maxLayer = std::max(maxLayer, result.layer[i]);
	result.orphanLayer = result.layer.empty() ? 0 : maxLayer + 2;
	for (auto& l: result.layer)
		if (l == initialOrphanLayer)
			l = result.orphanLayer;
	result.layerCount = result.orphanLayer + 1;
	result.nodesPerLayer.resize(result.layerCount);
	for (size_t i = 0; i < iSummaries.size(); ++i) result.nodesPerLayer[result.layer[i]].push_back(i);
	for (auto& bucket: result.nodesPerLayer) {
		std::ranges::sort(bucket, [&](size_t a, size_t b) -> bool {
			return iSummaries[a].relativePath < iSummaries[b].relativePath;
		});
	}
	return result;
}

/**
 * @brief
 *  Apply alternating forward/backward barycentre sweeps to reduce edge crossings.
 */
void applyBarycentreOrdering(const std::vector<SceneSummary>& iSummaries, const std::vector<size_t>& iLayer,
							 size_t iLayerCount, const std::vector<std::vector<size_t>>& iAdjacency,
							 std::vector<std::vector<size_t>>& ioNodesPerLayer) {
	std::vector<std::vector<size_t>> predecessors(iSummaries.size());
	for (size_t src = 0; src < iSummaries.size(); ++src)
		for (const auto dst: iAdjacency[src]) predecessors[dst].push_back(src);

	const auto rowOf = [&](size_t iNodeIndex, const std::vector<size_t>& iLayerOrder) -> float {
		for (size_t r = 0; r < iLayerOrder.size(); ++r)
			if (iLayerOrder[r] == iNodeIndex)
				return static_cast<float>(r);
		return 0.0f;
	};
	const auto barycentre = [&](size_t iNodeIndex, const std::vector<size_t>& iNeighbours,
								size_t iAdjacentLayer) -> float {
		float sum = 0.0f;
		size_t count = 0;
		for (const auto n: iNeighbours) {
			if (iLayer[n] == iAdjacentLayer) {
				sum += rowOf(n, ioNodesPerLayer[iAdjacentLayer]);
				++count;
			}
		}
		return count == 0 ? rowOf(iNodeIndex, ioNodesPerLayer[iLayer[iNodeIndex]]) : sum / static_cast<float>(count);
	};
	constexpr int kBarycenterSweeps = 3;
	for (int sweep = 0; sweep < kBarycenterSweeps; ++sweep) {
		for (size_t l = 1; l < iLayerCount; ++l) {
			std::vector<float> bary(iSummaries.size(), 0.0f);
			for (const auto idx: ioNodesPerLayer[l]) bary[idx] = barycentre(idx, predecessors[idx], l - 1);
			std::ranges::stable_sort(ioNodesPerLayer[l], [&](size_t a, size_t b) -> bool { return bary[a] < bary[b]; });
		}
		for (size_t l = iLayerCount - 1; l-- > 0;) {
			std::vector<float> bary(iSummaries.size(), 0.0f);
			for (const auto idx: ioNodesPerLayer[l]) bary[idx] = barycentre(idx, iAdjacency[idx], l + 1);
			std::ranges::stable_sort(ioNodesPerLayer[l], [&](size_t a, size_t b) -> bool { return bary[a] < bary[b]; });
		}
	}
}

/**
 * @brief
 *  Write a minimal but well-formed empty scene YAML to disk.
 */
auto writeEmptyScene(const std::filesystem::path& iAbsolutePath) -> bool {
	std::ofstream out(iAbsolutePath);
	if (!out) {
		OWL_CORE_ERROR("SceneFlow: cannot open '{}' for writing.", iAbsolutePath.string())
		return false;
	}
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
	m_canvas.setLinkValidator(
			[this](core::UUID iFromPin, core::UUID iToPin) -> bool { return validateLinkDraft(iFromPin, iToPin); });
	m_canvas.setOnLinkCreated([this](core::UUID iLinkId, core::UUID iFromPin, core::UUID iToPin) -> void {
		onLinkDrafted(iLinkId, iFromPin, iToPin);
	});
	m_canvas.setOnLinkDeleted([this](core::UUID iLinkId) -> void { onLinkErased(iLinkId); });
}

auto SceneFlowDocument::validateLinkDraft(core::UUID iFromPin, core::UUID iToPin) const -> bool {
	const auto* fromNode = m_canvas.findNodeByPin(iFromPin);
	const auto* toNode = m_canvas.findNodeByPin(iToPin);
	if (fromNode == nullptr || toNode == nullptr || fromNode == toNode)
		return false;
	// Source pin: must be the ghost pin (only it materializes new triggers; existing Teleport pins
	// are read-only in this iteration — re-route is out of scope).
	bool sourceIsGhost = false;
	for (const auto& pin: fromNode->outputs) {
		if (pin.id == iFromPin) {
			sourceIsGhost = pin.typeTag == g_ghostPinTypeTag;
			break;
		}
	}
	if (!sourceIsGhost)
		return false;
	// Destination pin: must be the entry pin of a scene node.
	for (const auto& pin: toNode->inputs) {
		if (pin.id == iToPin)
			return pin.typeTag == g_entryPinTypeTag;
	}
	return false;
}

auto SceneFlowDocument::relativePathFor(core::UUID iNodeId) const -> std::string {
	const auto abs = absolutePathFor(iNodeId);
	if (abs.empty() || m_projectDirectory.empty())
		return {};
	std::error_code ec;
	const auto rel = std::filesystem::relative(abs, m_projectDirectory, ec);
	if (ec)
		return {};
	return rel.generic_string();
}

void SceneFlowDocument::onLinkDrafted(core::UUID iLinkId, core::UUID iFromPin, core::UUID iToPin) {
	// Cancel the auto-link the canvas just created — the composite undo step will rebuild it from
	// scratch, with a real (non-ghost) output pin and a fresh link id we can track.
	m_canvas.removeLink(iLinkId);

	if (mp_editorLayer == nullptr)
		return;
	const auto* fromNode = m_canvas.findNodeByPin(iFromPin);
	const auto* toNode = m_canvas.findNodeByPin(iToPin);
	if (fromNode == nullptr || toNode == nullptr)
		return;

	const auto srcAbs = absolutePathFor(fromNode->id);
	const auto destRel = relativePathFor(toNode->id);
	if (srcAbs.empty() || destRel.empty())
		return;

	auto* doc = mp_editorLayer->loadOrOpenSceneDocument(srcAbs);
	if (doc == nullptr)
		return;
	const auto& scene = doc->getActiveScene();
	if (!scene)
		return;

	// Materialize the Trigger entity in the source scene at world origin.
	const auto destStem = std::filesystem::path{destRel}.stem().string();
	auto entity = scene->createEntity(std::format("Teleport_to_{}", destStem));
	entity.addComponent<scene::component::Transform>();// default-constructed at (0,0,0).
	auto& trig = entity.addComponent<scene::component::Trigger>();
	trig.trigger.type = scene::SceneTrigger::TriggerType::Teleport;
	trig.trigger.levelName = destRel;
	trig.trigger.targetName = "";

	// Build the canvas-side: a real output pin replacing the ghost-drag plus the link to the dest.
	gui::widgets::NodePin newPin;
	newPin.label = std::format("→ {}", destStem);
	newPin.typeTag = g_teleportPinTypeTag;
	newPin.kind = gui::widgets::PinKind::Output;
	newPin.labelColor = {1.f, 1.f, 1.f, 1.f};
	const auto realPinId = m_canvas.addOutputPin(fromNode->id, newPin);
	newPin.id = realPinId;// `addOutputPin` regenerates if zero — keep the snapshot in sync.
	const auto realLinkId = m_canvas.addLink(realPinId, iToPin);

	// Sceneside: capture a CreateEntityCommand from the live entity so undo can destroy + redo can restore.
	auto sceneCmd = mkUniq<commands::CreateEntityCommand>(entity);
	auto canvasCmd = mkUniq<commands::AddPinAndLinkCommand>(fromNode->id, newPin, iToPin);

	// Track the new link/pin for delete + targetName-edit lookups.
	const LinkOrigin origin{.scenePath = srcAbs,
							.triggerEntityUuid = entity.getUUID(),
							.sourceNodeId = fromNode->id,
							.outputPinId = realPinId,
							.inputPinId = iToPin};
	m_linkOrigins[realLinkId] = origin;
	m_pinToOrigin[realPinId] = origin;

	// Push (not execute) — the canvas + scene state already reflects the post-redo state.
	m_undoManager.push(mkUniq<commands::SceneFlowCompositeCommand>(std::move(sceneCmd), std::move(canvasCmd), srcAbs,
																   mp_editorLayer, "Add Teleport Link"));
}

void SceneFlowDocument::onLinkErased(core::UUID iLinkId) {
	const auto it = m_linkOrigins.find(iLinkId);
	if (it == m_linkOrigins.end())
		return;// Not a Teleport link we manage (Lua/Death/Victory or pre-existing edge case).
	const auto origin = it->second;
	m_linkOrigins.erase(it);
	m_pinToOrigin.erase(origin.outputPinId);

	if (mp_editorLayer == nullptr)
		return;
	auto* doc = mp_editorLayer->loadOrOpenSceneDocument(origin.scenePath);
	if (doc == nullptr)
		return;
	const auto& scene = doc->getActiveScene();
	if (!scene)
		return;
	auto entity = scene->findEntityByUUID(origin.triggerEntityUuid);
	if (!entity)
		return;

	// Snapshot the current pin (label/colour stay accurate even if it was edited) for undo.
	gui::widgets::NodePin pinSnapshot;
	if (const auto* node = m_canvas.findNode(origin.sourceNodeId)) {
		for (const auto& pin: node->outputs) {
			if (pin.id == origin.outputPinId) {
				pinSnapshot = pin;
				break;
			}
		}
	}
	if (static_cast<uint64_t>(pinSnapshot.id) == 0)
		return;
	const gui::widgets::Link linkSnapshot{.id = iLinkId, .fromPin = origin.outputPinId, .toPin = origin.inputPinId};

	// Capture commands BEFORE mutating — DeleteEntityCommand snapshots in its ctor, so the entity
	// must still exist; the canvas pin is similarly captured by snapshot.
	auto sceneCmd = mkUniq<commands::DeleteEntityCommand>(entity);
	auto canvasCmd = mkUniq<commands::RemovePinAndLinkCommand>(origin.sourceNodeId, pinSnapshot, linkSnapshot);

	// Now apply the post-state: drop the pin (canvas already removed the link automatically) and
	// destroy the entity.
	m_canvas.removeOutputPin(origin.sourceNodeId, origin.outputPinId);
	scene->destroyEntity(entity);

	m_undoManager.push(mkUniq<commands::SceneFlowCompositeCommand>(
			std::move(sceneCmd), std::move(canvasCmd), origin.scenePath, mp_editorLayer, "Remove Teleport Link"));
}

void SceneFlowDocument::refreshFromProject(const Project& iProject) {
	m_canvas.clear();
	m_linkOrigins.clear();
	m_pinToOrigin.clear();
	m_nodeToGhostPin.clear();
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
	std::vector<core::UUID> ghostPinByIndex(summaries.size());
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
		// Ghost "+ Add teleport" pin — drag-from-here to create a new Trigger entity. Always last,
		// so existing teleports stay visually grouped and the drag affordance lives at the bottom.
		gui::widgets::NodePin ghost;
		ghost.label = g_ghostPinLabel;
		ghost.typeTag = g_ghostPinTypeTag;
		ghost.kind = gui::widgets::PinKind::Output;
		ghost.labelColor = {0.6f, 0.6f, 0.6f, 1.0f};
		ghostPinByIndex[i] = ghost.id;
		node.outputs.push_back(std::move(ghost));
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
	const auto firstResolved = resolveLevelName(iProject.firstScene, indexByRel);
	auto layering = computeLayering(summaries, adjacency, firstResolved, indexByRel);
	const auto& layer = layering.layer;
	const size_t layerCount = layering.layerCount;
	const size_t orphanLayer = layering.orphanLayer;
	auto& nodesPerLayer = layering.nodesPerLayer;

	applyBarycentreOrdering(summaries, layer, layerCount, adjacency, nodesPerLayer);

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
	for (size_t l = 0; l < layerCount; ++l) layerX[l + 1] = layerX[l] + layerMaxWidth[l] + g_hSpacing;
	for (size_t l = 0; l < layerCount; ++l) {
		const auto& bucket = nodesPerLayer[l];
		const float pitch = globalRowHeight + g_vSpacing;
		const float layerHeight = static_cast<float>(bucket.size()) * pitch;
		// Vertically centre each column around y=0 so single-node layers stop hugging the top edge
		// — short and tall layers visually align around the same horizontal mid-line.
		// Plus alternate columns get nudged by half a row so two adjacent layers never share the
		// exact same Y for any of their nodes — this keeps cross-layer curves visibly separated
		// from neighbour nodes instead of fusing with them.
		const float halfRowStagger = (l % 2 == 0) ? 0.0f : pitch * 0.5f;
		const float yOffset = -layerHeight * 0.5f + halfRowStagger;
		for (size_t r = 0; r < bucket.size(); ++r) {
			const auto idx = bucket[r];
			stagedNodes[idx].position = {layerX[l], yOffset + static_cast<float>(r) * pitch};
			nodeIdsByIndex[idx] = m_canvas.addNode(std::move(stagedNodes[idx]));
		}
	}
	// Map each node to its ghost pin so the create flow can ignore drag-from-existing-pin events.
	for (size_t i = 0; i < summaries.size(); ++i) {
		if (static_cast<uint64_t>(ghostPinByIndex[i]) != 0)
			m_nodeToGhostPin[nodeIdsByIndex[i]] = ghostPinByIndex[i];
	}
	// Resolve link targets — every transition (Trigger or Lua) follows the same name resolution.
	for (size_t i = 0; i < summaries.size(); ++i) {
		const auto& summary = summaries[i];
		for (size_t t = 0; t < summary.transitions.size(); ++t) {
			const auto& tr = summary.transitions[t];
			const auto resolved = resolveLevelName(tr.levelName, indexByRel);
			if (resolved.empty())
				continue;
			const auto destIndex = indexByRel[resolved];
			const auto fromPin = outputPinsByIndex[i][t];
			const auto toPin = entryPinByIndex[destIndex];
			const auto linkId = m_canvas.addLink(fromPin, toPin);
			// Bookkeep only Teleport links — Death/Victory/Lua are read-only in the canvas (no
			// undo-able edit yet) and we don't want to materialize a Trigger entity on accidental
			// Delete on those.
			if (tr.kind == TransitionKind::Teleport && static_cast<uint64_t>(tr.triggerEntityUuid) != 0 &&
				static_cast<uint64_t>(linkId) != 0) {
				const LinkOrigin origin{.scenePath = summary.absolutePath,
										.triggerEntityUuid = tr.triggerEntityUuid,
										.sourceNodeId = nodeIdsByIndex[i],
										.outputPinId = fromPin,
										.inputPinId = toPin};
				m_linkOrigins[linkId] = origin;
				m_pinToOrigin[fromPin] = origin;
			}
		}
	}
	// Reachable / orphan info already produced by the layout BFS above — anything in `orphanLayer`
	// was unreachable from `firstScene`. Reuse it instead of running a second BFS.
	for (size_t i = 0; i < summaries.size(); ++i) {
		if (auto* node = m_canvas.findNode(nodeIdsByIndex[i]))
			node->titleColor = layer[i] == orphanLayer ? g_orphanTitleColor : g_normalTitleColor;
	}
	m_savedSnapshot = gui::widgets::NodeCanvasSerializer::serializeToString(m_canvas);
	// Auto-fit on every project rescan so the user always sees the full graph centred when the
	// Scene Flow tab opens (or after any add/delete/rename of a scene). GraphEditor consumes the
	// request once and resets it to `Fit_None` — subsequent frames keep the user's pan/zoom.
	m_canvas.requestFitToContent();
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
		OWL_CORE_TRACE("SceneFlow: ignored malformed customData on node {}.", static_cast<uint64_t>(iNodeId))
		return {};
	}
}

auto SceneFlowDocument::createSceneFile(const std::filesystem::path& iAbsolutePath) -> bool {
	if (std::filesystem::exists(iAbsolutePath)) {
		OWL_CORE_WARN("SceneFlow: scene already exists at '{}'.", iAbsolutePath.string())
		return false;
	}
	std::error_code ec;
	std::filesystem::create_directories(iAbsolutePath.parent_path(), ec);
	if (ec) {
		OWL_CORE_ERROR("SceneFlow: failed to create directories for '{}': {}.", iAbsolutePath.string(), ec.message())
		return false;
	}
	if (!writeEmptyScene(iAbsolutePath)) {
		OWL_CORE_ERROR("SceneFlow: failed to write empty scene at '{}'.", iAbsolutePath.string())
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
		OWL_CORE_ERROR("SceneFlow: failed to delete '{}': {}.", iAbsolutePath.string(), ec.message())
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
		ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
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
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && mp_editorLayer != nullptr) {
			if (const auto path = absolutePathFor(node.id); !path.empty() && std::filesystem::exists(path))
				mp_editorLayer->openScene(path);
		}
		ImGui::PopID();
	}
}

void SceneFlowDocument::renderPropertiesPanel() {
	if (static_cast<uint64_t>(m_selectedNodeId) == 0) {
		ImGui::TextDisabled("No scene selected.");
		ImGui::TextWrapped("Click a scene node in the canvas (or in the Scene Hierarchy panel) to inspect it here.");
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
	renderContextMenu();
	renderAddSceneModal();
	renderDeleteSceneModal();
	renderTargetNameModal();
}

void SceneFlowDocument::renderContextMenu() {
	if (m_openContextMenu) {
		ImGui::OpenPopup("##scene_flow_ctx");
		m_openContextMenu = false;
	}
	if (!ImGui::BeginPopup("##scene_flow_ctx"))
		return;
	const auto onNode = static_cast<uint64_t>(m_contextNodeId) != 0;
	if (!onNode) {
		ImGui::TextDisabled("Empty area");
		ImGui::Separator();
		if (ImGui::MenuItem("Add new scene...")) {
			m_openAddSceneModal = true;
			m_newSceneNameBuf[0] = '\0';
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
		return;
	}
	const auto* node = m_canvas.findNode(m_contextNodeId);
	if (node != nullptr)
		ImGui::TextDisabled("%s", node->title.c_str());
	ImGui::Separator();
	if (ImGui::MenuItem("Edit scene") && mp_editorLayer != nullptr) {
		if (const auto path = absolutePathFor(m_contextNodeId); !path.empty() && std::filesystem::exists(path))
			mp_editorLayer->openScene(path);
	}
	// Teleport-pin sub-menu — list every Teleport output pin so the user can edit its
	// `targetName` from the canvas without opening the source scene's hierarchy.
	if (node != nullptr) {
		const bool anyTeleport = std::ranges::any_of(
				node->outputs, [](const auto& iPin) -> bool { return iPin.typeTag == g_teleportPinTypeTag; });
		if (anyTeleport && ImGui::BeginMenu("Edit teleport target")) {
			for (const auto& pin: node->outputs) {
				if (pin.typeTag == g_teleportPinTypeTag && ImGui::MenuItem(pin.label.c_str()))
					openTargetNameModal(pin.id);
			}
			ImGui::EndMenu();
		}
	}
	if (ImGui::MenuItem("Delete scene")) {
		m_pendingDeletePath = absolutePathFor(m_contextNodeId);
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void SceneFlowDocument::renderAddSceneModal() {
	if (m_openAddSceneModal) {
		ImGui::OpenPopup("Add new scene");
		m_openAddSceneModal = false;
	}
	if (!ImGui::BeginPopupModal("Add new scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;
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
		const std::filesystem::path rel = name.find('/') == std::string::npos ? std::filesystem::path{"scenes"} / name
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

void SceneFlowDocument::renderDeleteSceneModal() {
	if (!m_pendingDeletePath.empty())
		ImGui::OpenPopup("Delete scene?");
	if (!ImGui::BeginPopupModal("Delete scene?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;
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

void SceneFlowDocument::openTargetNameModal(core::UUID iPinId) {
	const auto it = m_pinToOrigin.find(iPinId);
	if (it == m_pinToOrigin.end() || mp_editorLayer == nullptr)
		return;
	auto* doc = mp_editorLayer->loadOrOpenSceneDocument(it->second.scenePath);
	if (doc == nullptr)
		return;
	const auto& scene = doc->getActiveScene();
	if (!scene)
		return;
	auto entity = scene->findEntityByUUID(it->second.triggerEntityUuid);
	if (!entity || !entity.hasComponent<scene::component::Trigger>())
		return;
	const auto& trig = entity.getComponent<scene::component::Trigger>();
	const auto& current = trig.trigger.targetName;
	std::snprintf(m_targetNameBuf, sizeof(m_targetNameBuf), "%s", current.c_str());
	m_targetNameModalPin = iPinId;
}

void SceneFlowDocument::renderTargetNameModal() {
	if (static_cast<uint64_t>(m_targetNameModalPin) == 0)
		return;
	if (mp_editorLayer == nullptr) {
		m_targetNameModalPin = core::UUID{0};
		return;
	}
	const auto it = m_pinToOrigin.find(m_targetNameModalPin);
	if (it == m_pinToOrigin.end()) {
		m_targetNameModalPin = core::UUID{0};
		return;
	}
	auto* doc = mp_editorLayer->loadOrOpenSceneDocument(it->second.scenePath);
	if (doc == nullptr) {
		m_targetNameModalPin = core::UUID{0};
		return;
	}
	const auto& scene = doc->getActiveScene();
	if (!scene) {
		m_targetNameModalPin = core::UUID{0};
		return;
	}
	auto entity = scene->findEntityByUUID(it->second.triggerEntityUuid);
	if (!entity || !entity.hasComponent<scene::component::Trigger>()) {
		m_targetNameModalPin = core::UUID{0};
		return;
	}

	ImGui::OpenPopup("Edit teleport target");
	if (ImGui::BeginPopupModal("Edit teleport target", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextWrapped(
				"Target entity name (Tag) in the destination scene. Leave empty to use the default spawn point.");
		ImGui::SetNextItemWidth(280.0f);
		ImGui::InputText("##target", m_targetNameBuf, sizeof(m_targetNameBuf));
		ImGui::Separator();
		if (ImGui::Button("OK", {110.0f, 0.0f})) {
			// Capture before snapshot, mutate the live trigger, then push a ModifyEntityCommand on
			// the scene's own undo manager (not the canvas's) — pin labels are derived data and don't
			// need a canvas-side undo entry.
			auto before = EntitySnapshot::capture(entity);
			entity.getComponent<scene::component::Trigger>().trigger.targetName = std::string{m_targetNameBuf};
			auto cmd = mkUniq<commands::ModifyEntityCommand>(entity.getUUID(), std::move(before),
															 std::string{"Edit teleport target"});
			cmd->captureAfter(entity);
			doc->undoManager().push(std::move(cmd));
			m_targetNameModalPin = core::UUID{0};
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", {110.0f, 0.0f})) {
			m_targetNameModalPin = core::UUID{0};
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

}// namespace owl::nest
