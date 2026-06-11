/**
 * @file VoxelWorld.cpp
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/VoxelWorld.h"

namespace owl::scene::component {

namespace {
auto renderKindName(const data::voxel::BlockRenderKind iKind) -> const char* {
	switch (iKind) {
		case data::voxel::BlockRenderKind::Air:
			return "Air";
		case data::voxel::BlockRenderKind::Transparent:
			return "Transparent";
		case data::voxel::BlockRenderKind::Water:
			return "Water";
		case data::voxel::BlockRenderKind::Opaque:
			return "Opaque";
	}
	return "Opaque";
}

auto renderKindFrom(const std::string& iName) -> data::voxel::BlockRenderKind {
	if (iName == "Air")
		return data::voxel::BlockRenderKind::Air;
	if (iName == "Transparent")
		return data::voxel::BlockRenderKind::Transparent;
	if (iName == "Water")
		return data::voxel::BlockRenderKind::Water;
	return data::voxel::BlockRenderKind::Opaque;
}

void parseTerrain(const YAML::Node& iNode, data::voxel::TerrainParams& oTerrain) {
	if (iNode["Seed"])
		oTerrain.seed = iNode["Seed"].as<uint32_t>();
	if (iNode["Frequency"])
		oTerrain.frequency = iNode["Frequency"].as<float>();
	if (iNode["Octaves"])
		oTerrain.octaves = iNode["Octaves"].as<uint32_t>();
	if (iNode["Lacunarity"])
		oTerrain.lacunarity = iNode["Lacunarity"].as<float>();
	if (iNode["Persistence"])
		oTerrain.persistence = iNode["Persistence"].as<float>();
	if (iNode["BaseHeight"])
		oTerrain.baseHeight = iNode["BaseHeight"].as<int32_t>();
	if (iNode["Amplitude"])
		oTerrain.amplitude = iNode["Amplitude"].as<int32_t>();
	if (iNode["SeaLevel"])
		oTerrain.seaLevel = iNode["SeaLevel"].as<int32_t>();
	if (iNode["DirtDepth"])
		oTerrain.dirtDepth = iNode["DirtDepth"].as<int32_t>();
	if (iNode["CaveFrequency"])
		oTerrain.caveFrequency = iNode["CaveFrequency"].as<float>();
	if (iNode["CaveThreshold"])
		oTerrain.caveThreshold = iNode["CaveThreshold"].as<float>();
	if (iNode["Biomes"])
		oTerrain.biomes = iNode["Biomes"].as<bool>();
	if (iNode["BiomeFrequency"])
		oTerrain.biomeFrequency = iNode["BiomeFrequency"].as<float>();
	if (const auto ids = iNode["BlockIds"]; ids && ids.IsSequence() && ids.size() >= 5) {
		oTerrain.stone = ids[0].as<uint16_t>();
		oTerrain.grass = ids[1].as<uint16_t>();
		oTerrain.dirt = ids[2].as<uint16_t>();
		oTerrain.sand = ids[3].as<uint16_t>();
		oTerrain.water = ids[4].as<uint16_t>();
		if (ids.size() >= 6)
			oTerrain.snow = ids[5].as<uint16_t>();
	}
}
}// namespace

void VoxelWorld::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;// VoxelWorld
	emitter << YAML::Key << "SunDirection" << YAML::Value << YAML::Flow << YAML::BeginSeq << sunDirection.x()
			<< sunDirection.y() << sunDirection.z() << YAML::EndSeq;
	emitter << YAML::Key << "Ambient" << YAML::Value << YAML::Flow << YAML::BeginSeq << ambient.x() << ambient.y()
			<< ambient.z() << YAML::EndSeq;
	if (!tilesetPath.empty())
		emitter << YAML::Key << "Tileset" << YAML::Value << tilesetPath.generic_string();
	emitter << YAML::Key << "ProceduralTerrain" << YAML::Value << proceduralTerrain;
	emitter << YAML::Key << "StreamRadius" << YAML::Value << streamRadius;
	emitter << YAML::Key << "StreamHeight" << YAML::Value << streamHeight;
	emitter << YAML::Key << "AmbientOcclusion" << YAML::Value << ambientOcclusion;
	emitter << YAML::Key << "Terrain" << YAML::Value << YAML::BeginMap;
	emitter << YAML::Key << "Seed" << YAML::Value << terrain.seed;
	emitter << YAML::Key << "Frequency" << YAML::Value << terrain.frequency;
	emitter << YAML::Key << "Octaves" << YAML::Value << terrain.octaves;
	emitter << YAML::Key << "Lacunarity" << YAML::Value << terrain.lacunarity;
	emitter << YAML::Key << "Persistence" << YAML::Value << terrain.persistence;
	emitter << YAML::Key << "BaseHeight" << YAML::Value << terrain.baseHeight;
	emitter << YAML::Key << "Amplitude" << YAML::Value << terrain.amplitude;
	emitter << YAML::Key << "SeaLevel" << YAML::Value << terrain.seaLevel;
	emitter << YAML::Key << "DirtDepth" << YAML::Value << terrain.dirtDepth;
	emitter << YAML::Key << "CaveFrequency" << YAML::Value << terrain.caveFrequency;
	emitter << YAML::Key << "CaveThreshold" << YAML::Value << terrain.caveThreshold;
	emitter << YAML::Key << "Biomes" << YAML::Value << terrain.biomes;
	emitter << YAML::Key << "BiomeFrequency" << YAML::Value << terrain.biomeFrequency;
	emitter << YAML::Key << "BlockIds" << YAML::Value << YAML::Flow << YAML::BeginSeq << terrain.stone << terrain.grass
			<< terrain.dirt << terrain.sand << terrain.water << terrain.snow << YAML::EndSeq;
	emitter << YAML::EndMap;
	emitter << YAML::Key << "Blocks" << YAML::Value << YAML::BeginSeq;
	for (size_t id = 1; id < registry.count(); ++id) {
		const auto& block = registry.get(static_cast<data::voxel::BlockId>(id));
		emitter << YAML::BeginMap;
		emitter << YAML::Key << "id" << YAML::Value << id;
		emitter << YAML::Key << "name" << YAML::Value << block.name;
		emitter << YAML::Key << "render" << YAML::Value << renderKindName(block.renderKind);
		emitter << YAML::Key << "solid" << YAML::Value << block.solid;
		emitter << YAML::Key << "faces" << YAML::Value << YAML::Flow << YAML::BeginSeq;
		for (const auto face: block.faceTextures) emitter << face;
		emitter << YAML::EndSeq;
		emitter << YAML::EndMap;
	}
	emitter << YAML::EndSeq;
	// Procedural worlds stream their chunks from the seed at runtime, so authored chunks aren't persisted.
	if (!proceduralTerrain) {
		emitter << YAML::Key << "Chunks" << YAML::Value << YAML::BeginSeq;
		for (const auto& coord: world.chunkCoordinates()) {
			const auto chunk = world.getChunk(coord);
			if (!chunk || chunk->isEmpty())
				continue;
			emitter << YAML::BeginMap;
			emitter << YAML::Key << "Coord" << YAML::Value << YAML::Flow << YAML::BeginSeq << coord.x() << coord.y()
					<< coord.z() << YAML::EndSeq;
			emitter << YAML::Key << "Data" << YAML::Value << chunk->encode();
			emitter << YAML::EndMap;
		}
		emitter << YAML::EndSeq;
	}
	emitter << YAML::EndMap;// VoxelWorld
}

void VoxelWorld::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (const auto sun = node["SunDirection"]; sun && sun.IsSequence() && sun.size() >= 3)
		sunDirection = math::vec3{sun[0].as<float>(), sun[1].as<float>(), sun[2].as<float>()};
	if (const auto amb = node["Ambient"]; amb && amb.IsSequence() && amb.size() >= 3)
		ambient = math::vec3{amb[0].as<float>(), amb[1].as<float>(), amb[2].as<float>()};
	tilesetPath.clear();
	tileset.reset();
	if (const auto ts = node["Tileset"]; ts)
		tilesetPath = ts.as<std::string>();
	registry = data::voxel::BlockRegistry{};
	if (const auto blocks = node["Blocks"]; blocks && blocks.IsSequence()) {
		for (const auto& blockNode: blocks) {
			data::voxel::BlockType block;
			if (blockNode["name"])
				block.name = blockNode["name"].as<std::string>();
			block.renderKind = blockNode["render"] ? renderKindFrom(blockNode["render"].as<std::string>())
												   : data::voxel::BlockRenderKind::Opaque;
			block.solid = blockNode["solid"] ? blockNode["solid"].as<bool>() : true;
			if (const auto faces = blockNode["faces"]; faces && faces.IsSequence()) {
				for (size_t f = 0; f < data::voxel::g_FaceCount && f < faces.size(); ++f)
					block.faceTextures[f] = faces[f].as<uint16_t>();
			}
			(void) registry.registerBlock(block);
		}
	}
	proceduralTerrain = node["ProceduralTerrain"] ? node["ProceduralTerrain"].as<bool>() : false;
	if (const auto sr = node["StreamRadius"]; sr)
		streamRadius = sr.as<int32_t>();
	if (const auto sh = node["StreamHeight"]; sh)
		streamHeight = sh.as<int32_t>();
	if (const auto ao = node["AmbientOcclusion"]; ao)
		ambientOcclusion = ao.as<bool>();
	terrain = data::voxel::TerrainParams{};
	if (const auto t = node["Terrain"]; t && t.IsMap())
		parseTerrain(t, terrain);
	world.clear();
	if (const auto chunks = node["Chunks"]; chunks && chunks.IsSequence()) {
		for (const auto& chunkNode: chunks) {
			const auto coordNode = chunkNode["Coord"];
			if (!coordNode || !coordNode.IsSequence() || coordNode.size() < 3)
				continue;
			const math::vec3i coord{coordNode[0].as<int32_t>(), coordNode[1].as<int32_t>(), coordNode[2].as<int32_t>()};
			const auto chunk = world.getOrCreateChunk(coord);
			if (chunkNode["Data"])
				(void) chunk->decode(chunkNode["Data"].as<std::string>());
			chunk->markClean();
		}
	}
}

}// namespace owl::scene::component
