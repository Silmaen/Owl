/**
 * @file TilemapDocument.cpp
 * @author Silmaen
 * @date 08/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "TilemapDocument.h"

#include "EditorLayer.h"

#include <gui/IconBank.h>
#include <gui/utils.h>
#include <gui/widgets/AssetField.h>
#include <imgui_stdlib.h>
#include <scene/Tileset.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

namespace owl::nest {

namespace {
constexpr float g_minZoom = 4.f;
constexpr float g_maxZoom = 128.f;
constexpr float g_zoomWheelStep = 1.15f;

/**
 * @brief
 *  Coarse-grained undo command for `TilemapAsset` edits.
 *
 * Captures the asset YAML before and after a stroke; `undo` / `redo` deserialize
 * the saved state back into the live asset. Granularity is the whole asset, not
 * a single cell — the trade-off is simpler correctness (no command graph for
 * fill / rect / multi-layer ops) at the cost of strokes being heavier than
 * cell-by-cell deltas. Acceptable for the editor cadence (one push per stroke).
 */
class ModifyTilemapAssetCommand final : public UndoCommand<scene::TilemapAsset> {
public:
	ModifyTilemapAssetCommand(std::string iBeforeYaml, std::string iAfterYaml, std::string iDescription)
		: m_beforeYaml{std::move(iBeforeYaml)}, m_afterYaml{std::move(iAfterYaml)},
		  m_description{std::move(iDescription)} {}

	void undo(scene::TilemapAsset& ioTarget) override {
		// Preserve the resolved tileset across deserialise (the YAML only carries the
		// tilesetPath; reloading it would clear the in-memory texture handle).
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

/**
 * @brief
 *  Compose a YAML string of the asset suitable for undo capture.
 * @param[in] iAsset The asset to snapshot.
 * @return The YAML document.
 */
auto snapshotAsset(const scene::TilemapAsset& iAsset) -> std::string { return iAsset.serializeToString("undo"); }

/**
 * @brief
 *  Push an undoable mutation: snapshot the asset before, run the mutator, snapshot after,
 *  and push a `ModifyTilemapAssetCommand` if anything changed.
 * @tparam Fn Mutator callable type — invoked once with the live asset.
 * @param[in,out] ioAsset The asset to mutate.
 * @param[in,out] ioUndo The undo manager that receives the command.
 * @param[in] iDescription Human-readable description for the undo entry.
 * @param[in] iMutator Callable that performs the mutation.
 */
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

/**
 * @brief
 *  Render a small icon-only button with a tooltip.
 * @param[in] iIcon Icon name registered in the IconBank.
 * @param[in] iTooltip Tooltip text shown on hover.
 * @param[in] iSize Square button size in pixels.
 * @return True when the button was clicked.
 */
auto iconButton(const char* iIcon, const char* iTooltip, const float iSize = 20.f) -> bool {
	auto& iconBank = gui::IconBank::instance();
	const auto info = iconBank.getIcon(iIcon);
	bool clicked = false;
	if (info.has_value()) {
		clicked = ImGui::ImageButton(iIcon, static_cast<ImTextureID>(info->textureId), ImVec2{iSize, iSize},
									 gui::vec(info->uv0), gui::vec(info->uv1));
	} else {
		clicked = ImGui::SmallButton(iIcon);
	}
	if (ImGui::IsItemHovered() && iTooltip != nullptr) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.f);
		ImGui::SetWindowFontScale(0.85f);
		ImGui::TextUnformatted(iTooltip);
		ImGui::SetWindowFontScale(1.f);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
	return clicked;
}

/**
 * @brief
 *  Render a small, word-wrapped help text in the disabled colour.
 * @param[in] iText The help string.
 */
void helpText(const char* iText) {
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::SetWindowFontScale(0.85f);
	ImGui::TextWrapped("%s", iText);
	ImGui::SetWindowFontScale(1.f);
	ImGui::PopStyleColor();
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
	const auto winTitle = std::format("{}##tilemap_{:x}", display, static_cast<uint64_t>(id()));

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
	if (wantFocus) {
		// Re-issue inside Begin to bring docked tabs to the front (`SetNextWindowFocus`
		// alone is sometimes insufficient for newly-created tabs sharing a dock node).
		ImGui::SetWindowFocus();
		if (mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
	}
	if (open) {
		const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		// Pull document-manager focus on tab activation so SceneHierarchy / Properties / ribbon
		// reflect the document the user just clicked.
		if (windowFocused && !m_wasFocused && mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
		m_wasFocused = windowFocused;
		if (windowFocused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
			std::ignore = save();

		renderCanvas();
	}
	ImGui::End();

	// Palette panel — always rendered (`m_palette.onImGuiRender` opens its own window).
	m_palette.onImGuiRender(&m_asset);
}

void TilemapDocument::renderHierarchyPanel() {

	// --- Tileset slot — drop target for `.owltileset` -------------------------------------
	const std::string tsLabel =
			m_asset.tilesetPath.empty() ? "<drop a .owltileset>" : m_asset.tilesetPath.generic_string();
	ImGui::TextUnformatted("Tileset");
	ImGui::SameLine();
	// Stable button id (`##tilesetSlot`) so the drop target survives label changes.
	const std::string tsButtonId = std::format("{}##tilesetSlot", tsLabel);
	if (ImGui::Button(tsButtonId.c_str(), ImVec2(-1.f, 0.f))) {
		pushTilemapEdit(m_asset, m_undoManager, "Clear tileset", [this]() -> void {
			m_asset.tilesetPath.clear();
			m_asset.tileset.reset();
		});
	}
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const auto* data = static_cast<const char*>(payload->Data);
			const auto length = ::strnlen(data, static_cast<size_t>(payload->DataSize));
			const std::filesystem::path candidate{std::string(data, length)};
			if (gui::widgets::isPathOfKind(candidate, gui::widgets::AssetKind::Tileset)) {
				pushTilemapEdit(m_asset, m_undoManager, "Set tileset", [this, &candidate]() -> void {
					m_asset.tilesetPath = candidate;
					m_asset.tileset.reset();
					resolveTileset();
				});
			}
		}
		ImGui::EndDragDropTarget();
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Drop a .owltileset asset here, or click to clear.");
	// "Open in Tileset Editor" — only when the tileset path is set and the parent editor is wired.
	if (!m_asset.tilesetPath.empty() && mp_editorLayer != nullptr) {
		if (iconButton("owltileset_icon", "Open the referenced .owltileset in its own editor")) {
			std::filesystem::path resolved;
			if (core::Application::instanced()) {
				for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
					if (const auto candidate = assetsPath / m_asset.tilesetPath; exists(candidate)) {
						resolved = candidate;
						break;
					}
				}
			}
			if (!resolved.empty())
				mp_editorLayer->requestDeferredOpen(resolved);
			else
				OWL_CORE_WARN("Could not resolve tileset path '{}' against any asset directory.",
							  m_asset.tilesetPath.string())
		}
		ImGui::SameLine();
		ImGui::TextDisabled("Open in Tileset Editor");
	}

	// --- Grid dimensions -------------------------------------------------------------------
	int width = static_cast<int>(m_asset.width);
	int height = static_cast<int>(m_asset.height);
	const bool widthChanged = ImGui::DragInt("Width", &width, 1.f, 1, 1024);
	const bool widthCommitted = ImGui::IsItemDeactivatedAfterEdit();
	const bool heightChanged = ImGui::DragInt("Height", &height, 1.f, 1, 1024);
	const bool heightCommitted = ImGui::IsItemDeactivatedAfterEdit();
	if (widthChanged || heightChanged) {
		// Live preview during the drag; defer the undo command to the deactivation frame.
		m_asset.resize(static_cast<uint32_t>(std::max(1, width)), static_cast<uint32_t>(std::max(1, height)));
	}
	if (widthCommitted || heightCommitted) {
		// One snapshot per "released drag". Use the resized state directly — the live preview
		// already mutated `m_asset`, so we capture before / after by re-reading the saved
		// snapshot and comparing with the post-edit state.
		const auto after = snapshotAsset(m_asset);
		if (after != m_savedSnapshot) {
			auto cmd = mkUniq<ModifyTilemapAssetCommand>(m_savedSnapshot, after, "Resize grid");
			m_undoManager.push(std::move(cmd));
			m_savedSnapshot = after;// keep dirty tracking consistent — saved snapshot still drives `isDirty`
		}
	}

	float cellSize = m_asset.cellSize;
	if (ImGui::DragFloat("Cell Size", &cellSize, 0.05f, 0.01f, 100.f, "%.3f"))
		m_asset.cellSize = std::max(0.0001f, cellSize);

	// --- Layer manager --------------------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Layers (%zu)", m_asset.layers.size());
	ImGui::SameLine();
	if (iconButton("add_component", "Add a new layer")) {
		pushTilemapEdit(m_asset, m_undoManager, "Add layer",
						[this]() -> void { m_asset.addLayer(std::format("layer{}", m_asset.layers.size())); });
	}

	int eraseIndex = -1;
	int moveUpIndex = -1;
	int moveDownIndex = -1;
	for (size_t i = 0; i < m_asset.layers.size(); ++i) {
		auto& layer = m_asset.layers[i];
		ImGui::PushID(static_cast<int>(i));
		ImGui::Separator();

		// Buttons row first (right-aligned), input filling the remaining width on the first
		// line to keep the row on a single visual line regardless of panel width.
		const float buttonStride = 14.f + ImGui::GetStyle().FramePadding.x * 2.f + ImGui::GetStyle().ItemSpacing.x;
		const float buttonsReserved = buttonStride * 4.f + ImGui::GetStyle().ItemSpacing.x * 2.f;
		const bool isActive = (m_palette.getSelectedLayer() == static_cast<uint32_t>(i));
		if (ImGui::RadioButton("##active", isActive))
			m_palette.setSelectedLayer(static_cast<uint32_t>(i));
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Active painting layer");

		ImGui::SameLine();
		ImGui::SetNextItemWidth(-buttonsReserved);
		if (ImGui::InputText("##name", &layer.name, ImGuiInputTextFlags_EnterReturnsTrue))
			pushTilemapEdit(m_asset, m_undoManager, "Rename layer", []() -> void {});

		ImGui::SameLine();
		const auto* eyeIcon = layer.visible ? "eye_open" : "eye_closed";
		if (iconButton(eyeIcon, layer.visible ? "Hide layer" : "Show layer")) {
			pushTilemapEdit(m_asset, m_undoManager, "Toggle layer visibility",
							[&layer]() -> void { layer.visible = !layer.visible; });
		}
		ImGui::SameLine();
		if (iconButton("move_up", "Move layer up")) {
			if (i > 0)
				moveUpIndex = static_cast<int>(i);
		}
		ImGui::SameLine();
		if (iconButton("move_down", "Move layer down")) {
			if (i + 1 < m_asset.layers.size())
				moveDownIndex = static_cast<int>(i);
		}
		ImGui::SameLine();
		if (iconButton("delete", "Delete layer"))
			eraseIndex = static_cast<int>(i);

		float parallax[2] = {layer.parallax.x(), layer.parallax.y()};
		if (ImGui::DragFloat2("Parallax", parallax, 0.01f, 0.f, 4.f, "%.2f"))
			layer.parallax = math::vec2{parallax[0], parallax[1]};
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
	ImGui::TextDisabled("Selected Cell");
	if (m_inspectedCellX < 0 || m_inspectedCellY < 0) {
		helpText("Click a cell on the canvas to inspect its coordinates and content.");
		return;
	}
	const auto layerIdx = m_palette.getSelectedLayer();
	if (layerIdx >= m_asset.layers.size()) {
		helpText("No active layer — add one in the Scene Hierarchy panel.");
		return;
	}
	const auto cellX = static_cast<uint32_t>(m_inspectedCellX);
	const auto cellY = static_cast<uint32_t>(m_inspectedCellY);
	const int32_t tileAtCell = m_asset.getTile(layerIdx, cellX, cellY);

	ImGui::Text("Position: %d, %d", m_inspectedCellX, m_inspectedCellY);
	ImGui::Text("Layer: #%u %s", layerIdx, m_asset.layers[layerIdx].name.c_str());
	ImGui::Separator();

	if (tileAtCell < 0) {
		ImGui::TextUnformatted("Tile: <empty>");
		ImGui::TextColored({0.6f, 0.9f, 0.6f, 1.f}, "Passable: yes");
	} else {
		std::string tileLabel = std::format("Tile: #{}", tileAtCell);
		bool passable = true;
		if (m_asset.tileset && static_cast<uint32_t>(tileAtCell) < m_asset.tileset->tileCount()) {
			const auto& meta = m_asset.tileset->tiles[static_cast<size_t>(tileAtCell)];
			if (!meta.name.empty())
				tileLabel = std::format("Tile: #{} ({})", tileAtCell, meta.name);
			passable = !meta.collidable;
		}
		ImGui::TextUnformatted(tileLabel.c_str());
		if (passable)
			ImGui::TextColored({0.6f, 0.9f, 0.6f, 1.f}, "Passable: yes");
		else
			ImGui::TextColored({1.f, 0.5f, 0.5f, 1.f}, "Passable: no (solid)");
	}

	ImGui::Spacing();
	helpText("Tile-type metadata (name, collidable, …) lives on the .owltileset. Open the tileset editor (button next "
			 "to the Tileset slot in this panel) to edit it.");
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
		if (mp_editorLayer != nullptr && core::Application::instanced()) {
			std::filesystem::path resolved;
			for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
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
					// Tileset UVs follow Renderer2D's V-up convention; ImGui samples V-down,
					// so we feed the screen TL→TR→BR→BL corners with the BL→BR→TR→TL UVs.
					iDrawList->AddImageQuad(static_cast<ImTextureID>(tex->getRendererId()),
											ImVec2{cellMin.x, cellMin.y}, ImVec2{cellMax.x, cellMin.y},
											ImVec2{cellMax.x, cellMax.y}, ImVec2{cellMin.x, cellMax.y},
											gui::vec(uvs[0]), gui::vec(uvs[1]), gui::vec(uvs[2]), gui::vec(uvs[3]));
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

	// Any click selects the cell — empty cells included — so the Properties panel can inspect
	// coordinates / tile type / passability. Canvas clicks NEVER change the active brush.
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
	if (!m_asset.tileset && !m_asset.tilesetPath.empty() && core::Application::instanced()) {
		const auto& app = core::Application::get();
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
