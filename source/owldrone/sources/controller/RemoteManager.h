/**
 * @file RemoteManager.h
 * @author Silmaen
 * @date 15/10/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "owl.h"

namespace drone::controller {

/**
 * @brief Class RemoteManager
 */
class RemoteManager {
public:
	/**
	 * @brief Destructor.
	 */
	virtual ~RemoteManager();

	RemoteManager(const RemoteManager &) = delete;
	RemoteManager(RemoteManager &&) = delete;
	RemoteManager &operator=(const RemoteManager &) = delete;
	RemoteManager &operator=(RemoteManager &&) = delete;

	/**
	 * @brief Singleton accessor.
	 * @return Instance of this object.
	 */
	static RemoteManager &get() {
		static RemoteManager instance;
		return instance;
	}

	std::vector<std::string> getFlightControllerList();

private:
	/**
	 * @brief Constructor.
	 */
	RemoteManager();
};

}// namespace drone::controller
