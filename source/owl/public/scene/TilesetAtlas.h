/**
 * @file TilesetAtlas.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace owl::scene {

/**
 * @brief
 *  Pixels of a tileset atlas, edited tile by tile: the image the tileset editor composes from source images.
 *
 * RGBA8, rows stored from the top of the image down, the cells row-major from the top-left like the tile indices of a
 * `Tileset`. A resize keeps every cell at its column and row, an index never moves under a tilemap that uses it.
 */
class OWL_API TilesetAtlas final {
public:
	/**
	 * @brief
	 *  Build an empty atlas (no cell).
	 */
	TilesetAtlas() = default;

	/**
	 * @brief
	 *  Build a transparent atlas.
	 * @param[in] iTile Size of a cell in pixels (each side clamped to 1).
	 * @param[in] iColumns Number of columns (clamped to 1).
	 * @param[in] iRows Number of rows (clamped to 1).
	 */
	TilesetAtlas(const math::vec2ui& iTile, uint32_t iColumns, uint32_t iRows);

	/**
	 * @brief
	 *  Destructor.
	 */
	~TilesetAtlas() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	TilesetAtlas(const TilesetAtlas&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	TilesetAtlas(TilesetAtlas&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const TilesetAtlas&) -> TilesetAtlas& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(TilesetAtlas&&) -> TilesetAtlas& = default;

	/**
	 * @brief
	 *  Build an atlas on a grid from an image, which is cropped or padded with transparent pixels to the grid.
	 * @param[in] iRgba Pixels of the image, RGBA8, top row first.
	 * @param[in] iSize Size of the image.
	 * @param[in] iTile Size of a cell in pixels.
	 * @param[in] iColumns Number of columns.
	 * @param[in] iRows Number of rows.
	 * @return The atlas.
	 */
	[[nodiscard]] static auto fromImage(std::span<const uint8_t> iRgba, const math::vec2ui& iSize,
										const math::vec2ui& iTile, uint32_t iColumns, uint32_t iRows) -> TilesetAtlas;

	/**
	 * @brief
	 *  Reverse the row order of an RGBA8 image (top-first to bottom-first and back, the decoder order).
	 * @param[in] iRgba The pixels.
	 * @param[in] iSize Their size.
	 * @return The flipped pixels.
	 */
	[[nodiscard]] static auto flipRows(std::span<const uint8_t> iRgba, const math::vec2ui& iSize)
			-> std::vector<uint8_t>;

	/**
	 * @brief
	 *  Size of the atlas image in pixels.
	 * @return The size.
	 */
	[[nodiscard]] auto getSize() const -> math::vec2ui { return {m_tile.x() * m_columns, m_tile.y() * m_rows}; }

	/**
	 * @brief
	 *  Size of a cell in pixels.
	 * @return The cell size.
	 */
	[[nodiscard]] auto getTileSize() const -> const math::vec2ui& { return m_tile; }

	/**
	 * @brief
	 *  Number of columns.
	 * @return The column count.
	 */
	[[nodiscard]] auto getColumns() const -> uint32_t { return m_columns; }

	/**
	 * @brief
	 *  Number of rows.
	 * @return The row count.
	 */
	[[nodiscard]] auto getRows() const -> uint32_t { return m_rows; }

	/**
	 * @brief
	 *  Number of cells.
	 * @return `columns * rows`.
	 */
	[[nodiscard]] auto getTileCount() const -> uint32_t { return m_columns * m_rows; }

	/**
	 * @brief
	 *  Pixels of the whole atlas, RGBA8, top row first.
	 * @return The pixels.
	 */
	[[nodiscard]] auto getPixels() const -> const std::vector<uint8_t>& { return m_pixels; }

	/**
	 * @brief
	 *  Change the grid, each cell staying at its column and row; new cells are transparent, cells past the grid go.
	 * @param[in] iColumns New column count (clamped to 1).
	 * @param[in] iRows New row count (clamped to 1).
	 */
	void resize(uint32_t iColumns, uint32_t iRows);

	/**
	 * @brief
	 *  Pixels of one cell.
	 * @param[in] iIndex The cell.
	 * @return Its RGBA8 pixels, top row first, empty when the index is out of range.
	 */
	[[nodiscard]] auto getTile(uint32_t iIndex) const -> std::vector<uint8_t>;

	/**
	 * @brief
	 *  Replace the pixels of one cell by an image, scaled to the cell with nearest sampling when its size differs.
	 * @param[in] iIndex The cell.
	 * @param[in] iRgba Pixels of the image, RGBA8, top row first.
	 * @param[in] iSize Size of the image.
	 * @return True when the cell was written.
	 */
	auto setTile(uint32_t iIndex, std::span<const uint8_t> iRgba, const math::vec2ui& iSize) -> bool;

	/**
	 * @brief
	 *  Make a cell fully transparent, its index kept.
	 * @param[in] iIndex The cell.
	 */
	void clearTile(uint32_t iIndex);

	/**
	 * @brief
	 *  Exchange the pixels of two cells.
	 * @param[in] iLhs The first cell.
	 * @param[in] iRhs The second cell.
	 */
	void swapTiles(uint32_t iLhs, uint32_t iRhs);

	/**
	 * @brief
	 *  Whether a cell holds only transparent pixels.
	 * @param[in] iIndex The cell.
	 * @return True for a transparent or out-of-range cell.
	 */
	[[nodiscard]] auto isTileEmpty(uint32_t iIndex) const -> bool;

	/**
	 * @brief
	 *  Where a run of images dropped on a cell goes: the cell, then the next empty cells, with rows added at the
	 *  bottom when the grid is full.
	 * @param[in] iFrom The cell the images are dropped on.
	 * @param[in] iCount Number of images.
	 * @return The cells, one per image, and the row count the grid needs.
	 */
	[[nodiscard]] auto planInsertion(uint32_t iFrom, uint32_t iCount) const
			-> std::pair<std::vector<uint32_t>, uint32_t>;

private:
	/// Size of a cell in pixels.
	math::vec2ui m_tile{1, 1};
	/// Number of columns.
	uint32_t m_columns = 0;
	/// Number of rows.
	uint32_t m_rows = 0;
	/// Pixels, RGBA8, top row first.
	std::vector<uint8_t> m_pixels;
};

}// namespace owl::scene
