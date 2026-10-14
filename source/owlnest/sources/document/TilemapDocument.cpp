/**
 * @file TilemapDocument.cpp
 * @author Silmaen
 * @date 08/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "TilemapDocument.h"

#include "EditorLayer.h"

#include "../commands/TilemapCommands.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/component/render.h>
#include <gui/utils.h>
#include <gui/widgets/AssetField.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <imgui_stdlib.h>
#include <renderer/RendererIsometric.h>
#include <scene/TilemapTerrain.h>
#include <scene/Tileset.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <tuple>
#include <utility>

namespace owl::nest {

namespace {
constexpr float g_minZoom = 4.f;
constexpr float g_maxZoom = 128.f;
constexpr float g_zoomWheelStep = 1.15f;

template<typename Fn>
void pushTilemapEdit(scene::TilemapAsset& ioAsset, TilemapUndoManager& ioUndo, const std::string& iDescription,
					 Fn&& iMutator) {
	scene::TilemapAsset before = ioAsset;
	std::forward<Fn>(iMutator)();
	if (!before.hasSameContent(ioAsset))
		ioUndo.push(mkUniq<commands::ModifyTilemapLayoutCommand>(std::move(before), ioAsset, iDescription));
}

}// namespace

TilemapDocument::TilemapDocument() = default;

TilemapDocument::~TilemapDocument() = default;

auto TilemapDocument::title() const -> std::string {
	if (!m_path.empty())
		return m_path.stem().string();
	return "Untitled";
}

auto TilemapDocument::isDirty() const -> bool { return !m_asset.hasSameContent(m_savedAsset); }

auto TilemapDocument::recoverySnapshot() const -> std::optional<std::string> {
	return m_asset.serializeToString(m_path.empty() ? std::string{"untitled"} : m_path.stem().string());
}

auto TilemapDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	const auto tileset = m_asset.tileset;
	if (!m_asset.deserializeFromString(iSnapshot)) {
		OWL_WARN("Recovery: Cannot restore the autosave of tilemap '{}'.", title())
		return false;
	}
	m_asset.tileset = tileset;
	return true;
}

void TilemapDocument::onAttach(EditorLayer* iEditor) {
	mp_editorLayer = iEditor;
	resolveTileset();
	refreshSavedSnapshot();
}

void TilemapDocument::onDetach() { mp_editorLayer = nullptr; }

void TilemapDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {
	// Nothing time-stepped to advance — paint follows ImGui mouse events directly.
}

void TilemapDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {
	// ImGui consumes its own events; nothing to dispatch at the document level.
}

auto TilemapDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto TilemapDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	const auto displayName = iPath.stem().string();
	if (!m_asset.saveToFile(iPath, displayName)) {
		OWL_CORE_ERROR("TilemapDocument: failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	refreshSavedSnapshot();
	if (mp_editorLayer != nullptr)
		mp_editorLayer->onTilemapSaved(iPath);
	return true;
}

auto TilemapDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	if (!m_asset.loadFromFile(iPath)) {
		OWL_CORE_ERROR("TilemapDocument: failed to load '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	resolveTileset();
	refreshSavedSnapshot();
	return true;
}

void TilemapDocument::onImGuiRender() {
	const auto display =
			(mp_editorLayer != nullptr) ? mp_editorLayer->getDocumentManager().displayTitleFor(this) : title();
	const auto winTitle = documentWindowTitle(DocumentType::Tilemap, display, static_cast<uint64_t>(id()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;

	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);

	const bool wantFocus = consumeFocusRequest();
	if (wantFocus)
		ImGui::SetNextWindowFocus();
	const bool open = ImGui::Begin(winTitle.c_str(), &m_pOpen, flags);
	if (mp_editorLayer != nullptr)
		mp_editorLayer->renderDocumentTabMenu(*this);
	if (wantFocus) {
		ImGui::SetWindowFocus();
		if (mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
	}
	if (open) {
		const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (windowFocused && !m_wasFocused && mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
		m_wasFocused = windowFocused;
		if (windowFocused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
			std::ignore = save();

		renderCanvas();
	}
	ImGui::End();

	// Only the active tilemap owns the shared "Tile Palette" window, so it leaves with the last tilemap tab.
	if (mp_editorLayer != nullptr && mp_editorLayer->getDocumentManager().getActive() == this)
		m_palette.onImGuiRender(&m_asset);
}

void TilemapDocument::renderHierarchyPanel() {
	if (gui::widgets::sectionHeader("Grid", gui::icon::grid3x3, false).open) {
		renderGridSection();
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Layers", gui::icon::layers, false).open) {
		renderLayerSection();
		gui::widgets::endSection();
	}
}

void TilemapDocument::renderGridSection() {
	if (!gui::widgets::beginProperties("##tilemapGrid"))
		return;
	// Tileset: its path (a drop target), then Open in the Tileset Editor and Clear.
	gui::widgets::propertyLabel("Tileset", "Drop a .owltileset from the Content Browser.");
	const float side = ImGui::GetFrameHeight();
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	std::string shown = m_asset.tilesetPath.generic_string();
	ImGui::SetNextItemWidth(std::max(side, ImGui::GetContentRegionAvail().x - (side + spacing) * 2.f));
	ImGui::InputTextWithHint("##tileset", "drop a .owltileset", &shown, ImGuiInputTextFlags_ReadOnly);
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const auto* data = static_cast<const char*>(payload->Data);
			const auto length = ::strnlen(data, static_cast<size_t>(payload->DataSize));
			const std::filesystem::path candidate{std::string(data, length)};
			if (gui::widgets::isPathOfKind(candidate, gui::widgets::AssetKind::Tileset)) {
				pushTilemapEdit(m_asset, m_undoManager, "Set tileset", [this, &candidate]() -> void {
					if (m_asset.tileset)
						gui::UiLayer::deferTextureRelease(m_asset.tileset->texture);
					m_asset.tilesetPath = candidate;
					m_asset.tileset.reset();
					resolveTileset();
				});
			}
		}
		ImGui::EndDragDropTarget();
	}
	ImGui::SameLine();
	if (gui::widgets::iconButton("##openTileset", {.glyph = gui::icon::externalLink,
												   .tooltip = "Open",
												   .description = "Edit the tileset in the Tileset Editor.",
												   .disabled = m_asset.tilesetPath.empty()}))
		std::ignore = gui::component::openAsset(m_asset.tilesetPath);
	ImGui::SameLine();
	if (gui::widgets::iconButton("##clearTileset",
								 {.glyph = gui::icon::x, .tooltip = "Clear", .disabled = m_asset.tilesetPath.empty()}))
		pushTilemapEdit(m_asset, m_undoManager, "Clear tileset", [this]() -> void {
			if (m_asset.tileset)
				gui::UiLayer::deferTextureRelease(m_asset.tileset->texture);
			m_asset.tilesetPath.clear();
			m_asset.tileset.reset();
		});
	std::ignore = gui::widgets::propertyReset(false);

	// Size: previewed live while dragging, one undo step when the drag ends.
	int width = static_cast<int>(m_asset.width);
	int height = static_cast<int>(m_asset.height);
	gui::widgets::propertyLabel("Width", "Columns of cells; the content is kept when the grid grows or shrinks.");
	const bool widthChanged = ImGui::DragInt("##width", &width, 1.f, 1, 1024, "%d cells");
	const bool widthCommitted = ImGui::IsItemDeactivatedAfterEdit();
	std::ignore = gui::widgets::propertyReset(false);
	gui::widgets::propertyLabel("Height", "Rows of cells.");
	const bool heightChanged = ImGui::DragInt("##height", &height, 1.f, 1, 1024, "%d cells");
	const bool heightCommitted = ImGui::IsItemDeactivatedAfterEdit();
	std::ignore = gui::widgets::propertyReset(false);
	if (widthChanged || heightChanged) {
		// The preview always resizes the grid of the drag start, so shrinking then growing back loses nothing.
		if (!m_resizeOrigin)
			m_resizeOrigin = m_asset;
		m_asset = *m_resizeOrigin;
		m_asset.resize(static_cast<uint32_t>(std::max(1, width)), static_cast<uint32_t>(std::max(1, height)));
	}
	if ((widthCommitted || heightCommitted) && m_resizeOrigin) {
		if (!m_resizeOrigin->hasSameContent(m_asset))
			m_undoManager.push(
					mkUniq<commands::ModifyTilemapLayoutCommand>(std::move(*m_resizeOrigin), m_asset, "Resize grid"));
		m_resizeOrigin.reset();
	}
	if (gui::widgets::propertyFloat("Cell Size", m_asset.cellSize, 1.f, 0.05f, "%.3f",
									"Side of a cell in world units."))
		m_asset.cellSize = std::max(0.0001f, m_asset.cellSize);
	gui::widgets::endProperties();
}

void TilemapDocument::renderLayerSection() {
	if (gui::widgets::button("addLayer", {.glyph = gui::icon::plus, .label = "Add Layer"}))
		pushTilemapEdit(m_asset, m_undoManager, "Add layer",
						[this]() -> void { m_asset.addLayer(std::format("layer{}", m_asset.layers.size())); });
	int eraseIndex = -1;
	int moveUpIndex = -1;
	int moveDownIndex = -1;
	const float side = ImGui::GetFrameHeight();
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	for (size_t i = 0; i < m_asset.layers.size(); ++i) {
		auto& layer = m_asset.layers[i];
		ImGui::PushID(static_cast<int>(i));
		const bool isActive = (m_palette.getSelectedLayer() == static_cast<uint32_t>(i));
		if (gui::widgets::iconButton("##active", {.glyph = isActive ? gui::icon::paintbrush : gui::icon::circle,
												  .tooltip = "Paint on this layer",
												  .selected = isActive}))
			m_palette.setSelectedLayer(static_cast<uint32_t>(i));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(std::max(side, ImGui::GetContentRegionAvail().x - (side + spacing) * 4.f));
		if (ImGui::InputTextWithHint("##name", "Layer name", &layer.name, ImGuiInputTextFlags_EnterReturnsTrue))
			pushTilemapEdit(m_asset, m_undoManager, "Rename layer", []() -> void {});
		ImGui::SameLine();
		if (gui::widgets::iconButton("##visible", {.glyph = layer.visible ? gui::icon::eye : gui::icon::eyeOff,
												   .tooltip = layer.visible ? "Hide the layer" : "Show the layer"}))
			pushTilemapEdit(m_asset, m_undoManager, "Toggle layer visibility",
							[&layer]() -> void { layer.visible = !layer.visible; });
		ImGui::SameLine();
		if (gui::widgets::iconButton("##up", {.glyph = gui::icon::chevronUp, .tooltip = "Move up", .disabled = i == 0}))
			moveUpIndex = static_cast<int>(i);
		ImGui::SameLine();
		if (gui::widgets::iconButton("##down", {.glyph = gui::icon::chevronDown,
												.tooltip = "Move down",
												.disabled = i + 1 >= m_asset.layers.size()}))
			moveDownIndex = static_cast<int>(i);
		ImGui::SameLine();
		if (gui::widgets::iconButton("##delete", {.glyph = gui::icon::trash2, .tooltip = "Delete the layer"}))
			eraseIndex = static_cast<int>(i);
		if (gui::widgets::beginProperties("##layerProps")) {
			gui::widgets::propertyLabel("Parallax", "Scroll factor of the layer against the camera, per axis.");
			std::array parallax{layer.parallax.x(), layer.parallax.y()};
			if (gui::widgets::vectorField("##parallax", parallax, 0.01f))
				layer.parallax = math::vec2{std::clamp(parallax[0], 0.f, 4.f), std::clamp(parallax[1], 0.f, 4.f)};
			std::ignore = gui::widgets::propertyReset(false);
			gui::widgets::endProperties();
		}
		ImGui::PopID();
	}
	if (eraseIndex >= 0) {
		pushTilemapEdit(m_asset, m_undoManager, "Delete layer", [this, eraseIndex]() -> void {
			const auto idx = static_cast<size_t>(eraseIndex);
			m_asset.layers.erase(m_asset.layers.begin() + static_cast<ptrdiff_t>(idx));
		});
	}
	if (moveUpIndex > 0) {
		pushTilemapEdit(m_asset, m_undoManager, "Move layer up", [this, moveUpIndex]() -> void {
			const auto idx = static_cast<size_t>(moveUpIndex);
			std::swap(m_asset.layers[idx], m_asset.layers[idx - 1]);
		});
	}
	if (moveDownIndex >= 0) {
		pushTilemapEdit(m_asset, m_undoManager, "Move layer down", [this, moveDownIndex]() -> void {
			const auto idx = static_cast<size_t>(moveDownIndex);
			std::swap(m_asset.layers[idx], m_asset.layers[idx + 1]);
		});
	}
}

void TilemapDocument::renderPropertiesPanel() {
	if (m_inspectedCellX < 0 || m_inspectedCellY < 0) {
		std::ignore = gui::widgets::emptyState(gui::icon::mousePointerClick, "No cell selected",
											   "Click a cell on the canvas to inspect it.");
		return;
	}
	const auto layerIdx = m_palette.getSelectedLayer();
	if (layerIdx >= m_asset.layers.size()) {
		std::ignore = gui::widgets::emptyState(gui::icon::layers, "No layer", "Add a layer in the Layers section.");
		return;
	}
	const auto cellX = static_cast<uint32_t>(m_inspectedCellX);
	const auto cellY = static_cast<uint32_t>(m_inspectedCellY);
	const int32_t tileAtCell = m_asset.getTile(layerIdx, cellX, cellY);
	const auto& theme = gui::UiLayer::getTheme();
	if (!gui::widgets::sectionHeader("Cell", gui::icon::squareDashed, false).open)
		return;
	if (gui::widgets::beginProperties("##cell")) {
		const auto textRow = [](const char* iLabel, const std::string& iValue, const math::vec4& iColor) -> void {
			gui::widgets::propertyLabel(iLabel);
			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(gui::vec(iColor), "%s", iValue.c_str());
			std::ignore = gui::widgets::propertyReset(false);
		};
		textRow("Position", std::format("{}, {}", m_inspectedCellX, m_inspectedCellY), theme.text);
		textRow("Layer", std::format("#{} {}", layerIdx, m_asset.layers[layerIdx].name), theme.text);
		bool passable = true;
		std::string tileLabel = "Empty";
		if (tileAtCell >= 0) {
			tileLabel = std::format("#{}", tileAtCell);
			if (m_asset.tileset && static_cast<uint32_t>(tileAtCell) < m_asset.tileset->tileCount()) {
				const auto& meta = m_asset.tileset->tiles[static_cast<size_t>(tileAtCell)];
				if (!meta.name.empty())
					tileLabel = std::format("#{} {}", tileAtCell, meta.name);
				passable = !meta.collidable;
			}
		}
		textRow("Tile", tileLabel, tileAtCell >= 0 ? theme.text : theme.textDisabled);
		textRow("Passable", passable ? "Yes" : "No (solid)", passable ? theme.success : theme.error);
		if (isIsometric()) {
			const auto corners = m_asset.getCellCorners(cellX, cellY);
			textRow("Slope", std::string{scene::terrain::getSlopeName(m_asset.getCellSlope(cellX, cellY))}, theme.text);
			textRow("Corners", std::format("{} {} {} {}", corners[0], corners[1], corners[2], corners[3]), theme.text);
		}
		renderCellDataRows(layerIdx, cellX, cellY);
		gui::widgets::endProperties();
	}
	gui::widgets::notice("The name and the collision of a tile belong to the tileset: open it from the Grid section.",
						 gui::widgets::BadgeKind::Info);
	gui::widgets::endSection();
}

void TilemapDocument::renderCellDataRows(const uint32_t iLayer, const uint32_t iX, const uint32_t iY) {
	auto cell = m_asset.layers[iLayer].getCell(iX, iY);
	int overlay = cell.overlay;
	int flags = cell.flags;
	bool changed = gui::widgets::propertyInt("Overlay", overlay, scene::component::g_EmptyTileIndex,
											 scene::component::g_EmptyTileIndex, scene::component::g_MaxTileIndex, "%d",
											 "Tile drawn over the terrain (-1 for none): the isometric tracks "
											 "and roads, drawn along the slope.");
	changed |= gui::widgets::propertyInt("Flags", flags, 0, 0, 255, "0x%02X",
										 "Eight bits of the cell, saved with the map: bit 0 asks for the "
										 "foundation of the terrain tile, the others are free for games.");
	if (changed) {
		cell.overlay = overlay;
		cell.flags = static_cast<uint8_t>(std::clamp(flags, 0, 255));
		std::ignore = m_stroke.setCell(m_asset, iLayer, iX, iY, cell);
	}
	if (!m_strokeInProgress && !ImGui::IsAnyItemActive())
		endStroke("Edit cell");
}

auto TilemapDocument::isIsometric() const -> bool {
	return m_asset.tileset && m_asset.tileset->kind == scene::Tileset::Kind::Isometric;
}

void TilemapDocument::renderCanvas() {
	if (!m_asset.tileset || (!m_asset.tileset->texture && !isIsometric())) {
		ImGui::TextDisabled("Drop a .owltileset on the asset to start authoring.");
		return;
	}
	if (m_asset.layers.empty()) {
		ImGui::TextDisabled("Add at least one layer in the Scene Hierarchy panel.");
		return;
	}
	if (isIsometric()) {
		renderIsometricCanvas();
		return;
	}

	const auto availSize = ImGui::GetContentRegionAvail();
	if (availSize.x <= 0.f || availSize.y <= 0.f)
		return;

	const auto canvasOrigin = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize{availSize.x, availSize.y};
	auto* drawList = ImGui::GetWindowDrawList();

	ImGui::InvisibleButton("##canvas", canvasSize,
						   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
								   ImGuiButtonFlags_MouseButtonMiddle);
	handleCanvasDrop();
	const bool hovered = ImGui::IsItemHovered();
	handleCanvasZoomPan(canvasOrigin, hovered);

	const float cellPx = m_zoomPixelsPerCell;
	const auto gridW = static_cast<float>(m_asset.width);
	const auto gridH = static_cast<float>(m_asset.height);
	const ImVec2 gridTopLeft{canvasOrigin.x + m_panOffset.x(), canvasOrigin.y + m_panOffset.y()};
	const ImVec2 gridBottomRight{gridTopLeft.x + gridW * cellPx, gridTopLeft.y + gridH * cellPx};

	drawList->PushClipRect(canvasOrigin, ImVec2{canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y}, true);
	drawList->AddRectFilled(gridTopLeft, gridBottomRight, IM_COL32(40, 40, 50, 255));

	drawCanvasTiles(drawList, gridTopLeft, cellPx);

	// Grid overlay on top of the tiles.
	const ImU32 gridColor = IM_COL32(120, 120, 140, 80);
	for (uint32_t x = 0; x <= m_asset.width; ++x) {
		const float gx = gridTopLeft.x + static_cast<float>(x) * cellPx;
		drawList->AddLine({gx, gridTopLeft.y}, {gx, gridBottomRight.y}, gridColor);
	}
	for (uint32_t y = 0; y <= m_asset.height; ++y) {
		const float gy = gridTopLeft.y + static_cast<float>(y) * cellPx;
		drawList->AddLine({gridTopLeft.x, gy}, {gridBottomRight.x, gy}, gridColor);
	}

	// Persistent selection highlight (independent of hover).
	if (m_inspectedCellX >= 0 && m_inspectedCellY >= 0 && m_inspectedCellX < static_cast<int32_t>(m_asset.width) &&
		m_inspectedCellY < static_cast<int32_t>(m_asset.height)) {
		const ImVec2 selMin{gridTopLeft.x + static_cast<float>(m_inspectedCellX) * cellPx,
							gridTopLeft.y + static_cast<float>(m_inspectedCellY) * cellPx};
		const ImVec2 selMax{selMin.x + cellPx, selMin.y + cellPx};
		drawList->AddRect(selMin, selMax, IM_COL32(120, 220, 255, 220), 0.f, 3.f);
	}

	if (hovered)
		handleCanvasHoverAndPaint(drawList, gridTopLeft, ImVec2{gridW * cellPx, gridH * cellPx}, cellPx);
	else if (m_strokeInProgress && !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
			 !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		endStroke("Paint tiles");
		m_strokeInProgress = false;
	}

	drawList->PopClipRect();
}

void TilemapDocument::handleCanvasDrop() {
	if (!ImGui::BeginDragDropTarget())
		return;
	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
		const auto* data = static_cast<const char*>(payload->Data);
		const auto length = ::strnlen(data, static_cast<size_t>(payload->DataSize));
		const std::filesystem::path candidate{std::string(data, length)};
		if (mp_editorLayer != nullptr && app::Application::instanced()) {
			std::filesystem::path resolved;
			for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
				if (const auto p = assetsPath / candidate; exists(p)) {
					resolved = p;
					break;
				}
			}
			if (!resolved.empty())
				mp_editorLayer->requestDeferredOpen(resolved);
		}
	}
	ImGui::EndDragDropTarget();
}

void TilemapDocument::handleCanvasZoomPan(const ImVec2& iCanvasOrigin, const bool iHovered) {
	const auto& io = ImGui::GetIO();
	if (iHovered && io.MouseWheel != 0.f) {
		const float factor = io.MouseWheel > 0.f ? g_zoomWheelStep : 1.f / g_zoomWheelStep;
		const float oldZoom = m_zoomPixelsPerCell;
		m_zoomPixelsPerCell = std::clamp(m_zoomPixelsPerCell * factor, g_minZoom, g_maxZoom);
		const float zoomRatio = m_zoomPixelsPerCell / oldZoom;
		const ImVec2 cursorRel{io.MousePos.x - iCanvasOrigin.x - m_panOffset.x(),
							   io.MousePos.y - iCanvasOrigin.y - m_panOffset.y()};
		m_panOffset.x() += cursorRel.x - cursorRel.x * zoomRatio;
		m_panOffset.y() += cursorRel.y - cursorRel.y * zoomRatio;
	}
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
		m_panOffset.x() += io.MouseDelta.x;
		m_panOffset.y() += io.MouseDelta.y;
	}
}

void TilemapDocument::drawCanvasTiles(ImDrawList* iDrawList, const ImVec2& iGridTopLeft, const float iCellPx) const {
	const auto& tileset = *m_asset.tileset;
	const auto* tex = tileset.texture.get();
	for (const auto& layer: m_asset.layers) {
		if (!layer.visible)
			continue;
		for (uint32_t y = 0; y < m_asset.height; ++y) {
			for (uint32_t x = 0; x < m_asset.width; ++x) {
				const int32_t tileIdx = layer.getTile(x, y);
				if (tileIdx < 0)
					continue;
				const auto uvs = tileset.getTileUv(static_cast<uint32_t>(tileIdx));
				const ImVec2 cellMin{iGridTopLeft.x + static_cast<float>(x) * iCellPx,
									 iGridTopLeft.y + static_cast<float>(y) * iCellPx};
				const ImVec2 cellMax{cellMin.x + iCellPx, cellMin.y + iCellPx};
				if (tex != nullptr) {
					iDrawList->AddImageQuad(static_cast<ImTextureID>(tex->getRendererId()),
											ImVec2{cellMin.x, cellMin.y}, ImVec2{cellMax.x, cellMin.y},
											ImVec2{cellMax.x, cellMax.y}, ImVec2{cellMin.x, cellMax.y},
											gui::vec(uvs[3]), gui::vec(uvs[2]), gui::vec(uvs[1]), gui::vec(uvs[0]));
				} else {
					iDrawList->AddRectFilled(cellMin, cellMax, IM_COL32(150, 150, 200, 255));
				}
			}
		}
	}
}

void TilemapDocument::handleCanvasHoverAndPaint(ImDrawList* iDrawList, const ImVec2& iGridTopLeft,
												const ImVec2& iGridSize, const float iCellPx) {
	const auto& io = ImGui::GetIO();
	const float relX = io.MousePos.x - iGridTopLeft.x;
	const float relY = io.MousePos.y - iGridTopLeft.y;
	if (relX < 0.f || relY < 0.f || relX >= iGridSize.x || relY >= iGridSize.y)
		return;

	const auto cellX = static_cast<uint32_t>(relX / iCellPx);
	const auto cellY = static_cast<uint32_t>(relY / iCellPx);
	const ImVec2 hoverMin{iGridTopLeft.x + static_cast<float>(cellX) * iCellPx,
						  iGridTopLeft.y + static_cast<float>(cellY) * iCellPx};
	const ImVec2 hoverMax{hoverMin.x + iCellPx, hoverMin.y + iCellPx};
	iDrawList->AddRect(hoverMin, hoverMax, IM_COL32(255, 195, 38, 255), 0.f, 2.f);

	const bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	const bool rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		m_inspectedCellX = static_cast<int32_t>(cellX);
		m_inspectedCellY = static_cast<int32_t>(cellY);
	}

	if (!leftDown && !rightDown) {
		if (m_strokeInProgress) {
			endStroke("Paint tiles");
			m_strokeInProgress = false;
		}
		return;
	}
	applyBrush(cellX, cellY, leftDown, rightDown);
}

void TilemapDocument::applyBrush(const uint32_t iCellX, const uint32_t iCellY, const bool iLeftDown,
								 const bool iRightDown) {
	const uint32_t layerIdx = m_palette.getSelectedLayer();
	if (layerIdx >= m_asset.layers.size())
		return;
	int32_t brush = panel::g_TileBrushPick;
	if (iLeftDown && m_palette.getPrimaryBrush() != panel::g_TileBrushPick)
		brush = m_palette.getPrimaryBrush();
	else if (iRightDown && m_palette.getSecondaryBrush() != panel::g_TileBrushPick)
		brush = m_palette.getSecondaryBrush();
	if (brush == panel::g_TileBrushPick)
		return;
	m_strokeInProgress = true;
	const int32_t value = (brush == panel::g_TileBrushEraser) ? scene::component::g_EmptyTileIndex : brush;
	std::ignore = m_stroke.setTile(m_asset, layerIdx, iCellX, iCellY, value);
}

namespace {

// Screen projection of the isometric preview: the 2:1 view of `renderer::IsometricConfig` at a zoom.
struct IsoView {
	// Screen position of the world origin.
	ImVec2 origin;
	// Zoom against the 64x32 pixel tile.
	float scale = 1.f;

	[[nodiscard]] auto project(const float iX, const float iY, const float iHeight) const -> ImVec2 {
		const renderer::IsometricConfig config;
		const auto halfWidth = static_cast<float>(config.tileSize.x()) * 0.5f;
		const auto halfHeight = static_cast<float>(config.tileSize.y()) * 0.5f;
		return {origin.x + (iX - iY) * halfWidth * scale,
				origin.y + ((iX + iY) * halfHeight - iHeight * config.zStep) * scale};
	}
};

void drawCellSurface(ImDrawList* iDrawList, const IsoView& iView, const scene::TilemapAsset& iAsset, const uint32_t iX,
					 const uint32_t iY, const ImU32 iColor, const float iThickness) {
	const auto corners = iAsset.getCellCorners(iX, iY);
	const auto fx = static_cast<float>(iX);
	const auto fy = static_cast<float>(iY);
	const auto z = [&corners](const size_t iIndex) -> float {
		return static_cast<float>(corners[iIndex]) * scene::terrain::heightStep;
	};
	const std::array<ImVec2, 4> points{iView.project(fx, fy, z(0)), iView.project(fx + 1.f, fy, z(1)),
									   iView.project(fx + 1.f, fy + 1.f, z(2)), iView.project(fx, fy + 1.f, z(3))};
	iDrawList->AddPolyline(points.data(), static_cast<int>(points.size()), iColor, ImDrawFlags_Closed, iThickness);
}

}// namespace

void TilemapDocument::renderIsometricToolbar() {
	struct ToolEntry {
		IsometricTool tool;
		const char* id;
		const char* glyph;
		const char* tooltip;
		const char* description;
	};
	static constexpr std::array<ToolEntry, 5> tools{{
			{IsometricTool::Paint, "##toolPaint", gui::icon::paintbrush, "Paint",
			 "Paint terrain tiles with the palette brushes."},
			{IsometricTool::Raise, "##toolRaise", gui::icon::chevronUp, "Raise",
			 "Left click raises a corner (or the area), right click lowers it."},
			{IsometricTool::Lower, "##toolLower", gui::icon::chevronDown, "Lower",
			 "Left click lowers a corner (or the area), right click raises it."},
			{IsometricTool::Level, "##toolLevel", gui::icon::minus, "Level",
			 "Bring a corner (or the area) to the level height; Ctrl+click reads the height of a corner."},
			{IsometricTool::Overlay, "##toolOverlay", gui::icon::layers, "Overlay",
			 "Paint the overlay of the active layer (tracks, roads) with the primary brush, right click clears it."},
	}};
	for (size_t i = 0; i < tools.size(); ++i) {
		if (i > 0)
			ImGui::SameLine();
		const auto& entry = tools[i];
		if (gui::widgets::iconButton(entry.id, {.glyph = entry.glyph,
												.tooltip = entry.tooltip,
												.description = entry.description,
												.selected = m_isoTool == entry.tool}))
			m_isoTool = entry.tool;
	}
	if (m_isoTool == IsometricTool::Raise || m_isoTool == IsometricTool::Lower || m_isoTool == IsometricTool::Level) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.f);
		ImGui::DragInt("##terrainArea", &m_terrainBrushSize, 0.1f, 1, 16, "area %d corners");
		m_terrainBrushSize = std::clamp(m_terrainBrushSize, 1, 16);
	}
	if (m_isoTool == IsometricTool::Level) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.f);
		ImGui::DragInt("##levelHeight", &m_levelHeight, 0.1f, 0, scene::TilemapAsset::maxCornerHeight, "height %d");
	}
}

void TilemapDocument::renderIsometricCanvas() {
	renderIsometricToolbar();
	const auto available = ImGui::GetContentRegionAvail();
	if (available.x <= 0.f || available.y <= 0.f)
		return;
	const auto canvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 canvasMax{canvasMin.x + available.x, canvasMin.y + available.y};
	if (!m_isoCentred) {
		m_panOffset = math::vec2{available.x * 0.5f, 80.f};
		m_isoCentred = true;
	}
	ImGui::InvisibleButton("##isoCanvas", available,
						   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
								   ImGuiButtonFlags_MouseButtonMiddle);
	handleCanvasDrop();
	const bool hovered = ImGui::IsItemHovered();
	handleCanvasZoomPan(canvasMin, hovered);
	const ImVec2 origin{canvasMin.x + m_panOffset.x(), canvasMin.y + m_panOffset.y()};

	auto* drawList = ImGui::GetWindowDrawList();
	drawList->PushClipRect(canvasMin, canvasMax, true);
	drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(40, 40, 50, 255));
	drawIsometricTiles(drawList, origin, canvasMin, canvasMax);
	const IsoView view{origin, m_zoomPixelsPerCell / 32.f};
	if (m_inspectedCellX >= 0 && m_inspectedCellY >= 0 && m_inspectedCellX < static_cast<int32_t>(m_asset.width) &&
		m_inspectedCellY < static_cast<int32_t>(m_asset.height))
		drawCellSurface(drawList, view, m_asset, static_cast<uint32_t>(m_inspectedCellX),
						static_cast<uint32_t>(m_inspectedCellY), IM_COL32(120, 220, 255, 220), 3.f);
	if (hovered)
		handleIsometricInput(drawList, origin);
	else {
		m_isoHover.reset();
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
			finishIsometricGesture();
	}
	drawList->PopClipRect();
}

void TilemapDocument::drawIsometricTiles(ImDrawList* iDrawList, const ImVec2& iOrigin, const ImVec2& iCanvasMin,
										 const ImVec2& iCanvasMax) const {
	const IsoView view{iOrigin, m_zoomPixelsPerCell / 32.f};
	const renderer::IsometricConfig config;
	const auto& tileset = *m_asset.tileset;
	const auto* texture = tileset.texture.get();
	const auto tileWidth = static_cast<float>(config.tileSize.x());
	const float spriteWidth = tileWidth * view.scale;
	const float spriteHeight =
			static_cast<float>(tileset.tileHeight) * (tileWidth / static_cast<float>(tileset.tileWidth)) * view.scale;
	const float raisePixels =
			static_cast<float>(m_asset.getMaxCornerHeight()) * scene::terrain::heightStep * config.zStep * view.scale;
	const float halfWidthPixels = tileWidth * 0.5f * view.scale;
	const auto width = static_cast<int32_t>(m_asset.width);
	const auto height = static_cast<int32_t>(m_asset.height);

	const auto drawTile = [&](const int32_t iTile, const ImVec2& iFoot, const uint32_t iX, const uint32_t iY) -> void {
		if (texture == nullptr || iTile < 0 || static_cast<uint32_t>(iTile) >= tileset.tileCount()) {
			drawCellSurface(iDrawList, view, m_asset, iX, iY, IM_COL32(150, 150, 200, 255), 1.f);
			return;
		}
		const auto uvs = tileset.getTileUv(static_cast<uint32_t>(iTile));
		iDrawList->AddImage(static_cast<ImTextureID>(texture->getRendererId()),
							ImVec2{iFoot.x - spriteWidth * 0.5f, iFoot.y - spriteHeight},
							ImVec2{iFoot.x + spriteWidth * 0.5f, iFoot.y}, gui::vec(uvs[3]), gui::vec(uvs[1]));
	};

	for (int32_t diagonal = 0; diagonal <= width + height - 2; ++diagonal) {
		const float footY = iOrigin.y + static_cast<float>(diagonal + 2) * static_cast<float>(config.tileSize.y()) *
												0.5f * view.scale;
		if (footY < iCanvasMin.y || footY - raisePixels - spriteHeight > iCanvasMax.y)
			continue;
		const float columnLow = (iCanvasMin.x - halfWidthPixels - iOrigin.x) / halfWidthPixels;
		const float columnHigh = (iCanvasMax.x + halfWidthPixels - iOrigin.x) / halfWidthPixels;
		const int32_t first =
				std::max({0, diagonal - (height - 1),
						  static_cast<int32_t>(std::ceil((columnLow + static_cast<float>(diagonal)) * 0.5f))});
		const int32_t last =
				std::min({diagonal, width - 1,
						  static_cast<int32_t>(std::floor((columnHigh + static_cast<float>(diagonal)) * 0.5f))});
		for (int32_t x = first; x <= last; ++x) {
			const auto cellX = static_cast<uint32_t>(x);
			const auto cellY = static_cast<uint32_t>(diagonal - x);
			const uint8_t slope = m_asset.getCellSlope(cellX, cellY);
			const float baseZ =
					static_cast<float>(m_asset.getCellBaseHeight(cellX, cellY)) * scene::terrain::heightStep;
			const ImVec2 foot = view.project(static_cast<float>(cellX + 1), static_cast<float>(cellY + 1), baseZ);
			for (const auto& layer: m_asset.layers) {
				if (!layer.visible)
					continue;
				const auto cell = layer.getCell(cellX, cellY);
				if (cell.terrain >= 0)
					drawTile(tileset.resolveTile(cell.terrain, slope,
												 (cell.flags & scene::terrain::foundationFlag) != 0),
							 foot, cellX, cellY);
				if (cell.overlay >= 0)
					drawTile(tileset.resolveTile(cell.overlay, slope), foot, cellX, cellY);
			}
		}
	}
}

void TilemapDocument::handleIsometricInput(ImDrawList* iDrawList, const ImVec2& iOrigin) {
	const auto& io = ImGui::GetIO();
	const IsoView view{iOrigin, m_zoomPixelsPerCell / 32.f};
	const renderer::IsometricConfig config;
	const float top = static_cast<float>(m_asset.getMaxCornerHeight()) * scene::terrain::heightStep + 1.f;
	const math::vec2 screen{(io.MousePos.x - iOrigin.x) / view.scale, (io.MousePos.y - iOrigin.y) / view.scale};
	const math::vec3 start = renderer::screenToWorld(screen, top, config);
	const math::vec3 ground = renderer::screenToWorld(screen, 0.f, config);
	m_isoHover = scene::terrain::pickTerrain(m_asset, math::vec3{0.f, 0.f, 0.f}, start, ground - start);
	const bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	const bool rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
	if (!m_isoHover) {
		if (!leftDown && !rightDown)
			finishIsometricGesture();
		return;
	}
	const auto& hit = *m_isoHover;
	drawCellSurface(iDrawList, view, m_asset, hit.cell.x(), hit.cell.y(), IM_COL32(255, 195, 38, 255), 2.f);
	iDrawList->AddCircleFilled(
			view.project(static_cast<float>(hit.corner.x()), static_cast<float>(hit.corner.y()),
						 static_cast<float>(m_asset.getCornerHeight(hit.corner.x(), hit.corner.y())) *
								 scene::terrain::heightStep),
			4.f, IM_COL32(255, 195, 38, 255));
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		m_inspectedCellX = static_cast<int32_t>(hit.cell.x());
		m_inspectedCellY = static_cast<int32_t>(hit.cell.y());
	}
	if (!leftDown && !rightDown) {
		finishIsometricGesture();
		return;
	}
	applyIsometricTool(hit, leftDown, rightDown);
}

void TilemapDocument::applyIsometricTool(const scene::terrain::TerrainHit& iHit, const bool iLeftDown,
										 const bool iRightDown) {
	if (m_isoTool == IsometricTool::Paint) {
		applyBrush(iHit.cell.x(), iHit.cell.y(), iLeftDown, iRightDown);
		return;
	}
	if (m_isoTool == IsometricTool::Overlay) {
		const uint32_t layerIdx = m_palette.getSelectedLayer();
		const int32_t brush = m_palette.getPrimaryBrush();
		if (layerIdx >= m_asset.layers.size() || (iLeftDown && brush == panel::g_TileBrushPick))
			return;
		auto cell = m_asset.layers[layerIdx].getCell(iHit.cell.x(), iHit.cell.y());
		cell.overlay = iLeftDown && brush >= 0 ? brush : scene::component::g_EmptyTileIndex;
		m_strokeInProgress = true;
		std::ignore = m_stroke.setCell(m_asset, layerIdx, iHit.cell.x(), iHit.cell.y(), cell);
		return;
	}
	if (m_lastTerrainCorner == iHit.corner)
		return;
	m_lastTerrainCorner = iHit.corner;
	if (m_isoTool == IsometricTool::Level && ImGui::GetIO().KeyCtrl) {
		m_levelHeight = m_asset.getCornerHeight(iHit.corner.x(), iHit.corner.y());
		return;
	}
	const auto side = static_cast<uint32_t>(std::max(1, m_terrainBrushSize));
	const uint32_t back = (side - 1) / 2;
	const math::vec2ui first{iHit.corner.x() > back ? iHit.corner.x() - back : 0u,
							 iHit.corner.y() > back ? iHit.corner.y() - back : 0u};
	const math::vec2ui last{first.x() + side - 1, first.y() + side - 1};
	if (m_isoTool == IsometricTool::Level) {
		m_terrainStroke.add(m_asset.levelArea(first, last, static_cast<uint8_t>(std::clamp(m_levelHeight, 0, 255))));
		return;
	}
	const bool raise = (m_isoTool == IsometricTool::Raise) ? iLeftDown : iRightDown;
	if (side == 1)
		m_terrainStroke.add(raise ? m_asset.raiseCorner(first.x(), first.y())
								  : m_asset.lowerCorner(first.x(), first.y()));
	else
		m_terrainStroke.add(raise ? m_asset.raiseArea(first, last) : m_asset.lowerArea(first, last));
}

void TilemapDocument::finishIsometricGesture() {
	if (auto command = m_terrainStroke.finish("Terraform"); command)
		m_undoManager.push(std::move(command));
	m_lastTerrainCorner.reset();
	if (m_strokeInProgress) {
		endStroke(m_isoTool == IsometricTool::Overlay ? "Paint overlay" : "Paint tiles");
		m_strokeInProgress = false;
	}
}

void TilemapDocument::refreshSavedSnapshot() { m_savedAsset = m_asset; }

void TilemapDocument::resolveTileset() {
	if (!m_asset.tileset && !m_asset.tilesetPath.empty() && app::Application::instanced()) {
		const auto& app = app::Application::get();
		auto resolved = mkShared<scene::Tileset>();
		bool loaded = false;
		for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
			if (const auto fullPath = assetsPath / m_asset.tilesetPath; exists(fullPath)) {
				loaded = resolved->loadFromFile(fullPath);
				if (loaded)
					break;
			}
		}
		if (!loaded && exists(m_asset.tilesetPath))
			loaded = resolved->loadFromFile(m_asset.tilesetPath);
		if (loaded)
			m_asset.tileset = std::move(resolved);
	}
}

void TilemapDocument::endStroke(const std::string& iDescription) {
	if (auto command = m_stroke.finish(m_asset, iDescription); command)
		m_undoManager.push(std::move(command));
}

}// namespace owl::nest
