/**
 * @file Block.cpp
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/external/yaml.h"
#include "data/voxel/Block.h"

#include <array>
#include <sstream>

namespace owl::data::voxel {

namespace {
auto makeAir() -> BlockType {
	BlockType air;
	air.name = "air";
	air.renderKind = BlockRenderKind::Air;
	air.solid = false;
	return air;
}

auto renderKindToString(const BlockRenderKind iKind) -> const char* {
	switch (iKind) {
		case BlockRenderKind::Air:
			return "Air";
		case BlockRenderKind::Opaque:
			return "Opaque";
		case BlockRenderKind::Transparent:
			return "Transparent";
		case BlockRenderKind::Water:
			return "Water";
	}
	return "Opaque";
}

auto renderKindFromString(const std::string& iName) -> BlockRenderKind {
	if (iName == "Air")
		return BlockRenderKind::Air;
	if (iName == "Transparent")
		return BlockRenderKind::Transparent;
	if (iName == "Water")
		return BlockRenderKind::Water;
	return BlockRenderKind::Opaque;
}

// Per-orientation permutation: index by world face (XNeg,XPos,YNeg,YPos,ZNeg,ZPos), value is the local face shown there.
constexpr std::array<std::array<BlockFace, g_FaceCount>, g_OrientationCount> k_FacePermutation{{
		{BlockFace::XNeg, BlockFace::XPos, BlockFace::YNeg, BlockFace::YPos, BlockFace::ZNeg, BlockFace::ZPos},
		{BlockFace::ZNeg, BlockFace::ZPos, BlockFace::YNeg, BlockFace::YPos, BlockFace::XPos, BlockFace::XNeg},
		{BlockFace::XPos, BlockFace::XNeg, BlockFace::YNeg, BlockFace::YPos, BlockFace::ZPos, BlockFace::ZNeg},
		{BlockFace::ZPos, BlockFace::ZNeg, BlockFace::YNeg, BlockFace::YPos, BlockFace::XNeg, BlockFace::XPos},
		{BlockFace::YNeg, BlockFace::YPos, BlockFace::XPos, BlockFace::XNeg, BlockFace::ZNeg, BlockFace::ZPos},
		{BlockFace::XNeg, BlockFace::XPos, BlockFace::ZPos, BlockFace::ZNeg, BlockFace::YNeg, BlockFace::YPos},
}};
}// namespace

auto orientedFace(const BlockFace iWorldFace, const BlockOrientation iOrientation) noexcept -> BlockFace {
	const auto orient = static_cast<size_t>(iOrientation);
	if (orient >= g_OrientationCount)
		return iWorldFace;
	return k_FacePermutation[orient][static_cast<size_t>(iWorldFace)];
}

BlockRegistry::BlockRegistry() { m_types.push_back(makeAir()); }

auto BlockRegistry::registerBlock(const BlockType& iType) -> BlockId {
	const auto id = static_cast<BlockId>(m_types.size());
	m_types.push_back(iType);
	return id;
}

auto BlockRegistry::get(const BlockId iId) const -> const BlockType& {
	if (iId >= m_types.size())
		return m_types[g_AirBlock];
	return m_types[iId];
}

auto BlockRegistry::findByName(const std::string_view iName) const -> std::optional<BlockId> {
	for (size_t i = 0; i < m_types.size(); ++i) {
		if (m_types[i].name == iName)
			return static_cast<BlockId>(i);
	}
	return std::nullopt;
}

auto BlockRegistry::isOpaque(const BlockId iId) const -> bool {
	return iId < m_types.size() && m_types[iId].isOpaque();
}

auto BlockRegistry::isSolid(const BlockId iId) const -> bool {
	return iId != g_AirBlock && iId < m_types.size() && m_types[iId].solid;
}

auto BlockRegistry::serializeToString(const std::string_view iName) const -> std::string {
	YAML::Emitter emitter;
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "BlockRegistry" << YAML::Value << std::string{iName};
	emitter << YAML::Key << "Version" << YAML::Value << 1;
	emitter << YAML::Key << "Blocks" << YAML::Value << YAML::BeginSeq;
	for (size_t id = 1; id < m_types.size(); ++id) {
		const auto& type = m_types[id];
		emitter << YAML::BeginMap;
		emitter << YAML::Key << "id" << YAML::Value << id;
		emitter << YAML::Key << "name" << YAML::Value << type.name;
		emitter << YAML::Key << "render" << YAML::Value << renderKindToString(type.renderKind);
		emitter << YAML::Key << "solid" << YAML::Value << type.solid;
		emitter << YAML::Key << "faces" << YAML::Value << YAML::Flow << YAML::BeginSeq;
		for (const auto face: type.faceTextures) emitter << face;
		emitter << YAML::EndSeq;
		emitter << YAML::EndMap;
	}
	emitter << YAML::EndSeq;
	emitter << YAML::EndMap;
	return std::string{emitter.c_str()};
}

auto BlockRegistry::deserializeFromString(const std::string_view iYaml) -> bool {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("BlockRegistry: failed to parse YAML — {}.", e.what())
		return false;
	}
	if (!root || !root.IsMap() || !root["BlockRegistry"])
		return false;
	std::vector<BlockType> parsed;
	parsed.push_back(makeAir());
	if (const auto blocks = root["Blocks"]; blocks && blocks.IsSequence()) {
		for (const auto& node: blocks) {
			const auto id = node["id"] ? node["id"].as<size_t>() : parsed.size();
			if (id >= parsed.size())
				parsed.resize(id + 1, makeAir());
			BlockType type;
			if (node["name"])
				type.name = node["name"].as<std::string>();
			type.renderKind =
					node["render"] ? renderKindFromString(node["render"].as<std::string>()) : BlockRenderKind::Opaque;
			type.solid = node["solid"] ? node["solid"].as<bool>() : true;
			if (const auto faces = node["faces"]; faces && faces.IsSequence()) {
				for (size_t f = 0; f < g_FaceCount && f < faces.size(); ++f)
					type.faceTextures[f] = faces[f].as<uint16_t>();
			}
			parsed[id] = std::move(type);
		}
	}
	m_types = std::move(parsed);
	return true;
}

}// namespace owl::data::voxel
