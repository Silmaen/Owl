/**
 * @file TilesetDocument.cpp
 * @author Silmaen
 * @date 09/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "TilesetDocument.h"

#include "EditorLayer.h"

#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/AssetField.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/ImageTile.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Tooltip.h>
#include <imgui_stdlib.h>
#include <renderer/Renderer.h>
#include <renderer/gpu/Texture.h>
#include <scene/TilemapTerrain.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <tuple>
#include <utility>

namespace owl::nest {

namespace {
constexpr float g_minZoom = 0.25f;
constexpr float g_maxZoom = 8.f;
constexpr float g_zoomWheelStep = 1.15f;

class ModifyTilesetCommand final : public UndoCommand<scene::Tileset> {
public:
	ModifyTilesetCommand(std::string iBeforeYaml, std::string iAfterYaml, std::string iDescription)
		: m_beforeYaml{std::move(iBeforeYaml)}, m_afterYaml{std::move(iAfterYaml)},
		  m_description{std::move(iDescription)} {}

	void undo(scene::Tileset& ioTarget) override {
		const auto savedTexture = ioTarget.texture;
		std::ignore = ioTarget.deserializeFromString(m_beforeYaml);
		if (!ioTarget.texture)
			ioTarget.texture = savedTexture;
	}

	void redo(scene::Tileset& ioTarget) override {
		const auto savedTexture = ioTarget.texture;
		std::ignore = ioTarget.deserializeFromString(m_afterYaml);
		if (!ioTarget.texture)
			ioTarget.texture = savedTexture;
	}

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

private:
	std::string m_beforeYaml;
	std::string m_afterYaml;
	std::string m_description;
};

auto snapshotTileset(const scene::Tileset& iTileset) -> std::string { return iTileset.serializeToString("undo"); }

template<typename Fn>
void pushTilesetEdit(scene::Tileset& ioTileset, TilesetUndoManager& ioUndo, const std::string& iDescription,
					 Fn&& iMutator) {
	const auto before = snapshotTileset(ioTileset);
	std::forward<Fn>(iMutator)();
	const auto after = snapshotTileset(ioTileset);
	if (before != after) {
		auto cmd = mkUniq<ModifyTilesetCommand>(before, after, iDescription);
		ioUndo.push(std::move(cmd));
	}
}

}// namespace

TilesetDocument::TilesetDocument() = default;

TilesetDocument::~TilesetDocument() = default;

auto TilesetDocument::title() const -> std::string {
	if (!m_path.empty())
		return m_path.stem().string();
	return "Untitled";
}

auto TilesetDocument::isDirty() const -> bool {
	const auto displayName = m_path.empty() ? std::string{"untitled"} : m_path.stem().string();
	return m_tileset.serializeToString(displayName) != m_savedSnapshot;
}

auto TilesetDocument::recoverySnapshot() const -> std::optional<std::string> {
	return m_tileset.serializeToString(m_path.empty() ? std::string{"untitled"} : m_path.stem().string());
}

auto TilesetDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	const auto texture = m_tileset.texture;
	if (!m_tileset.deserializeFromString(iSnapshot)) {
		OWL_WARN("Recovery: Cannot restore the autosave of tileset '{}'.", title())
		return false;
	}
	if (!m_tileset.texture)
		m_tileset.texture = texture;
	return true;
}

void TilesetDocument::onAttach(EditorLayer* iEditor) {
	mp_editorLayer = iEditor;
	resolveTexture();
	refreshSavedSnapshot();
}

void TilesetDocument::onDetach() { mp_editorLayer = nullptr; }

void TilesetDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {}

void TilesetDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto TilesetDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto TilesetDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	const auto displayName = iPath.stem().string();
	if (!m_tileset.saveToFile(iPath, displayName)) {
		OWL_CORE_ERROR("TilesetDocument: failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	refreshSavedSnapshot();
	if (mp_editorLayer != nullptr)
		mp_editorLayer->onTilesetSaved(iPath);
	return true;
}

auto TilesetDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	if (!m_tileset.loadFromFile(iPath)) {
		OWL_CORE_ERROR("TilesetDocument: failed to load '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	resolveTexture();
	refreshSavedSnapshot();
	return true;
}

void TilesetDocument::onImGuiRender() {
	const auto display =
			(mp_editorLayer != nullptr) ? mp_editorLayer->getDocumentManager().displayTitleFor(this) : title();
	const auto winTitle = documentWindowTitle(DocumentType::Tileset, display, static_cast<uint64_t>(id()));

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
}

void TilesetDocument::renderHierarchyPanel() {
	if (!gui::widgets::sectionHeader("Atlas", gui::icon::image, false).open)
		return;
	const auto textureBefore = snapshotTileset(m_tileset);
	if (gui::widgets::textureField("##texture", m_tileset.texture, ImVec2{96.f, 96.f})) {
		if (m_tileset.texture)
			m_tileset.texture->setFilterMode(m_tileset.filterMode);
	}
	gui::widgets::itemTooltip("Texture", nullptr, "Drop a texture from the Content Browser.");
	if (const auto textureAfter = snapshotTileset(m_tileset); textureAfter != textureBefore) {
		auto cmd = mkUniq<ModifyTilesetCommand>(textureBefore, textureAfter, "Set texture");
		m_undoManager.push(std::move(cmd));
	}
	if (!gui::widgets::beginProperties("##atlas")) {
		gui::widgets::endSection();
		return;
	}
	// A count with the buttons that add or remove a column or a row.
	const auto countRow = [this](const char* iLabel, const uint32_t iCount, const char* iWhat, auto&& iAdd,
								 auto&& iRemove) -> void {
		gui::widgets::propertyLabel(iLabel);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%u", iCount);
		ImGui::SameLine();
		const auto add = std::format("Add a {}", iWhat);
		const auto remove = std::format("Remove a {}", iWhat);
		if (gui::widgets::iconButton("##add", {.glyph = gui::icon::plus, .tooltip = add.c_str()}))
			pushTilesetEdit(m_tileset, m_undoManager, add, iAdd);
		ImGui::SameLine();
		if (gui::widgets::iconButton("##remove",
									 {.glyph = gui::icon::minus, .tooltip = remove.c_str(), .disabled = iCount <= 1}))
			pushTilesetEdit(m_tileset, m_undoManager, remove, iRemove);
		std::ignore = gui::widgets::propertyReset(false);
	};
	countRow(
			"Columns", m_tileset.columns, "column", [this]() -> void { m_tileset.addColumn(); },
			[this]() -> void { m_tileset.removeColumn(); });
	countRow(
			"Rows", m_tileset.rows, "row", [this]() -> void { m_tileset.addRow(); },
			[this]() -> void { m_tileset.removeRow(); });
	auto tileWidth = static_cast<int>(m_tileset.tileWidth);
	auto tileHeight = static_cast<int>(m_tileset.tileHeight);
	if (gui::widgets::propertyInt("Tile Width", tileWidth, tileWidth, 1, 1024, "%d px"))
		m_tileset.tileWidth = static_cast<uint32_t>(tileWidth);
	if (gui::widgets::propertyInt("Tile Height", tileHeight, tileHeight, 1, 1024, "%d px"))
		m_tileset.tileHeight = static_cast<uint32_t>(tileHeight);

	const auto& current = m_tileset.filterMode;
	const char* currentLabel = (current == renderer::gpu::FilterMode::Nearest) ? "Nearest" : "Linear";
	gui::widgets::propertyLabel("Filter", "Nearest keeps the pixels sharp, Linear smooths them.");
	if (gui::widgets::beginCombo("##filter", currentLabel)) {
		if (ImGui::Selectable("Linear", current == renderer::gpu::FilterMode::Linear))
			pushTilesetEdit(m_tileset, m_undoManager, "Set filter to Linear", [this]() -> void {
				m_tileset.filterMode = renderer::gpu::FilterMode::Linear;
				if (m_tileset.texture)
					m_tileset.texture->setFilterMode(m_tileset.filterMode);
			});
		if (ImGui::Selectable("Nearest", current == renderer::gpu::FilterMode::Nearest))
			pushTilesetEdit(m_tileset, m_undoManager, "Set filter to Nearest", [this]() -> void {
				m_tileset.filterMode = renderer::gpu::FilterMode::Nearest;
				if (m_tileset.texture)
					m_tileset.texture->setFilterMode(m_tileset.filterMode);
			});
		ImGui::EndCombo();
	}
	std::ignore = gui::widgets::propertyReset(false);

	using Kind = scene::Tileset::Kind;
	const bool isometric = m_tileset.kind == Kind::Isometric;
	gui::widgets::propertyLabel("Kind", "Orthogonal: square tiles of the 2D tilemaps. Isometric: 2:1 diamond tiles "
										"drawn by the isometric layer.");
	if (gui::widgets::beginCombo("##kind", isometric ? "Isometric" : "Orthogonal")) {
		for (const auto& [kind, label]:
			 {std::pair{Kind::Orthogonal, "Orthogonal"}, std::pair{Kind::Isometric, "Isometric"}}) {
			if (ImGui::Selectable(label, m_tileset.kind == kind) && m_tileset.kind != kind)
				pushTilesetEdit(m_tileset, m_undoManager, std::format("Set kind to {}", label),
								[this, kind]() -> void { m_tileset.kind = kind; });
		}
		ImGui::EndCombo();
	}
	if (gui::widgets::propertyReset(isometric))
		pushTilesetEdit(m_tileset, m_undoManager, "Set kind to Orthogonal",
						[this]() -> void { m_tileset.kind = Kind::Orthogonal; });
	gui::widgets::endProperties();
	gui::widgets::endSection();
	if (m_tileset.kind == Kind::Isometric)
		renderSlopeSection();
}

void TilesetDocument::renderSlopeSection() {
	if (!gui::widgets::sectionHeader("Slopes", gui::icon::layers, false).open)
		return;
	ImGui::TextWrapped("Tile drawn for a terrain or overlay id on each slope; -1 falls back on the flat tile. A "
					   "foundation replaces the terrain tile of a cell whose flag bit 0 is set.");
	if (gui::widgets::button("addSlopeSet", {.glyph = gui::icon::plus, .label = "Add Terrain"})) {
		pushTilesetEdit(m_tileset, m_undoManager, "Add slope table", [this]() -> void {
			scene::SlopeSet set;
			set.id = std::max(0, m_inspectedTile);
			set.tiles[scene::terrain::flatSlope] = set.id;
			m_tileset.slopeSets.push_back(set);
		});
	}
	std::optional<size_t> eraseIndex;
	for (size_t i = 0; i < m_tileset.slopeSets.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		const auto label = std::format("Terrain {}", m_tileset.slopeSets[i].id);
		const bool open = ImGui::TreeNodeEx("##set", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap,
											"%s", label.c_str());
		ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - ImGui::GetFrameHeight());
		if (gui::widgets::iconButton("##delete", {.glyph = gui::icon::trash2, .tooltip = "Delete the table"}))
			eraseIndex = i;
		if (open) {
			if (gui::widgets::beginProperties("##slopeSet")) {
				const auto& set = m_tileset.slopeSets[i];
				int id = set.id;
				if (gui::widgets::propertyInt("Id", id, 0, 0, scene::component::g_MaxTileIndex, "%d",
											  "Terrain or overlay id stored in the cells."))
					pushTilesetEdit(m_tileset, m_undoManager, "Set slope table id",
									[this, i, id]() -> void { m_tileset.slopeSets[i].id = id; });
				for (uint8_t slope = 0; slope < scene::terrain::slopeCount; ++slope) {
					const auto name = std::string{scene::terrain::getSlopeName(slope)};
					int tile = set.tiles[slope];
					if (gui::widgets::propertyInt(name.c_str(), tile, -1, -1,
												  static_cast<int>(m_tileset.tileCount()) - 1, "%d",
												  "Tile index on this slope (-1: the flat tile)."))
						pushTilesetEdit(m_tileset, m_undoManager, "Set slope tile", [this, i, slope, tile]() -> void {
							m_tileset.slopeSets[i].tiles[slope] = tile;
						});
				}
				gui::widgets::endProperties();
			}
			if (gui::widgets::beginProperties("##foundations")) {
				const auto& set = m_tileset.slopeSets[i];
				for (uint8_t slope = 0; slope < scene::terrain::slopeCount; ++slope) {
					const auto name = std::format("{} foundation", scene::terrain::getSlopeName(slope));
					int tile = set.foundations[slope];
					if (gui::widgets::propertyInt(name.c_str(), tile, -1, -1,
												  static_cast<int>(m_tileset.tileCount()) - 1, "%d",
												  "Foundation tile on this slope (-1: none)."))
						pushTilesetEdit(
								m_tileset, m_undoManager, "Set foundation tile",
								[this, i, slope, tile]() -> void { m_tileset.slopeSets[i].foundations[slope] = tile; });
				}
				gui::widgets::endProperties();
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (eraseIndex)
		pushTilesetEdit(m_tileset, m_undoManager, "Delete slope table", [this, eraseIndex]() -> void {
			m_tileset.slopeSets.erase(m_tileset.slopeSets.begin() + static_cast<ptrdiff_t>(*eraseIndex));
		});
	gui::widgets::endSection();
}

void TilesetDocument::renderPropertiesPanel() {
	if (m_inspectedTile < 0) {
		std::ignore = gui::widgets::emptyState(gui::icon::grid3x3, "No tile selected",
											   "Click a tile in the atlas to edit its metadata.");
		return;
	}
	if (static_cast<uint32_t>(m_inspectedTile) >= m_tileset.tileCount()) {
		std::ignore = gui::widgets::emptyState(gui::icon::triangleAlert, "Tile out of range",
											   "The selected tile is past the end of the tileset.");
		return;
	}
	auto& meta = m_tileset.tiles[static_cast<size_t>(m_inspectedTile)];
	// Header: the tile itself and its number.
	{
		const auto uvs = m_tileset.getTileUv(static_cast<uint32_t>(m_inspectedTile));
		const bool loaded = m_tileset.texture && m_tileset.texture->isLoaded();
		std::ignore =
				gui::widgets::imageTile("##inspected", {.textureId = loaded ? m_tileset.texture->getRendererId() : 0,
														.uv0 = uvs[3],
														.uv1 = uvs[1],
														.size = ImGui::GetFrameHeight() * 1.6f});
		ImGui::SameLine();
		gui::UiLayer::pushTextStyle(gui::TextStyle::Strong);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Tile #%d", m_inspectedTile);
		gui::UiLayer::popTextStyle();
	}
	if (gui::widgets::sectionHeader("Tile", gui::icon::tag, false).open) {
		if (gui::widgets::beginProperties("##tileMeta")) {
			std::ignore = gui::widgets::propertyText("Name", meta.name, "grass",
													 "Optional name shown in the Tile Palette tooltip.");
			std::ignore = gui::widgets::propertyBool("Collidable", meta.collidable, false,
													 "Solid tiles produce a static collider in the physics world.");
			gui::widgets::endProperties();
		}
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Raycast", gui::icon::scan, false).open) {
		if (gui::widgets::beginProperties("##tileRaycast")) {
			if (gui::widgets::propertyFloat(
						"Wall Height", meta.wallHeight, 1.f, 0.05f, "%.2f",
						"Height of the wall in cells for the raycast renderer: 1 is floor to ceiling, more pokes into "
						"the sky (towers), less keeps a low wall on the floor. Ignored by the 2D renderer."))
				meta.wallHeight = std::clamp(meta.wallHeight, 0.f, 8.f);
			std::ignore = gui::widgets::propertyBool(
					"Transparent", meta.transparent, false,
					"The raycast keeps stepping through this tile (up to 8 layers per ray) and draws the hits back to "
					"front: author the texture with a PNG alpha channel; chroma keying is not supported.");
			gui::widgets::endProperties();
		}
		gui::widgets::endSection();
	}
}

void TilesetDocument::renderCanvas() {
	if (!m_tileset.texture || !m_tileset.texture->isLoaded()) {
		ImGui::TextDisabled("Drop a texture in the Scene Hierarchy panel to populate the atlas.");
		return;
	}
	const auto availSize = ImGui::GetContentRegionAvail();
	if (availSize.x <= 0.f || availSize.y <= 0.f)
		return;

	const auto canvasOrigin = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize{availSize.x, availSize.y};
	auto* drawList = ImGui::GetWindowDrawList();

	ImGui::InvisibleButton("##atlas_canvas", canvasSize,
						   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
	// Canvas-wide drop target: dropping a Content Browser asset opens it as a new document tab.
	if (ImGui::BeginDragDropTarget()) {
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
	const bool hovered = ImGui::IsItemHovered();
	const auto& io = ImGui::GetIO();
	if (hovered && io.MouseWheel != 0.f) {
		const float factor = io.MouseWheel > 0.f ? g_zoomWheelStep : 1.f / g_zoomWheelStep;
		const float oldZoom = m_zoom;
		m_zoom = std::clamp(m_zoom * factor, g_minZoom, g_maxZoom);
		const float zoomRatio = m_zoom / oldZoom;
		const ImVec2 cursorRel{io.MousePos.x - canvasOrigin.x - m_panOffset.x(),
							   io.MousePos.y - canvasOrigin.y - m_panOffset.y()};
		m_panOffset.x() += cursorRel.x - cursorRel.x * zoomRatio;
		m_panOffset.y() += cursorRel.y - cursorRel.y * zoomRatio;
	}
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
		m_panOffset.x() += io.MouseDelta.x;
		m_panOffset.y() += io.MouseDelta.y;
	}

	const float texW = static_cast<float>(m_tileset.columns * std::max(1u, m_tileset.tileWidth));
	const float texH = static_cast<float>(m_tileset.rows * std::max(1u, m_tileset.tileHeight));
	const ImVec2 atlasTopLeft{canvasOrigin.x + m_panOffset.x(), canvasOrigin.y + m_panOffset.y()};
	const ImVec2 atlasBottomRight{atlasTopLeft.x + texW * m_zoom, atlasTopLeft.y + texH * m_zoom};

	drawList->PushClipRect(canvasOrigin, ImVec2{canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y}, true);
	drawList->AddRectFilled(atlasTopLeft, atlasBottomRight, IM_COL32(40, 40, 50, 255));

	if (m_tileset.texture && m_tileset.texture->isLoaded()) {
		drawList->AddImage(static_cast<ImTextureID>(m_tileset.texture->getRendererId()), atlasTopLeft, atlasBottomRight,
						   {0.f, 1.f}, {1.f, 0.f});
	}

	const float cellW = static_cast<float>(m_tileset.tileWidth) * m_zoom;
	const float cellH = static_cast<float>(m_tileset.tileHeight) * m_zoom;
	const ImU32 gridColor = IM_COL32(120, 120, 140, 160);
	for (uint32_t c = 0; c <= m_tileset.columns; ++c) {
		const float x = atlasTopLeft.x + static_cast<float>(c) * cellW;
		drawList->AddLine({x, atlasTopLeft.y}, {x, atlasBottomRight.y}, gridColor);
	}
	for (uint32_t r = 0; r <= m_tileset.rows; ++r) {
		const float y = atlasTopLeft.y + static_cast<float>(r) * cellH;
		drawList->AddLine({atlasTopLeft.x, y}, {atlasBottomRight.x, y}, gridColor);
	}

	// Highlight collidable tiles + the selected tile.
	for (uint32_t r = 0; r < m_tileset.rows; ++r) {
		for (uint32_t c = 0; c < m_tileset.columns; ++c) {
			const auto idx = r * m_tileset.columns + c;
			if (idx >= m_tileset.tiles.size())
				continue;
			const ImVec2 cellMin{atlasTopLeft.x + static_cast<float>(c) * cellW,
								 atlasTopLeft.y + static_cast<float>(r) * cellH};
			const ImVec2 cellMax{cellMin.x + cellW, cellMin.y + cellH};
			if (m_tileset.tiles[idx].collidable)
				drawList->AddRectFilled(cellMin, cellMax, IM_COL32(255, 90, 90, 60));
			if (static_cast<int32_t>(idx) == m_inspectedTile)
				drawList->AddRect(cellMin, cellMax, IM_COL32(255, 195, 38, 255), 0.f, 3.f);
		}
	}

	if (hovered) {
		const float relX = io.MousePos.x - atlasTopLeft.x;
		const float relY = io.MousePos.y - atlasTopLeft.y;
		if (relX >= 0.f && relY >= 0.f && relX < texW * m_zoom && relY < texH * m_zoom) {
			const auto col = static_cast<uint32_t>(relX / cellW);
			const auto row = static_cast<uint32_t>(relY / cellH);
			const ImVec2 hoverMin{atlasTopLeft.x + static_cast<float>(col) * cellW,
								  atlasTopLeft.y + static_cast<float>(row) * cellH};
			const ImVec2 hoverMax{hoverMin.x + cellW, hoverMin.y + cellH};
			drawList->AddRect(hoverMin, hoverMax, IM_COL32(150, 220, 255, 200), 0.f, 1.5f);
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				m_inspectedTile = static_cast<int32_t>(row * m_tileset.columns + col);
			}
		}
	}

	drawList->PopClipRect();
}

void TilesetDocument::refreshSavedSnapshot() {
	const auto displayName = m_path.empty() ? std::string{"untitled"} : m_path.stem().string();
	m_savedSnapshot = m_tileset.serializeToString(displayName);
}

void TilesetDocument::resolveTexture() {}

}// namespace owl::nest
