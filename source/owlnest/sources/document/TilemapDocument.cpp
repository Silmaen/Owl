/**
 * @file TilemapDocument.cpp
 * @author Silmaen
 * @date 08/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "TilemapDocument.h"

#include "EditorLayer.h"

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
#include <scene/Tileset.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <tuple>
#include <utility>

namespace owl::nest {

namespace {
constexpr float g_minZoom = 4.f;
constexpr float g_maxZoom = 128.f;
constexpr float g_zoomWheelStep = 1.15f;

class ModifyTilemapAssetCommand final : public UndoCommand<scene::TilemapAsset> {
public:
	ModifyTilemapAssetCommand(std::string iBeforeYaml, std::string iAfterYaml, std::string iDescription)
		: m_beforeYaml{std::move(iBeforeYaml)}, m_afterYaml{std::move(iAfterYaml)},
		  m_description{std::move(iDescription)} {}

	void undo(scene::TilemapAsset& ioTarget) override {
		const auto savedTileset = ioTarget.tileset;
		std::ignore = ioTarget.deserializeFromString(m_beforeYaml);
		ioTarget.tileset = savedTileset;
	}

	void redo(scene::TilemapAsset& ioTarget) override {
		const auto savedTileset = ioTarget.tileset;
		std::ignore = ioTarget.deserializeFromString(m_afterYaml);
		ioTarget.tileset = savedTileset;
	}

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

private:
	std::string m_beforeYaml;
	std::string m_afterYaml;
	std::string m_description;
};

auto snapshotAsset(const scene::TilemapAsset& iAsset) -> std::string { return iAsset.serializeToString("undo"); }

template<typename Fn>
void pushTilemapEdit(scene::TilemapAsset& ioAsset, TilemapUndoManager& ioUndo, const std::string& iDescription,
					 Fn&& iMutator) {
	const auto before = snapshotAsset(ioAsset);
	std::forward<Fn>(iMutator)();
	const auto after = snapshotAsset(ioAsset);
	if (before != after) {
		auto cmd = mkUniq<ModifyTilemapAssetCommand>(before, after, iDescription);
		ioUndo.push(std::move(cmd));
	}
}

}// namespace

TilemapDocument::TilemapDocument() = default;

TilemapDocument::~TilemapDocument() = default;

auto TilemapDocument::title() const -> std::string {
	if (!m_path.empty())
		return m_path.stem().string();
	return "Untitled";
}

auto TilemapDocument::isDirty() const -> bool {
	const auto displayName = m_path.empty() ? std::string{"untitled"} : m_path.stem().string();
	return m_asset.serializeToString(displayName) != m_savedSnapshot;
}

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
	if (widthChanged || heightChanged)
		m_asset.resize(static_cast<uint32_t>(std::max(1, width)), static_cast<uint32_t>(std::max(1, height)));
	if (widthCommitted || heightCommitted) {
		const auto after = snapshotAsset(m_asset);
		if (after != m_savedSnapshot) {
			auto cmd = mkUniq<ModifyTilemapAssetCommand>(m_savedSnapshot, after, "Resize grid");
			m_undoManager.push(std::move(cmd));
			m_savedSnapshot = after;// keep dirty tracking consistent — saved snapshot still drives `isDirty`
		}
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
		gui::widgets::endProperties();
	}
	gui::widgets::notice("The name and the collision of a tile belong to the tileset: open it from the Grid section.",
						 gui::widgets::BadgeKind::Info);
	gui::widgets::endSection();
}

void TilemapDocument::renderCanvas() {
	if (!m_asset.tileset || !m_asset.tileset->texture) {
		ImGui::TextDisabled("Drop a .owltileset on the asset to start authoring.");
		return;
	}
	if (m_asset.layers.empty()) {
		ImGui::TextDisabled("Add at least one layer in the Scene Hierarchy panel.");
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
		drawList->AddRect(selMin, selMax, IM_COL32(120, 220, 255, 220), 0.f, 0, 3.f);
	}

	if (hovered)
		handleCanvasHoverAndPaint(drawList, gridTopLeft, ImVec2{gridW * cellPx, gridH * cellPx}, cellPx);
	else if (m_strokeInProgress && !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
			 !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		endStroke();
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
				const size_t flat = static_cast<size_t>(y) * m_asset.width + x;
				if (flat >= layer.tiles.size())
					continue;
				const int32_t tileIdx = layer.tiles[flat];
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
	iDrawList->AddRect(hoverMin, hoverMax, IM_COL32(255, 195, 38, 255), 0.f, 0, 2.f);

	const bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	const bool rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		m_inspectedCellX = static_cast<int32_t>(cellX);
		m_inspectedCellY = static_cast<int32_t>(cellY);
	}

	if (!leftDown && !rightDown) {
		if (m_strokeInProgress) {
			endStroke();
			m_strokeInProgress = false;
		}
		return;
	}
	const uint32_t layerIdx = m_palette.getSelectedLayer();
	if (layerIdx >= m_asset.layers.size())
		return;
	int32_t brush = panel::g_TileBrushPick;
	if (leftDown && m_palette.getPrimaryBrush() != panel::g_TileBrushPick)
		brush = m_palette.getPrimaryBrush();
	else if (rightDown && m_palette.getSecondaryBrush() != panel::g_TileBrushPick)
		brush = m_palette.getSecondaryBrush();
	if (brush == panel::g_TileBrushPick)
		return;
	if (!m_strokeInProgress) {
		beginStroke();
		m_strokeInProgress = true;
	}
	const int32_t value = (brush == panel::g_TileBrushEraser) ? scene::component::g_EmptyTileIndex : brush;
	if (m_asset.getTile(layerIdx, cellX, cellY) != value)
		m_asset.setTile(layerIdx, cellX, cellY, value);
}

void TilemapDocument::refreshSavedSnapshot() {
	const auto displayName = m_path.empty() ? std::string{"untitled"} : m_path.stem().string();
	m_savedSnapshot = m_asset.serializeToString(displayName);
}

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

void TilemapDocument::beginStroke() { m_strokeBeforeYaml = snapshotAsset(m_asset); }

void TilemapDocument::endStroke() {
	if (m_strokeBeforeYaml.empty())
		return;
	const auto after = snapshotAsset(m_asset);
	if (after != m_strokeBeforeYaml) {
		auto cmd = mkUniq<ModifyTilemapAssetCommand>(m_strokeBeforeYaml, after, "Paint tiles");
		m_undoManager.push(std::move(cmd));
	}
	m_strokeBeforeYaml.clear();
}

}// namespace owl::nest
