/**
 * @file VoxelPalette.cpp
 * @author Silmaen
 * @date 12/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "VoxelPalette.h"

#include <data/voxel/VoxelStructure.h>

#include <imgui.h>

#include <fstream>
#include <sstream>

namespace owl::nest::panel {

namespace {
auto findVoxelWorld(const shared<scene::Scene>& iScene) -> scene::component::VoxelWorld* {
	if (!iScene)
		return nullptr;
	const auto view = iScene->registry.view<scene::component::VoxelWorld>();
	if (view.begin() == view.end())
		return nullptr;
	return &view.get<scene::component::VoxelWorld>(*view.begin());
}
}// namespace

auto VoxelPalette::structureDirectory() const -> std::filesystem::path {
	return m_projectDirectory.empty() ? std::filesystem::path{"structures"} : m_projectDirectory / "structures";
}

void VoxelPalette::saveStructure(const std::string& iName) const {
	const auto* voxelWorld = findVoxelWorld(m_scene);
	if (voxelWorld == nullptr)
		return;
	const data::voxel::VoxelStructure structure =
			data::voxel::VoxelStructure::captureFromWorld(voxelWorld->world, voxelWorld->registry);
	if (structure.isEmpty()) {
		OWL_WARN("VoxelPalette: nothing to save, the world has no solid block.")
		return;
	}
	const std::filesystem::path dir = structureDirectory();
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	const std::filesystem::path file = dir / (iName + ".owlvoxstruct");
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) {
		OWL_WARN("VoxelPalette: failed to write structure {}.", file.generic_string())
		return;
	}
	out << structure.serializeToString(iName);
	OWL_INFO("VoxelPalette: saved structure {}.", file.generic_string())
}

void VoxelPalette::onImGuiRender(VoxelBrush& ioBrush) {
	if (!m_visible)
		return;
	if (!ImGui::Begin("Voxel Palette", &m_visible)) {
		ImGui::End();
		return;
	}
	auto* voxelWorld = findVoxelWorld(m_scene);
	if (voxelWorld == nullptr) {
		ImGui::TextDisabled("No VoxelWorld in the active scene.");
		ioBrush.active = false;
		ImGui::End();
		return;
	}

	ImGui::Checkbox("Brush active", &ioBrush.active);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("In Edit mode: left-click places / stamps, right-click erases the targeted block.");
	ImGui::Separator();

	if (ImGui::Selectable("Eraser", ioBrush.eraser && !ioBrush.structure.has_value())) {
		ioBrush.eraser = true;
		ioBrush.structure.reset();
		ioBrush.structureName.clear();
	}
	ImGui::Separator();
	ImGui::TextDisabled("Blocks");
	const auto& tileset = voxelWorld->tileset;
	constexpr float kThumb = 24.f;
	for (data::voxel::BlockId id = 1; id < static_cast<data::voxel::BlockId>(voxelWorld->registry.count()); ++id) {
		ImGui::PushID(static_cast<int>(id));
		const auto& block = voxelWorld->registry.get(id);
		if (tileset && tileset->texture) {
			const auto uvs = tileset->getTileUv(block.faceTexture(data::voxel::BlockFace::YPos));
			ImGui::Image(static_cast<ImTextureID>(tileset->texture->getRendererId()), ImVec2{kThumb, kThumb},
						 gui::vec(uvs[3]), gui::vec(uvs[1]));
			ImGui::SameLine();
			ImGui::AlignTextToFramePadding();
		}
		const bool selected = !ioBrush.eraser && !ioBrush.structure.has_value() && ioBrush.paintBlock == id;
		const std::string label = std::format("{}: {}", id, block.name);
		if (ImGui::Selectable(label.c_str(), selected)) {
			ioBrush.paintBlock = id;
			ioBrush.eraser = false;
			ioBrush.structure.reset();
			ioBrush.structureName.clear();
		}
		ImGui::PopID();
	}

	ImGui::Separator();
	ImGui::TextDisabled("Structures");
	std::array<char, 64> nameBuf{};
	std::snprintf(nameBuf.data(), nameBuf.size(), "%s", m_saveName.c_str());
	if (ImGui::InputText("Name", nameBuf.data(), nameBuf.size()))
		m_saveName = nameBuf.data();
	ImGui::SameLine();
	if (ImGui::Button("Save") && !m_saveName.empty())
		saveStructure(m_saveName);
	if (ioBrush.structure.has_value()) {
		ImGui::Text("Stamping: %s", ioBrush.structureName.c_str());
		if (ImGui::Button("Clear structure")) {
			ioBrush.structure.reset();
			ioBrush.structureName.clear();
		}
	}
	const std::filesystem::path dir = structureDirectory();
	if (std::error_code ec; std::filesystem::is_directory(dir, ec)) {
		for (const auto& entry: std::filesystem::directory_iterator(dir, ec)) {
			if (!entry.is_regular_file() || entry.path().extension() != ".owlvoxstruct")
				continue;
			const std::string name = entry.path().stem().string();
			if (ImGui::Selectable(name.c_str(), ioBrush.structureName == name)) {
				const std::ifstream in(entry.path(), std::ios::binary);
				std::stringstream buffer;
				buffer << in.rdbuf();
				data::voxel::VoxelStructure structure;
				if (structure.deserializeFromString(buffer.str())) {
					ioBrush.structure = std::move(structure);
					ioBrush.structureName = name;
					ioBrush.eraser = false;
				} else {
					OWL_WARN("VoxelPalette: failed to load structure {}.", entry.path().generic_string())
				}
			}
		}
	}
	ImGui::End();
}

}// namespace owl::nest::panel
