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
#include <gui/utils.h>
#include <gui/widgets/AssetField.h>
#include <imgui_stdlib.h>
#include <renderer/Renderer.h>
#include <renderer/gpu/Texture.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

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
	const auto winTitle = std::format("{}##tileset_{:x}", display, static_cast<uint64_t>(id()));

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

namespace {
void helpText(const char* iText) {
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::SetWindowFontScale(0.85f);
	ImGui::TextWrapped("%s", iText);
	ImGui::SetWindowFontScale(1.f);
	ImGui::PopStyleColor();
}
}// namespace

void TilesetDocument::renderHierarchyPanel() {

	// --- Texture slot ----------------------------------------------------------------------
	ImGui::TextUnformatted("Texture");
	const auto textureBefore = snapshotTileset(m_tileset);
	if (gui::widgets::textureField("##texture", m_tileset.texture, ImVec2{96.f, 96.f})) {
		if (m_tileset.texture)
			m_tileset.texture->setFilterMode(m_tileset.filterMode);
	}
	if (const auto textureAfter = snapshotTileset(m_tileset); textureAfter != textureBefore) {
		auto cmd = mkUniq<ModifyTilesetCommand>(textureBefore, textureAfter, "Set texture");
		m_undoManager.push(std::move(cmd));
	}

	ImGui::Text("Columns: %u", m_tileset.columns);
	ImGui::SameLine();
	if (ImGui::SmallButton("+##addCol"))
		pushTilesetEdit(m_tileset, m_undoManager, "Add column", [this]() -> void { m_tileset.addColumn(); });
	ImGui::SameLine();
	if (ImGui::SmallButton("-##removeCol"))
		pushTilesetEdit(m_tileset, m_undoManager, "Remove column", [this]() -> void { m_tileset.removeColumn(); });
	ImGui::Text("Rows: %u", m_tileset.rows);
	ImGui::SameLine();
	if (ImGui::SmallButton("+##addRow"))
		pushTilesetEdit(m_tileset, m_undoManager, "Add row", [this]() -> void { m_tileset.addRow(); });
	ImGui::SameLine();
	if (ImGui::SmallButton("-##removeRow"))
		pushTilesetEdit(m_tileset, m_undoManager, "Remove row", [this]() -> void { m_tileset.removeRow(); });

	int tileWidth = static_cast<int>(m_tileset.tileWidth);
	int tileHeight = static_cast<int>(m_tileset.tileHeight);
	if (ImGui::DragInt("Tile Width", &tileWidth, 1.f, 1, 1024))
		m_tileset.tileWidth = static_cast<uint32_t>(std::max(1, tileWidth));
	if (ImGui::DragInt("Tile Height", &tileHeight, 1.f, 1, 1024))
		m_tileset.tileHeight = static_cast<uint32_t>(std::max(1, tileHeight));

	// --- Filter mode -----------------------------------------------------------------------
	const auto& current = m_tileset.filterMode;
	const char* currentLabel = (current == renderer::gpu::FilterMode::Nearest) ? "Nearest" : "Linear";
	if (ImGui::BeginCombo("Filter", currentLabel)) {
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
}

void TilesetDocument::renderPropertiesPanel() {
	ImGui::TextDisabled("Tile Metadata");
	if (m_inspectedTile < 0) {
		helpText("Click a tile in the atlas to inspect / edit its metadata.");
		return;
	}
	if (static_cast<uint32_t>(m_inspectedTile) >= m_tileset.tileCount()) {
		ImGui::TextDisabled("Selected tile is out of range for the current tileset.");
		return;
	}
	auto& meta = m_tileset.tiles[static_cast<size_t>(m_inspectedTile)];

	ImGui::Text("Tile #%d", m_inspectedTile);
	ImGui::Separator();

	bool collidable = meta.collidable;
	if (ImGui::Checkbox("Collidable", &collidable))
		meta.collidable = collidable;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Solid tiles produce a static collider in the physics world.");

	std::string name = meta.name;
	ImGui::TextUnformatted("Name");
	ImGui::SameLine();
	if (ImGui::InputText("##tile_name", &name))
		meta.name = std::move(name);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Optional designer-friendly name shown in the tile palette tooltip.");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Raycast");
	float wallHeight = meta.wallHeight;
	if (ImGui::DragFloat("Wall height", &wallHeight, 0.05f, 0.f, 8.f, "%.2f"))
		meta.wallHeight = std::clamp(wallHeight, 0.f, 8.f);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Vertical scale of the wall in cell units when rendered by the raycast renderer. "
						  "1.0 is a standard ceiling-to-floor wall; larger values poke into the sky (towers), "
						  "smaller values keep the wall planted on the floor (low half-walls). Bottom-anchored. "
						  "Ignored by the legacy 2D rendering path.");

	bool transparent = meta.transparent;
	if (ImGui::Checkbox("Transparent (raycast)", &transparent))
		meta.transparent = transparent;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("When enabled, the raycast DDA does not stop at this tile — it records the hit and keeps "
						  "stepping (up to 8 transparent layers per ray). The collected hits are drawn back-to-front "
						  "so alpha-blended textures composite correctly. Combine with a PNG alpha channel or chroma "
						  "key for see-through pixels.");

	helpText("Transparency is alpha-channel only — author the tile texture with proper PNG alpha (RGBA) and toggle "
			 "the Transparent flag above. Chroma keying is intentionally not supported.");
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
				drawList->AddRect(cellMin, cellMax, IM_COL32(255, 195, 38, 255), 0.f, 0, 3.f);
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
			drawList->AddRect(hoverMin, hoverMax, IM_COL32(150, 220, 255, 200), 0.f, 0, 1.5f);
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
