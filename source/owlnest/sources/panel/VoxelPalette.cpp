/**
 * @file VoxelPalette.cpp
 * @author Silmaen
 * @date 12/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "VoxelPalette.h"

#include "../DockLayout.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/ImageTile.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Tooltip.h>
#include <gui/widgets/TreeRow.h>

#include <data/voxel/VoxelStructure.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>

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
	if (!structure.saveToFile(file, iName)) {
		OWL_WARN("VoxelPalette: Failed to write structure {}.", file.generic_string())
		return;
	}
	OWL_INFO("VoxelPalette: saved structure {}.", file.generic_string())
}

void VoxelPalette::onImGuiRender(VoxelBrush& ioBrush, const bool iShown) {
	auto* voxelWorld = iShown ? findVoxelWorld(m_scene) : nullptr;
	if (voxelWorld == nullptr) {
		ioBrush.active = false;
		return;
	}
	if (!m_visible)
		return;
	if (!ImGui::Begin(panelWindow::title(gui::icon::boxes, "Voxel Palette", panelWindow::voxelPalette).c_str(),
					  &m_visible)) {
		ImGui::End();
		return;
	}
	renderBrushSection(ioBrush);
	renderBlockSection(ioBrush, *voxelWorld);
	renderStructureSection(ioBrush);
	ImGui::End();
}

void VoxelPalette::renderBrushSection(VoxelBrush& ioBrush) {
	if (!gui::widgets::sectionHeader("Brush", gui::icon::paintbrush, false).open)
		return;
	if (gui::widgets::beginProperties("##voxelBrush")) {
		std::ignore = gui::widgets::propertyBool(
				"Active", ioBrush.active, false,
				"In Edit mode: left-click places or stamps, right-click erases the targeted block.");
		constexpr std::array<const char*, data::voxel::g_OrientationCount> kOrientationNames{
				"Identity", "Yaw 90 CW", "Yaw 180", "Yaw 90 CCW", "Axis X", "Axis Z"};
		int orientation = static_cast<int>(ioBrush.orientation);
		gui::widgets::propertyLabel("Orientation",
									"Orientation of the painted blocks: remaps their face textures, the geometry is "
									"unchanged.");
		if (gui::widgets::combo("##orientation", &orientation, kOrientationNames))
			ioBrush.orientation = static_cast<data::voxel::BlockOrientation>(orientation);
		std::ignore = gui::widgets::propertyReset(false);
		int state = static_cast<int>(ioBrush.state);
		if (gui::widgets::propertyInt("State", state, 0, 0, 255, "%d",
									  "Free gameplay or editor byte stored on the painted blocks; the renderer ignores "
									  "it."))
			ioBrush.state = static_cast<uint8_t>(state);
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

void VoxelPalette::renderBlockSection(VoxelBrush& ioBrush, const scene::component::VoxelWorld& iWorld) {
	if (!gui::widgets::sectionHeader("Blocks", gui::icon::cuboid, false).open)
		return;
	const auto& theme = gui::UiLayer::getTheme();
	const bool erasing = ioBrush.eraser && !ioBrush.structure.has_value();
	if (gui::widgets::iconButton("eraser", {.glyph = gui::icon::eraser,
											.tooltip = "Eraser",
											.description = "Left-click removes the targeted block.",
											.selected = erasing})) {
		ioBrush.eraser = true;
		ioBrush.structure.reset();
		ioBrush.structureName.clear();
	}
	// One square per block, wrapped to the width of the panel; the name is in the tooltip.
	const auto& tileset = iWorld.tileset;
	const float side = std::round(ImGui::GetFrameHeight() * 1.4f);
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	float x = ImGui::GetCursorPosX() + side + spacing;
	const float right = ImGui::GetContentRegionMax().x;
	for (data::voxel::BlockId id = 1; id < static_cast<data::voxel::BlockId>(iWorld.registry.count()); ++id) {
		ImGui::PushID(static_cast<int>(id));
		const auto& block = iWorld.registry.get(id);
		if (x + side <= right) {
			ImGui::SameLine();
		} else {
			x = ImGui::GetStyle().WindowPadding.x;
		}
		x += side + spacing;
		const bool selected = !ioBrush.eraser && !ioBrush.structure.has_value() && ioBrush.paintBlock == id;
		gui::widgets::ImageTileSpec tile{.size = side, .outline = selected ? theme.accent : math::vec4{}};
		if (tileset && tileset->texture) {
			const auto uvs = tileset->getTileUv(block.faceTexture(data::voxel::BlockFace::YPos));
			tile.textureId = tileset->texture->getRendererId();
			tile.uv0 = uvs[3];
			tile.uv1 = uvs[1];
		}
		if (gui::widgets::imageTile("##block", tile).left) {
			ioBrush.paintBlock = id;
			ioBrush.eraser = false;
			ioBrush.structure.reset();
			ioBrush.structureName.clear();
		}
		gui::widgets::itemTooltip(block.name.c_str(), nullptr, std::format("Block {}", id).c_str());
		ImGui::PopID();
	}
	gui::widgets::endSection();
}

void VoxelPalette::renderStructureSection(VoxelBrush& ioBrush) {
	if (!gui::widgets::sectionHeader("Structures", gui::icon::boxes, false).open)
		return;
	if (gui::widgets::beginProperties("##voxelStructure")) {
		std::ignore = gui::widgets::propertyText("Name", m_saveName, "my_house",
												 "File name of the structure saved from the whole world.");
		gui::widgets::endProperties();
	}
	if (gui::widgets::button("save", {.glyph = gui::icon::save,
									  .label = "Save World as Structure",
									  .tooltip = "Save",
									  .description = "Write every solid block of the world to structures/<name>.",
									  .disabled = m_saveName.empty()}))
		saveStructure(m_saveName);
	if (ioBrush.structure.has_value()) {
		ImGui::SameLine();
		if (gui::widgets::button(
					"clear", {.glyph = gui::icon::x, .label = "Stop Stamping", .tooltip = "Back to painting blocks"})) {
			ioBrush.structure.reset();
			ioBrush.structureName.clear();
		}
	}
	const std::filesystem::path dir = structureDirectory();
	std::error_code ec;
	if (!std::filesystem::is_directory(dir, ec)) {
		ImGui::TextDisabled("No structure saved yet.");
		gui::widgets::endSection();
		return;
	}
	for (const auto& entry: std::filesystem::directory_iterator(dir, ec)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".owlvoxstruct")
			continue;
		const std::string name = entry.path().stem().string();
		const auto row = gui::widgets::treeRow(
				name.c_str(),
				{.label = name.c_str(), .glyph = gui::icon::cuboid, .selected = ioBrush.structureName == name});
		gui::widgets::itemTooltip(name.c_str(), nullptr, "Click to stamp this structure with the brush.");
		if (!row.clicked)
			continue;
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
	gui::widgets::endSection();
}

}// namespace owl::nest::panel
