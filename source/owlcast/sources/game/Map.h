/**
 * @file Map.h
 * @author Silmaen
 * @date 23/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::raycaster::game {

class MapCell {
public:
	bool isVisible() const { return m_attributes & 0b1; }
	bool isPassable() const { return m_attributes & 0b10; }
	bool alreadySeen() const { return m_attributes & 0b100; }
	void markSeen() { m_attributes |= 0b100; }
	uint8_t getTextureId() const { return m_textureId; }
	uint8_t encode() const { return static_cast<uint8_t>(m_attributes << 6u) | static_cast<uint8_t>(m_textureId & 0b00111111); }
	void decode(const uint8_t iCode) {
		m_textureId = iCode & 0b00111111;
		m_attributes = iCode >> 6u;
	}

private:
	uint8_t m_attributes{0};
	uint8_t m_textureId{0};
};

/**
 * @brief Class Map.
 */
class Map final {
public:
	using gridCoordinates = math::vec2i;
	using worldCoordinates = math::vec2f;
	/**
	 * @brief Default constructor.
	 */
	Map();
	/**
	 * @brief Default destructor.
	 */
	~Map();
	/**
	 * @brief Default copy constructor.
	 */
	Map(const Map &) = default;
	/**
	 * @brief Default move constructor.
	 */
	Map(Map &&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const Map &) -> Map & = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(Map &&) -> Map & = default;

	Map(const size_t iWidth, const size_t iHeight, uint8_t iCubeSize) : m_cubeSize{iCubeSize} {
		reset(iWidth, iHeight);
	}
	/**
	 * @brief Access to map value at the coordinate
	 * @param location coordinates
	 * @return The local map data
	 */
	auto operator()(const gridCoordinates &location) -> MapCell & { return at(location); }
	/**
	 * @brief Access to map value at the coordinate
	 * @param location coordinates
	 * @return The local map data
	 */
	auto operator()(const gridCoordinates &location) const -> const MapCell & { return at(location); }

	/**
	 * @brief Access to map value at the coordinate
	 * @param location coordinates
	 * @return The local map data
	 */
	auto at(const gridCoordinates &location) -> MapCell &;
	/**
	 * @brief Access to map value at the coordinate
	 * @param location coordinates
	 * @return The local map data
	 */
	[[nodiscard]] auto at(const gridCoordinates &location) const -> const MapCell &;
	/**
	 * @brief Determine the cell where the point lies.
	 * @param from The point to check
	 * @return The cell
	 */
	[[nodiscard]] auto whichCell(const worldCoordinates &from) const -> gridCoordinates;
	/**
	 * @brief Check if a point is in the map
	 * @param from The point to check
	 * @return True if in the map
	 */
	[[nodiscard]] auto isIn(const worldCoordinates &from) const -> bool;
	/**
	 * @brief Checks if grid coordinates are in the map
	 * @param from Grid coordinates to check
	 * @return True if in the map
	 */
	[[nodiscard]] auto isIn(const gridCoordinates &from) const -> bool;
	/**
	 * @brief Check if a point is in the map and player can pass through
	 * @param from The point to check
	 * @return True if in the map
	 */
	[[nodiscard]] auto isInPassable(const worldCoordinates &from) const -> bool;
	/**
	 * @brief Checks if grid coordinates are in the map and player can pass through
	 * @param from Grid coordinates to check
	 * @return True if in the map
	 */
	[[nodiscard]] auto isInPassable(const gridCoordinates &from) const -> bool;

	/**
	 * @brief Check if a point is in the map and player can see through
	 * @param from The point to check
	 * @return True if in the map
	 */
	[[nodiscard]] auto isInVisible(const worldCoordinates &from) const -> bool;
	/**
	 * @brief Checks if grid coordinates are in the map and player can see through
	 * @param from Grid coordinates to check
	 * @return True if in the map
	 */
	[[nodiscard]] auto isInVisible(const gridCoordinates &from) const -> bool;
	/**
	 * @brief Get the cube's size
	 * @return Cube's size
	 */
	[[nodiscard]] auto getCellSize() const -> uint8_t { return m_cubeSize; }
	/**
	 * @brief Get the full pixel width of the map
	 * @return Pixel width of the map
	 */
	[[nodiscard]] auto fullWidth() const -> uint32_t { return static_cast<uint32_t>(m_width) * m_cubeSize; }
	/**
	 * @brief Get the full pixel height of the map
	 * @return Pixel height of the map
	 */
	[[nodiscard]] auto fullHeight() const -> uint32_t { return static_cast<uint32_t>(m_height) * m_cubeSize; }
	/**
	 * @brief Define player starts
	 * @param pos Position
	 * @param dir Direction
	 */
	void setPlayerStart(const worldCoordinates &pos, const worldCoordinates &dir) {
		m_playerInitialPosition = pos;
		m_playerInitialDirection = dir;
	}
	/**
	 * @brief Check and modify the expected move according to map constrains
	 * @param Start Actual Position
	 * @param Expected Expected move
	 * @return Effective move
	 */
	[[nodiscard]] auto possibleMove(const worldCoordinates &Start, const worldCoordinates &Expected) const -> worldCoordinates;
	/**
	 * @brief Get player start information
	 * @return Player start state
	 */
	[[nodiscard]] auto getPlayerStart() const -> std::tuple<const worldCoordinates &, const worldCoordinates &> {
		return {m_playerInitialPosition, m_playerInitialDirection};
	}

private:
	void reset(size_t iWidth, size_t iHeight);
	size_t m_width{0};
	size_t m_height{0};
	uint8_t m_cubeSize{64};
	std::vector<MapCell> m_cells;
	worldCoordinates m_playerInitialPosition = {0, 0};
	worldCoordinates m_playerInitialDirection = {0, 1};
};

}// namespace owl::raycaster::game
