/**
 * @file TilePalette.cpp
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "TilePalette.h"

#include <gui/IconBank.h>
#include <gui/utils.h>
#include <imgui.h>
#include <scene/TilemapAsset.h>
#include <scene/Tileset.h>

namespace owl::nest::panel {

namespace {
constexpr ImU32 k_PrimaryHighlight = IM_COL32(255, 195, 38, 255);// editor accent
constexpr ImU32 k_SecondaryHighlight = IM_COL32(120, 180, 255, 255);// blue, distinct from primary
constexpr ImU32 k_BothHighlight = IM_COL32(220, 90, 220, 255);// magenta, used when both brushes share a tile

auto highlightFor(const int32_t iTile, const int32_t iPrimary, const int32_t iSecondary) -> ImU32 {
	const bool primary = (iPrimary == iTile);
	const bool secondary = (iSecondary == iTile);
	if (primary && secondary)
		return k_BothHighlight;
	if (primary)
		return k_PrimaryHighlight;
	if (secondary)
		return k_SecondaryHighlight;
	return 0;
}

auto tileButton(const char* iId, const shared<renderer::gpu::Texture2D>& iTex, const std::array<math::vec2, 4>& iUvs,
				const ImVec2 iSize, const ImU32 iHighlight) -> std::pair<bool, bool> {
	if (iHighlight != 0) {
		ImGui::PushStyleColor(ImGuiCol_Border, iHighlight);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.f);
	}
	const ImVec2 uv0 = gui::vec(iUvs[3]);// top-left
	const ImVec2 uv1 = gui::vec(iUvs[1]);// bottom-right
	bool leftClicked = false;
	bool rightClicked = false;
	if (iTex && iTex->isLoaded()) {
		ImGui::ImageButton(iId, static_cast<ImTextureID>(iTex->getRendererId()), iSize, uv0, uv1);
	} else {
		ImGui::Button(iId, iSize);
	}
	if (ImGui::IsItemHovered()) {
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			leftClicked = true;
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			rightClicked = true;
	}
	if (iHighlight != 0) {
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}
	return {leftClicked, rightClicked};
}

void toggleBrush(int32_t& ioBrush, const int32_t iTarget) {
	ioBrush = (ioBrush == iTarget) ? g_TileBrushPick : iTarget;
}

auto brushLabel(const int32_t iBrush) -> const char* {
	if (iBrush == g_TileBrushPick)
		return "Pick";
	if (iBrush == g_TileBrushEraser)
		return "Eraser";
	return "Tile";
}

}// namespace

void TilePalette::onImGuiRender(scene::TilemapAsset* iTarget) {
	ImGui::Begin("Tile Palette");
	if (iTarget == nullptr) {
		ImGui::TextDisabled("Open a .owltilemap document to start painting.");
		ImGui::End();
		return;
	}
	auto& asset = *iTarget;
	if (!asset.tileset) {
		ImGui::TextDisabled("Drop a .owltileset on the asset's Tileset slot to populate.");
		ImGui::End();
		return;
	}

	// --- Active layer dropdown -------------------------------------------------------------
	if (asset.layers.empty()) {
		ImGui::TextDisabled("Add at least one layer in the Scene Hierarchy panel.");
		ImGui::End();
		return;
	}
	if (m_selectedLayer >= asset.layers.size())
		m_selectedLayer = 0;
	const auto& currentLayerName = asset.layers[m_selectedLayer].name;
	if (ImGui::BeginCombo("Layer", currentLayerName.c_str())) {
		for (size_t i = 0; i < asset.layers.size(); ++i) {
			const bool selected = i == m_selectedLayer;
			const auto label = asset.layers[i].name.empty() ? std::format("layer{}", i) : asset.layers[i].name;
			if (ImGui::Selectable(label.c_str(), selected))
				m_selectedLayer = static_cast<uint32_t>(i);
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	// --- Brush summary ---------------------------------------------------------------------
	ImGui::Separator();
	const auto* primaryName = brushLabel(m_primaryBrush);
	const auto* secondaryName = brushLabel(m_secondaryBrush);
	ImGui::TextColored({1.f, 0.76f, 0.15f, 1.f}, "L: %s", primaryName);
	if (m_primaryBrush >= 0)
		ImGui::SameLine(), ImGui::TextDisabled("#%d", m_primaryBrush);
	ImGui::TextColored({0.47f, 0.71f, 1.f, 1.f}, "R: %s", secondaryName);
	if (m_secondaryBrush >= 0)
		ImGui::SameLine(), ImGui::TextDisabled("#%d", m_secondaryBrush);
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::SetWindowFontScale(0.85f);
	ImGui::TextWrapped("Click on the atlas to assign — left mouse for L, right for R. Re-click to clear.");
	ImGui::SetWindowFontScale(1.0f);
	ImGui::PopStyleColor();

	// --- Eraser button ---------------------------------------------------------------------
	auto& iconBank = gui::IconBank::instance();
	const auto eraserIcon = iconBank.getIcon("eraser");
	const ImU32 eraserHighlight = highlightFor(g_TileBrushEraser, m_primaryBrush, m_secondaryBrush);
	if (eraserHighlight != 0) {
		ImGui::PushStyleColor(ImGuiCol_Border, eraserHighlight);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.f);
	}
	if (eraserIcon.has_value()) {
		ImGui::ImageButton("##eraser", static_cast<ImTextureID>(eraserIcon->textureId), ImVec2{32.f, 32.f},
						   gui::vec(eraserIcon->uv0), gui::vec(eraserIcon->uv1));
	} else {
		ImGui::Button("Erase##eraser", ImVec2{80.f, 32.f});
	}
	if (ImGui::IsItemHovered()) {
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			toggleBrush(m_primaryBrush, g_TileBrushEraser);
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			toggleBrush(m_secondaryBrush, g_TileBrushEraser);
		ImGui::SetTooltip("Eraser. Left-click to set as primary brush, right-click for secondary.");
	}
	if (eraserHighlight != 0) {
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}

	// --- Tile grid ------------------------------------------------------------------------
	ImGui::Separator();
	const auto& tileset = *asset.tileset;
	constexpr float kTileButtonSize = 32.f;
	const float avail = ImGui::GetContentRegionAvail().x;
	const float framePadX = ImGui::GetStyle().FramePadding.x * 2.f;
	const float itemSpacingX = ImGui::GetStyle().ItemSpacing.x;
	const float stride = kTileButtonSize + framePadX + itemSpacingX;
	const auto perRow = std::max(1u, static_cast<uint32_t>(avail / stride));
	for (uint32_t i = 0; i < tileset.tileCount(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		const auto uvs = tileset.getTileUv(i);
		const auto tileIdx = static_cast<int32_t>(i);
		const auto highlight = highlightFor(tileIdx, m_primaryBrush, m_secondaryBrush);
		const auto [left, right] =
				tileButton("##tile", tileset.texture, uvs, ImVec2(kTileButtonSize, kTileButtonSize), highlight);
		if (left) {
			toggleBrush(m_primaryBrush, tileIdx);
			m_inspectedTile = tileIdx;
		}
		if (right)
			toggleBrush(m_secondaryBrush, tileIdx);
		if (ImGui::IsItemHovered()) {
			const auto& meta = tileset.getTileMeta(i);
			if (!meta.name.empty())
				ImGui::SetTooltip("#%u %s%s", i, meta.name.c_str(), meta.collidable ? " (solid)" : "");
			else
				ImGui::SetTooltip("#%u%s", i, meta.collidable ? " (solid)" : "");
		}
		ImGui::PopID();
		if ((i + 1) % perRow != 0 && (i + 1) < tileset.tileCount())
			ImGui::SameLine();
	}

	ImGui::End();
}

}// namespace owl::nest::panel
