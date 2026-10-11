/**
 * @file Backstage.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Backstage.h"

#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Tooltip.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <functional>
#include <string>

namespace owl::nest::panel {

namespace {

auto toU32(const math::vec4& iColor) -> ImU32 { return ImGui::GetColorU32(gui::vec(iColor)); }

// Navigation entry: glyph and label on a full-width row; returns true on click.
auto navItem(const char* iIcon, const char* iLabel, const bool iEnabled = true, const char* iShortcut = nullptr)
		-> bool {
	const auto& theme = gui::UiLayer::getTheme();
	const float height = ImGui::GetFrameHeight() + 6.f;
	const float width = ImGui::GetContentRegionAvail().x;
	ImGui::BeginDisabled(!iEnabled);
	const bool clicked = ImGui::InvisibleButton(iLabel, {width, height});
	const auto min = ImGui::GetItemRectMin();
	auto* draw = ImGui::GetWindowDrawList();
	if (ImGui::IsItemHovered())
		draw->AddRectFilled(min, {min.x + width, min.y + height}, toU32(theme.surface2), theme.frameRounding);
	const auto color = iEnabled ? theme.text : theme.textDisabled;
	const float textY = std::floor(min.y + (height - ImGui::GetTextLineHeight()) * 0.5f);
	const float pad = ImGui::GetStyle().FramePadding.x + 2.f;
	const float iconSize = ImGui::GetFontSize();
	if (const auto icon = gui::IconBank::instance().getIcon(iIcon, iconSize); icon.has_value()) {
		const ImVec2 iconMin{std::floor(min.x + pad), std::floor(min.y + (height - iconSize) * 0.5f)};
		// The style alpha greys the icon of a disabled entry.
		draw->AddImage(static_cast<ImTextureID>(icon->textureId), iconMin, {iconMin.x + iconSize, iconMin.y + iconSize},
					   gui::vec(icon->uv0), gui::vec(icon->uv1), ImGui::GetColorU32(IM_COL32_WHITE));
	}
	draw->AddText({min.x + pad + ImGui::GetFontSize() * 1.6f, textY}, toU32(color), iLabel);
	if (iShortcut != nullptr) {
		const auto size = ImGui::CalcTextSize(iShortcut);
		draw->AddText({min.x + width - pad - size.x, textY}, toU32(theme.textDisabled), iShortcut);
	}
	ImGui::EndDisabled();
	return clicked && iEnabled;
}

// Large card with a glyph, a title and a line of description; returns true on click.
auto card(const char* iIcon, const char* iTitle, const char* iDescription, const float iWidth) -> bool {
	const auto& theme = gui::UiLayer::getTheme();
	const float height = ImGui::GetFontSize() * 4.2f;
	const bool clicked = ImGui::InvisibleButton(iTitle, {iWidth, height});
	const bool hovered = ImGui::IsItemHovered();
	const auto min = ImGui::GetItemRectMin();
	const ImVec2 max{min.x + iWidth, min.y + height};
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(min, max, toU32(theme.surface2), theme.windowRounding * 1.5f);
	draw->AddRect(min, max, toU32(hovered ? gui::Theme::withAlpha(theme.accent, 0.6f) : theme.line),
				  theme.windowRounding * 1.5f);
	const float pad = ImGui::GetFontSize();
	const float iconSize = std::round(ImGui::GetFontSize() * 1.5f);
	if (const auto icon = gui::IconBank::instance().getIcon(iIcon, iconSize); icon.has_value())
		draw->AddImage(static_cast<ImTextureID>(icon->textureId), {std::floor(min.x + pad), std::floor(min.y + pad)},
					   {std::floor(min.x + pad) + iconSize, std::floor(min.y + pad) + iconSize}, gui::vec(icon->uv0),
					   gui::vec(icon->uv1));
	const float glyphWidth = iconSize;
	gui::UiLayer::pushTextStyle(gui::TextStyle::Strong);
	draw->AddText({min.x + pad + glyphWidth + pad * 0.5f, min.y + pad}, toU32(theme.text), iTitle);
	gui::UiLayer::popTextStyle();
	draw->PushClipRect(min, max, true);
	draw->AddText(nullptr, 0.f, {min.x + pad + glyphWidth + pad * 0.5f, min.y + pad + ImGui::GetTextLineHeight() + 4.f},
				  toU32(theme.textMuted), iDescription, nullptr, iWidth - glyphWidth - pad * 2.5f);
	draw->PopClipRect();
	return clicked;
}

void call(const std::function<void()>& iAction) {
	if (iAction)
		iAction();
}

}// namespace

void Backstage::onImGuiRender(const BackstageContext& iContext) {
	if (!m_open)
		return;
	const auto& theme = gui::UiLayer::getTheme();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::SetNextWindowFocus();
	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
									   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, gui::vec(theme.surface1));
	const bool visible = ImGui::Begin("##backstage", nullptr, flags);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
	if (!visible) {
		ImGui::End();
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		m_open = false;
	const bool hasProject = !iContext.projectName.empty();
	bool act = false;
	const auto run = [&act](const std::function<void()>& iAction) -> void {
		call(iAction);
		act = true;
	};

	// Navigation column.
	const float navWidth = ImGui::GetFontSize() * 14.f;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, gui::vec(theme.surface0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.f, 12.f});
	ImGui::BeginChild("##backstageNav", {navWidth, 0.f}, ImGuiChildFlags_AlwaysUseWindowPadding);
	{
		gui::UiLayer::pushTextStyle(gui::TextStyle::Strong);
		ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(theme.accent));
		if (navItem("ribbon_back", "Back to the editor"))
			m_open = false;
		ImGui::PopStyleColor();
		gui::UiLayer::popTextStyle();
		ImGui::Spacing();
		if (navItem("ribbon_folder_new", "New project"))
			run(iContext.newProject);
		if (navItem("ribbon_open", "Open project"))
			run(iContext.openProject);
		ImGui::Separator();
		if (navItem("ribbon_save", "Save project", hasProject))
			run(iContext.saveProject);
		if (navItem("ribbon_save", "Save project as...", hasProject))
			run(iContext.saveProjectAs);
		if (navItem("ribbon_pack", "Package the game", hasProject))
			run(iContext.packGame);
		ImGui::Separator();
		if (navItem("ribbon_settings", "Editor settings"))
			run(iContext.editorSettings);
		if (navItem("ribbon_tune", "Project settings", hasProject))
			run(iContext.projectSettings);
		if (navItem("ribbon_stats", "Engine parameters"))
			run(iContext.engineSettings);
		if (navItem("ribbon_help", "Help", true, "F1")) {
			if (iContext.help)
				iContext.help({});
			act = true;
		}
		ImGui::Separator();
		if (navItem("ribbon_close", "Close project", hasProject))
			run(iContext.closeProject);
		if (navItem("ribbon_exit", "Exit"))
			run(iContext.exit);
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	// Home: project, start cards and recent projects.
	ImGui::SameLine(0.f, 0.f);
	const float pad = ImGui::GetFontSize() * 2.f;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {pad, pad});
	ImGui::BeginChild("##backstageHome", {0.f, 0.f}, ImGuiChildFlags_AlwaysUseWindowPadding);
	{
		gui::UiLayer::pushTextStyle(gui::TextStyle::Title);
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.5f);
		ImGui::TextUnformatted(hasProject ? iContext.projectName.c_str() : "Welcome to Owl Nest");
		ImGui::PopFont();
		gui::UiLayer::popTextStyle();
		ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(theme.textMuted));
		if (hasProject)
			ImGui::TextUnformatted(iContext.projectDirectory.string().c_str());
		else
			ImGui::TextUnformatted("Create a project or open one to start.");
		ImGui::PopStyleColor();
		ImGui::Dummy({0.f, ImGui::GetFontSize()});

		const float maxWidth = std::min(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 54.f);
		const float spacing = ImGui::GetStyle().ItemSpacing.x * 1.5f;
		const float cardWidth = std::floor((maxWidth - 2.f * spacing) / 3.f);
		if (card("ribbon_folder_new", "New project", "From a template: 2D, raycast, voxel or empty.", cardWidth))
			run(iContext.newProject);
		ImGui::SameLine(0.f, spacing);
		if (card("ribbon_open", "Open project", "Pick an owl_project.yml or its folder.", cardWidth))
			run(iContext.openProject);
		ImGui::SameLine(0.f, spacing);
		if (card("ribbon_book", "Getting started", "A short tour of the editor and its panels.", cardWidth)) {
			if (iContext.help)
				iContext.help("getting_started");
			act = true;
		}

		ImGui::Dummy({0.f, ImGui::GetFontSize()});
		gui::UiLayer::pushTextStyle(gui::TextStyle::Caption);
		ImGui::TextColored(gui::vec(theme.textDisabled), "RECENT PROJECTS");
		gui::UiLayer::popTextStyle();
		if (iContext.recentProjects.empty()) {
			ImGui::TextColored(gui::vec(theme.textMuted), "No recent project yet.");
		} else {
			std::filesystem::path toForget;
			const float rowHeight = ImGui::GetTextLineHeight() * 2.f + 12.f;
			for (const auto& recent: iContext.recentProjects) {
				const std::filesystem::path path{recent};
				ImGui::PushID(recent.c_str());
				const auto origin = ImGui::GetCursorScreenPos();
				ImGui::SetNextItemAllowOverlap();
				if (ImGui::InvisibleButton("##recent", {maxWidth, rowHeight})) {
					if (iContext.openRecent)
						iContext.openRecent(path);
					act = true;
				}
				const bool hovered = ImGui::IsItemHovered();
				auto* draw = ImGui::GetWindowDrawList();
				if (hovered)
					draw->AddRectFilled(origin, {origin.x + maxWidth, origin.y + rowHeight}, toU32(theme.surface2),
										theme.frameRounding);
				gui::UiLayer::pushTextStyle(gui::TextStyle::Strong);
				draw->AddText({origin.x + 10.f, origin.y + 6.f}, toU32(theme.text), path.filename().string().c_str());
				gui::UiLayer::popTextStyle();
				draw->AddText({origin.x + 10.f, origin.y + 8.f + ImGui::GetTextLineHeight()}, toU32(theme.textDisabled),
							  recent.c_str());
				ImGui::SetCursorScreenPos({origin.x + maxWidth - ImGui::GetFrameHeight() - 6.f,
										   origin.y + (rowHeight - ImGui::GetFrameHeight()) * 0.5f});
				if (gui::widgets::iconButton("forget", {.glyph = gui::icon::x, .tooltip = "Remove from the list"}))
					toForget = path;
				ImGui::SetCursorScreenPos({origin.x, origin.y + rowHeight});
				ImGui::Dummy({0.f, 0.f});
				ImGui::PopID();
			}
			if (!toForget.empty() && iContext.forgetRecent)
				iContext.forgetRecent(toForget);
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::End();
	if (act)
		m_open = false;
}

}// namespace owl::nest::panel
