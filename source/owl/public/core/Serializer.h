/**
 * @file Serializer.h
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::core {

struct SerializerImpl;

/**
 * @brief Frontend class for serializing anything.
 */
class Serializer {
public:
	/**
	 * @brief Default constructor.
	 */
	Serializer();
	/**
	 * @brief Get access to the backend implementation.
	 * @return The backend implementation.
	 */
	[[nodiscard]] auto getImpl() const -> const shared<SerializerImpl>&;

private:
	shared<SerializerImpl> mp_impl;
};

}// namespace owl::core
