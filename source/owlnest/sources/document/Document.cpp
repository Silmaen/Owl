/**
 * @file Document.cpp
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Document.h"

#include <gui/IconFont.h>

#include <format>

namespace owl::nest {
Document::~Document() = default;

auto documentWindowTitle(const DocumentType iType, const std::string_view iName, const uint64_t iId) -> std::string {
	const char* glyph = gui::icon::file;
	const char* kind = "doc";
	switch (iType) {
		case DocumentType::Scene:
			glyph = gui::icon::clapperboard;
			kind = "scene";
			break;
		case DocumentType::Code:
			glyph = gui::icon::fileCode2;
			kind = "code";
			break;
		case DocumentType::NodeGraph:
			glyph = gui::icon::workflow;
			kind = "nodegraph";
			break;
		case DocumentType::Animation:
			glyph = gui::icon::film;
			kind = "animation";
			break;
		case DocumentType::Tilemap:
			glyph = gui::icon::grid3x3;
			kind = "tilemap";
			break;
		case DocumentType::Tileset:
			glyph = gui::icon::layoutGrid;
			kind = "tileset";
			break;
	}
	return std::format("{}  {}###{}_{:x}", glyph, iName.empty() ? std::string_view{"Untitled"} : iName, kind, iId);
}

}// namespace owl::nest
