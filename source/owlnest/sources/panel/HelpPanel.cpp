/**
 * @file HelpPanel.cpp
 * @author Silmaen
 * @date 28/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HelpPanel.h"

#include "../DockLayout.h"

#include <app/Application.h>
#include <core/Log.h>
#include <core/Macros.h>
#include <gui/IconFont.h>
#include <gui/Theme.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/TreeRow.h>

#include <yaml-cpp/yaml.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace owl::nest::panel {

namespace {
constexpr const char* kIndexFileName = "index.yml";

[[nodiscard]] auto toLower(std::string iText) -> std::string {
	std::ranges::transform(iText, iText.begin(),
						   [](const unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
	return iText;
}

[[nodiscard]] auto readFileToString(const std::filesystem::path& iPath) -> std::string {
	const std::ifstream stream(iPath, std::ios::binary);
	if (!stream)
		return {};
	std::stringstream buffer;
	buffer << stream.rdbuf();
	return buffer.str();
}

}// namespace

HelpPanel::HelpPanel() {
	m_renderer.setLinkCallback([this](const std::string& iHref) -> void { onLinkClicked(iHref); });
}

void HelpPanel::open() {
	if (m_pages.empty())
		loadIndex();
	if (m_currentPageId.empty())
		loadPage({});
	m_visible = true;
}

void HelpPanel::open(const std::string& iPageId) {
	if (m_pages.empty())
		loadIndex();
	loadPage(iPageId);
	m_visible = true;
}

auto HelpPanel::resolveHelpRoot() -> std::filesystem::path {
	std::vector<std::filesystem::path> assetDirs;
	if (app::Application::instanced()) {
		for (const auto& dir: app::Application::get().getAssetDirectories()) assetDirs.push_back(dir.assetsPath);
	}
#ifdef OWL_HELP_BUILD_DIR
	const std::filesystem::path buildBundle{OWL_HELP_BUILD_DIR};
#else
	const std::filesystem::path buildBundle;
#endif
	return resolveHelpRoot(buildBundle, assetDirs);
}

auto HelpPanel::resolveHelpRoot(const std::filesystem::path& iBuildBundle,
								const std::vector<std::filesystem::path>& iAssetDirs) -> std::filesystem::path {
	if (!iBuildBundle.empty() && exists(iBuildBundle / kIndexFileName))
		return iBuildBundle;
	for (const auto& dir: iAssetDirs) {
		if (auto candidate = dir / "help"; exists(candidate / kIndexFileName))
			return candidate;
	}
	return {};
}

auto HelpPanel::readIndex(const std::filesystem::path& iIndexFile) -> std::vector<HelpPage> {
	YAML::Node root;
	try {
		root = YAML::LoadFile(iIndexFile.string());
	} catch (const std::exception& ex) {
		OWL_CORE_ERROR("HelpPanel: Failed to parse '{}' ({}).", iIndexFile.string(), ex.what())
		return {};
	}
	const auto pagesNode = root["Help"]["Pages"];
	if (!pagesNode || !pagesNode.IsSequence()) {
		OWL_CORE_WARN("HelpPanel: '{}' has no Help/Pages sequence.", iIndexFile.string())
		return {};
	}
	std::vector<HelpPage> pages;
	pages.reserve(pagesNode.size());
	for (const auto& entry: pagesNode) {
		HelpPage page;
		page.id = entry["id"].as<std::string>("");
		page.title = entry["title"].as<std::string>(page.id);
		page.group = entry["group"].as<std::string>("Help");
		page.path = entry["path"].as<std::string>("");
		if (!page.id.empty() && !page.path.empty())
			pages.push_back(std::move(page));
	}
	return pages;
}

void HelpPanel::loadIndex() {
	m_pages.clear();
	m_helpRoot = resolveHelpRoot();
	if (m_helpRoot.empty()) {
		OWL_CORE_WARN("HelpPanel: Could not locate the help bundle (help/index.yml).")
		return;
	}
	// The renderer needs the help root to resolve relative image paths (`![](images/foo.svg)`).
	m_renderer.setBaseDirectory(m_helpRoot);
	m_pages = readIndex(m_helpRoot / kIndexFileName);
}

void HelpPanel::loadPage(const std::string& iId) {
	// Page ids are matched case-insensitively: `CONTRIBUTING.md` and `contributing.md` are one page.
	const auto findEntry = [this](const std::string& iLookup) -> const HelpPage* {
		const auto lookup = toLower(iLookup);
		for (const auto& p: m_pages)
			if (toLower(p.id) == lookup)
				return &p;
		return nullptr;
	};
	// Resolution chain: requested id → start page (the first of the index).
	const HelpPage* match = iId.empty() ? nullptr : findEntry(iId);
	if (match == nullptr && !m_pages.empty())
		match = &m_pages.front();
	if (match == nullptr) {
		m_currentPageId.clear();
		m_currentContent = "# Help unavailable\n\nThe bundled help pages were not found.\n";
		m_renderer.setText(m_currentContent);
		return;
	}
	m_currentPageId = match->id;
	m_currentContent = readFileToString(m_helpRoot / match->path);
	if (m_currentContent.empty())
		m_currentContent = "# " + match->title + "\n\n_(empty page)_\n";
	m_renderer.setText(m_currentContent);
	m_renderer.scrollToAnchor({});
}

auto HelpPanel::parseLink(const std::string& iHref) -> LinkTarget {
	LinkTarget target;
	std::string page = iHref;
	if (const auto hash = page.find('#'); hash != std::string::npos) {
		target.anchor = page.substr(hash + 1);
		page.resize(hash);
	}
	if (const auto slash = page.find_last_of("/\\"); slash != std::string::npos)
		page = page.substr(slash + 1);
	if (page.ends_with(".md"))
		page.resize(page.size() - 3);
	else if (page.ends_with(".html"))
		page.resize(page.size() - 5);
	target.pageId = std::move(page);
	return target;
}

void HelpPanel::navigateTo(const std::string& iId, const std::string& iAnchor) {
	const bool samePage = iId.empty() || toLower(iId) == toLower(m_currentPageId);
	if (samePage && iAnchor.empty()) {
		m_renderer.scrollToAnchor({});
		return;
	}
	if (!m_currentPageId.empty())
		m_backStack.push_back({.pageId = m_currentPageId, .scrollY = m_renderer.getScrollY()});
	m_forwardStack.clear();
	if (!samePage)
		loadPage(iId);
	m_renderer.scrollToAnchor(iAnchor);
}

void HelpPanel::goBack() {
	if (m_backStack.empty())
		return;
	if (!m_currentPageId.empty())
		m_forwardStack.push_back({.pageId = m_currentPageId, .scrollY = m_renderer.getScrollY()});
	const auto entry = m_backStack.back();
	m_backStack.pop_back();
	if (entry.pageId != m_currentPageId)
		loadPage(entry.pageId);
	m_renderer.scrollTo(entry.scrollY);
}

void HelpPanel::goForward() {
	if (m_forwardStack.empty())
		return;
	if (!m_currentPageId.empty())
		m_backStack.push_back({.pageId = m_currentPageId, .scrollY = m_renderer.getScrollY()});
	const auto entry = m_forwardStack.back();
	m_forwardStack.pop_back();
	if (entry.pageId != m_currentPageId)
		loadPage(entry.pageId);
	m_renderer.scrollTo(entry.scrollY);
}

void HelpPanel::onLinkClicked(const std::string& iHref) {
	const auto [pageId, anchor] = parseLink(iHref);
	if (pageId.empty() && anchor.empty())
		return;
	navigateTo(pageId, anchor);
}

void HelpPanel::renderNavigationBar() {
	const auto& theme = gui::UiLayer::getTheme();
	const auto navButton = [&theme](const char* iGlyph, const char* iId, const bool iEnabled,
									const char* iTooltip) -> bool {
		ImGui::BeginDisabled(!iEnabled);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{8.0f, 4.0f});
		ImGui::PushStyleColor(ImGuiCol_Button, gui::vec(theme.surface3));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gui::vec(gui::Theme::withAlpha(theme.accent, 0.35f)));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, gui::vec(gui::Theme::withAlpha(theme.accent, 0.55f)));
		const bool clicked = ImGui::Button(std::format("{}##{}", iGlyph, iId).c_str());
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayShort))
			ImGui::SetTooltip("%s", iTooltip);
		return clicked && iEnabled;
	};
	if (navButton(gui::icon::arrowLeft, "help_back", !m_backStack.empty(), "Back (mouse back button, Alt+Left)"))
		goBack();
	ImGui::SameLine(0.0f, 4.0f);
	if (navButton(gui::icon::arrowRight, "help_forward", !m_forwardStack.empty(),
				  "Forward (mouse forward button, Alt+Right)"))
		goForward();
	ImGui::SameLine(0.0f, 4.0f);
	if (navButton(gui::icon::house, "help_home", !m_pages.empty(), "Start page"))
		navigateTo(m_pages.front().id);
	ImGui::SameLine(0.0f, 12.0f);
	const auto current = std::ranges::find(m_pages, m_currentPageId, &HelpPage::id);
	ImGui::AlignTextToFramePadding();
	if (current == m_pages.end()) {
		ImGui::TextDisabled("No page");
	} else {
		ImGui::TextDisabled("%s", current->group.c_str());
		ImGui::SameLine(0.0f, 6.0f);
		ImGui::TextDisabled("%s", gui::icon::chevronRight);
		ImGui::SameLine(0.0f, 6.0f);
		ImGui::TextUnformatted(current->title.c_str());
	}
}

void HelpPanel::onImGuiRender(const core::Timestep& iTimeStep) {
	if (!m_visible)
		return;
	if (m_pages.empty())
		loadIndex();

	// Always feed the renderer so the debounce timer ticks even when inactive.
	m_renderer.update(iTimeStep, m_currentContent);

	ImGui::SetNextWindowSize({720, 540}, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(panelWindow::title(gui::icon::circleHelp, "Help", panelWindow::help).c_str(), &m_visible)) {
		ImGui::End();
		return;
	}
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) ||
		ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
		const bool alt = ImGui::GetIO().KeyAlt;
		if (ImGui::IsMouseClicked(3) || (alt && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)))
			goBack();
		else if (ImGui::IsMouseClicked(4) || (alt && ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)))
			goForward();
	}

	// ---- Left side: search + page tree ---------------------------------
	const auto avail = ImGui::GetContentRegionAvail();
	constexpr float kSplitterW = 6.0f;
	constexpr float kMinSideW = 120.0f;
	const float minRatio = avail.x > 0.0f ? std::clamp(kMinSideW / avail.x, 0.05f, 0.45f) : 0.05f;
	const float maxRatio = 1.0f - minRatio;
	m_splitRatio = std::clamp(m_splitRatio, minRatio, maxRatio);
	const float leftW = std::max(kMinSideW, avail.x * m_splitRatio);
	const float rightW = std::max(kMinSideW, avail.x - leftW - kSplitterW);

	ImGui::BeginChild("##help_tree", ImVec2{leftW, avail.y}, ImGuiChildFlags_Borders);
	{

		ImGui::SetNextItemWidth(-FLT_MIN);
		char buf[128];

		std::snprintf(buf, sizeof(buf), "%s", m_search.c_str());
		if (ImGui::InputTextWithHint("##help_search", "Filter…", buf, sizeof(buf)))
			m_search = buf;

		ImGui::Separator();

		const auto needle = toLower(m_search);
		std::string currentGroup;
		bool groupOpen = false;
		for (const auto& page: m_pages) {
			if (!needle.empty() && toLower(page.title).find(needle) == std::string::npos)
				continue;
			if (page.group != currentGroup) {
				if (!currentGroup.empty() && groupOpen)
					gui::widgets::endTreeRow();
				currentGroup = page.group;
				groupOpen = gui::widgets::treeRow(currentGroup.c_str(), {.label = currentGroup.c_str(),
																		 .glyph = gui::icon::bookOpen,
																		 .hasChildren = true,
																		 .defaultOpen = true})
									.open;
			}
			if (!groupOpen)
				continue;
			if (gui::widgets::treeRow(
						page.id.c_str(),
						{.label = page.title.c_str(), .glyph = gui::icon::file, .selected = page.id == m_currentPageId})
						.clicked)
				navigateTo(page.id);
		}
		if (groupOpen)
			gui::widgets::endTreeRow();
	}

	ImGui::EndChild();

	ImGui::SameLine(0.0f, 0.0f);

	// ---- Draggable splitter --------------------------------------------
	ImGui::Button("##help_split", ImVec2{kSplitterW, avail.y});
	if (ImGui::IsItemActive()) {
		const float dx = ImGui::GetIO().MouseDelta.x;
		if (avail.x > 0.0f)
			m_splitRatio = std::clamp(m_splitRatio + dx / avail.x, minRatio, maxRatio);
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())

		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

	ImGui::SameLine(0.0f, 0.0f);

	// ---- Right side: navigation bar + Markdown content -----------------
	ImGui::BeginChild("##help_content", ImVec2{rightW, avail.y});
	{
		renderNavigationBar();
		ImGui::Separator();

		const auto contentAvail = ImGui::GetContentRegionAvail();
		m_renderer.render(contentAvail);
	}

	ImGui::EndChild();

	ImGui::End();
}

}// namespace owl::nest::panel
