/**
 * @file TilePalette.cpp
 * @author Silmaen
 * @date 02/05/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "TilePalette.h"

#include <gui/utils.h>
#include <imgui.h>
#include <scene/Tileset.h>
#include <scene/component/Tilemap.h>

namespace owl::nest::panel {

namespace {

/// Render a single tile button at the given size, returning true on click.
auto tileButton(const char* iId, const shared<renderer::gpu::Texture2D>& iTex, const std::array<math::vec2, 4>& iUvs,
				const ImVec2 iSize, const bool iSelected) -> bool {
	if (iSelected) {
		ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 195, 38, 255));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.f);
	}
	const ImVec2 uv0 = gui::vec(iUvs[3]);// top-left
	const ImVec2 uv1 = gui::vec(iUvs[1]);// bottom-right
	bool clicked = false;
	if (iTex && iTex->isLoaded()) {
		clicked = ImGui::ImageButton(iId, static_cast<ImTextureID>(iTex->getRendererId()), iSize, uv0, uv1);
	} else {
		clicked = ImGui::Button(iId, iSize);
	}
	if (iSelected) {
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}
	return clicked;
}

}// namespace

auto TilePalette::isPaintActive() const -> bool {
	if (!m_selection)
		return false;
	if (!m_selection.hasComponent<scene::component::Tilemap>())
		return false;
	const auto& tilemap = m_selection.getComponent<scene::component::Tilemap>();
	if (!tilemap.tileset || !tilemap.tileset->texture)
		return false;
	if (m_selectedLayer >= tilemap.layers.size())
		return false;
	return true;// `-1` (eraser) is also a valid brush
}

void TilePalette::onImGuiRender() {
	ImGui::Begin("Tile Palette");
	if (!m_selection || !m_selection.hasComponent<scene::component::Tilemap>()) {
		ImGui::TextDisabled("Select an entity with a Tilemap component to paint.");
		ImGui::End();
		return;
	}
	auto& tilemap = m_selection.getComponent<scene::component::Tilemap>();
	if (!tilemap.tileset) {
		ImGui::TextDisabled("Drop a .owltileset on the Tilemap inspector to populate.");
		ImGui::End();
		return;
	}

	// --- Active layer dropdown -------------------------------------------------------------
	if (tilemap.layers.empty()) {
		ImGui::TextDisabled("Add at least one layer to start painting.");
		ImGui::End();
		return;
	}
	if (m_selectedLayer >= tilemap.layers.size())
		m_selectedLayer = 0;
	const auto& currentLayerName = tilemap.layers[m_selectedLayer].name;
	if (ImGui::BeginCombo("Layer", currentLayerName.c_str())) {
		for (size_t i = 0; i < tilemap.layers.size(); ++i) {
			const bool selected = i == m_selectedLayer;
			const auto label = tilemap.layers[i].name.empty() ? std::format("layer{}", i) : tilemap.layers[i].name;
			if (ImGui::Selectable(label.c_str(), selected))
				m_selectedLayer = static_cast<uint32_t>(i);
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	// --- Eraser button --------------------------------------------------------------------
	const bool eraserSelected = m_selectedTile < 0;
	if (eraserSelected) {
		ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 195, 38, 200));
	}
	if (ImGui::Button("Eraser", ImVec2(120.f, 0.f)))
		m_selectedTile = -1;
	if (eraserSelected)
		ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::TextDisabled("(left-click to paint, right-click also erases)");

	// --- Tile grid ------------------------------------------------------------------------
	ImGui::Separator();
	const auto& tileset = *tilemap.tileset;
	constexpr float kTileButtonSize = 32.f;
	const float avail = ImGui::GetContentRegionAvail().x;
	const auto perRow = static_cast<uint32_t>(std::max(1.f, std::floor(avail / (kTileButtonSize + 8.f))));
	uint32_t shown = 0;
	for (uint32_t i = 0; i < tileset.tileCount(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		const auto uvs = tileset.getTileUv(i);
		const bool selected = (m_selectedTile == static_cast<int32_t>(i));
		if (tileButton("##tile", tileset.texture, uvs, ImVec2(kTileButtonSize, kTileButtonSize), selected))
			m_selectedTile = static_cast<int32_t>(i);
		if (ImGui::IsItemHovered()) {
			const auto& meta = tileset.getTileMeta(i);
			if (!meta.name.empty())
				ImGui::SetTooltip("#%u %s%s", i, meta.name.c_str(), meta.collidable ? " (solid)" : "");
			else
				ImGui::SetTooltip("#%u%s", i, meta.collidable ? " (solid)" : "");
		}
		ImGui::PopID();
		++shown;
		if (shown % perRow != 0)
			ImGui::SameLine();
	}

	ImGui::End();
}

}// namespace owl::nest::panel
