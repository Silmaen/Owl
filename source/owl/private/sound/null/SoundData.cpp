/**
 * @file SoundData.cpp
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SoundData.h"

namespace owl::sound::null {

SoundData::SoundData(const Specification& iSpecifications) : sound::SoundData{iSpecifications} {}

SoundData::~SoundData() = default;

}// namespace owl::sound::null
