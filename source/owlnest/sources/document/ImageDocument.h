/**
 * @file ImageDocument.h
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "viewer/ImageViewMath.h"
#include "viewer/TilesetUsage.h"

#include <math/vectors.h>
#include <renderer/gpu/Texture.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  What the image viewer says about an image file.
 */
struct ImageInfo {
	/// Size in pixels.
	math::vec2ui size{0, 0};
	/// Channels of the pixels (3 or 4).
	uint32_t channels = 0;
	/// Size of the file in bytes.
	uint64_t fileBytes = 0;
};

/**
 * @brief
 *  Read-only viewer of an image asset (`.png`, `.jpg`, ...): zoom, pan, pixel grid, checkerboard behind the
 *  transparency, file information and the tile grid of the tilesets that use the image as their atlas.
 *
 * Nothing in it edits the file, so it has no undo stack and is never dirty.
 */
class ImageDocument final : public Document {
public:
	ImageDocument(const ImageDocument&) = delete;

	ImageDocument(ImageDocument&&) = delete;

	auto operator=(const ImageDocument&) -> ImageDocument& = delete;

	auto operator=(ImageDocument&&) -> ImageDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	ImageDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~ImageDocument() override;

	/**
	 * @brief
	 *  Document type.
	 * @return `DocumentType::Image`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Image; }

	/**
	 * @brief
	 *  Tab title: the file name.
	 * @return The title.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  Path of the image file.
	 * @return The path.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  A viewer has nothing to save.
	 * @return False.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return false; }

	/**
	 * @brief
	 *  Attach to the editor.
	 * @param[in] iEditor The editor layer.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Detach from the editor.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Per-frame update (nothing to do).
	 * @param[in] iTimeStep The frame time.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Event hook (ImGui handles the input).
	 * @param[in,out] ioEvent The event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Draw the document window: toolbar, image canvas and status line.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Nothing to save.
	 * @return False.
	 */
	auto save() -> bool override { return false; }

	/**
	 * @brief
	 *  Nothing to save.
	 * @param[in] iPath Unused.
	 * @return False.
	 */
	auto saveAs([[maybe_unused]] const std::filesystem::path& iPath) -> bool override { return false; }

	/**
	 * @brief
	 *  Unused scene undo manager, required by the interface.
	 * @return The placeholder manager.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Unused scene undo manager, required by the interface.
	 * @return The placeholder manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  The document draws its own Hierarchy and Properties panels.
	 * @return True.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Hierarchy panel: file information.
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Properties panel: view options and the tilesets using the image.
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title of the Hierarchy panel.
	 * @return The title.
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Image"; }

	/**
	 * @brief
	 *  Title of the Properties panel.
	 * @return The title.
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Image View"; }

	/**
	 * @brief
	 *  Read an image file (size, channels, file size) and look for the tilesets that use it.
	 * @param[in] iPath The file.
	 * @return True on success.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Look for the tilesets of some folders that use the image as their atlas.
	 * @param[in] iRoots The folders searched recursively.
	 * @param[in] iAssetDirectories The asset folders a tileset texture name is relative to.
	 */
	void findTilesets(std::span<const std::filesystem::path> iRoots,
					  std::span<const std::filesystem::path> iAssetDirectories);

	/**
	 * @brief
	 *  What is known of the image.
	 * @return The information.
	 */
	[[nodiscard]] auto info() const -> const ImageInfo& { return m_info; }

	/**
	 * @brief
	 *  The tilesets that use the image as their atlas.
	 * @return Their grids.
	 */
	[[nodiscard]] auto tilesets() const -> const std::vector<viewer::TilesetGrid>& { return m_tilesets; }

	/**
	 * @brief
	 *  Index of the tileset whose grid is drawn over the image.
	 * @return The index in `tilesets()`, nothing without overlay.
	 */
	[[nodiscard]] auto overlay() const -> std::optional<size_t> { return m_overlay; }

	/**
	 * @brief
	 *  Choose the tileset whose grid is drawn over the image.
	 * @param[in] iOverlay The index in `tilesets()` (out of range or nothing: no overlay).
	 */
	void setOverlay(std::optional<size_t> iOverlay);

	/**
	 * @brief
	 *  Current zoom and pan.
	 * @return The view.
	 */
	[[nodiscard]] auto view() const -> const viewer::ImageView& { return m_view; }

	/**
	 * @brief
	 *  Check whether the document tab is open.
	 * @return True while open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Set the open state of the tab.
	 * @param[in] iOpen The state.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/**
	 * @brief
	 *  Draw the canvas: checkerboard, image, pixel grid, tile overlay, pan and zoom gestures.
	 * @param[in] iSize Size of the canvas in screen pixels.
	 */
	void drawCanvas(const math::vec2& iSize);

	/// Path of the image file.
	std::filesystem::path m_path;
	/// Size, channels and file size.
	ImageInfo m_info;
	/// GPU texture, made on the first frame.
	shared<renderer::gpu::Texture2D> m_texture;
	/// Tilesets using the image.
	std::vector<viewer::TilesetGrid> m_tilesets;
	/// Tileset whose grid is drawn.
	std::optional<size_t> m_overlay;
	/// Zoom and pan.
	viewer::ImageView m_view;
	/// True until the first frame with a canvas fitted the image.
	bool m_needsFit{true};
	/// Draw the pixel grid at high zoom.
	bool m_showPixelGrid{true};
	/// Editor back-pointer.
	EditorLayer* mp_editorLayer{nullptr};
	/// ImGui open state of the tab.
	bool m_pOpen{true};
	/// Whether the window was focused last frame.
	bool m_wasFocused{false};
	/// Placeholder: `Document::undoManager()` returns a `SceneUndoManager&`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
