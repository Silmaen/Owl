/**
 * @file UUID.cpp
 * @author Silmaen
 * @date 14/01/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/UUID.h"

namespace owl::core {

namespace {
// NOLINTBEGIN(bugprone-throwing-static-initialization)
std::random_device g_RandomDevice;

std::mt19937_64 g_Engine(g_RandomDevice());
std::uniform_int_distribution<uint64_t> g_UniformDistribution;
// NOLINTEND(bugprone-throwing-static-initialization)
}// namespace

UUID::UUID() : m_uuid{g_UniformDistribution(g_Engine)} {}

}// namespace owl::core
