/**
 * @file HelpPanel.cpp
 * @author Silmaen
 * @date 28/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HelpPanel.h"

#include <core/Application.h>
#include <core/Log.h>
#include <core/Macros.h>

#include <yaml-cpp/yaml.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace owl::nest::panel {

namespace {
constexpr const char* kDefaultPageId = "README";
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
		loadPage(kDefaultPageId);
	m_visible = true;
}

void HelpPanel::open(const std::string& iPageId) {
	if (m_pages.empty())
		loadIndex();
	loadPage(iPageId);
	m_visible = true;
}

auto HelpPanel::resolveHelpRoot() -> std::filesystem::path {
	if (!core::Application::instanced())
		return {};
	for (const auto& dir: core::Application::get().getAssetDirectories()) {
		if (auto candidate = dir.assetsPath / "help"; exists(candidate / kIndexFileName))
			return candidate;
	}
	return {};
}

void HelpPanel::loadIndex() {
	m_pages.clear();
	m_helpRoot = resolveHelpRoot();
	if (m_helpRoot.empty()) {
		OWL_CORE_WARN("HelpPanel: could not locate engine_assets/help/index.yml.")
		return;
	}
	// The renderer needs the help root to resolve relative image paths (`![](images/foo.svg)`).
	m_renderer.setBaseDirectory(m_helpRoot);
	const auto indexFile = m_helpRoot / kIndexFileName;
	YAML::Node root;
	try {
		root = YAML::LoadFile(indexFile.string());
	} catch (const std::exception& ex) {
		OWL_CORE_ERROR("HelpPanel: failed to parse '{}' ({}).", indexFile.string(), ex.what())
		return;
	}
	const auto pagesNode = root["Help"]["Pages"];
	if (!pagesNode || !pagesNode.IsSequence()) {
		OWL_CORE_WARN("HelpPanel: '{}' has no Help/Pages sequence.", indexFile.string())
		return;
	}
	m_pages.reserve(pagesNode.size());
	for (const auto& entry: pagesNode) {
		PageEntry page;
		page.id = entry["id"].as<std::string>("");
		page.title = entry["title"].as<std::string>(page.id);
		page.category = entry["category"].as<std::string>("guides");
		page.path = entry["path"].as<std::string>("");
		if (!page.id.empty() && !page.path.empty())
			m_pages.push_back(std::move(page));
	}
	std::ranges::sort(m_pages, [](const PageEntry& a, const PageEntry& b) -> bool {
		if (a.category != b.category)
			return a.category < b.category;
		return a.title < b.title;
	});
}

void HelpPanel::loadPage(const std::string& iId) {
	const auto findEntry = [this](const std::string& iLookup) -> const PageEntry* {
		for (const auto& p: m_pages)
			if (p.id == iLookup)
				return &p;
		return nullptr;
	};
	// Resolution chain: requested id → default landing page → first available page.
	const PageEntry* match = findEntry(iId);
	if (match == nullptr && iId != kDefaultPageId)
		match = findEntry(kDefaultPageId);
	if (match == nullptr && !m_pages.empty())
		match = &m_pages.front();
	if (match == nullptr) {
		m_currentPageId.clear();
		m_currentContent = "# Help unavailable\n\nThe bundled help pages were not found.\n";
		return;
	}
	m_currentPageId = match->id;
	m_currentContent = readFileToString(m_helpRoot / match->path);
	if (m_currentContent.empty())
		m_currentContent = "# " + match->title + "\n\n_(empty page)_\n";
}

void HelpPanel::navigateTo(const std::string& iId) {
	if (iId == m_currentPageId)
		return;
	if (!m_currentPageId.empty())
		m_backStack.push_back(m_currentPageId);
	m_forwardStack.clear();
	loadPage(iId);
}

void HelpPanel::onLinkClicked(const std::string& iHref) {
	// External URLs (http(s)://, mailto:) are handled directly by MarkdownPreview, so we only see
	// internal page references here.
	std::string target = iHref;
	// Strip Doxygen anchor fragment if any.
	if (const auto hash = target.find('#'); hash != std::string::npos)
		target.resize(hash);
	// Strip directory prefixes.
	if (const auto slash = target.find_last_of("/\\"); slash != std::string::npos)
		target = target.substr(slash + 1);
	// Strip .md / .html extensions.
	if (target.ends_with(".md"))
		target.resize(target.size() - 3);
	else if (target.ends_with(".html"))
		target.resize(target.size() - 5);
	if (target.empty())
		return;
	navigateTo(target);
}

void HelpPanel::onImGuiRender(const core::Timestep& iTimeStep) {
	if (!m_visible)
		return;
	if (m_pages.empty())

		loadIndex();

	// Always feed the renderer so the debounce timer ticks even when inactive.
	m_renderer.update(iTimeStep, m_currentContent);

	ImGui::SetNextWindowSize({720, 540}, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Help##help_panel", &m_visible)) {
		ImGui::End();
		return;
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
		std::string currentCategory;
		bool categoryOpen = false;
		for (const auto& page: m_pages) {
			if (!needle.empty() && toLower(page.title).find(needle) == std::string::npos)
				continue;
			if (page.category != currentCategory) {
				if (!currentCategory.empty() && categoryOpen)

					ImGui::TreePop();
				currentCategory = page.category;
				categoryOpen = ImGui::TreeNodeEx(currentCategory.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			}
			if (!categoryOpen)
				continue;
			const bool selected = page.id == m_currentPageId;
			if (ImGui::Selectable(page.title.c_str(), selected))

				navigateTo(page.id);
		}
		if (categoryOpen)

			ImGui::TreePop();
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
		const bool canBack = !m_backStack.empty();
		const bool canForward = !m_forwardStack.empty();

		ImGui::BeginDisabled(!canBack);
		if (ImGui::Button("<##help_back") && canBack) {
			if (!m_currentPageId.empty())
				m_forwardStack.push_back(m_currentPageId);
			const auto prev = m_backStack.back();
			m_backStack.pop_back();

			loadPage(prev);
		}

		ImGui::EndDisabled();

		ImGui::SameLine();

		ImGui::BeginDisabled(!canForward);
		if (ImGui::Button(">##help_forward") && canForward) {
			if (!m_currentPageId.empty())
				m_backStack.push_back(m_currentPageId);
			const auto next = m_forwardStack.back();
			m_forwardStack.pop_back();

			loadPage(next);
		}

		ImGui::EndDisabled();

		ImGui::SameLine();
		if (m_currentPageId.empty())

			ImGui::TextDisabled("No page");
		else

			ImGui::TextDisabled("%s", m_currentPageId.c_str());

		ImGui::Separator();

		const auto contentAvail = ImGui::GetContentRegionAvail();
		m_renderer.render(contentAvail);
	}

	ImGui::EndChild();

	ImGui::End();
}

}// namespace owl::nest::panel
