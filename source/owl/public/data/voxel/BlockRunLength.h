/**
 * @file BlockRunLength.h
 * @author Silmaen
 * @date 26/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/voxel/Block.h"

#include <string>
#include <string_view>
#include <vector>

namespace owl::data::voxel {

/**
 * @brief
 *  Run-length encode a parallel block-id / metadata grid to a compact string.
 *
 * Consecutive cells sharing both id and metadata collapse to one run. A run with
 * default metadata is written as `<count>x<id>`; a run with non-default metadata
 * as `<count>x<id>:<meta>`. The metadata suffix is therefore only paid for when
 * present, and the output is byte-identical to the legacy id-only format whenever
 * every cell uses default metadata.
 * @param[in] iBlocks The block ids (the authoritative length).
 * @param[in] iMeta The packed metadata, one per block (treated as all-default when shorter).
 * @return The encoded run string (space-separated runs).
 */
[[nodiscard]] OWL_API auto encodeBlockRuns(const std::vector<BlockId>& iBlocks, const std::vector<PackedMeta>& iMeta)
		-> std::string;

/**
 * @brief
 *  Decode a run string produced by `encodeBlockRuns` into parallel id / metadata grids.
 *
 * Backward compatible with the legacy id-only format: a run without a `:<meta>`
 * suffix decodes to default metadata. Both output vectors are sized to `iVolume`
 * (filled with air / default metadata first), so a short or malformed string
 * still leaves well-formed grids.
 * @param[in] iEncoded The encoded run string.
 * @param[out] oBlocks Receives the decoded block ids (resized to `iVolume`).
 * @param[out] oMeta Receives the decoded metadata (resized to `iVolume`).
 * @param[in] iVolume The expected cell count.
 * @return True when the decoded run count matched `iVolume` exactly.
 */
[[nodiscard]] OWL_API auto decodeBlockRuns(std::string_view iEncoded, std::vector<BlockId>& oBlocks,
										   std::vector<PackedMeta>& oMeta, size_t iVolume) -> bool;

}// namespace owl::data::voxel
