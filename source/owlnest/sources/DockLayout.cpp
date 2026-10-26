/**
 * @file DockLayout.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DockLayout.h"

#include <core/Macros.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest {

auto panelWindow::title(const std::string_view iGlyph, const std::string_view iLabel, const std::string_view iWindow)
		-> std::string {
	return std::format("{}  {}{}", iGlyph, iLabel, iWindow);
}

namespace {

// Share of the dockspace width given to the right column.
constexpr float g_rightRatio = 0.24f;
// Share of the remaining width given to the left column (HierarchyLeft).
constexpr float g_leftRatio = 0.2f;
// Share of the documents column height given to the bottom panels.
constexpr float g_bottomRatio = 0.3f;
// Share of the right column height given to the hierarchy (Default).
constexpr float g_hierarchyRatio = 0.42f;

constexpr std::array g_panels{panelWindow::hierarchy,    panelWindow::properties,  panelWindow::contentBrowser,
							  panelWindow::log,          panelWindow::console,     panelWindow::stats,
							  panelWindow::voxelPalette, panelWindow::tilePalette, panelWindow::sceneSettings,
							  panelWindow::assetImport,  panelWindow::help};

auto isPanelWindow(const ImGuiID iWindowId) -> bool {
	return std::ranges::any_of(g_panels,
							   [iWindowId](const char* iName) -> bool { return ImHashStr(iName) == iWindowId; });
}

auto isInDockSpace(const ImGuiID iDockId, const ImGuiID iDockSpaceId) -> bool {
	if (iDockId == 0)
		return false;
	auto* node = ImGui::DockBuilderGetNode(iDockId);
	return node != nullptr && ImGui::DockNodeGetRootNode(node)->ID == iDockSpaceId;
}

// DockBuilderDockWindow forgets the tab order; tabs appearing on the same frame are sorted by it.
void dockInOrder(const std::initializer_list<const char*> iNames, const ImGuiID iNodeId) {
	short order = 0;
	for (const auto* name: iNames) {
		ImGui::DockBuilderDockWindow(name, iNodeId);
		if (auto* window = ImGui::FindWindowByName(name); window != nullptr)
			window->DockOrder = order;
		else if (auto* settings = ImGui::FindWindowSettingsByID(ImHashStr(name)); settings != nullptr)
			settings->DockOrder = order;
		++order;
	}
}

}// namespace

auto DockLayout::getPresetName(const LayoutPreset iPreset) -> std::string_view {
	switch (iPreset) {
		case LayoutPreset::Default:
			return "Default";
		case LayoutPreset::HierarchyLeft:
			return "HierarchyLeft";
	}
	return "Default";
}

auto DockLayout::presetFromName(const std::string_view iName) -> LayoutPreset {
	if (iName == getPresetName(LayoutPreset::HierarchyLeft))
		return LayoutPreset::HierarchyLeft;
	return LayoutPreset::Default;
}

void DockLayout::build(const uint32_t iDockSpaceId, const LayoutPreset iPreset,
					   const std::vector<std::string>& iDocumentWindows) {
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	ImGui::DockBuilderRemoveNode(iDockSpaceId);
	ImGui::DockBuilderAddNode(iDockSpaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(iDockSpaceId, ImVec2(std::max(avail.x, 64.f), std::max(avail.y, 64.f)));

	ImGuiID center = iDockSpaceId;
	const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, g_rightRatio, nullptr, &center);
	ImGuiID hierarchy = 0;
	if (iPreset == LayoutPreset::HierarchyLeft)
		hierarchy = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, g_leftRatio, nullptr, &center);
	const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, g_bottomRatio, nullptr, &center);
	ImGuiID properties = right;
	if (hierarchy == 0)
		hierarchy = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, g_hierarchyRatio, nullptr, &properties);

	ImGui::DockBuilderDockWindow(panelWindow::hierarchy, hierarchy);
	for (const auto* name: {panelWindow::properties, panelWindow::tilePalette, panelWindow::voxelPalette,
							panelWindow::sceneSettings, panelWindow::assetImport})
		ImGui::DockBuilderDockWindow(name, properties);
	// The help pages need width: they open among the documents.
	ImGui::DockBuilderDockWindow(panelWindow::help, center);
	dockInOrder({panelWindow::contentBrowser, panelWindow::log, panelWindow::console, panelWindow::stats}, bottom);
	for (const auto& name: iDocumentWindows) ImGui::DockBuilderDockWindow(name.c_str(), center);
	// A new tab bar shows its node's saved tab, else the last tab added: the Stats panel.
	if (auto* bottomNode = ImGui::DockBuilderGetNode(bottom); bottomNode != nullptr)
		bottomNode->SelectedTabId = ImHashStr("#TAB", 0, ImHashStr(panelWindow::contentBrowser));
	ImGui::DockBuilderFinish(iDockSpaceId);
}

auto DockLayout::collectDocumentWindows(const uint32_t iDockSpaceId) -> std::vector<std::string> {
	std::vector<std::string> names;
	std::vector<ImGuiID> seen;
	const auto keep = [&](const ImGuiID iId, const char* iName, const ImGuiID iDockId) -> void {
		if (isPanelWindow(iId) || std::ranges::contains(seen, iId) || !isInDockSpace(iDockId, iDockSpaceId))
			return;
		seen.push_back(iId);
		names.emplace_back(iName);
	};
	ImGuiContext& ctx = *ImGui::GetCurrentContext();
	for (const ImGuiWindow* window: ctx.Windows) {
		// Docked windows carry the child flag too; plain child windows are filtered out by their null dock ID.
		if (window->DockNodeAsHost != nullptr)
			continue;
		keep(window->ID, window->Name, window->DockId);
	}
	// Saved windows not submitted yet (documents reopened later by the session restore) still hold a dock ID.
	for (auto* settings = ctx.SettingsWindows.begin(); settings != nullptr;
		 settings = ctx.SettingsWindows.next_chunk(settings))
		keep(settings->ID, settings->GetName(), settings->DockId);
	return names;
}

void DockLayout::request(const LayoutPreset iPreset) {
	m_preset = iPreset;
	m_pending = true;
}

auto DockLayout::onDockSpace(const uint32_t iDockSpaceId, int& ioSavedVersion) -> bool {
	bool rebuild = m_pending;
	if (!m_checked) {
		m_checked = true;
		const auto* node = ImGui::DockBuilderGetNode(iDockSpaceId);
		rebuild = rebuild || node == nullptr || !node->IsSplitNode() || ioSavedVersion < version;
	}
	if (!rebuild)
		return false;
	build(iDockSpaceId, m_preset, collectDocumentWindows(iDockSpaceId));
	m_pending = false;
	ioSavedVersion = version;
	return true;
}

}// namespace owl::nest
