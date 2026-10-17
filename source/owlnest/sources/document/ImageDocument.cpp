/**
 * @file ImageDocument.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ImageDocument.h"

#include "EditorLayer.h"

#include <app/Application.h>
#include <gui/IconFont.h>
#include <gui/utils.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <renderer/TextureDecoder.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>
#include <vector>

namespace owl::nest {

namespace {
constexpr float g_fitMargin = 16.f;
constexpr float g_checkerSize = 12.f;

auto toVec2(const ImVec2& iVec) -> math::vec2 { return {iVec.x, iVec.y}; }

auto toImVec2(const math::vec2& iVec) -> ImVec2 { return {iVec.x(), iVec.y()}; }

void drawCheckerboard(ImDrawList* iDraw, const ImVec2& iMin, const ImVec2& iMax) {
	constexpr auto light = IM_COL32(150, 150, 150, 255);
	constexpr auto dark = IM_COL32(100, 100, 100, 255);
	iDraw->AddRectFilled(iMin, iMax, dark);
	iDraw->PushClipRect(iMin, iMax, true);
	int row = 0;
	for (float y = iMin.y; y < iMax.y; y += g_checkerSize, ++row) {
		int column = row % 2;
		for (float x = iMin.x + static_cast<float>(column) * g_checkerSize; x < iMax.x; x += 2.f * g_checkerSize)
			iDraw->AddRectFilled({x, y}, {x + g_checkerSize, y + g_checkerSize}, light);
	}
	iDraw->PopClipRect();
}
}// namespace

ImageDocument::ImageDocument() = default;

ImageDocument::~ImageDocument() = default;

auto ImageDocument::title() const -> std::string {
	return m_path.empty() ? std::string{"Untitled"} : m_path.filename().string();
}

void ImageDocument::onAttach(EditorLayer* iEditor) { mp_editorLayer = iEditor; }

void ImageDocument::onDetach() {
	mp_editorLayer = nullptr;
	m_texture.reset();
}

void ImageDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {}

void ImageDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto ImageDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	const auto decoded = renderer::decodeImageFile(iPath);
	if (!decoded.valid) {
		OWL_WARN("ImageDocument: '{}' is not a readable image.", iPath.string())
		return false;
	}
	m_path = iPath;
	std::error_code error;
	const auto bytes = std::filesystem::file_size(iPath, error);
	m_info = {.size = decoded.size,
			  .channels = decoded.format == renderer::gpu::ImageFormat::Rgb8 ? 3u : 4u,
			  .fileBytes = error ? 0 : static_cast<uint64_t>(bytes)};
	m_texture.reset();
	m_needsFit = true;
	m_tilesets.clear();
	m_overlay.reset();
	if (app::Application::instanced()) {
		std::vector<std::filesystem::path> directories;
		for (const auto& directory: app::Application::get().getAssetDirectories())
			directories.push_back(directory.assetsPath);
		findTilesets(directories, directories);
	}
	return true;
}

void ImageDocument::findTilesets(const std::span<const std::filesystem::path> iRoots,
								 const std::span<const std::filesystem::path> iAssetDirectories) {
	m_tilesets = viewer::findTilesetGrids(iRoots, m_path, iAssetDirectories);
	m_overlay.reset();
}

void ImageDocument::setOverlay(const std::optional<size_t> iOverlay) {
	m_overlay = iOverlay.has_value() && *iOverlay < m_tilesets.size() ? iOverlay : std::nullopt;
}

void ImageDocument::onImGuiRender() {
	const auto display =
			(mp_editorLayer != nullptr) ? mp_editorLayer->getDocumentManager().displayTitleFor(this) : title();
	const auto winTitle = documentWindowTitle(DocumentType::Image, display, static_cast<uint64_t>(id()));
	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);
	const bool wantFocus = consumeFocusRequest();
	if (wantFocus)
		ImGui::SetNextWindowFocus();
	const bool open = ImGui::Begin(winTitle.c_str(), &m_pOpen,
								   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
										   ImGuiWindowFlags_NoScrollWithMouse);
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
		if (m_texture == nullptr)
			m_texture = renderer::gpu::Texture2D::create(m_path);
		if (const auto id = gui::imTexture(m_texture); !id.has_value()) {
			std::ignore = gui::widgets::emptyState(gui::icon::image, "Image not loaded",
												   "The image could not be uploaded to the GPU.");
		} else {
			const auto avail = ImGui::GetContentRegionAvail();
			drawCanvas({avail.x, std::max(avail.y - ImGui::GetFrameHeightWithSpacing(), 1.f)});
		}
	}
	ImGui::End();
}

void ImageDocument::drawCanvas(const math::vec2& iSize) {
	const auto origin = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##imageCanvas", toImVec2(iSize),
						   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();
	const auto& io = ImGui::GetIO();
	const auto& imageSize = m_info.size;
	if (m_needsFit && iSize.x() > 1.f) {
		m_view = viewer::centeredImageView(imageSize, iSize, viewer::fitImageZoom(imageSize, iSize, g_fitMargin));
		m_needsFit = false;
	}
	const math::vec2 cursor = toVec2({io.MousePos.x - origin.x, io.MousePos.y - origin.y});
	if (active &&
		(ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.f))) {
		m_view.offset = {m_view.offset.x() + io.MouseDelta.x, m_view.offset.y() + io.MouseDelta.y};
	}
	if (hovered && io.MouseWheel != 0.f)
		m_view = viewer::zoomImageView(m_view, cursor, viewer::stepImageZoom(m_view.zoom, io.MouseWheel));
	if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		m_view = viewer::centeredImageView(imageSize, iSize, viewer::fitImageZoom(imageSize, iSize, g_fitMargin));
	if (hovered && ImGui::IsKeyPressed(ImGuiKey_F, false))
		m_view = viewer::centeredImageView(imageSize, iSize, viewer::fitImageZoom(imageSize, iSize, g_fitMargin));
	if (hovered && ImGui::IsKeyPressed(ImGuiKey_1, false))
		m_view = viewer::zoomImageView(m_view, cursor, 1.f);

	auto* draw = ImGui::GetWindowDrawList();
	const ImVec2 canvasMax{origin.x + iSize.x(), origin.y + iSize.y()};
	draw->AddRectFilled(origin, canvasMax, IM_COL32(28, 28, 32, 255));
	draw->PushClipRect(origin, canvasMax, true);
	const auto topLeft = viewer::imageToView(m_view, {0.f, 0.f});
	const auto bottomRight =
			viewer::imageToView(m_view, {static_cast<float>(imageSize.x()), static_cast<float>(imageSize.y())});
	const ImVec2 min{origin.x + topLeft.x(), origin.y + topLeft.y()};
	const ImVec2 max{origin.x + bottomRight.x(), origin.y + bottomRight.y()};
	const ImVec2 visibleMin{std::max(min.x, origin.x), std::max(min.y, origin.y)};
	const ImVec2 visibleMax{std::min(max.x, canvasMax.x), std::min(max.y, canvasMax.y)};
	if (visibleMin.x < visibleMax.x && visibleMin.y < visibleMax.y)
		drawCheckerboard(draw, visibleMin, visibleMax);
	// Snap to the texels once the image is enlarged, smooth it when reduced.
	const auto wanted = m_view.zoom >= 1.f ? renderer::gpu::FilterMode::Nearest : renderer::gpu::FilterMode::Linear;
	if (m_texture->getFilterMode() != wanted)
		m_texture->setFilterMode(wanted);
	draw->AddImage(static_cast<ImTextureID>(m_texture->getRendererId()), min, max, {0.f, 1.f}, {1.f, 0.f});
	draw->AddRect(min, max, IM_COL32(200, 200, 210, 120));

	const bool gridVisible = m_showPixelGrid && viewer::isPixelGridVisible(m_view.zoom);
	if (gridVisible) {
		const auto first = viewer::viewToImage(m_view, {visibleMin.x - origin.x, visibleMin.y - origin.y});
		const auto last = viewer::viewToImage(m_view, {visibleMax.x - origin.x, visibleMax.y - origin.y});
		const ImU32 color = IM_COL32(255, 255, 255, 50);
		for (auto x = static_cast<int>(std::floor(first.x())); x <= static_cast<int>(std::ceil(last.x())); ++x) {
			const float screenX = origin.x + viewer::imageToView(m_view, {static_cast<float>(x), 0.f}).x();
			draw->AddLine({screenX, visibleMin.y}, {screenX, visibleMax.y}, color);
		}
		for (auto y = static_cast<int>(std::floor(first.y())); y <= static_cast<int>(std::ceil(last.y())); ++y) {
			const float screenY = origin.y + viewer::imageToView(m_view, {0.f, static_cast<float>(y)}).y();
			draw->AddLine({visibleMin.x, screenY}, {visibleMax.x, screenY}, color);
		}
	}

	const auto pixel = hovered ? viewer::pixelAt(m_view, cursor, imageSize) : std::nullopt;
	std::optional<uint32_t> hoveredTile;
	if (m_overlay.has_value()) {
		const auto& grid = m_tilesets[*m_overlay];
		const ImU32 lineColor = IM_COL32(255, 200, 60, 190);
		const auto cellWidth = static_cast<float>(grid.tileWidth);
		const auto cellHeight = static_cast<float>(grid.tileHeight);
		const auto gridWidth = cellWidth * static_cast<float>(grid.columns);
		const auto gridHeight = cellHeight * static_cast<float>(grid.rows);
		for (uint32_t column = 0; column <= grid.columns; ++column) {
			const auto top = viewer::imageToView(m_view, {static_cast<float>(column) * cellWidth, 0.f});
			const auto bottom = viewer::imageToView(m_view, {static_cast<float>(column) * cellWidth, gridHeight});
			draw->AddLine({origin.x + top.x(), origin.y + top.y()}, {origin.x + bottom.x(), origin.y + bottom.y()},
						  lineColor);
		}
		for (uint32_t row = 0; row <= grid.rows; ++row) {
			const auto left = viewer::imageToView(m_view, {0.f, static_cast<float>(row) * cellHeight});
			const auto right = viewer::imageToView(m_view, {gridWidth, static_cast<float>(row) * cellHeight});
			draw->AddLine({origin.x + left.x(), origin.y + left.y()}, {origin.x + right.x(), origin.y + right.y()},
						  lineColor);
		}
		if (pixel.has_value())
			hoveredTile = viewer::tileIndexAt(*pixel, {grid.tileWidth, grid.tileHeight}, grid.columns, grid.rows);
		if (hoveredTile.has_value()) {
			const auto column = static_cast<float>(*hoveredTile % grid.columns);
			const auto row = static_cast<float>(*hoveredTile / grid.columns);
			const auto tileMin = viewer::imageToView(m_view, {column * cellWidth, row * cellHeight});
			const auto tileMax = viewer::imageToView(m_view, {(column + 1.f) * cellWidth, (row + 1.f) * cellHeight});
			draw->AddRectFilled({origin.x + tileMin.x(), origin.y + tileMin.y()},
								{origin.x + tileMax.x(), origin.y + tileMax.y()}, IM_COL32(255, 200, 60, 60));
			draw->AddRect({origin.x + tileMin.x(), origin.y + tileMin.y()},
						  {origin.x + tileMax.x(), origin.y + tileMax.y()}, IM_COL32(255, 200, 60, 255), 0.f, 0, 2.f);
		}
	}
	draw->PopClipRect();
	if (hoveredTile.has_value())
		ImGui::SetTooltip("Tile %u", *hoveredTile);

	// Toolbar line under the canvas.
	if (ImGui::Button("Fit"))
		m_view = viewer::centeredImageView(imageSize, iSize, viewer::fitImageZoom(imageSize, iSize, g_fitMargin));
	ImGui::SameLine();
	if (ImGui::Button("1:1"))
		m_view = viewer::centeredImageView(imageSize, iSize, 1.f);
	ImGui::SameLine();
	ImGui::Text("%.0f%%", static_cast<double>(m_view.zoom) * 100.0);
	ImGui::SameLine();
	ImGui::TextDisabled("| %u x %u", imageSize.x(), imageSize.y());
	if (pixel.has_value()) {
		ImGui::SameLine();
		ImGui::TextDisabled("| (%u, %u)", pixel->x(), pixel->y());
	}
}

void ImageDocument::renderHierarchyPanel() {
	if (!gui::widgets::sectionHeader("Image", gui::icon::image, false).open)
		return;
	if (gui::widgets::beginProperties("##imageInfo")) {
		gui::widgets::propertyLabel("File");
		ImGui::TextUnformatted(m_path.filename().string().c_str());
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::propertyLabel("Size");
		ImGui::Text("%u x %u", m_info.size.x(), m_info.size.y());
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::propertyLabel("Channels", "3 for an opaque image, 4 when it has an alpha channel.");
		ImGui::Text("%u", m_info.channels);
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::propertyLabel("File size");
		ImGui::TextUnformatted(viewer::formatFileSize(m_info.fileBytes).c_str());
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::propertyLabel("Filter", "Sampling of a tileset that uses the image as its atlas.");
		if (m_tilesets.empty()) {
			ImGui::TextDisabled("no tileset");
		} else {
			const bool nearest = std::ranges::all_of(
					m_tilesets, [](const viewer::TilesetGrid& iGrid) -> bool { return iGrid.nearest; });
			ImGui::TextUnformatted(nearest ? "Nearest" : "Linear");
		}
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

void ImageDocument::renderPropertiesPanel() {
	if (gui::widgets::sectionHeader("View", gui::icon::scan, false).open) {
		if (gui::widgets::beginProperties("##imageView")) {
			std::ignore = gui::widgets::propertyBool("Pixel grid", m_showPixelGrid, true,
													 "Draw the pixel grid from a zoom of 800%.");
			gui::widgets::endProperties();
		}
		ImGui::TextDisabled("Wheel: zoom. Drag: pan. F or double-click: fit. 1: 1:1.");
		gui::widgets::endSection();
	}
	if (!gui::widgets::sectionHeader("Tilesets", gui::icon::layoutGrid, false).open)
		return;
	if (m_tilesets.empty()) {
		ImGui::TextDisabled("No tileset of the project uses this image.");
		gui::widgets::endSection();
		return;
	}
	if (ImGui::Selectable("No overlay", !m_overlay.has_value()))
		setOverlay(std::nullopt);
	for (size_t index = 0; index < m_tilesets.size(); ++index) {
		const auto& grid = m_tilesets[index];
		const auto label = std::format("{}  ({}x{} tiles of {}x{})###tileset{}", grid.tileset, grid.columns, grid.rows,
									   grid.tileWidth, grid.tileHeight, index);
		if (ImGui::Selectable(label.c_str(), m_overlay == index))
			setOverlay(m_overlay == index ? std::nullopt : std::optional<size_t>{index});
	}
	gui::widgets::endSection();
}

}// namespace owl::nest
