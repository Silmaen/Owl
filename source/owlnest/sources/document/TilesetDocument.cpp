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
#include <gui/widgets/Menu.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Tooltip.h>
#include <gui/widgets/TreeRow.h>
#include <imgui_stdlib.h>
#include <platform/FileDialog.h>
#include <renderer/AtlasMips.h>
#include <renderer/Renderer.h>
#include <renderer/TextureDecoder.h>
#include <renderer/gpu/Texture.h>
#include <scene/TilemapTerrain.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

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

using AtlasCells = std::vector<std::tuple<uint32_t, uint32_t, std::vector<uint8_t>>>;

// One composition of the atlas: the metadata as YAML and the cells whose pixels changed, at their column and row.
class ModifyTileAtlasCommand final : public UndoCommand<scene::Tileset> {
public:
	// Grid and changed cells of one side of the edit.
	struct Side {
		std::string yaml;
		uint32_t columns = 1;
		uint32_t rows = 1;
		AtlasCells cells;
	};

	ModifyTileAtlasCommand(TilesetDocument& ioDocument, Side iBefore, Side iAfter, std::string iDescription)
		: m_document{ioDocument}, m_before{std::move(iBefore)}, m_after{std::move(iAfter)},
		  m_description{std::move(iDescription)} {}

	void undo(scene::Tileset& ioTarget) override { apply(ioTarget, m_before); }

	void redo(scene::Tileset& ioTarget) override { apply(ioTarget, m_after); }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

private:
	void apply(scene::Tileset& ioTarget, const Side& iSide) const {
		const auto texture = ioTarget.texture;
		const auto normal = ioTarget.normalTexture;
		const auto roughness = ioTarget.roughnessTexture;
		std::ignore = ioTarget.deserializeFromString(iSide.yaml);
		ioTarget.texture = texture;
		ioTarget.normalTexture = normal;
		ioTarget.roughnessTexture = roughness;
		m_document.restoreAtlas(iSide.columns, iSide.rows, iSide.cells);
	}

	TilesetDocument& m_document;
	Side m_before;
	Side m_after;
	std::string m_description;
};

auto hashPixels(const std::vector<uint8_t>& iPixels) -> uint64_t {
	uint64_t hash = 14695981039346656037ull;
	for (const auto byte: iPixels) {
		hash ^= byte;
		hash *= 1099511628211ull;
	}
	return hash;
}

auto isImageFile(const std::filesystem::path& iPath) -> bool {
	const auto extension = iPath.extension().string();
	return std::ranges::contains(renderer::gpu::Texture2D::extension(), extension);
}

// The image files of a list, a folder standing for its images sorted by name.
auto expandImages(const std::vector<std::filesystem::path>& iPaths) -> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> result;
	for (const auto& path: iPaths) {
		if (std::error_code error; std::filesystem::is_directory(path, error)) {
			std::set<std::filesystem::path> images;
			for (const auto& entry: std::filesystem::directory_iterator(path, error)) {
				if (entry.is_regular_file() && isImageFile(entry.path()))
					images.insert(entry.path());
			}
			result.insert(result.end(), images.begin(), images.end());
		} else if (isImageFile(path)) {
			result.push_back(path);
		}
	}
	return result;
}

// An image file as RGBA8 pixels, top row first.
auto readImage(const std::filesystem::path& iPath) -> std::optional<std::pair<std::vector<uint8_t>, math::vec2ui>> {
	const auto decoded = renderer::decodeImageFile(iPath, 4);
	if (!decoded.valid) {
		OWL_WARN("TilesetDocument: Cannot read the image '{}'.", iPath.string())
		return std::nullopt;
	}
	return std::pair{scene::TilesetAtlas::flipRows(decoded.pixels, decoded.size), decoded.size};
}

// A path given relative to an asset directory (Content Browser payload, texture name), resolved on disk.
auto resolveAssetPath(const std::filesystem::path& iPath) -> std::filesystem::path {
	if (iPath.is_absolute() || !app::Application::instanced())
		return iPath;
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (const auto candidate = assetsPath / iPath; exists(candidate))
			return candidate;
	}
	return iPath;
}

// The cells whose pixels differ between two atlases, by column and row.
auto diffAtlases(const scene::TilesetAtlas& iBefore, const scene::TilesetAtlas& iAfter)
		-> std::pair<AtlasCells, AtlasCells> {
	std::pair<AtlasCells, AtlasCells> result;
	const auto cellOf = [](const scene::TilesetAtlas& iAtlas, const uint32_t iColumn,
						   const uint32_t iRow) -> std::vector<uint8_t> {
		if (iColumn >= iAtlas.getColumns() || iRow >= iAtlas.getRows())
			return std::vector<uint8_t>(static_cast<size_t>(iAtlas.getTileSize().surface()) * 4, 0);
		return iAtlas.getTile(iRow * iAtlas.getColumns() + iColumn);
	};
	for (uint32_t row = 0; row < std::max(iBefore.getRows(), iAfter.getRows()); ++row) {
		for (uint32_t column = 0; column < std::max(iBefore.getColumns(), iAfter.getColumns()); ++column) {
			auto before = cellOf(iBefore, column, row);
			auto after = cellOf(iAfter, column, row);
			if (before == after)
				continue;
			result.first.emplace_back(column, row, std::move(before));
			result.second.emplace_back(column, row, std::move(after));
		}
	}
	return result;
}

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

// A slope name as a property label: every word capitalised ("Edge Back-Right").
auto toTitleCase(const std::string_view iName) -> std::string {
	std::string label{iName};
	bool wordStart = true;
	for (auto& character: label) {
		if (wordStart)
			character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
		wordStart = character == ' ' || character == '-';
	}
	return label;
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
	return m_tileset.serializeToString(displayName) != m_savedSnapshot || m_atlasHash != m_savedAtlasHash;
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
	if (m_atlas && m_atlasHash != m_savedAtlasHash) {
		if (!renderer::writeImagePng(m_atlasFile, m_atlas->getSize(), m_atlas->getPixels())) {
			OWL_CORE_ERROR("TilesetDocument: failed to write the atlas '{}'.", m_atlasFile.string())
			return false;
		}
		m_savedAtlasHash = m_atlasHash;
	}
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
	m_atlas.reset();
	m_atlasHash = m_savedAtlasHash = 0;
	resolveTexture();
	refreshSavedSnapshot();
	return true;
}

auto TilesetDocument::ensureAtlas() -> bool {
	if (m_atlas)
		return true;
	const math::vec2ui tile{m_tileset.tileWidth, m_tileset.tileHeight};
	if (!m_tileset.texture) {
		if (m_path.empty()) {
			OWL_WARN("TilesetDocument: Save the tileset before composing its atlas.")
			return false;
		}
		m_atlasFile = m_path.parent_path() / (m_path.stem().string() + ".png");
		if (exists(m_atlasFile)) {
			OWL_WARN("TilesetDocument: '{}' exists, drop it as the atlas texture to compose it.", m_atlasFile.string())
			return false;
		}
		m_atlas.emplace(tile, m_tileset.columns, m_tileset.rows);
		if (!renderer::writeImagePng(m_atlasFile, m_atlas->getSize(), m_atlas->getPixels())) {
			OWL_WARN("TilesetDocument: Cannot create the atlas '{}'.", m_atlasFile.string())
			m_atlas.reset();
			return false;
		}
		m_tileset.texture = renderer::gpu::Texture2D::create(m_atlasFile);
		m_tileset.applyFilterMode();
		m_atlasHash = m_savedAtlasHash = hashPixels(m_atlas->getPixels());
		return true;
	}
	m_atlasFile = m_tileset.texture->getPath();
	if (m_atlasFile.empty())
		m_atlasFile = resolveAssetPath(m_tileset.texture->getName());
	const auto image = readImage(m_atlasFile);
	if (!image) {
		OWL_WARN("TilesetDocument: The atlas texture has no image file to compose.")
		return false;
	}
	const math::vec2ui grid{tile.x() * m_tileset.columns, tile.y() * m_tileset.rows};
	if (image->second.x() > grid.x() || image->second.y() > grid.y()) {
		OWL_WARN("TilesetDocument: The {}x{} atlas image exceeds the {}x{} grid, fix the grid first.",
				 image->second.x(), image->second.y(), grid.x(), grid.y())
		return false;
	}
	m_atlas = scene::TilesetAtlas::fromImage(image->first, image->second, tile, m_tileset.columns, m_tileset.rows);
	m_atlasHash = hashPixels(m_atlas->getPixels());
	m_savedAtlasHash = image->second == grid ? m_atlasHash : 0;
	return true;
}

auto TilesetDocument::composeAtlas(const std::string& iDescription, const std::function<bool()>& iMutator) -> bool {
	if (!ensureAtlas())
		return false;
	ModifyTileAtlasCommand::Side before{.yaml = snapshotTileset(m_tileset),
										.columns = m_atlas->getColumns(),
										.rows = m_atlas->getRows(),
										.cells = {}};
	const auto previous = *m_atlas;
	if (!iMutator())
		return false;
	ModifyTileAtlasCommand::Side after{.yaml = snapshotTileset(m_tileset),
									   .columns = m_atlas->getColumns(),
									   .rows = m_atlas->getRows(),
									   .cells = {}};
	std::tie(before.cells, after.cells) = diffAtlases(previous, *m_atlas);
	if (before.cells.empty() && before.yaml == after.yaml && before.columns == after.columns &&
		before.rows == after.rows)
		return false;
	m_undoManager.push(mkUniq<ModifyTileAtlasCommand>(*this, std::move(before), std::move(after), iDescription));
	m_atlasHash = hashPixels(m_atlas->getPixels());
	uploadAtlas();
	return true;
}

void TilesetDocument::restoreAtlas(const uint32_t iColumns, const uint32_t iRows, const AtlasCells& iCells) {
	if (!m_atlas)
		return;
	m_atlas->resize(iColumns, iRows);
	for (const auto& [column, row, pixels]: iCells) {
		if (column < iColumns && row < iRows)
			std::ignore = m_atlas->setTile(row * iColumns + column, pixels, m_atlas->getTileSize());
	}
	m_atlasHash = hashPixels(m_atlas->getPixels());
	uploadAtlas();
}

void TilesetDocument::uploadAtlas() {
	if (!m_atlas || !m_tileset.texture)
		return;
	const auto size = m_atlas->getSize();
	auto pixels = scene::TilesetAtlas::flipRows(m_atlas->getPixels(), size);
	if (m_tileset.filterMode == renderer::gpu::FilterMode::PixelArt) {
		std::ignore = m_tileset.texture->setLevels(renderer::buildCellMips(pixels, size, m_atlas->getTileSize()));
		return;
	}
	std::ignore = m_tileset.texture->setLevels(
			{.size = size, .format = renderer::gpu::ImageFormat::Rgba8, .levels = {std::move(pixels)}});
}

auto TilesetDocument::setTileImage(const uint32_t iIndex, const std::filesystem::path& iImage) -> bool {
	return insertTileImages(iIndex, {iImage});
}

auto TilesetDocument::insertTileImages(const uint32_t iFrom, const std::vector<std::filesystem::path>& iImages)
		-> bool {
	std::vector<std::pair<std::vector<uint8_t>, math::vec2ui>> images;
	for (const auto& path: expandImages(iImages)) {
		if (auto image = readImage(path); image)
			images.push_back(std::move(*image));
	}
	if (images.empty()) {
		OWL_WARN("TilesetDocument: No image to put in the atlas.")
		return false;
	}
	const auto description = images.size() == 1 ? std::format("Set the image of tile {}", iFrom)
												: std::format("Insert {} tile images", images.size());
	return composeAtlas(description, [this, iFrom, &images]() -> bool {
		const auto [slots, rows] = m_atlas->planInsertion(iFrom, static_cast<uint32_t>(images.size()));
		if (rows > m_atlas->getRows()) {
			m_atlas->resize(m_atlas->getColumns(), rows);
			while (m_tileset.rows < rows) m_tileset.addRow();
		}
		bool changed = false;
		for (size_t i = 0; i < slots.size(); ++i)
			changed = m_atlas->setTile(slots[i], images[i].first, images[i].second) || changed;
		return changed;
	});
}

auto TilesetDocument::clearTiles(const std::vector<uint32_t>& iTiles) -> bool {
	return composeAtlas(iTiles.size() == 1 ? "Clear a tile" : std::format("Clear {} tiles", iTiles.size()),
						[this, &iTiles]() -> bool {
							for (const auto index: iTiles) {
								m_atlas->clearTile(index);
								if (index < m_tileset.tiles.size())
									m_tileset.tiles[index] = scene::TileMeta{};
							}
							return !iTiles.empty();
						});
}

auto TilesetDocument::moveTiles(const std::vector<uint32_t>& iTiles, const int32_t iOffset) -> bool {
	if (iTiles.empty() || iOffset == 0)
		return false;
	const auto count = static_cast<int64_t>(m_tileset.tileCount());
	for (const auto index: iTiles) {
		if (const auto target = static_cast<int64_t>(index) + iOffset; target < 0 || target >= count) {
			OWL_WARN("TilesetDocument: Tile {} would leave the grid, nothing moves.", index)
			return false;
		}
	}
	auto order = iTiles;
	std::ranges::sort(order);
	if (iOffset > 0)
		std::ranges::reverse(order);
	const bool moved = composeAtlas(std::format("Move {} tiles", iTiles.size()), [this, &order, iOffset]() -> bool {
		for (const auto index: order) {
			const auto target = static_cast<uint32_t>(static_cast<int64_t>(index) + iOffset);
			m_atlas->swapTiles(index, target);
			m_tileset.swapTiles(index, target);
		}
		return true;
	});
	if (moved) {
		for (auto& index: m_selectedTiles) index = static_cast<uint32_t>(static_cast<int64_t>(index) + iOffset);
		std::ranges::sort(m_selectedTiles);
		if (m_inspectedTile >= 0)
			m_inspectedTile += iOffset;
	}
	return moved;
}

void TilesetDocument::resizeGrid(const uint32_t iColumns, const uint32_t iRows) {
	const auto columns = std::max(1u, iColumns);
	const auto rows = std::max(1u, iRows);
	const auto mutate = [this, columns, rows]() -> bool {
		while (m_tileset.columns < columns) m_tileset.addColumn();
		while (m_tileset.columns > columns) m_tileset.removeColumn();
		while (m_tileset.rows < rows) m_tileset.addRow();
		while (m_tileset.rows > rows) m_tileset.removeRow();
		if (m_atlas)
			m_atlas->resize(columns, rows);
		return true;
	};
	const auto description = std::format("Resize the grid to {}x{}", columns, rows);
	if (m_atlas) {
		std::ignore = composeAtlas(description, mutate);
		return;
	}
	pushTilesetEdit(m_tileset, m_undoManager, description, mutate);
}

void TilesetDocument::selectTile(const uint32_t iIndex, const bool iToggle, const bool iRange) {
	if (iRange && m_inspectedTile >= 0) {
		const auto [low, high] = std::minmax(static_cast<uint32_t>(m_inspectedTile), iIndex);
		m_selectedTiles.clear();
		for (uint32_t index = low; index <= high; ++index) m_selectedTiles.push_back(index);
		return;
	}
	if (iToggle) {
		if (const auto found = std::ranges::find(m_selectedTiles, iIndex); found != m_selectedTiles.end()) {
			m_selectedTiles.erase(found);
			if (m_inspectedTile == static_cast<int32_t>(iIndex))
				m_inspectedTile = m_selectedTiles.empty() ? -1 : static_cast<int32_t>(m_selectedTiles.back());
			return;
		}
		m_selectedTiles.insert(std::ranges::upper_bound(m_selectedTiles, iIndex), iIndex);
	} else {
		m_selectedTiles = {iIndex};
	}
	m_inspectedTile = static_cast<int32_t>(iIndex);
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
	if (!gui::widgets::sectionHeader("Atlas", "owltileset_icon", false).open)
		return;
	if (!gui::widgets::beginProperties("##atlas")) {
		gui::widgets::endSection();
		return;
	}
	// Unsaved composed pixels belong to the current atlas image: its texture and tile size wait for the save.
	const bool composing = m_atlas && m_atlasHash != m_savedAtlasHash;
	const auto textureBefore = snapshotTileset(m_tileset);
	const float thumbnail = ImGui::GetFrameHeight() * 1.6f;
	const auto textureRow = [&](const char* iLabel, const char* iId, shared<renderer::gpu::Texture2D>& ioMap,
								const char* iTooltip) -> bool {
		gui::widgets::propertyLabel(iLabel, iTooltip);
		ImGui::BeginDisabled(composing);
		const bool changed = gui::widgets::textureField(iId, ioMap, ImVec2{thumbnail, thumbnail});
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginGroup();
		if (ioMap) {
			ImGui::TextUnformatted(ioMap->getName().c_str());
			ImGui::TextDisabled("%u x %u px", ioMap->getSize().x(), ioMap->getSize().y());
		} else {
			ImGui::TextDisabled("None");
			ImGui::TextDisabled("Drop an image");
		}
		ImGui::EndGroup();
		if (composing)
			gui::widgets::itemTooltip(iLabel, nullptr,
									  "Save the tileset first: its composed atlas is not written yet.");
		std::ignore = gui::widgets::propertyReset(false);
		return changed;
	};
	if (textureRow("Texture", "##texture", m_tileset.texture,
				   "Colours of the tiles, alpha included. Drop a texture from the Content Browser.")) {
		m_atlas.reset();
		m_tileset.applyFilterMode();
	}
	if (textureRow("Normal Map", "##normal", m_tileset.normalTexture,
				   "Tangent-space normals on the grid of the atlas (OpenGL, green up): the relief of the tiles in a "
				   "voxel world. Drop a texture from the Content Browser."))
		m_tileset.applyFilterMode();
	if (textureRow("Roughness Map", "##roughness", m_tileset.roughnessTexture,
				   "Roughness on the grid of the atlas: 50 % grey keeps the roughness of a voxel block, darker is "
				   "smoother. Drop a texture from the Content Browser."))
		m_tileset.applyFilterMode();
	if (const auto textureAfter = snapshotTileset(m_tileset); textureAfter != textureBefore) {
		auto cmd = mkUniq<ModifyTilesetCommand>(textureBefore, textureAfter, "Set texture");
		m_undoManager.push(std::move(cmd));
	}
	// A count with the buttons that add or remove a column or a row.
	const auto countRow = [](const char* iLabel, const uint32_t iCount, const char* iWhat, auto&& iAdd,
							 auto&& iRemove) -> void {
		gui::widgets::propertyLabel(iLabel);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%u", iCount);
		ImGui::SameLine();
		const auto add = std::format("Add a {}", iWhat);
		const auto remove = std::format("Remove a {}", iWhat);
		if (gui::widgets::iconButton("##add", {.glyph = gui::icon::plus, .tooltip = add.c_str()}))
			iAdd();
		ImGui::SameLine();
		if (gui::widgets::iconButton("##remove",
									 {.glyph = gui::icon::minus, .tooltip = remove.c_str(), .disabled = iCount <= 1}))
			iRemove();
		std::ignore = gui::widgets::propertyReset(false);
	};
	countRow(
			"Columns", m_tileset.columns, "column",
			[this]() -> void { resizeGrid(m_tileset.columns + 1, m_tileset.rows); },
			[this]() -> void { resizeGrid(m_tileset.columns - 1, m_tileset.rows); });
	countRow(
			"Rows", m_tileset.rows, "row", [this]() -> void { resizeGrid(m_tileset.columns, m_tileset.rows + 1); },
			[this]() -> void { resizeGrid(m_tileset.columns, m_tileset.rows - 1); });
	ImGui::BeginDisabled(composing);
	auto tileWidth = static_cast<int>(m_tileset.tileWidth);
	auto tileHeight = static_cast<int>(m_tileset.tileHeight);
	if (gui::widgets::propertyInt("Tile Width", tileWidth, tileWidth, 1, 1024, "%d px")) {
		m_tileset.tileWidth = static_cast<uint32_t>(tileWidth);
		m_atlas.reset();
		m_tileset.applyFilterMode();
	}
	if (gui::widgets::propertyInt("Tile Height", tileHeight, tileHeight, 1, 1024, "%d px")) {
		m_tileset.tileHeight = static_cast<uint32_t>(tileHeight);
		m_atlas.reset();
		m_tileset.applyFilterMode();
	}
	ImGui::EndDisabled();

	using renderer::gpu::FilterMode;
	const auto current = m_tileset.filterMode;
	gui::widgets::propertyLabel("Filter", "Nearest keeps the pixels sharp, Linear smooths them. Pixel Art keeps them "
										  "sharp up close and filters them far away, each tile on its own (mipmaps "
										  "built tile by tile, best with power-of-two tile sizes).");
	constexpr std::array filters{std::pair{FilterMode::Linear, "Linear"}, std::pair{FilterMode::Nearest, "Nearest"},
								 std::pair{FilterMode::PixelArt, "Pixel Art"}};
	const auto* currentFilter = std::ranges::find(filters, current, &std::pair<FilterMode, const char*>::first);
	if (gui::widgets::beginCombo("##filter", currentFilter != filters.end() ? currentFilter->second : "Linear")) {
		for (const auto& [mode, label]: filters) {
			if (ImGui::Selectable(label, current == mode) && current != mode)
				pushTilesetEdit(m_tileset, m_undoManager, std::format("Set filter to {}", label),
								[this, mode]() -> void {
									m_tileset.filterMode = mode;
									m_tileset.applyFilterMode();
									uploadAtlas();
								});
		}
		ImGui::EndCombo();
	}
	if (gui::widgets::propertyReset(current != FilterMode::Linear))
		pushTilesetEdit(m_tileset, m_undoManager, "Set filter to Linear", [this]() -> void {
			m_tileset.filterMode = FilterMode::Linear;
			m_tileset.applyFilterMode();
		});

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
	if (!gui::widgets::sectionHeader("Slopes", "comp_voxel_world", false).open)
		return;
	if (gui::widgets::button("addSlopeSet", {.glyph = gui::icon::plus,
											 .label = "Add Terrain",
											 .tooltip = "Add Terrain",
											 .description = "Tile drawn for a terrain or overlay id on each slope; a "
															"foundation replaces the terrain tile of a cell whose "
															"flag bit 0 is set."})) {
		pushTilesetEdit(m_tileset, m_undoManager, "Add slope table", [this]() -> void {
			scene::SlopeSet set;
			set.id = std::max(0, m_inspectedTile);
			set.tiles[scene::terrain::flatSlope] = set.id;
			m_tileset.slopeSets.push_back(set);
		});
	}
	const int lastTile = static_cast<int>(m_tileset.tileCount()) - 1;
	std::optional<size_t> eraseIndex;
	for (size_t i = 0; i < m_tileset.slopeSets.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		const auto& set = m_tileset.slopeSets[i];
		const auto label = std::format("Terrain {}", set.id);
		const auto* flat = set.id >= 0 && std::cmp_less(set.id, m_tileset.tiles.size())
								   ? &m_tileset.tiles[static_cast<size_t>(set.id)].name
								   : nullptr;
		const auto row = gui::widgets::treeRow("##set", {.label = label.c_str(),
														 .iconName = "comp_voxel_world",
														 .trailing = flat != nullptr ? flat->c_str() : nullptr,
														 .hasChildren = true});
		if (ImGui::BeginPopupContextItem("##setMenu")) {
			gui::widgets::menuHeader(label.c_str(), "Terrain");
			if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Delete Terrain", .danger = true}))
				eraseIndex = i;
			ImGui::EndPopup();
		}
		if (row.open) {
			if (gui::widgets::beginProperties("##slopeSet")) {
				int id = set.id;
				if (gui::widgets::propertyInt("Id", id, 0, 0, scene::component::g_MaxTileIndex, "%d",
											  "Terrain or overlay id stored in the cells."))
					pushTilesetEdit(m_tileset, m_undoManager, "Set slope table id",
									[this, i, id]() -> void { m_tileset.slopeSets[i].id = id; });
				gui::widgets::endProperties();
			}
			if (gui::widgets::subHeader("Slope Tiles", true) && gui::widgets::beginProperties("##slopeTiles")) {
				for (uint8_t slope = 0; slope < scene::terrain::slopeCount; ++slope) {
					const auto name = toTitleCase(scene::terrain::getSlopeName(slope));
					int tile = set.tiles[slope];
					if (gui::widgets::propertyInt(name.c_str(), tile, -1, -1, lastTile, "%d",
												  "Tile index on this slope (-1: the flat tile)."))
						pushTilesetEdit(m_tileset, m_undoManager, "Set slope tile", [this, i, slope, tile]() -> void {
							m_tileset.slopeSets[i].tiles[slope] = tile;
						});
				}
				gui::widgets::endProperties();
			}
			if (gui::widgets::subHeader("Foundations") && gui::widgets::beginProperties("##foundations")) {
				for (uint8_t slope = 0; slope < scene::terrain::slopeCount; ++slope) {
					const auto name = toTitleCase(scene::terrain::getSlopeName(slope));
					int tile = set.foundations[slope];
					if (gui::widgets::propertyInt(name.c_str(), tile, -1, -1, lastTile, "%d",
												  "Foundation tile on this slope (-1: none)."))
						pushTilesetEdit(
								m_tileset, m_undoManager, "Set foundation tile",
								[this, i, slope, tile]() -> void { m_tileset.slopeSets[i].foundations[slope] = tile; });
				}
				gui::widgets::endProperties();
			}
			if (gui::widgets::button("##deleteSet", {.glyph = gui::icon::trash2, .label = "Delete Terrain"}))
				eraseIndex = i;
			gui::widgets::endTreeRow();
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
	if (gui::widgets::sectionHeader("Tile", "properties", false).open) {
		if (gui::widgets::beginProperties("##tileMeta")) {
			std::ignore = gui::widgets::propertyText("Name", meta.name, "grass",
													 "Optional name shown in the Tile Palette tooltip.");
			std::ignore = gui::widgets::propertyBool("Collidable", meta.collidable, false,
													 "Solid tiles produce a static collider in the physics world.");
			gui::widgets::endProperties();
		}
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Navigation", "ribbon_flow", false).open) {
		renderConnectionsSection();
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Raycast", "ribbon_viewpoint", false).open) {
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

void TilesetDocument::renderConnectionsSection() {
	const auto index = static_cast<size_t>(m_inspectedTile);
	const auto connections = m_tileset.tiles[index].connections;
	if (!gui::widgets::beginProperties("##tileNavigation"))
		return;
	bool road = connections.has_value();
	if (gui::widgets::propertyBool(
				"Road", road, false,
				"Vehicles and walkers travel on a cell holding this tile (as overlay) and leave it by "
				"the joined sides."))
		pushTilesetEdit(m_tileset, m_undoManager, road ? "Make tile a road" : "Make tile no road",
						[this, index, road]() -> void {
							m_tileset.tiles[index].connections =
									road ? std::optional<uint8_t>{0} : std::optional<uint8_t>{};
						});
	if (!road) {
		gui::widgets::endProperties();
		return;
	}
	// The four sides as they look on the screen of the isometric view: north up-right, east down-right.
	struct Side {
		scene::nav::Direction direction;
		const char* glyph;
		const char* id;
		const char* name;
	};
	constexpr std::array<std::array<Side, 2>, 2> layout{{
			{{{scene::nav::Direction::West, gui::icon::arrowUpLeft, "##west", "West"},
			  {scene::nav::Direction::North, gui::icon::arrowUpRight, "##north", "North"}}},
			{{{scene::nav::Direction::South, gui::icon::arrowDownLeft, "##south", "South"},
			  {scene::nav::Direction::East, gui::icon::arrowDownRight, "##east", "East"}}},
	}};
	const uint8_t mask = connections.value_or(0);
	std::optional<scene::nav::Direction> toggled;
	gui::widgets::propertyLabel("Joined Sides", "Sides a road leaves the cell by, as they look on the screen of the "
												"isometric view. Click a side to join or cut it.");
	ImGui::BeginGroup();
	for (const auto& row: layout) {
		for (size_t column = 0; column < row.size(); ++column) {
			const auto& entry = row[column];
			if (column != 0)
				ImGui::SameLine();
			const bool on = (mask & scene::nav::directionBit(entry.direction)) != 0;
			const auto tooltip = std::format("{}: {}", entry.name, on ? "joined" : "cut");
			if (gui::widgets::iconButton(entry.id, {.glyph = entry.glyph, .tooltip = tooltip.c_str(), .selected = on}))
				toggled = entry.direction;
		}
	}
	ImGui::EndGroup();
	if (gui::widgets::propertyReset(mask != 0))
		pushTilesetEdit(m_tileset, m_undoManager, "Change tile connections",
						[this, index]() -> void { m_tileset.tiles[index].connections = 0; });
	gui::widgets::endProperties();
	if (toggled.has_value()) {
		const uint8_t bit = scene::nav::directionBit(*toggled);
		pushTilesetEdit(m_tileset, m_undoManager, "Change tile connections", [this, index, bit]() -> void {
			auto& value = m_tileset.tiles[index].connections;
			value = static_cast<uint8_t>(value.value_or(0) ^ bit);
		});
	}
}

void TilesetDocument::renderCanvas() {
	const auto availSize = ImGui::GetContentRegionAvail();
	if (availSize.x <= 0.f || availSize.y <= 0.f)
		return;
	const bool hasTexture = m_tileset.texture && m_tileset.texture->isLoaded();
	if (!hasTexture)
		ImGui::TextDisabled("Drop images on the slots to compose the atlas, or a texture in the Tileset panel.");

	const auto canvasOrigin = ImGui::GetCursorScreenPos();
	const auto canvasAvail = ImGui::GetContentRegionAvail();
	const ImVec2 canvasSize{std::max(1.f, canvasAvail.x), std::max(1.f, canvasAvail.y)};
	auto* drawList = ImGui::GetWindowDrawList();
	const float texW = static_cast<float>(m_tileset.columns * std::max(1u, m_tileset.tileWidth));
	const float texH = static_cast<float>(m_tileset.rows * std::max(1u, m_tileset.tileHeight));
	const ImVec2 atlasTopLeft{canvasOrigin.x + m_panOffset.x(), canvasOrigin.y + m_panOffset.y()};
	const ImVec2 atlasBottomRight{atlasTopLeft.x + texW * m_zoom, atlasTopLeft.y + texH * m_zoom};
	const float cellW = static_cast<float>(m_tileset.tileWidth) * m_zoom;
	const float cellH = static_cast<float>(m_tileset.tileHeight) * m_zoom;
	const auto& io = ImGui::GetIO();
	const auto cellAt = [&](const ImVec2& iPos) -> std::optional<uint32_t> {
		const float relX = iPos.x - atlasTopLeft.x;
		const float relY = iPos.y - atlasTopLeft.y;
		if (relX < 0.f || relY < 0.f || relX >= texW * m_zoom || relY >= texH * m_zoom)
			return std::nullopt;
		return static_cast<uint32_t>(relY / cellH) * m_tileset.columns + static_cast<uint32_t>(relX / cellW);
	};

	ImGui::InvisibleButton("##atlas_canvas", canvasSize,
						   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
								   ImGuiButtonFlags_MouseButtonRight);
	// An image or a folder dropped on a slot composes the atlas; any other asset opens as a document tab.
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const auto* data = static_cast<const char*>(payload->Data);
			const auto length = ::strnlen(data, static_cast<size_t>(payload->DataSize));
			const auto resolved = resolveAssetPath(std::filesystem::path{std::string(data, length)});
			const auto slot = cellAt(io.MousePos);
			if (std::error_code error;
				slot && (isImageFile(resolved) || std::filesystem::is_directory(resolved, error)))
				std::ignore = insertTileImages(*slot, {resolved});
			else if (mp_editorLayer != nullptr && exists(resolved))
				mp_editorLayer->requestDeferredOpen(resolved);
		}
		ImGui::EndDragDropTarget();
	}
	const bool hovered = ImGui::IsItemHovered();
	const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
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
	const auto hoveredCell = hovered ? cellAt(io.MousePos) : std::nullopt;
	if (hoveredCell && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		const bool selected = std::ranges::contains(m_selectedTiles, *hoveredCell);
		if (io.KeyCtrl || io.KeyShift || !selected)
			selectTile(*hoveredCell, io.KeyCtrl, io.KeyShift);
		m_dragFrom = io.KeyCtrl || io.KeyShift ? std::nullopt : hoveredCell;
	}
	if (m_dragFrom && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		if (const auto target = cellAt(io.MousePos); target && *target != *m_dragFrom)
			std::ignore = moveTiles(m_selectedTiles, static_cast<int32_t>(*target) - static_cast<int32_t>(*m_dragFrom));
		else if (target)
			selectTile(*target);
		m_dragFrom.reset();
	}
	if (hoveredCell && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		if (!std::ranges::contains(m_selectedTiles, *hoveredCell))
			selectTile(*hoveredCell);
		ImGui::OpenPopup("##atlasMenu");
	}
	if (focused && !m_selectedTiles.empty() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		std::ignore = clearTiles(m_selectedTiles);
	if (focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
		m_inspectedTile = 0;
		selectTile(m_tileset.tileCount() - 1, false, true);
	}
	renderCanvasMenu();

	drawList->PushClipRect(canvasOrigin, ImVec2{canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y}, true);
	drawList->AddRectFilled(atlasTopLeft, atlasBottomRight, IM_COL32(40, 40, 50, 255));
	if (hasTexture)
		drawList->AddImage(static_cast<ImTextureID>(m_tileset.texture->getRendererId()), atlasTopLeft, atlasBottomRight,
						   {0.f, 1.f}, {1.f, 0.f});
	const ImU32 gridColor = IM_COL32(120, 120, 140, 160);
	for (uint32_t c = 0; c <= m_tileset.columns; ++c) {
		const float x = atlasTopLeft.x + static_cast<float>(c) * cellW;
		drawList->AddLine({x, atlasTopLeft.y}, {x, atlasBottomRight.y}, gridColor);
	}
	for (uint32_t r = 0; r <= m_tileset.rows; ++r) {
		const float y = atlasTopLeft.y + static_cast<float>(r) * cellH;
		drawList->AddLine({atlasTopLeft.x, y}, {atlasBottomRight.x, y}, gridColor);
	}
	const auto cellRect = [&](const uint32_t iIndex) -> std::pair<ImVec2, ImVec2> {
		const uint32_t column = iIndex % m_tileset.columns;
		const uint32_t row = iIndex / m_tileset.columns;
		const ImVec2 min{atlasTopLeft.x + static_cast<float>(column) * cellW,
						 atlasTopLeft.y + static_cast<float>(row) * cellH};
		return {min, ImVec2{min.x + cellW, min.y + cellH}};
	};
	for (uint32_t idx = 0;
		 idx < std::min<uint32_t>(m_tileset.tileCount(), static_cast<uint32_t>(m_tileset.tiles.size())); ++idx) {
		if (m_tileset.tiles[idx].collidable) {
			const auto [min, max] = cellRect(idx);
			drawList->AddRectFilled(min, max, IM_COL32(255, 90, 90, 60));
		}
	}
	for (const auto idx: m_selectedTiles) {
		const auto [min, max] = cellRect(idx);
		drawList->AddRectFilled(min, max, IM_COL32(255, 195, 38, 40));
		drawList->AddRect(min, max, IM_COL32(255, 195, 38, 255), 0.f,
						  static_cast<int32_t>(idx) == m_inspectedTile ? 3.f : 1.5f);
	}
	if (hoveredCell) {
		const auto [min, max] = cellRect(*hoveredCell);
		drawList->AddRect(min, max, IM_COL32(150, 220, 255, 200), 0.f, 1.5f);
		if (m_dragFrom && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && *hoveredCell != *m_dragFrom)
			drawList->AddRectFilled(min, max, IM_COL32(150, 220, 255, 60));
	}
	drawList->PopClipRect();
}

void TilesetDocument::renderCanvasMenu() {
	if (!ImGui::BeginPopup("##atlasMenu"))
		return;
	const auto first = m_selectedTiles.empty() ? 0u : m_selectedTiles.front();
	if (ImGui::MenuItem("Insert Image...")) {
		if (const auto file = platform::FileDialog::openFile("Image (*.png *.jpg)|png,jpg\n"); !file.empty())
			std::ignore = insertTileImages(first, {file});
	}
	if (ImGui::MenuItem("Insert Folder...")) {
		if (const auto folder = platform::FileDialog::pickFolder(); !folder.empty())
			std::ignore = insertTileImages(first, {folder});
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Clear Image", "Del", false, !m_selectedTiles.empty()))
		std::ignore = clearTiles(m_selectedTiles);
	if (ImGui::MenuItem("Select All", "Ctrl+A")) {
		m_inspectedTile = 0;
		selectTile(m_tileset.tileCount() - 1, false, true);
	}
	ImGui::EndPopup();
}

void TilesetDocument::refreshSavedSnapshot() {
	const auto displayName = m_path.empty() ? std::string{"untitled"} : m_path.stem().string();
	m_savedSnapshot = m_tileset.serializeToString(displayName);
}

void TilesetDocument::resolveTexture() {}

}// namespace owl::nest
