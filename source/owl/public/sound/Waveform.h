/**
 * @file Waveform.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace owl::sound {

/**
 * @brief
 *  Read the outline of a sound file: the peak amplitude of each of a number of equal slices, all channels mixed.
 *
 * Meant for thumbnails and previews; the whole file is read once, in blocks.
 * @param[in] iPath The sound file (any format the sound module reads).
 * @param[in] iBuckets Number of slices.
 * @return One peak per slice, from 0 to 1; empty when the file cannot be read or the audio module is not built.
 */
OWL_API auto readWaveform(const std::filesystem::path& iPath, size_t iBuckets) -> std::vector<float>;

}// namespace owl::sound
