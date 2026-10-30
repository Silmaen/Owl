/**
 * @file Planet.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/component/Planet.h"

#include "core/SerializerImpl.h"
#include "data/assets/AssetSearchPaths.h"
#include "math/YamlSerializers.h"
#include "scene/pcg/PcgGraph.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <utility>

namespace owl::scene::component {

namespace {

template<typename T>
void readValue(const core::YamlNode& iNode, T& oValue) {
	if (iNode)
		oValue = iNode.as<T>();
}

void emitSettings(YAML::Emitter& ioOut, const renderer::PlanetSettings& iSettings) {
	ioOut << YAML::Key << "radius" << YAML::Value << iSettings.radius;
	ioOut << YAML::Key << "heightScale" << YAML::Value << iSettings.heightScale;
	ioOut << YAML::Key << "seaLevel" << YAML::Value << iSettings.seaLevel;
	ioOut << YAML::Key << "detailAmplitude" << YAML::Value << iSettings.detailAmplitude;
	ioOut << YAML::Key << "detailFrequency" << YAML::Value << iSettings.detailFrequency;
	ioOut << YAML::Key << "maxLevel" << YAML::Value << iSettings.maxLevel;
	ioOut << YAML::Key << "lodFactor" << YAML::Value << iSettings.lodFactor;
	ioOut << YAML::Key << "chunkResolution" << YAML::Value << iSettings.chunkResolution;
	ioOut << YAML::Key << "seaColor" << YAML::Value << iSettings.seaColor;
	ioOut << YAML::Key << "lowColor" << YAML::Value << iSettings.lowColor;
	ioOut << YAML::Key << "highColor" << YAML::Value << iSettings.highColor;
	ioOut << YAML::Key << "peakColor" << YAML::Value << iSettings.peakColor;
	ioOut << YAML::Key << "atmosphere" << YAML::Value << iSettings.atmosphere;
	ioOut << YAML::Key << "atmosphereHeight" << YAML::Value << iSettings.atmosphereHeight;
	ioOut << YAML::Key << "rayleighDepth" << YAML::Value << iSettings.rayleighDepth;
	ioOut << YAML::Key << "mieDepth" << YAML::Value << iSettings.mieDepth;
	ioOut << YAML::Key << "scatteringIntensity" << YAML::Value << iSettings.scatteringIntensity;
	ioOut << YAML::Key << "scaledSpaceDistance" << YAML::Value << iSettings.scaledSpaceDistance;
}

void readSettings(const core::YamlNode& iNode, renderer::PlanetSettings& ioSettings) {
	readValue(iNode["radius"], ioSettings.radius);
	readValue(iNode["heightScale"], ioSettings.heightScale);
	readValue(iNode["seaLevel"], ioSettings.seaLevel);
	readValue(iNode["detailAmplitude"], ioSettings.detailAmplitude);
	readValue(iNode["detailFrequency"], ioSettings.detailFrequency);
	readValue(iNode["maxLevel"], ioSettings.maxLevel);
	readValue(iNode["lodFactor"], ioSettings.lodFactor);
	readValue(iNode["chunkResolution"], ioSettings.chunkResolution);
	readValue(iNode["seaColor"], ioSettings.seaColor);
	readValue(iNode["lowColor"], ioSettings.lowColor);
	readValue(iNode["highColor"], ioSettings.highColor);
	readValue(iNode["peakColor"], ioSettings.peakColor);
	readValue(iNode["atmosphere"], ioSettings.atmosphere);
	readValue(iNode["atmosphereHeight"], ioSettings.atmosphereHeight);
	readValue(iNode["rayleighDepth"], ioSettings.rayleighDepth);
	readValue(iNode["mieDepth"], ioSettings.mieDepth);
	readValue(iNode["scatteringIntensity"], ioSettings.scatteringIntensity);
	readValue(iNode["scaledSpaceDistance"], ioSettings.scaledSpaceDistance);
	ioSettings = ioSettings.clamped();
}

}// namespace

void Planet::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key() << YAML::BeginMap;
	out << YAML::Key << "graph" << YAML::Value << graph;
	out << YAML::Key << "seed" << YAML::Value << seed;
	out << YAML::Key << "heightResolution" << YAML::Value << heightResolution;
	emitSettings(out, settings);
	out << YAML::EndMap;
}

void Planet::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	settings = {};
	graph.clear();
	seed = 0;
	heightResolution = Planet{}.heightResolution;
	readValue(node["graph"], graph);
	readValue(node["seed"], seed);
	readValue(node["heightResolution"], heightResolution);
	heightResolution = std::clamp(heightResolution, minHeightResolution, maxHeightResolution);
	readSettings(node, settings);
	m_runtime = PlanetRuntime{};
}

auto evaluatePlanetHeights(const std::string& iGraphText, const std::string& iSourceName, const uint32_t iResolution,
						   const uint64_t iSeed) -> renderer::PlanetHeightMap {
	OWL_PROFILE_FUNCTION()

	const auto graph = pcg::deserialize(iGraphText, iSourceName);
	if (!graph) {
		OWL_CORE_WARN("Planet: The graph '{}' cannot be read, the planet keeps its detail noise only.", iSourceName)
		return {};
	}
	const uint32_t side = std::clamp(iResolution, Planet::minHeightResolution, Planet::maxHeightResolution);
	const auto result = pcg::evaluate(*graph, {.size = {side, side}, .seed = iSeed});
	if (!result) {
		OWL_CORE_WARN("Planet: The graph '{}' failed to evaluate, the planet keeps its detail noise only.", iSourceName)
		return {};
	}
	if (result->height.size.surface() == 0 || result->height.values.empty()) {
		OWL_CORE_WARN("Planet: The graph '{}' has no Write Heights node, the planet keeps its detail noise only.",
					  iSourceName)
		return {};
	}
	renderer::PlanetHeightMap map{.size = result->height.size, .values = result->height.values};
	for (auto& value: map.values) value = std::clamp(value, 0.f, 1.f);
	return map;
}

auto Planet::getSurface() -> const shared<const renderer::PlanetSurface>& {
	const auto clamped = settings.clamped();
	const auto key = std::format("{}|{}|{}", graph, seed, heightResolution);
	if (m_runtime.surface && m_runtime.surfaceKey == key && m_runtime.surface->getSettings() == clamped)
		return m_runtime.surface;
	renderer::PlanetHeightMap heights;
	if (m_runtime.surface && m_runtime.surfaceKey == key) {
		heights = m_runtime.surface->getHeights();
	} else if (!graph.empty()) {
		if (const auto text = data::assets::readAssetText(graph); text)
			heights = evaluatePlanetHeights(*text, graph, heightResolution, seed);
		else
			OWL_CORE_WARN("Planet: The graph '{}' is not found, the planet keeps its detail noise only.", graph)
	}
	m_runtime.surface = mkShared<const renderer::PlanetSurface>(clamped, std::move(heights), seed);
	m_runtime.surfaceKey = key;
	return m_runtime.surface;
}

auto Planet::getTerrain() -> renderer::PlanetTerrain& {
	if (!m_runtime.terrain)
		m_runtime.terrain = mkShared<renderer::PlanetTerrain>();
	m_runtime.terrain->setSurface(getSurface());
	return *m_runtime.terrain;
}

}// namespace owl::scene::component
