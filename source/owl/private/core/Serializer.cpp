/**
 * @file Serializer.cpp
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/Serializer.h"
#include "core/SerializerImpl.h"

namespace owl::core {

Serializer::Serializer() { mp_impl = mkShared<SerializerImpl>(); }

auto Serializer::getImpl() const -> const shared<SerializerImpl>& { return mp_impl; }

}// namespace owl::core
