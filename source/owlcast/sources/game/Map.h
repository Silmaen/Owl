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
	uint8_t encode() const { return m_attributes << 6u | (m_textureId & 0b00111111); }
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
	using gridCoordinates = glm::i32vec2;
	using worldCoordinates = glm::vec2;
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
	Map &operator=(const Map &) = default;
	/**
	 * @brief Default move affectation operator.
	 */
	Map &operator=(Map &&) = default;

	Map(const size_t iWidth, const size_t iHeight, uint8_t iCubeSize) : m_cubeSize{iCubeSize} {
		reset(iWidth, iHeight);
	}
	/**
	 * @brief Access to map value at the coordinate
	 * @param location coordinates
	 * @return The local map data
	 */
	MapCell &operator()(const gridCoordinates &location) { return at(location); }
	/**
	 * @brief Access to map value at the coordinate
	 * @param location coordinates
	 * @return The local map data
	 */
	const MapCell &operator()(const gridCoordinates &location) const { return at(location); }

	/**
	 * @brief Access to map value at the coordinate
	 * @param location coordinates
	 * @return The local map data
	 */
	MapCell &at(const gridCoordinates &location);
	/**
	 * @brief Access to map value at the coordinate
	 * @param location coordinates
	 * @return The local map data
	 */
	[[nodiscard]] const MapCell &at(const gridCoordinates &location) const;
	/**
	 * @brief Determine the cell where the point lies.
	 * @param from The point to check
	 * @return The cell
	 */
	[[nodiscard]] gridCoordinates whichCell(const worldCoordinates &from) const;
	/**
	 * @brief Check if a point is in the map
	 * @param from The point to check
	 * @return True if in the map
	 */
	[[nodiscard]] bool isIn(const worldCoordinates &from) const;
	/**
	 * @brief Checks if grid coordinates are in the map
	 * @param from Grid coordinates to check
	 * @return True if in the map
	 */
	[[nodiscard]] bool isIn(const gridCoordinates &from) const;
	/**
	 * @brief Check if a point is in the map and player can pass through
	 * @param from The point to check
	 * @return True if in the map
	 */
	[[nodiscard]] bool isInPassable(const worldCoordinates &from) const;
	/**
	 * @brief Checks if grid coordinates are in the map and player can pass through
	 * @param from Grid coordinates to check
	 * @return True if in the map
	 */
	[[nodiscard]] bool isInPassable(const gridCoordinates &from) const;

	/**
	 * @brief Check if a point is in the map and player can see through
	 * @param from The point to check
	 * @return True if in the map
	 */
	[[nodiscard]] bool isInVisible(const worldCoordinates &from) const;
	/**
	 * @brief Checks if grid coordinates are in the map and player can see through
	 * @param from Grid coordinates to check
	 * @return True if in the map
	 */
	[[nodiscard]] bool isInVisible(const gridCoordinates &from) const;
	/**
	 * @brief Get the cube's size
	 * @return Cube's size
	 */
	[[nodiscard]] uint8_t getCellSize() const { return m_cubeSize; }
	/**
	 * @brief Get the full pixel width of the map
	 * @return Pixel width of the map
	 */
	[[nodiscard]] uint32_t fullWidth() const { return static_cast<uint32_t>(m_width) * m_cubeSize; }
	/**
	 * @brief Get the full pixel height of the map
	 * @return Pixel height of the map
	 */
	[[nodiscard]] uint32_t fullHeight() const { return static_cast<uint32_t>(m_height) * m_cubeSize; }
	/**
	 * @brief Define player starts
	 * @param pos Position
	 * @param dir Direction
	 */
	void setPlayerStart(const worldCoordinates &pos, const worldCoordinates &dir) {
		m_PlayerInitialPosition = pos;
		m_PlayerInitialDirection = dir;
	}
	/**
	 * @brief Check and modify the expected move according to map constrains
	 * @param Start Actual Position
	 * @param Expected Expected move
	 * @return Effective move
	 */
	[[nodiscard]] worldCoordinates possibleMove(const worldCoordinates &Start, const worldCoordinates &Expected) const;
	/**
	 * @brief Get player start information
	 * @return Player start state
	 */
	[[nodiscard]] std::tuple<const worldCoordinates &, const worldCoordinates &> getPlayerStart() const {
		return {m_PlayerInitialPosition, m_PlayerInitialDirection};
	}

private:
	void reset(size_t iWidth, size_t iHeight);
	size_t m_width{0};
	size_t m_height{0};
	uint8_t m_cubeSize{64};
	std::vector<MapCell> m_cells;
	worldCoordinates m_PlayerInitialPosition = {0, 0};
	worldCoordinates m_PlayerInitialDirection = {0, 1};
};

}// namespace owl::raycaster::game
