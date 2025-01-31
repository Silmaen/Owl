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

class Serializer {
public:
	Serializer();
	auto getImpl() const -> const shared<SerializerImpl>&;

private:
	shared<SerializerImpl> mp_impl;
};

}// namespace owl::core
