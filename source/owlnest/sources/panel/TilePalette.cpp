/**
 * @file TilePalette.cpp
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "TilePalette.h"

#include "../DockLayout.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/ImageTile.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Tooltip.h>
#include <imgui.h>
#include <scene/TilemapAsset.h>
#include <scene/Tileset.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <tuple>

namespace owl::nest::panel {

namespace {

// Outline of a tile chosen by the left brush, the right one, or both.
auto highlightFor(const int32_t iTile, const int32_t iPrimary, const int32_t iSecondary) -> math::vec4 {
	const auto& theme = gui::UiLayer::getTheme();
	const bool primary = (iPrimary == iTile);
	const bool secondary = (iSecondary == iTile);
	if (primary && secondary)
		return gui::Theme::mix(theme.accent, theme.info, 0.5f);
	if (primary)
		return theme.accent;
	if (secondary)
		return theme.info;
	return {};
}

void toggleBrush(int32_t& ioBrush, const int32_t iTarget) {
	ioBrush = (ioBrush == iTarget) ? g_TileBrushPick : iTarget;
}

auto brushLabel(const int32_t iBrush) -> std::string {
	if (iBrush == g_TileBrushPick)
		return "Pick";
	if (iBrush == g_TileBrushEraser)
		return "Eraser";
	return std::format("Tile #{}", iBrush);
}

}// namespace

void TilePalette::onImGuiRender(scene::TilemapAsset* iTarget) {
	ImGui::Begin(panelWindow::title(gui::icon::grid3x3, "Tile Palette", panelWindow::tilePalette).c_str());
	if (iTarget == nullptr) {
		std::ignore =
				gui::widgets::emptyState(gui::icon::grid3x3, "No tilemap", "Open a .owltilemap document to paint.");
		ImGui::End();
		return;
	}
	auto& asset = *iTarget;
	if (!asset.tileset) {
		std::ignore = gui::widgets::emptyState(gui::icon::image, "No tileset",
											   "Drop a .owltileset on the Tileset field of the tilemap.");
		ImGui::End();
		return;
	}
	if (asset.layers.empty()) {
		std::ignore = gui::widgets::emptyState(gui::icon::layers, "No layer",
											   "Add a layer in the Hierarchy panel to paint on it.");
		ImGui::End();
		return;
	}
	renderBrushSection(asset);
	renderTileSection(asset);
	ImGui::End();
}

void TilePalette::renderBrushSection(const scene::TilemapAsset& iAsset) {
	if (!gui::widgets::sectionHeader("Brush", gui::icon::paintbrush, false).open)
		return;
	const auto& theme = gui::UiLayer::getTheme();
	if (m_selectedLayer >= iAsset.layers.size())
		m_selectedLayer = 0;
	if (gui::widgets::beginProperties("##tileBrush")) {
		const auto& currentLayerName = iAsset.layers[m_selectedLayer].name;
		gui::widgets::propertyLabel("Layer", "Layer the brushes paint on.");
		if (gui::widgets::beginCombo("##layer", currentLayerName.c_str())) {
			for (size_t i = 0; i < iAsset.layers.size(); ++i) {
				const bool selected = i == m_selectedLayer;
				const auto label = iAsset.layers[i].name.empty() ? std::format("layer{}", i) : iAsset.layers[i].name;
				if (ImGui::Selectable(label.c_str(), selected))
					m_selectedLayer = static_cast<uint32_t>(i);
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		std::ignore = gui::widgets::propertyReset(false);
		const auto brushRow = [&theme](const char* iLabel, const char* iTooltip, const int32_t iBrush,
									   const math::vec4& iColor) -> void {
			gui::widgets::propertyLabel(iLabel, iTooltip);
			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(gui::vec(iColor), "%s", gui::icon::circle);
			ImGui::SameLine();
			ImGui::TextColored(gui::vec(iBrush == g_TileBrushPick ? theme.textDisabled : theme.text), "%s",
							   brushLabel(iBrush).c_str());
			std::ignore = gui::widgets::propertyReset(false);
		};
		brushRow("Left Button", "Brush of the left mouse button: click a tile below to set it, again to clear it.",
				 m_primaryBrush, theme.accent);
		brushRow("Right Button", "Brush of the right mouse button: right-click a tile below to set it.",
				 m_secondaryBrush, theme.info);
		gui::widgets::endProperties();
	}
	const auto eraser = gui::widgets::imageTile(
			"##eraser", {.size = ImGui::GetFrameHeight(),
						 .outline = highlightFor(g_TileBrushEraser, m_primaryBrush, m_secondaryBrush)});
	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 glyph = ImGui::CalcTextSize(gui::icon::eraser);
	ImGui::GetWindowDrawList()->AddText({std::floor(min.x + (ImGui::GetFrameHeight() - glyph.x) * 0.5f),
										 std::floor(min.y + (ImGui::GetFrameHeight() - glyph.y) * 0.5f)},
										ImGui::GetColorU32(gui::vec(theme.text)), gui::icon::eraser);
	gui::widgets::itemTooltip("Eraser", nullptr, "Left-click for the left brush, right-click for the right one.");
	if (eraser.left)
		toggleBrush(m_primaryBrush, g_TileBrushEraser);
	if (eraser.right)
		toggleBrush(m_secondaryBrush, g_TileBrushEraser);
	gui::widgets::endSection();
}

void TilePalette::renderTileSection(const scene::TilemapAsset& iAsset) {
	if (!gui::widgets::sectionHeader("Tiles", gui::icon::grid3x3, false).open)
		return;
	const auto& tileset = *iAsset.tileset;
	const float side = std::round(ImGui::GetFrameHeight() * 1.4f);
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const auto perRow = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (side + spacing)));
	const bool loaded = tileset.texture && tileset.texture->isLoaded();
	for (uint32_t i = 0; i < tileset.tileCount(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		if (i % static_cast<uint32_t>(perRow) != 0)
			ImGui::SameLine();
		const auto uvs = tileset.getTileUv(i);
		const auto tileIdx = static_cast<int32_t>(i);
		const auto tile =
				gui::widgets::imageTile("##tile", {.textureId = loaded ? tileset.texture->getRendererId() : 0,
												   .uv0 = uvs[3],
												   .uv1 = uvs[1],
												   .size = side,
												   .outline = highlightFor(tileIdx, m_primaryBrush, m_secondaryBrush)});
		const auto& meta = tileset.getTileMeta(i);
		const auto title = meta.name.empty() ? std::format("Tile #{}", i) : std::format("#{} {}", i, meta.name);
		gui::widgets::itemTooltip(title.c_str(), nullptr, meta.collidable ? "Solid" : nullptr);
		if (tile.left) {
			toggleBrush(m_primaryBrush, tileIdx);
			m_inspectedTile = tileIdx;
		}
		if (tile.right)
			toggleBrush(m_secondaryBrush, tileIdx);
		ImGui::PopID();
	}
	gui::widgets::endSection();
}

}// namespace owl::nest::panel
