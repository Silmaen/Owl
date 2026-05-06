/**
 * @file RenderStack.cpp
 * @author Silmaen
 * @date 30/04/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RenderLayerFactory.h"
#include "renderer/RenderStack.h"

#include <algorithm>
#include <ranges>
#include <unordered_set>
#include <utility>

namespace owl::renderer {

namespace {

constexpr auto g_DefaultLayerName = "default";
constexpr auto g_DefaultLayerType = "Renderer2D";

/**
 * @brief Merge `iOverride` keys onto `ioBase`, override wins on conflicts.
 *
 * Iterative implementation (worklist of `(base, override)` pairs) to keep
 * `misc-no-recursion` clean. yaml-cpp `Node` is a reference-counted handle,
 * so pushing aliases of the sub-nodes onto the worklist edits the same
 * underlying structure as a recursive walk would.
 */
void mergeYaml(YAML::Node& ioBase, const YAML::Node& iOverride) {
	std::vector<std::pair<YAML::Node, YAML::Node>> work;
	work.emplace_back(ioBase, iOverride);
	while (!work.empty()) {
		auto [base, override_] = std::move(work.back());
		work.pop_back();
		if (!override_ || !override_.IsMap())
			continue;
		for (const auto& kv: override_) {
			const auto key = kv.first.as<std::string>();
			if (kv.second.IsMap() && base[key] && base[key].IsMap()) {
				const YAML::Node sub = base[key];
				work.emplace_back(sub, kv.second);
			} else {
				base[key] = kv.second;
			}
		}
	}
}

}// namespace

// ---------------------------------------------------------------- RendererStackConfig

auto RendererStackConfig::find(const std::string& iName) const -> const RendererStackEntry* {
	const auto it = std::ranges::find_if(entries, [&](const auto& e) -> bool { return e.name == iName; });
	if (it == entries.end())
		return nullptr;
	return &*it;
}

auto RendererStackConfig::makeDefault() -> RendererStackConfig {
	RendererStackConfig cfg;
	cfg.entries.push_back({.typeKey = g_DefaultLayerType, .name = g_DefaultLayerName, .defaultConfig = YAML::Node{}});
	return cfg;
}

auto RendererStackConfig::toYaml() const -> YAML::Node {
	YAML::Node out{YAML::NodeType::Sequence};
	for (const auto& entry: entries) {
		YAML::Node item{YAML::NodeType::Map};
		item["Type"] = entry.typeKey;
		item["Name"] = entry.name;
		if (entry.defaultConfig && entry.defaultConfig.size() > 0)
			item["DefaultConfig"] = entry.defaultConfig;
		out.push_back(item);
	}
	return out;
}

auto RendererStackConfig::fromYaml(const YAML::Node& iNode) -> RendererStackConfig {
	RendererStackConfig cfg;
	if (!iNode || !iNode.IsSequence())
		return cfg;
	std::unordered_set<std::string> seenNames;
	for (const auto& item: iNode) {
		if (!item.IsMap()) {
			OWL_CORE_WARN("RendererStackConfig: skipping non-map entry.")
			continue;
		}
		RendererStackEntry entry;
		if (const auto t = item["Type"]; t && t.IsScalar())
			entry.typeKey = t.as<std::string>();
		if (const auto n = item["Name"]; n && n.IsScalar())
			entry.name = n.as<std::string>();
		if (entry.typeKey.empty() || entry.name.empty()) {
			OWL_CORE_WARN("RendererStackConfig: skipping entry with missing Type/Name.")
			continue;
		}
		if (!seenNames.insert(entry.name).second) {
			OWL_CORE_WARN("RendererStackConfig: duplicate name '{}' — keeping first.", entry.name)
			continue;
		}
		if (const auto cfgNode = item["DefaultConfig"])
			entry.defaultConfig = YAML::Clone(cfgNode);
		cfg.entries.push_back(std::move(entry));
	}
	return cfg;
}

// ---------------------------------------------------------------- EnabledRenderersConfig

auto EnabledRenderersConfig::find(const std::string& iName) const -> const Entry* {
	const auto it = std::ranges::find_if(entries, [&](const auto& e) -> bool { return e.name == iName; });
	if (it == entries.end())
		return nullptr;
	return &*it;
}

auto EnabledRenderersConfig::toYaml() const -> YAML::Node {
	YAML::Node out{YAML::NodeType::Sequence};
	for (const auto& entry: entries) {
		YAML::Node item{YAML::NodeType::Map};
		item["Name"] = entry.name;
		item["Enabled"] = entry.enabled;
		if (entry.overrides && entry.overrides.size() > 0)
			item["Overrides"] = entry.overrides;
		out.push_back(item);
	}
	return out;
}

auto EnabledRenderersConfig::fromYaml(const YAML::Node& iNode) -> EnabledRenderersConfig {
	EnabledRenderersConfig cfg;
	if (!iNode || !iNode.IsSequence())
		return cfg;
	for (const auto& item: iNode) {
		if (!item.IsMap())
			continue;
		Entry entry;
		if (const auto n = item["Name"]; n && n.IsScalar())
			entry.name = n.as<std::string>();
		if (entry.name.empty())
			continue;
		if (const auto e = item["Enabled"])
			entry.enabled = e.as<bool>(true);
		if (const auto o = item["Overrides"])
			entry.overrides = YAML::Clone(o);
		cfg.entries.push_back(std::move(entry));
	}
	return cfg;
}

// ---------------------------------------------------------------- RenderStack

auto RenderStack::buildFromConfig(const RendererStackConfig& iProject,
								  const EnabledRenderersConfig& iScene) -> RenderStack {
	RenderStack stack;
	const RendererStackConfig fallback = iProject.isEmpty() ? RendererStackConfig::makeDefault() : RendererStackConfig{};
	const RendererStackConfig& effective = iProject.isEmpty() ? fallback : iProject;
	if (iProject.isEmpty()) {
		OWL_CORE_WARN("RenderStack: empty project config — falling back to default Renderer2D.")
	}

	const auto findProject = [&](const std::string& iName) -> const RendererStackEntry* {
		const auto it = std::ranges::find_if(effective.entries,
											 [&](const auto& iE) -> bool { return iE.name == iName; });
		return it == effective.entries.end() ? nullptr : &*it;
	};

	std::unordered_set<std::string> emitted;
	const auto emit = [&](const RendererStackEntry& iProjectEntry,
						  const EnabledRenderersConfig::Entry* iSceneEntry) -> void {
		emitted.insert(iProjectEntry.name);
		const bool enabled = (iSceneEntry == nullptr) || iSceneEntry->enabled;
		if (!enabled)
			return;
		auto layer = RenderLayerFactory::create(iProjectEntry.typeKey, iProjectEntry.name);
		if (layer == nullptr) {
			OWL_CORE_ERROR("RenderStack: failed to create layer '{}' (type '{}'), skipped.", iProjectEntry.name,
						   iProjectEntry.typeKey)
			return;
		}
		// Build merged config: clone project default, overlay scene overrides.
		// `merged` MUST be initialised as an explicit Map even when the project
		// has no `DefaultConfig` — yaml-cpp's auto-vivify on `node[key] = ...`
		// updates the *local* node pointer but does not propagate back through
		// the caller's reference, so passing a default-constructed
		// (Undefined) `YAML::Node{}` to `mergeYaml` silently drops every
		// override. Cloning a Map (or starting as a Map) gives `mergeYaml` a
		// real underlying memory block to mutate.
		YAML::Node merged = iProjectEntry.defaultConfig && iProjectEntry.defaultConfig.IsMap()
									? YAML::Clone(iProjectEntry.defaultConfig)
									: YAML::Node{YAML::NodeType::Map};
		if (iSceneEntry != nullptr && iSceneEntry->overrides)
			mergeYaml(merged, iSceneEntry->overrides);
		layer->applyConfig(merged);
		stack.m_layers.push_back(std::move(layer));
	};

	// Pass 1 — scene-listed layers in scene order. The scene authors the order
	// here; unlisted layers fall through to pass 2 in project order. A scene
	// that does not declare `EnabledRenderers` skips this pass entirely and
	// inherits the project's order verbatim (the legacy default).
	for (const auto& sceneEntry: iScene.entries) {
		if (emitted.contains(sceneEntry.name))
			continue;// duplicate name in scene listing — first wins.
		const auto* projectEntry = findProject(sceneEntry.name);
		if (projectEntry == nullptr) {
			OWL_CORE_WARN("RenderStack: scene references unknown layer '{}' — skipped.", sceneEntry.name)
			continue;
		}
		emit(*projectEntry, &sceneEntry);
	}

	// Pass 2 — any project layer the scene did not mention, in project order,
	// enabled by default (scene silence == "use the project default").
	for (const auto& projectEntry: effective.entries) {
		if (emitted.contains(projectEntry.name))
			continue;
		emit(projectEntry, nullptr);
	}

	return stack;
}

auto RenderStack::findByName(const std::string& iName) const -> shared<RenderLayer> {
	const auto it = std::ranges::find_if(m_layers,
										 [&](const auto& layer) -> bool { return layer->getName() == iName; });
	if (it == m_layers.end())
		return nullptr;
	return *it;
}

auto RenderStack::getDefaultLayer() const -> shared<RenderLayer> {
	if (m_layers.empty())
		return nullptr;
	return m_layers.front();
}

void RenderStack::beginFrame(const Camera& iCamera) const {
	for (const auto& layer: m_layers)
		layer->onBeginFrame(iCamera);
}

void RenderStack::renderScene(scene::Scene& ioScene) const {
	for (const auto& layer: m_layers)
		layer->onRender(ioScene);
}

void RenderStack::endFrame() {
	for (const auto& layer: std::ranges::reverse_view(m_layers))
		layer->onEndFrame();
}

}// namespace owl::renderer
