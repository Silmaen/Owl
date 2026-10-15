/**
 * @file IsometricObjects.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/IsometricObjects.h"

#include "core/external/yaml.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::renderer {

namespace {

auto readTileWidth(const YAML::Node& iZoom) -> float {
	if (!iZoom || !iZoom.IsScalar())
		return 64.f;
	const auto text = iZoom.as<std::string>();
	if (text == "half")
		return 32.f;
	try {
		return 64.f * std::stof(text);
	} catch (const std::exception&) { return 64.f; }
}

auto readSlice(const YAML::Node& iNode, IsometricSlice& oSlice) -> bool {
	const auto cell = iNode["cell"];
	const auto rect = iNode["rect"];
	const auto anchor = iNode["anchor"];
	const auto box = iNode["box"];
	if (!cell || !cell.IsSequence() || cell.size() != 2 || !rect || !rect.IsSequence() || rect.size() != 4 || !anchor ||
		!anchor.IsSequence() || anchor.size() != 2 || !box || !box["min"] || !box["max"])
		return false;
	oSlice.cell = {cell[0].as<uint32_t>(), cell[1].as<uint32_t>()};
	oSlice.rect = {rect[0].as<float>(), rect[1].as<float>(), rect[2].as<float>(), rect[3].as<float>()};
	oSlice.anchor = {anchor[0].as<float>(), anchor[1].as<float>()};
	const auto lo = box["min"];
	const auto hi = box["max"];
	if (!lo.IsSequence() || lo.size() != 3 || !hi.IsSequence() || hi.size() != 3)
		return false;
	oSlice.boxMin = {lo[0].as<float>(), lo[1].as<float>(), lo[2].as<float>()};
	oSlice.boxMax = {hi[0].as<float>(), hi[1].as<float>(), hi[2].as<float>()};
	return oSlice.rect.z() > 0.f && oSlice.rect.w() > 0.f;
}

auto readObject(const std::string& iName, const YAML::Node& iNode, IsometricObjectDef& oObject) -> bool {
	const auto footprint = iNode["footprint"];
	const auto slices = iNode["slices"];
	if (!footprint || !footprint.IsSequence() || footprint.size() != 2 || !slices || !slices.IsSequence() ||
		slices.size() == 0)
		return false;
	oObject.name = iName;
	oObject.footprint = {footprint[0].as<uint32_t>(), footprint[1].as<uint32_t>()};
	if (oObject.footprint.x() == 0 || oObject.footprint.y() == 0)
		return false;
	for (const auto& sliceNode: slices) {
		IsometricSlice slice;
		if (!readSlice(sliceNode, slice))
			return false;
		oObject.slices.push_back(slice);
	}
	return true;
}

}// namespace

auto IsometricObjectSet::parse(const std::string_view iText)
		-> owl::expected<IsometricObjectSet, IsometricObjectError> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iText});
	} catch (const YAML::Exception& error) {
		OWL_CORE_WARN("IsometricObjectSet: invalid document ({}).", error.what())
		return owl::unexpected{IsometricObjectError::InvalidDocument};
	}
	if (!root || !root.IsMap()) {
		OWL_CORE_WARN("IsometricObjectSet: the document is not a map.")
		return owl::unexpected{IsometricObjectError::InvalidDocument};
	}
	const auto buildings = root["buildings"];
	if (!buildings || !buildings.IsMap() || buildings.size() == 0) {
		OWL_CORE_WARN("IsometricObjectSet: the document has no objects.")
		return owl::unexpected{IsometricObjectError::NoObjects};
	}
	IsometricObjectSet set;
	set.m_tileWidth = readTileWidth(root["zoom"]);
	try {
		for (const auto& entry: buildings) {
			IsometricObjectDef object;
			if (const auto name = entry.first.as<std::string>(); !readObject(name, entry.second, object)) {
				OWL_CORE_WARN("IsometricObjectSet: object {} is malformed.", name)
				return owl::unexpected{IsometricObjectError::InvalidObject};
			}
			set.m_objects.push_back(std::move(object));
		}
	} catch (const YAML::Exception& error) {
		OWL_CORE_WARN("IsometricObjectSet: malformed value ({}).", error.what())
		return owl::unexpected{IsometricObjectError::InvalidObject};
	}
	std::ranges::sort(set.m_objects, [](const IsometricObjectDef& iA, const IsometricObjectDef& iB) -> bool {
		return iA.name < iB.name;
	});
	return set;
}

auto IsometricObjectDef::getHeight() const -> float {
	float height = 0.f;
	for (const auto& slice: slices) height = std::max(height, slice.boxMax.z());
	return height;
}

auto IsometricObjectSet::find(const std::string& iName) const -> const IsometricObjectDef* {
	const auto it = std::ranges::find(m_objects, iName, &IsometricObjectDef::name);
	return it == m_objects.end() ? nullptr : &*it;
}

}// namespace owl::renderer
