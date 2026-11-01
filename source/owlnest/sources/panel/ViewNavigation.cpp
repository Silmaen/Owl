/**
 * @file ViewNavigation.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ViewNavigation.h"

#include <algorithm>
#include <cctype>

namespace owl::nest::panel {

auto getName(const ViewNavigationMode iMode) -> std::string_view {
	switch (iMode) {
		case ViewNavigationMode::Select:
			return "Select";
		case ViewNavigationMode::Orbit:
			return "Orbit";
		case ViewNavigationMode::Fly:
			return "Fly";
		case ViewNavigationMode::Pan:
			return "Pan";
	}
	return "Select";
}

auto parseViewNavigationMode(const std::string_view iName) -> std::optional<ViewNavigationMode> {
	const auto same = [](const char iLeft, const char iRight) -> bool {
		return std::tolower(static_cast<unsigned char>(iLeft)) == std::tolower(static_cast<unsigned char>(iRight));
	};
	for (const auto mode: g_viewNavigationModes) {
		if (std::ranges::equal(getName(mode), iName, same))
			return mode;
	}
	return std::nullopt;
}

auto getHelp(const ViewNavigationMode iMode) -> const char* {
	switch (iMode) {
		case ViewNavigationMode::Select:
			return "Left click selects, left drag moves the gizmo. Right drag looks around (W A S D Q E fly, the wheel "
				   "sets the speed), middle drag pans, the wheel zooms.";
		case ViewNavigationMode::Orbit:
			return "Left drag orbits around the point the view turns around; a click still selects. Right drag looks "
				   "around and flies, middle drag pans, the wheel zooms. Pans in 2D and isometric views.";
		case ViewNavigationMode::Fly:
			return "Left drag looks around from the eye, W A S D move, Q and E go down and up, Shift is faster, the "
				   "wheel sets the speed; a click still selects. Pans in 2D and isometric views.";
		case ViewNavigationMode::Pan:
			return "Left drag pans the view like a hand; a click still selects. Right drag looks around, the wheel "
				   "zooms.";
	}
	return "";
}

auto getActionId(const ViewNavigationMode iMode) -> const char* {
	switch (iMode) {
		case ViewNavigationMode::Select:
			return "view.navigateSelect";
		case ViewNavigationMode::Orbit:
			return "view.navigateOrbit";
		case ViewNavigationMode::Fly:
			return "view.navigateFly";
		case ViewNavigationMode::Pan:
			return "view.navigatePan";
	}
	return "view.navigateSelect";
}

auto getLeftDrag(const ViewNavigationMode iMode) -> renderer::CameraEditor::LeftDrag {
	using LeftDrag = renderer::CameraEditor::LeftDrag;
	switch (iMode) {
		case ViewNavigationMode::Select:
			return LeftDrag::None;
		case ViewNavigationMode::Orbit:
			return LeftDrag::Orbit;
		case ViewNavigationMode::Fly:
			return LeftDrag::Look;
		case ViewNavigationMode::Pan:
			return LeftDrag::Pan;
	}
	return LeftDrag::None;
}

auto ViewNavigation::pressLeft(const bool iFree) -> bool {
	leftDragNavigates = iFree && mode != ViewNavigationMode::Select;
	pickOnRelease = leftDragNavigates;
	return iFree && !leftDragNavigates;
}

auto ViewNavigation::releaseLeft(const float iTravel) -> bool {
	const bool pick = pickOnRelease && iTravel < clickTravel;
	leftDragNavigates = false;
	pickOnRelease = false;
	return pick;
}

auto ViewNavigation::getActiveLeftDrag() const -> renderer::CameraEditor::LeftDrag {
	return leftDragNavigates ? getLeftDrag(mode) : renderer::CameraEditor::LeftDrag::None;
}

}// namespace owl::nest::panel
