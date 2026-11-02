/**
 * @file NewProjectDialog.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "NewProjectDialog.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <platform/FileDialog.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace owl::nest::panel {

namespace {
constexpr const char* g_popupName = "New Project";
constexpr const char* g_defaultName = "MyGame";
constexpr std::string_view g_reservedCharacters = "/\\:*?\"<>|";

auto toU32(const math::vec4& iColor) -> ImU32 { return ImGui::GetColorU32(gui::vec(iColor)); }

// The text cut with an ellipsis so it fits in `iWidth` pixels.
auto fitText(const std::string& iText, const float iWidth) -> std::string {
	if (ImGui::CalcTextSize(iText.c_str()).x <= iWidth)
		return iText;
	std::string cut = iText;
	while (!cut.empty() && ImGui::CalcTextSize((cut + "...").c_str()).x > iWidth) cut.pop_back();
	return cut + "...";
}

// Small rounded tag pills from `iOrigin` along a row; returns the x where the row ends.
auto drawTagPills(ImDrawList* ioDraw, const std::vector<std::string>& iTags, const ImVec2& iOrigin, const float iMaxX)
		-> float {
	const auto& theme = gui::UiLayer::getTheme();
	gui::UiLayer::pushTextStyle(gui::TextStyle::Caption);
	const float padX = std::round(ImGui::GetFontSize() * 0.5f);
	const float height = std::round(ImGui::GetTextLineHeight() + ImGui::GetFontSize() * 0.3f);
	float x = iOrigin.x;
	for (const auto& tag: iTags) {
		const float width = ImGui::CalcTextSize(tag.c_str()).x + padX * 2.f;
		if (x + width > iMaxX)
			break;
		ioDraw->AddRectFilled({x, iOrigin.y}, {x + width, iOrigin.y + height}, toU32(theme.surface4), height * 0.5f);
		ioDraw->AddText({x + padX, std::floor(iOrigin.y + (height - ImGui::GetTextLineHeight()) * 0.5f)},
						toU32(theme.textMuted), tag.c_str());
		x += width + padX * 0.5f;
	}
	gui::UiLayer::popTextStyle();
	return x;
}

// Preview image, or a well with an image glyph when there is none, in a rounded frame.
void drawPreview(ImDrawList* ioDraw, const shared<renderer::gpu::Texture2D>& iTexture, const ImVec2& iMin,
				 const ImVec2& iMax, const float iRounding, const ImDrawFlags iFlags) {
	const auto& theme = gui::UiLayer::getTheme();
	if (const auto texture = gui::imTexture(iTexture); texture.has_value()) {
		ioDraw->AddImageRounded(*texture, iMin, iMax, {0.f, 1.f}, {1.f, 0.f}, IM_COL32_WHITE, iRounding, iFlags);
		return;
	}
	ioDraw->AddRectFilled(iMin, iMax, toU32(theme.well), iRounding, iFlags);
	const auto size = ImGui::CalcTextSize(gui::icon::image);
	ioDraw->AddText({std::floor((iMin.x + iMax.x - size.x) * 0.5f), std::floor((iMin.y + iMax.y - size.y) * 0.5f)},
					toU32(theme.textDisabled), gui::icon::image);
}

auto isValidFolderName(const std::string& iName) -> bool {
	if (iName == "." || iName == ".." || iName.front() == ' ' || iName.back() == ' ' || iName.back() == '.')
		return false;
	return std::ranges::none_of(iName, [](const char iChar) -> bool {
		return static_cast<unsigned char>(iChar) < 0x20 || g_reservedCharacters.find(iChar) != std::string_view::npos;
	});
}

}// namespace

auto describe(const NewProjectIssue iIssue) -> std::string_view {
	switch (iIssue) {
		case NewProjectIssue::None:
			return "";
		case NewProjectIssue::EmptyName:
			return "Type a name for the project.";
		case NewProjectIssue::InvalidName:
			return "The name cannot be a folder name: no / \\ : * ? \" < > |, no leading or trailing space or dot.";
		case NewProjectIssue::EmptyLocation:
			return "Pick the folder that will hold the project.";
		case NewProjectIssue::MissingLocation:
			return "The location is not an existing folder.";
		case NewProjectIssue::DestinationNotEmpty:
			return "A folder of that name already holds files: pick another name or location.";
	}
	return "";
}

auto validateNewProject(const std::string& iName, const std::string& iParentDirectory) -> NewProjectIssue {
	if (iName.empty())
		return NewProjectIssue::EmptyName;
	if (!isValidFolderName(iName))
		return NewProjectIssue::InvalidName;
	if (iParentDirectory.empty())
		return NewProjectIssue::EmptyLocation;
	std::error_code ec;
	const std::filesystem::path parent{iParentDirectory};
	if (!is_directory(parent, ec))
		return NewProjectIssue::MissingLocation;
	if (const auto target = parent / iName; exists(target, ec) && (!is_directory(target, ec) || !is_empty(target, ec)))
		return NewProjectIssue::DestinationNotEmpty;
	return NewProjectIssue::None;
}

auto moveInGrid(const size_t iCurrent, const size_t iCount, const size_t iColumns, const int iDeltaX, const int iDeltaY)
		-> size_t {
	if (iCount == 0)
		return 0;
	const auto count = static_cast<int64_t>(iCount);
	const auto columns = static_cast<int64_t>(std::max<size_t>(iColumns, 1));
	auto index = std::min(static_cast<int64_t>(iCurrent), count - 1);
	if (const auto row = index + iDeltaY * columns; row >= 0 && row < count)
		index = row;
	else if (iDeltaY > 0 && index / columns < (count - 1) / columns)
		index = count - 1;
	return static_cast<size_t>(std::clamp<int64_t>(index + iDeltaX, 0, count - 1));
}

void NewProjectDialog::open(const std::vector<ProjectTemplate>& iTemplates,
							const std::filesystem::path& iParentDirectory) {
	m_templates = iTemplates;
	m_previews.clear();
	m_parentDirectory = iParentDirectory.string();
	m_name = g_defaultName;
	for (int suffix = 2;
		 validateNewProject(m_name, m_parentDirectory) == NewProjectIssue::DestinationNotEmpty && suffix < 100;
		 ++suffix)
		m_name = std::format("{}{}", g_defaultName, suffix);
	m_tagFilter.clear();
	m_selected = 0;
	m_open = true;
	m_openRequested = true;
	m_scrollToSelection = false;
}

void NewProjectDialog::setFields(const std::string& iName, const std::string& iParentDirectory,
								 const size_t iTemplateIndex) {
	m_name = iName;
	m_parentDirectory = iParentDirectory;
	m_selected = iTemplateIndex;
}

auto NewProjectDialog::makeRequest() const -> std::optional<NewProjectRequest> {
	if (const auto issue = getIssue(); issue != NewProjectIssue::None) {
		OWL_WARN("New Project: {}", describe(issue))
		return std::nullopt;
	}
	NewProjectRequest request{.projectTemplate = std::nullopt,
							  .directory = std::filesystem::path{m_parentDirectory} / m_name,
							  .name = m_name};
	if (m_selected < m_templates.size())
		request.projectTemplate = m_templates[m_selected];
	return request;
}

auto NewProjectDialog::getTags() const -> std::vector<std::string> {
	std::vector<std::string> tags;
	for (const auto& tpl: m_templates) {
		for (const auto& tag: tpl.tags) {
			if (std::ranges::find(tags, tag) == tags.end())
				tags.push_back(tag);
		}
	}
	return tags;
}

void NewProjectDialog::setTagFilter(const std::string& iTag) {
	m_tagFilter = iTag;
	const auto visible = getVisibleTemplates();
	if (!visible.empty() && std::ranges::find(visible, m_selected) == visible.end())
		m_selected = visible.front();
}

auto NewProjectDialog::getVisibleTemplates() const -> std::vector<size_t> {
	std::vector<size_t> visible;
	for (size_t i = 0; i < m_templates.size(); ++i) {
		if (m_tagFilter.empty() || std::ranges::find(m_templates[i].tags, m_tagFilter) != m_templates[i].tags.end())
			visible.push_back(i);
	}
	return visible;
}

void NewProjectDialog::moveSelection(const size_t iColumns, const int iDeltaX, const int iDeltaY) {
	const auto visible = getVisibleTemplates();
	if (visible.empty())
		return;
	const auto found = std::ranges::find(visible, m_selected);
	const auto position = static_cast<size_t>(std::distance(visible.begin(), found));
	m_selected = visible[moveInGrid(found == visible.end() ? 0 : position, visible.size(), iColumns, iDeltaX, iDeltaY)];
}

auto NewProjectDialog::getPreview(const size_t iIndex) -> shared<renderer::gpu::Texture2D> {
	if (const auto found = m_previews.find(iIndex); found != m_previews.end())
		return found->second;
	shared<renderer::gpu::Texture2D> texture;
	if (iIndex < m_templates.size() && !m_templates[iIndex].preview.empty())
		texture = renderer::gpu::Texture2D::create(m_templates[iIndex].preview);
	m_previews.emplace(iIndex, texture);
	return texture;
}

void NewProjectDialog::renderTemplateGrid(const float iHeight) {
	const auto& theme = gui::UiLayer::getTheme();
	const float em = ImGui::GetFontSize();
	if (gui::widgets::chip("All", m_tagFilter.empty()))
		setTagFilter({});
	for (const auto& tag: getTags()) {
		ImGui::SameLine();
		if (gui::widgets::chip(tag.c_str(), m_tagFilter == tag))
			setTagFilter(m_tagFilter == tag ? std::string{} : tag);
	}
	ImGui::Spacing();
	ImGui::BeginChild("##cards", {0.f, iHeight - ImGui::GetCursorPosY()}, ImGuiChildFlags_None,
					  ImGuiWindowFlags_NoNavInputs);
	const float spacing = std::round(em * 0.75f);
	const float available = ImGui::GetContentRegionAvail().x;
	const float minWidth = em * 12.f;
	m_columns = std::max<size_t>(1, static_cast<size_t>((available + spacing) / (minWidth + spacing)));
	const auto columns = static_cast<float>(m_columns);
	const float cardWidth = std::floor((available - (columns - 1.f) * spacing) / columns);
	const float previewHeight = std::floor(cardWidth * 9.f / 16.f);
	const float lineHeight = ImGui::GetTextLineHeight();
	const float pad = std::round(em * 0.6f);
	const float cardHeight = previewHeight + pad * 4.f + lineHeight * 3.f;
	const float rounding = theme.windowRounding * 1.5f;
	auto* draw = ImGui::GetWindowDrawList();
	size_t shown = 0;
	ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
	for (const auto index: getVisibleTemplates()) {
		const auto& tpl = m_templates[index];
		if (shown++ % m_columns != 0)
			ImGui::SameLine(0.f, spacing);
		ImGui::PushID(static_cast<int>(index));
		if (ImGui::InvisibleButton("##card", {cardWidth, cardHeight}))
			m_selected = index;
		const bool hovered = ImGui::IsItemHovered();
		const bool selected = m_selected == index;
		if (selected && m_scrollToSelection) {
			ImGui::SetScrollHereY();
			m_scrollToSelection = false;
		}
		const auto min = ImGui::GetItemRectMin();
		const ImVec2 max{min.x + cardWidth, min.y + cardHeight};
		draw->AddRectFilled(min, max, toU32(selected ? theme.surface3 : theme.surface2), rounding);
		drawPreview(draw, getPreview(index), min, {max.x, min.y + previewHeight}, rounding,
					ImDrawFlags_RoundCornersTop);
		const float textX = min.x + pad;
		float y = min.y + previewHeight + pad;
		gui::UiLayer::pushTextStyle(gui::TextStyle::Strong);
		draw->AddText({textX, y}, toU32(theme.text), fitText(tpl.name, cardWidth - pad * 2.f).c_str());
		gui::UiLayer::popTextStyle();
		y += lineHeight + pad * 0.5f;
		draw->AddText({textX, y}, toU32(theme.textMuted), fitText(tpl.description, cardWidth - pad * 2.f).c_str());
		y += lineHeight + pad;
		std::ignore = drawTagPills(draw, tpl.tags, {textX, y}, max.x - pad);
		if (selected)
			draw->AddRect(min, max, toU32(theme.accent), rounding, 0, 2.f);
		else if (hovered)
			draw->AddRect(min, max, toU32(gui::Theme::withAlpha(theme.accent, 0.6f)), rounding);
		else
			draw->AddRect(min, max, toU32(theme.line), rounding);
		if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			m_createRequested = true;
		ImGui::PopID();
	}
	ImGui::PopItemFlag();
	if (m_templates.empty())
		gui::widgets::notice("No template installed: the project starts empty.", gui::widgets::BadgeKind::Info);
	ImGui::EndChild();
}

void NewProjectDialog::renderDetails(const ProjectTemplate& iTemplate, const float iWidth) {
	const auto& theme = gui::UiLayer::getTheme();
	const float previewHeight = std::floor(iWidth * 9.f / 16.f);
	const auto origin = ImGui::GetCursorScreenPos();
	drawPreview(ImGui::GetWindowDrawList(), getPreview(m_selected), origin,
				{origin.x + iWidth, origin.y + previewHeight}, theme.windowRounding * 1.5f,
				ImDrawFlags_RoundCornersAll);
	ImGui::Dummy({iWidth, previewHeight});
	ImGui::Spacing();
	gui::UiLayer::pushTextStyle(gui::TextStyle::Title);
	ImGui::TextUnformatted(iTemplate.name.c_str());
	gui::UiLayer::popTextStyle();
	for (size_t i = 0; i < iTemplate.tags.size(); ++i) {
		if (i > 0)
			ImGui::SameLine();
		gui::widgets::badge(iTemplate.tags[i].c_str(),
							i == 0 ? gui::widgets::BadgeKind::Accent : gui::widgets::BadgeKind::Neutral);
	}
	ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(theme.textMuted));
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextUnformatted(iTemplate.description.c_str());
	ImGui::PopTextWrapPos();
	ImGui::PopStyleColor();
	if (!iTemplate.contents.empty() && gui::widgets::sectionHeader("Contents", gui::icon::package, false).open) {
		ImGui::PushTextWrapPos(0.f);
		for (const auto& item: iTemplate.contents) {
			ImGui::Bullet();
			ImGui::SameLine();
			ImGui::TextUnformatted(item.c_str());
		}
		ImGui::PopTextWrapPos();
		gui::widgets::endSection();
	}
}

void NewProjectDialog::renderProjectFields() {
	if (!gui::widgets::sectionHeader("Project", gui::icon::folder, false).open)
		return;
	if (gui::widgets::beginProperties("##newProject")) {
		std::ignore = gui::widgets::propertyText("Name", m_name, g_defaultName, "Name of the game, also its folder.");
		gui::widgets::propertyLabel("Location", "Folder that will hold the project folder.");
		const float buttonWidth = ImGui::GetFrameHeight();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x);
		ImGui::InputTextWithHint("##location", "Parent folder", &m_parentDirectory);
		ImGui::SameLine();
		if (gui::widgets::iconButton("browse", {.glyph = gui::icon::folderOpen, .tooltip = "Pick the folder"})) {
			if (const auto picked = platform::FileDialog::pickFolder(); !picked.empty())
				m_parentDirectory = picked.string();
		}
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::endProperties();
	}
	if (const auto issue = getIssue(); issue != NewProjectIssue::None) {
		gui::widgets::notice(std::string{describe(issue)}.c_str(), gui::widgets::BadgeKind::Error);
	} else {
		const auto target = (std::filesystem::path{m_parentDirectory} / m_name).string();
		ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(gui::UiLayer::getTheme().textDisabled));
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextUnformatted(std::format("Creates {}", target).c_str());
		ImGui::PopTextWrapPos();
		ImGui::PopStyleColor();
	}
	gui::widgets::endSection();
}

auto NewProjectDialog::onImGuiRender() -> std::optional<NewProjectRequest> {
	if (!m_open)
		return std::nullopt;
	if (m_openRequested) {
		ImGui::OpenPopup(g_popupName);
		m_openRequested = false;
	}
	const float em = ImGui::GetFontSize();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowSize(
			{std::min(em * 74.f, viewport->WorkSize.x * 0.94f), std::min(em * 46.f, viewport->WorkSize.y * 0.92f)},
			ImGuiCond_Appearing);
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
	if (!ImGui::BeginPopupModal(g_popupName, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse))
		return std::nullopt;
	const auto& theme = gui::UiLayer::getTheme();
	const auto& style = ImGui::GetStyle();
	m_createRequested = false;

	const float footerHeight = ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.f + style.WindowPadding.y * 0.5f;
	const float bodyHeight = std::max(em * 10.f, ImGui::GetContentRegionAvail().y - footerHeight);
	const float detailsWidth = std::max(em * 22.f, ImGui::GetContentRegionAvail().x * 0.36f);

	ImGui::BeginChild("##templates",
					  {ImGui::GetContentRegionAvail().x - detailsWidth - style.ItemSpacing.x, bodyHeight},
					  ImGuiChildFlags_None, ImGuiWindowFlags_NoNavInputs);
	renderTemplateGrid(bodyHeight);
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_ChildBg, gui::vec(theme.surface0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {em * 0.8f, em * 0.8f});
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.windowRounding * 1.5f);
	ImGui::BeginChild("##details", {0.f, bodyHeight}, ImGuiChildFlags_AlwaysUseWindowPadding);
	if (m_selected < m_templates.size())
		renderDetails(m_templates[m_selected], ImGui::GetContentRegionAvail().x);
	ImGui::Spacing();
	renderProjectFields();
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();

	// Arrows walk the cards, Enter creates, Escape cancels, unless a text field has the keyboard.
	const bool typing = ImGui::IsAnyItemActive();
	if (!typing && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
		const std::pair<ImGuiKey, std::pair<int, int>> moves[] = {{ImGuiKey_LeftArrow, {-1, 0}},
																  {ImGuiKey_RightArrow, {1, 0}},
																  {ImGuiKey_UpArrow, {0, -1}},
																  {ImGuiKey_DownArrow, {0, 1}}};
		for (const auto& [key, delta]: moves) {
			if (ImGui::IsKeyPressed(key)) {
				moveSelection(m_columns, delta.first, delta.second);
				m_scrollToSelection = true;
			}
		}
	}
	const bool valid = getIssue() == NewProjectIssue::None;
	if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
		m_createRequested = true;
	bool close = !typing && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

	ImGui::Separator();
	ImGui::Dummy({0.f, style.WindowPadding.y * 0.25f});
	const float buttonWidth = em * 7.f;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonWidth * 2.f -
						 style.ItemSpacing.x);
	if (gui::widgets::button("Cancel", {.glyph = gui::icon::x, .label = "Cancel", .width = buttonWidth}))
		close = true;
	ImGui::SameLine();
	if (gui::widgets::button("Create", {.glyph = gui::icon::folderPlus,
										.label = "Create",
										.tooltip = valid ? nullptr : describe(getIssue()).data(),
										.primary = true,
										.disabled = !valid,
										.width = buttonWidth}))
		m_createRequested = true;

	std::optional<NewProjectRequest> request;
	if (m_createRequested && valid) {
		request = makeRequest();
		close = true;
	}
	m_createRequested = false;
	if (close) {
		m_open = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
	return request;
}

}// namespace owl::nest::panel
