/**
 * @file RenderStack.cpp
 * @author Silmaen
 * @date 30/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RenderLayerFactory.h"
#include "renderer/RenderStack.h"
#include "renderer/RenderStackYaml.h"

#include <algorithm>
#include <ranges>
#include <unordered_set>
#include <utility>

namespace owl::renderer {

namespace {
constexpr auto g_DefaultLayerName = "default";
constexpr auto g_DefaultLayerType = "Renderer2D";

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

auto parseYamlText(const std::string& iYaml) -> YAML::Node {
	if (iYaml.empty())
		return YAML::Node{};
	try {
		return YAML::Load(iYaml);
	} catch (const YAML::Exception& e) {
		OWL_CORE_WARN("RenderStack: Invalid YAML config ignored ({}).", e.what())
		return YAML::Node{};
	}
}

auto dumpYamlText(const YAML::Node& iNode) -> std::string {
	if (!iNode || iNode.IsNull() || (!iNode.IsScalar() && iNode.size() == 0))
		return {};
	return YAML::Dump(iNode);
}

auto stackToYaml(const RendererStackConfig& iConfig) -> YAML::Node {
	YAML::Node out{YAML::NodeType::Sequence};
	for (const auto& entry: iConfig.entries) {
		YAML::Node item{YAML::NodeType::Map};
		item["Type"] = entry.typeKey;
		item["Name"] = entry.name;
		if (const auto cfg = parseYamlText(entry.defaultConfig); cfg && cfg.size() > 0)
			item["DefaultConfig"] = cfg;
		out.push_back(item);
	}
	return out;
}

auto stackFromYaml(const YAML::Node& iNode) -> RendererStackConfig {
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
		entry.defaultConfig = dumpYamlText(item["DefaultConfig"]);
		cfg.entries.push_back(std::move(entry));
	}
	return cfg;
}

auto enabledToYaml(const EnabledRenderersConfig& iConfig) -> YAML::Node {
	YAML::Node out{YAML::NodeType::Sequence};
	for (const auto& entry: iConfig.entries) {
		YAML::Node item{YAML::NodeType::Map};
		item["Name"] = entry.name;
		item["Enabled"] = entry.enabled;
		if (const auto overrides = parseYamlText(entry.overrides); overrides && overrides.size() > 0)
			item["Overrides"] = overrides;
		out.push_back(item);
	}
	return out;
}

auto enabledFromYaml(const YAML::Node& iNode) -> EnabledRenderersConfig {
	EnabledRenderersConfig cfg;
	if (!iNode || !iNode.IsSequence())
		return cfg;
	for (const auto& item: iNode) {
		if (!item.IsMap())
			continue;
		EnabledRenderersConfig::Entry entry;
		if (const auto n = item["Name"]; n && n.IsScalar())
			entry.name = n.as<std::string>();
		if (entry.name.empty())
			continue;
		if (const auto e = item["Enabled"])
			entry.enabled = e.as<bool>(true);
		entry.overrides = dumpYamlText(item["Overrides"]);
		cfg.entries.push_back(std::move(entry));
	}
	return cfg;
}

// ---------------------------------------------------------------- RendererStackConfig
auto RendererStackConfig::find(const std::string& iName) const -> const RendererStackEntry* {
	const auto it = std::ranges::find_if(entries, [&](const auto& e) -> bool { return e.name == iName; });
	if (it == entries.end())
		return nullptr;
	return &*it;
}

auto RendererStackConfig::makeDefault() -> RendererStackConfig {
	RendererStackConfig cfg;
	cfg.entries.push_back({.typeKey = g_DefaultLayerType, .name = g_DefaultLayerName, .defaultConfig = {}});
	return cfg;
}

auto RendererStackConfig::toYaml() const -> std::string { return YAML::Dump(stackToYaml(*this)); }

auto RendererStackConfig::fromYaml(const std::string& iYaml) -> RendererStackConfig {
	return stackFromYaml(parseYamlText(iYaml));
}

// ---------------------------------------------------------------- EnabledRenderersConfig
auto EnabledRenderersConfig::find(const std::string& iName) const -> const Entry* {
	const auto it = std::ranges::find_if(entries, [&](const auto& e) -> bool { return e.name == iName; });
	if (it == entries.end())
		return nullptr;
	return &*it;
}

auto EnabledRenderersConfig::toYaml() const -> std::string { return YAML::Dump(enabledToYaml(*this)); }

auto EnabledRenderersConfig::fromYaml(const std::string& iYaml) -> EnabledRenderersConfig {
	return enabledFromYaml(parseYamlText(iYaml));
}

// ---------------------------------------------------------------- RenderStack
auto RenderStack::buildFromConfig(const RendererStackConfig& iProject, const EnabledRenderersConfig& iScene)
		-> RenderStack {
	RenderStack stack;
	const RendererStackConfig fallback =
			iProject.isEmpty() ? RendererStackConfig::makeDefault() : RendererStackConfig{};
	const RendererStackConfig& effective = iProject.isEmpty() ? fallback : iProject;
	if (iProject.isEmpty()) {
		OWL_CORE_WARN("RenderStack: empty project config — falling back to default Renderer2D.")
	}

	const auto findProject = [&](const std::string& iName) -> const RendererStackEntry* {
		const auto it =
				std::ranges::find_if(effective.entries, [&](const auto& iE) -> bool { return iE.name == iName; });
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
		YAML::Node merged = parseYamlText(iProjectEntry.defaultConfig);
		if (!merged.IsMap())
			merged = YAML::Node{YAML::NodeType::Map};
		if (iSceneEntry != nullptr)
			mergeYaml(merged, parseYamlText(iSceneEntry->overrides));
		layer->applyConfig(YAML::Dump(merged));
		stack.m_layers.push_back(std::move(layer));
	};

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

	for (const auto& projectEntry: effective.entries) {
		if (emitted.contains(projectEntry.name))
			continue;
		emit(projectEntry, nullptr);
	}

	return stack;
}

auto RenderStack::findByName(const std::string& iName) const -> shared<RenderLayer> {
	const auto it =
			std::ranges::find_if(m_layers, [&](const auto& layer) -> bool { return layer->getName() == iName; });
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
	for (const auto& layer: m_layers) layer->onBeginFrame(iCamera);
}

void RenderStack::renderScene(scene::Scene& ioScene) const {
	for (const auto& layer: m_layers) layer->onRender(ioScene);
}

void RenderStack::endFrame() {
	for (const auto& layer: std::ranges::reverse_view(m_layers)) layer->onEndFrame();
}

}// namespace owl::renderer
