/**
 * @file TilesetAtlas.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/TilesetAtlas.h"

#include <algorithm>
#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::scene {

namespace {
constexpr size_t g_pixelSize = 4;
}// namespace

TilesetAtlas::TilesetAtlas(const math::vec2ui& iTile, const uint32_t iColumns, const uint32_t iRows)
	: m_tile{std::max(1u, iTile.x()), std::max(1u, iTile.y())}, m_columns{std::max(1u, iColumns)},
	  m_rows{std::max(1u, iRows)}, m_pixels(static_cast<size_t>(getSize().surface()) * g_pixelSize, 0) {}

auto TilesetAtlas::fromImage(const std::span<const uint8_t> iRgba, const math::vec2ui& iSize, const math::vec2ui& iTile,
							 const uint32_t iColumns, const uint32_t iRows) -> TilesetAtlas {
	TilesetAtlas atlas{iTile, iColumns, iRows};
	if (iRgba.size() < static_cast<size_t>(iSize.surface()) * g_pixelSize) {
		OWL_CORE_WARN("TilesetAtlas: {} bytes given for a {}x{} image, the atlas stays empty.", iRgba.size(), iSize.x(),
					  iSize.y())
		return atlas;
	}
	const auto size = atlas.getSize();
	const size_t rowBytes = static_cast<size_t>(std::min(size.x(), iSize.x())) * g_pixelSize;
	for (uint32_t y = 0; y < std::min(size.y(), iSize.y()); ++y)
		std::copy_n(iRgba.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(y) * iSize.x() * g_pixelSize),
					rowBytes, atlas.m_pixels.begin() + static_cast<std::ptrdiff_t>(y * size.x() * g_pixelSize));
	return atlas;
}

auto TilesetAtlas::flipRows(const std::span<const uint8_t> iRgba, const math::vec2ui& iSize) -> std::vector<uint8_t> {
	const size_t rowBytes = static_cast<size_t>(iSize.x()) * g_pixelSize;
	std::vector<uint8_t> result(iRgba.begin(), iRgba.end());
	for (uint32_t y = 0; y < iSize.y() && (static_cast<size_t>(y) + 1) * rowBytes <= iRgba.size(); ++y)
		std::copy_n(iRgba.begin() + static_cast<std::ptrdiff_t>(y * rowBytes), rowBytes,
					result.begin() + static_cast<std::ptrdiff_t>((iSize.y() - 1 - y) * rowBytes));
	return result;
}

void TilesetAtlas::resize(const uint32_t iColumns, const uint32_t iRows) {
	TilesetAtlas next{m_tile, iColumns, iRows};
	for (uint32_t row = 0; row < std::min(m_rows, next.m_rows); ++row) {
		for (uint32_t column = 0; column < std::min(m_columns, next.m_columns); ++column) {
			const auto tile = getTile(row * m_columns + column);
			std::ignore = next.setTile(row * next.m_columns + column, tile, m_tile);
		}
	}
	*this = std::move(next);
}

auto TilesetAtlas::getTile(const uint32_t iIndex) const -> std::vector<uint8_t> {
	if (iIndex >= getTileCount())
		return {};
	const uint32_t originX = (iIndex % m_columns) * m_tile.x();
	const uint32_t originY = (iIndex / m_columns) * m_tile.y();
	const size_t atlasRow = static_cast<size_t>(getSize().x()) * g_pixelSize;
	const size_t tileRow = static_cast<size_t>(m_tile.x()) * g_pixelSize;
	std::vector<uint8_t> result(tileRow * m_tile.y());
	for (uint32_t y = 0; y < m_tile.y(); ++y)
		std::copy_n(m_pixels.begin() +
							static_cast<std::ptrdiff_t>((originY + y) * atlasRow + originX * g_pixelSize),
					tileRow, result.begin() + static_cast<std::ptrdiff_t>(y * tileRow));
	return result;
}

auto TilesetAtlas::setTile(const uint32_t iIndex, const std::span<const uint8_t> iRgba, const math::vec2ui& iSize)
		-> bool {
	if (iIndex >= getTileCount()) {
		OWL_CORE_WARN("TilesetAtlas: Tile {} is past the {} tiles of the atlas.", iIndex, getTileCount())
		return false;
	}
	if (iSize.surface() == 0 || iRgba.size() < static_cast<size_t>(iSize.surface()) * g_pixelSize) {
		OWL_CORE_WARN("TilesetAtlas: {} bytes given for a {}x{} tile image.", iRgba.size(), iSize.x(), iSize.y())
		return false;
	}
	const uint32_t originX = (iIndex % m_columns) * m_tile.x();
	const uint32_t originY = (iIndex / m_columns) * m_tile.y();
	const size_t atlasRow = static_cast<size_t>(getSize().x()) * g_pixelSize;
	for (uint32_t y = 0; y < m_tile.y(); ++y) {
		const size_t sourceY = static_cast<size_t>(y) * iSize.y() / m_tile.y();
		for (uint32_t x = 0; x < m_tile.x(); ++x) {
			const size_t sourceX = static_cast<size_t>(x) * iSize.x() / m_tile.x();
			std::copy_n(iRgba.begin() + static_cast<std::ptrdiff_t>((sourceY * iSize.x() + sourceX) * g_pixelSize),
						g_pixelSize,
						m_pixels.begin() +
								static_cast<std::ptrdiff_t>((originY + y) * atlasRow + (originX + x) * g_pixelSize));
		}
	}
	return true;
}

void TilesetAtlas::clearTile(const uint32_t iIndex) {
	if (iIndex >= getTileCount())
		return;
	const std::vector<uint8_t> transparent(static_cast<size_t>(m_tile.surface()) * g_pixelSize, 0);
	std::ignore = setTile(iIndex, transparent, m_tile);
}

void TilesetAtlas::swapTiles(const uint32_t iLhs, const uint32_t iRhs) {
	if (iLhs >= getTileCount() || iRhs >= getTileCount() || iLhs == iRhs)
		return;
	const auto lhs = getTile(iLhs);
	const auto rhs = getTile(iRhs);
	std::ignore = setTile(iLhs, rhs, m_tile);
	std::ignore = setTile(iRhs, lhs, m_tile);
}

auto TilesetAtlas::isTileEmpty(const uint32_t iIndex) const -> bool {
	const auto tile = getTile(iIndex);
	for (size_t alpha = 3; alpha < tile.size(); alpha += g_pixelSize) {
		if (tile[alpha] != 0)
			return false;
	}
	return true;
}

auto TilesetAtlas::planInsertion(const uint32_t iFrom, const uint32_t iCount) const
		-> std::pair<std::vector<uint32_t>, uint32_t> {
	std::vector<uint32_t> slots;
	slots.reserve(iCount);
	const uint32_t columns = std::max(1u, m_columns);
	uint32_t index = iFrom;
	while (slots.size() < iCount) {
		if (slots.empty() || index >= getTileCount() || isTileEmpty(index))
			slots.push_back(index);
		++index;
	}
	const uint32_t last = slots.empty() ? 0 : slots.back();
	return {std::move(slots), std::max(m_rows, last / columns + 1)};
}

}// namespace owl::scene
