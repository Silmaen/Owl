/**
 * @file Player.h
 * @author Silmaen
 * @date 23/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl::raycaster::game {

/**
 * @brief Class Player.
 */
class Player {
public:
	/**
	 * @brief Default constructor.
	 */
	Player();
	/**
	 * @brief Default destructor.
	 */
	virtual ~Player();
	/**
	 * @brief Default copy constructor.
	 */
	Player(const Player &) = default;
	/**
	 * @brief Default move constructor.
	 */
	Player(Player &&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	Player &operator=(const Player &) = default;
	/**
	 * @brief Default move affectation operator.
	 */
	Player &operator=(Player &&) = default;

private:
};

}// namespace owl::raycaster::game
