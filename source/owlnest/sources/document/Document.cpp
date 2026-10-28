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

namespace {
// Glyph and ImGui id prefix of a document type.
struct DocumentLabel {
	// Lucide glyph of the tab.
	const char* glyph;
	// Stable part of the window id.
	const char* kind;
};

auto documentLabel(const DocumentType iType) -> DocumentLabel {
	switch (iType) {
		case DocumentType::Scene:
			return {.glyph = gui::icon::clapperboard, .kind = "scene"};
		case DocumentType::Code:
			return {.glyph = gui::icon::fileCode2, .kind = "code"};
		case DocumentType::NodeGraph:
			return {.glyph = gui::icon::workflow, .kind = "nodegraph"};
		case DocumentType::Animation:
			return {.glyph = gui::icon::film, .kind = "animation"};
		case DocumentType::Tilemap:
			return {.glyph = gui::icon::grid3x3, .kind = "tilemap"};
		case DocumentType::Tileset:
			return {.glyph = gui::icon::layoutGrid, .kind = "tileset"};
		case DocumentType::Font:
			return {.glyph = gui::icon::type, .kind = "font"};
		case DocumentType::ActionMap:
			return {.glyph = gui::icon::gamepad2, .kind = "actionmap"};
		case DocumentType::Image:
			return {.glyph = gui::icon::image, .kind = "image"};
		case DocumentType::Mesh:
			return {.glyph = gui::icon::box, .kind = "mesh"};
		case DocumentType::Material:
			return {.glyph = gui::icon::paintbrush, .kind = "material"};
	}
	return {.glyph = gui::icon::file, .kind = "doc"};
}
}// namespace

auto documentWindowTitle(const DocumentType iType, const std::string_view iName, const uint64_t iId) -> std::string {
	return std::format("{}  {}{}", documentLabel(iType).glyph, iName.empty() ? std::string_view{"Untitled"} : iName,
					   documentWindowId(iType, iId));
}

auto documentWindowId(const DocumentType iType, const uint64_t iId) -> std::string {
	return std::format("###{}_{:x}", documentLabel(iType).kind, iId);
}

}// namespace owl::nest
