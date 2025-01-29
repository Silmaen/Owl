/**
 * @file UUID.cpp
 * @author Silmaen
 * @date 14/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/UUID.h"

namespace owl::core {

namespace {
std::random_device s_randomDevice;
std::mt19937_64 s_engine(s_randomDevice());
std::uniform_int_distribution<uint64_t> s_uniformDistribution;
}// namespace

UUID::UUID() : m_uuid{s_uniformDistribution(s_engine)} {}

}// namespace owl::core
