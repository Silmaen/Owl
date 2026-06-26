/**
 * @file BlockRunLength.cpp
 * @author Silmaen
 * @date 26/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/voxel/BlockRunLength.h"

#include <charconv>

namespace owl::data::voxel {

auto encodeBlockRuns(const std::vector<BlockId>& iBlocks, const std::vector<PackedMeta>& iMeta) -> std::string {
	const auto metaAt = [&iMeta](const size_t iIndex) -> PackedMeta {
		return iIndex < iMeta.size() ? iMeta[iIndex] : g_DefaultMeta;
	};
	std::string out;
	size_t i = 0;
	bool first = true;
	while (i < iBlocks.size()) {
		const BlockId value = iBlocks[i];
		const PackedMeta meta = metaAt(i);
		size_t run = 1;
		while (i + run < iBlocks.size() && iBlocks[i + run] == value && metaAt(i + run) == meta) ++run;
		if (!first)
			out.push_back(' ');
		out += std::to_string(run);
		out.push_back('x');
		out += std::to_string(value);
		if (meta != g_DefaultMeta) {
			out.push_back(':');
			out += std::to_string(meta);
		}
		first = false;
		i += run;
	}
	return out;
}

auto decodeBlockRuns(const std::string_view iEncoded, std::vector<BlockId>& oBlocks, std::vector<PackedMeta>& oMeta,
					 const size_t iVolume) -> bool {
	oBlocks.assign(iVolume, g_AirBlock);
	oMeta.assign(iVolume, g_DefaultMeta);
	size_t cursor = 0;
	size_t written = 0;
	while (cursor < iEncoded.size() && written < iVolume) {
		while (cursor < iEncoded.size() && iEncoded[cursor] == ' ') ++cursor;
		if (cursor >= iEncoded.size())
			break;
		const size_t sep = iEncoded.find('x', cursor);
		if (sep == std::string_view::npos)
			return false;
		size_t spaceEnd = iEncoded.find(' ', sep);
		if (spaceEnd == std::string_view::npos)
			spaceEnd = iEncoded.size();
		const size_t metaSep = iEncoded.find(':', sep);
		const size_t idEnd = metaSep != std::string_view::npos && metaSep < spaceEnd ? metaSep : spaceEnd;
		size_t run = 0;
		BlockId value = g_AirBlock;
		PackedMeta meta = g_DefaultMeta;
		if (std::from_chars(iEncoded.data() + cursor, iEncoded.data() + sep, run).ec != std::errc{})
			return false;
		if (std::from_chars(iEncoded.data() + sep + 1, iEncoded.data() + idEnd, value).ec != std::errc{})
			return false;
		if (idEnd < spaceEnd &&
			std::from_chars(iEncoded.data() + idEnd + 1, iEncoded.data() + spaceEnd, meta).ec != std::errc{})
			return false;
		for (size_t k = 0; k < run && written < iVolume; ++k) {
			oBlocks[written] = value;
			oMeta[written] = meta;
			++written;
		}
		cursor = spaceEnd;
	}
	return written == iVolume;
}

}// namespace owl::data::voxel
