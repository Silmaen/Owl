/**
 * @file ContentBrowser.cpp
 * @author Silmaen
 * @date 10/01/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ContentBrowser.h"

#include "../AssetKind.h"
#include "../DockLayout.h"
#include "../document/PcgGraphDocument.h"
#include "../document/VisualScriptDocument.h"

#include <data/assets/AssetMeta.h>
#include <data/localisation/Localisation.h>
#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/SearchField.h>
#include <gui/widgets/Tooltip.h>
#include <gui/widgets/TreeRow.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <input/ActionMap.h>
#include <platform/AtomicFile.h>
#include <platform/FileDialog.h>
#include <scene/pcg/PcgGraph.h>
#include <script/visual/VisualScript.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::nest::panel {

namespace {
// Family of an asset: the colour of its tile bar and the name shown under it.
struct KindInfo {
	math::vec4 color;
	const char* label;
};

auto rgb(const uint32_t iHex) -> math::vec4 {
	return {static_cast<float>((iHex >> 16U) & 0xFFU) / 255.f, static_cast<float>((iHex >> 8U) & 0xFFU) / 255.f,
			static_cast<float>(iHex & 0xFFU) / 255.f, 1.f};
}

auto describeKind(const std::filesystem::path& iPath, const bool iDirectory) -> KindInfo {
	if (iDirectory)
		return {rgb(0xc09a55), "Folder"};
	if (iPath.filename() == input::ActionMap::fileName)
		return {rgb(0x3fa8d8), "Input actions"};
	static const std::unordered_map<std::string, KindInfo> kinds = {
			{".owl", {rgb(0xd8a520), "Scene"}},          {".owlprefab", {rgb(0x4f8fe0), "Prefab"}},
			{".owlhud", {rgb(0x2fa8b8), "HUD"}},         {".png", {rgb(0xe0534a), "Texture"}},
			{".jpg", {rgb(0xe0534a), "Texture"}},        {".jpeg", {rgb(0xe0534a), "Texture"}},
			{".svg", {rgb(0xe0534a), "Vector image"}},   {".owltileset", {rgb(0xe48a3a), "Tileset"}},
			{".owltilemap", {rgb(0xd65fb8), "Tilemap"}}, {".wav", {rgb(0x3faa72), "Sound"}},
			{".ogg", {rgb(0x3faa72), "Sound"}},          {".flac", {rgb(0x3faa72), "Sound"}},
			{".mp3", {rgb(0x3faa72), "Sound"}},          {".lua", {rgb(0x2fb5a0), "Lua script"}},
			{".py", {rgb(0x2fb5a0), "Python"}},          {".c", {rgb(0x5b8fd6), "C source"}},
			{".cpp", {rgb(0x5b8fd6), "C++ source"}},     {".h", {rgb(0x5b8fd6), "Header"}},
			{".hpp", {rgb(0x5b8fd6), "Header"}},         {".glsl", {rgb(0xa274e8), "Shader"}},
			{".slang", {rgb(0xa274e8), "Shader"}},       {".ttf", {rgb(0x9a7ae0), "Font"}},
			{".otf", {rgb(0x9a7ae0), "Font"}},           {".owlanim", {rgb(0xef6c8f), "Animation"}},
			{".owlflow", {rgb(0x5aa2f0), "Node graph"}}, {".owlvs", {rgb(0xe8c547), "Visual script"}},
			{".owlpcg", {rgb(0x7fd18b), "PCG graph"}},   {".obj", {rgb(0x7fb83e), "3D model"}},
			{".fbx", {rgb(0x7fb83e), "3D model"}},       {".gltf", {rgb(0x7fb83e), "3D model"}},
			{".glb", {rgb(0x7fb83e), "3D model"}},       {".yml", {rgb(0x8a929e), "Settings"}},
			{".yaml", {rgb(0x8a929e), "Settings"}},      {".json", {rgb(0x8a929e), "Data"}},
			{".md", {rgb(0x8a929e), "Document"}},        {".owlfont", {rgb(0x9a7ae0), "Font settings"}},
			{".owlmat", {rgb(0xc77dff), "Material"}},    {".owlsound", {rgb(0x3faa72), "Sound cue"}},
			{".owltable", {rgb(0x4fb3d9), "Data table"}},  {".owldlg", {rgb(0xd98f4f), "Dialogue"}}};
	if (const auto it = kinds.find(iPath.extension().string()); it != kinds.end())
		return it->second;
	return {rgb(0x8a929e), "File"};
}

auto getFileIcon(const std::filesystem::path& iPath) -> std::optional<gui::IconBank::IconInfo> {
	const auto& iconBank = gui::IconBank::instance();
	if (is_directory(iPath))
		return iconBank.getIcon("folder_icon");
	if (iPath.extension() == ".glsl" || iPath.extension() == ".frag" || iPath.extension() == ".vert")
		return iconBank.getIcon("glsl_icon");
	if (iPath.extension() == ".jpg")
		return iconBank.getIcon("jpg_icon");
	if (iPath.extension() == ".json")
		return iconBank.getIcon("json_icon");
	if (iPath.extension() == ".owl")
		return iconBank.getIcon("owl_icon");
	if (iPath.extension() == ".owlprefab")
		return iconBank.getIcon("prefab_icon");
	if (iPath.extension() == ".owlhud")
		return iconBank.getIcon("owlhud_icon");
	if (iPath.extension() == ".owlanim")
		return iconBank.getIcon("owlanim_icon");
	if (iPath.extension() == ".owltileset")
		return iconBank.getIcon("owltileset_icon");
	if (iPath.extension() == ".owltilemap")
		return iconBank.getIcon("owltilemap_icon");
	if (iPath.extension() == ".png")
		return iconBank.getIcon("png_icon");
	if (iPath.extension() == ".svg")
		return iconBank.getIcon("svg_icon");
	if (iPath.extension() == ".ttf" || iPath.extension() == ".otf" || iPath.extension() == ".owlfont")
		return iconBank.getIcon("ttf_icon");
	if (iPath.extension() == ".yml" || iPath.extension() == ".yaml")
		return iconBank.getIcon("yml_icon");
	if (iPath.extension() == ".lua")
		return iconBank.getIcon("lua_icon");
	if (iPath.extension() == ".owlvs")
		return iconBank.getIcon("owlvs_icon");
	if (iPath.extension() == ".wav")
		return iconBank.getIcon("wav_icon");
	if (iPath.extension() == ".mp3")
		return iconBank.getIcon("mp3_icon");
	if (iPath.extension() == ".ogg")
		return iconBank.getIcon("ogg_icon");
	if (iPath.extension() == ".flac")
		return iconBank.getIcon("flac_icon");
	if (iPath.extension() == ".obj")
		return iconBank.getIcon("obj_icon");
	if (iPath.extension() == ".gltf")
		return iconBank.getIcon("gltf_icon");
	if (iPath.extension() == ".glb")
		return iconBank.getIcon("glb_icon");
	if (iPath.extension() == ".fbx")
		return iconBank.getIcon("fbx_icon");
	if (iPath.extension() == ".py")
		return iconBank.getIcon("py_icon");
	if (iPath.extension() == ".cpp" || iPath.extension() == ".cxx" || iPath.extension() == ".cc")
		return iconBank.getIcon("cpp_icon");
	if (iPath.extension() == ".h" || iPath.extension() == ".hpp" || iPath.extension() == ".hxx")
		return iconBank.getIcon("h_icon");
	if (iPath.extension() == ".c")
		return iconBank.getIcon("c_icon");
	if (iPath.extension() == ".md")
		return iconBank.getIcon("md_icon");
	return iconBank.getIcon("text_icon");
}

auto upperCase(std::string iText) -> std::string {
	std::ranges::transform(iText, iText.begin(), [](const char iChar) -> char {
		return static_cast<char>(std::toupper(static_cast<unsigned char>(iChar)));
	});
	return iText;
}

// What a tile shows and where.
struct TileView {
	std::filesystem::path path;
	std::string name;
	bool directory = false;
	const AssetThumbnails::Preview* preview = nullptr;
	ImVec2 min;
	ImVec2 max;
	float width = 0.f;
	float height = 0.f;
	float thumbSize = 0.f;
	bool list = false;
	bool hovered = false;
	bool selected = false;
};

void drawTile(ImDrawList* iDraw, const TileView& iTile) {
	const auto& theme = gui::UiLayer::getTheme();
	const auto toU32 = [](const math::vec4& iColor) -> ImU32 { return ImGui::GetColorU32(gui::vec(iColor)); };
	const auto kind = describeKind(iTile.path, iTile.directory);
	math::vec4 background = iTile.list ? gui::Theme::withAlpha(theme.surface0, 0.f) : theme.surface2;
	if (iTile.hovered)
		background = theme.surface3;
	if (iTile.selected)
		background = theme.getSelection();
	iDraw->AddRectFilled(iTile.min, iTile.max, toU32(background), theme.frameRounding);
	if (iTile.selected && !iTile.list)
		iDraw->AddRect(iTile.min, iTile.max, toU32(theme.accent), theme.frameRounding);
	const auto icon = getFileIcon(iTile.path);
	const auto* preview = iTile.preview;
	const auto thumbnail = preview != nullptr ? preview->texture : nullptr;
	std::optional<ImTextureID> thumbTexture = gui::imTexture(thumbnail);
	ImVec2 renderUv0{0.f, 1.f};
	ImVec2 renderUv1{1.f, 0.f};
	if (preview != nullptr && preview->framebuffer) {
		thumbTexture = gui::imTexture(preview->framebuffer, 0);
		renderUv0 = gui::vec(preview->framebuffer->getLowerData());
		renderUv1 = gui::vec(preview->framebuffer->getUpperData());
	}
	const bool hasWaveform = preview != nullptr && !preview->waveform.empty();
	const bool hasMesh = preview != nullptr && !preview->mesh.empty();
	if (iTile.list) {
		const float iconSize = ImGui::GetFontSize();
		const float y = std::floor(iTile.min.y + (iTile.height - iconSize) * 0.5f);
		iDraw->AddRectFilled({iTile.min.x + 4.f, y + 1.f}, {iTile.min.x + 7.f, y + iconSize - 1.f}, toU32(kind.color),
							 1.f);
		if (thumbTexture.has_value())
			iDraw->AddImage(thumbTexture.value(), {iTile.min.x + 12.f, y},
							{iTile.min.x + 12.f + iconSize, y + iconSize}, renderUv0, renderUv1);
		else if (icon.has_value())
			iDraw->AddImage(static_cast<ImTextureID>(icon->textureId), {iTile.min.x + 12.f, y},
							{iTile.min.x + 12.f + iconSize, y + iconSize}, gui::vec(icon->uv0), gui::vec(icon->uv1));
		const float textY = std::floor(iTile.min.y + (iTile.height - ImGui::GetTextLineHeight()) * 0.5f);
		iDraw->AddText({iTile.min.x + 20.f + iconSize, textY}, toU32(theme.text), iTile.name.c_str());
		iDraw->AddText({iTile.min.x + std::max(iTile.width * 0.6f, 220.f), textY}, toU32(theme.textDisabled),
					   kind.label);
	} else {
		// Thumbnail area: the image fills the square; other kinds show their icon, centred.
		const ImVec2 thumbMin{iTile.min.x + 1.f, iTile.min.y + 1.f};
		const ImVec2 thumbMax{iTile.max.x - 1.f, iTile.min.y + iTile.thumbSize};
		iDraw->AddRectFilled(thumbMin, thumbMax,
							 toU32(gui::Theme::mix(theme.surface0, kind.color, thumbTexture.has_value() ? 0.f : 0.08f)),
							 theme.frameRounding, ImDrawFlags_RoundCornersTop);
		if (thumbTexture.has_value() && thumbnail) {
			const auto size = thumbnail->getSize();
			const float ratio = size.y() > 0 ? static_cast<float>(size.x()) / static_cast<float>(size.y()) : 1.f;
			// Cover the square: crop the long side so the image keeps its proportions.
			ImVec2 uv0{0.f, 1.f};
			ImVec2 uv1{1.f, 0.f};
			if (ratio > 1.f) {
				const float crop = (1.f - 1.f / ratio) * 0.5f;
				uv0.x = crop;
				uv1.x = 1.f - crop;
			} else if (ratio < 1.f) {
				const float crop = (1.f - ratio) * 0.5f;
				uv0.y = 1.f - crop;
				uv1.y = crop;
			}
			iDraw->AddImageRounded(thumbTexture.value(), thumbMin, thumbMax, uv0, uv1, IM_COL32_WHITE,
								   theme.frameRounding, ImDrawFlags_RoundCornersTop);
		} else if (thumbTexture.has_value()) {
			// A rendered scene or prefab: square, drawn whole.
			iDraw->AddImageRounded(thumbTexture.value(), thumbMin, thumbMax, renderUv0, renderUv1, IM_COL32_WHITE,
								   theme.frameRounding, ImDrawFlags_RoundCornersTop);
		} else if (hasMesh) {
			// Back to front, each face in the family colour scaled by its light.
			const float side = std::min(thumbMax.x - thumbMin.x, thumbMax.y - thumbMin.y);
			const ImVec2 origin{thumbMin.x + (thumbMax.x - thumbMin.x - side) * 0.5f,
								thumbMin.y + (thumbMax.y - thumbMin.y - side) * 0.5f};
			const auto at = [&](const math::vec2& iCorner) -> ImVec2 {
				return {origin.x + iCorner.x() * side, origin.y + iCorner.y() * side};
			};
			for (const auto& triangle: preview->mesh) {
				const math::vec4 color{kind.color.x() * triangle.shade, kind.color.y() * triangle.shade,
									   kind.color.z() * triangle.shade, 1.f};
				iDraw->AddTriangleFilled(at(triangle.corners[0]), at(triangle.corners[1]), at(triangle.corners[2]),
										 toU32(color));
			}
		} else if (hasWaveform) {
			// One bar per slice, mirrored around the middle line, in the colour of the family.
			const auto& peaks = preview->waveform;
			const float left = thumbMin.x + 8.f;
			const float span = thumbMax.x - 8.f - left;
			const float middle = (thumbMin.y + thumbMax.y) * 0.5f;
			const float halfHeight = (thumbMax.y - thumbMin.y) * 0.32f;
			const float slot = span / static_cast<float>(peaks.size());
			for (size_t i = 0; i < peaks.size(); ++i) {
				const float x = std::floor(left + slot * static_cast<float>(i));
				const float h = std::max(1.f, std::round(peaks[i] * halfHeight));
				iDraw->AddRectFilled({x, middle - h}, {x + std::max(1.f, std::floor(slot * 0.6f)), middle + h},
									 toU32(kind.color), 1.f);
			}
		} else if (icon.has_value()) {
			const float iconSize = std::floor(iTile.thumbSize * 0.5f);
			const ImVec2 iconMin{std::floor(iTile.min.x + (iTile.width - iconSize) * 0.5f),
								 std::floor(iTile.min.y + (iTile.thumbSize - iconSize) * 0.5f)};
			iDraw->AddImage(static_cast<ImTextureID>(icon->textureId), iconMin,
							{iconMin.x + iconSize, iconMin.y + iconSize}, gui::vec(icon->uv0), gui::vec(icon->uv1));
		}
		iDraw->AddRectFilled({iTile.min.x, iTile.min.y + iTile.thumbSize},
							 {iTile.max.x, iTile.min.y + iTile.thumbSize + 2.f}, toU32(kind.color));
		// The badge only fits short extensions; the kind under the name says the rest.
		if (const auto extension = upperCase(iTile.path.extension().string());
			!iTile.directory && extension.size() > 1 && extension.size() <= 5) {
			gui::UiLayer::pushTextStyle(gui::TextStyle::Caption);
			const char* text = extension.c_str() + 1;
			const ImVec2 textSize = ImGui::CalcTextSize(text);
			const ImVec2 badgeMax{iTile.max.x - 4.f, iTile.min.y + 4.f + textSize.y + 2.f};
			const ImVec2 badgeMin{badgeMax.x - textSize.x - 8.f, iTile.min.y + 4.f};
			iDraw->AddRectFilled(badgeMin, badgeMax, toU32(gui::Theme::withAlpha(theme.surface0, 0.85f)), 3.f);
			iDraw->AddText({badgeMin.x + 4.f, badgeMin.y + 1.f}, toU32(kind.color), text);
			gui::UiLayer::popTextStyle();
		}
		const ImVec4 clip{iTile.min.x + 6.f, iTile.min.y + iTile.thumbSize + 4.f, iTile.max.x - 4.f,
						  iTile.max.y - ImGui::GetFontSize() * 0.9f - 2.f};
		iDraw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.92f, {clip.x, clip.y}, toU32(theme.text),
					   iTile.name.c_str(), nullptr, iTile.width - 10.f, &clip);
		gui::UiLayer::pushTextStyle(gui::TextStyle::Caption);
		iDraw->AddText({iTile.min.x + 6.f, iTile.max.y - ImGui::GetFontSize() - 4.f}, toU32(theme.textDisabled),
					   upperCase(kind.label).c_str());
		gui::UiLayer::popTextStyle();
	}
}

}// namespace

ContentBrowser::ContentBrowser() = default;

void ContentBrowser::detach() { m_previews.clear(); }

void ContentBrowser::reveal(const std::filesystem::path& iPath) {
	const auto file = iPath.lexically_normal();
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		const auto root = assetsPath.lexically_normal();
		if (const auto rel = file.lexically_relative(root); rel.empty() || rel.begin()->string() == "..")
			continue;
		m_currentRootPath = assetsPath;
		m_currentPath = file.parent_path();
		m_selectedPath = iPath;
		m_searchFilter.clear();
		m_kindFilter.clear();
		return;
	}
	OWL_WARN("ContentBrowser: {} is in no asset folder.", iPath.string())
}

void ContentBrowser::attach() {
	m_currentRootPath = app::Application::get().getAssetDirectories().front().assetsPath;
	m_currentPath = m_currentRootPath;
	m_cachedEntries.clear();
	m_cachedPath.clear();
	requestScan(m_currentPath);
}

void ContentBrowser::requestScan(const std::filesystem::path& iPath) {
	// If already scanning this exact path, skip.
	if (m_scanInProgress.load() && m_pendingScanPath == iPath)
		return;
	m_pendingScanPath = iPath;
	m_scanInProgress.store(true);
	m_pendingEntries = mkShared<std::vector<std::filesystem::directory_entry>>();

	auto buffer = m_pendingEntries;
	auto* flag = &m_scanInProgress;
	app::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[buffer, iPath]() -> void {
				if (!exists(iPath) || !is_directory(iPath))
					return;
				std::error_code ec;
				for (const auto& entry: std::filesystem::directory_iterator(iPath, ec))
					if (entry.path().extension() != data::assets::g_MetaExtension)
						buffer->push_back(entry);
				std::ranges::sort(*buffer, [](const auto& iA, const auto& iB) -> auto {
					if (iA.is_directory() != iB.is_directory())
						return iA.is_directory();
					return iA.path().filename().string() < iB.path().filename().string();
				});
			},
			[flag]() -> void { flag->store(false); }));
}

void ContentBrowser::onImGuiRender() {
	ImGui::Begin(panelWindow::title(gui::icon::folderOpen, "Content Browser", panelWindow::contentBrowser).c_str());

	// Swap in the latest background scan when it completes.
	if (!m_scanInProgress.load() && m_pendingEntries) {
		m_cachedEntries = std::move(*m_pendingEntries);
		m_cachedPath = m_pendingScanPath;
		m_pendingEntries.reset();
		m_folderCache.clear();
	}

	// Navigation or external trigger may require a rescan.
	if (m_currentPath != m_cachedPath && m_currentPath != m_pendingScanPath)
		requestScan(m_currentPath);
	else if (m_rescanRequested) {
		m_rescanRequested = false;
		requestScan(m_currentPath);
	}

	const auto& theme = gui::UiLayer::getTheme();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 4.f});
	ImGui::BeginChild("##cbTree", {ImGui::GetFontSize() * 13.f, 0.f}, ImGuiChildFlags_ResizeX);
	renderFolderTree();
	ImGui::EndChild();
	ImGui::PopStyleVar();
	{
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		ImGui::GetWindowDrawList()->AddLine({max.x, min.y}, {max.x, max.y},
											ImGui::GetColorU32(gui::vec(theme.lineSoft)));
	}
	ImGui::SameLine();
	ImGui::BeginChild("##cbMain", {0.f, 0.f});
	renderTopBand();
	renderContent();
	// In the child: the popup is opened from its tiles, and a popup id belongs to the window that opens it.
	renderContextMenu();
	ImGui::EndChild();

	ImGui::End();
}

auto ContentBrowser::getSubFolders(const std::filesystem::path& iPath) -> const std::vector<std::filesystem::path>& {
	const auto key = iPath.string();
	if (const auto it = m_folderCache.find(key); it != m_folderCache.end())
		return it->second;
	std::vector<std::filesystem::path> folders;
	std::error_code error;
	for (const auto& entry: std::filesystem::directory_iterator(iPath, error)) {
		if (entry.is_directory(error) && !entry.path().filename().string().starts_with('.'))
			folders.push_back(entry.path());
	}
	std::ranges::sort(folders);
	return m_folderCache.emplace(key, std::move(folders)).first->second;
}

void ContentBrowser::renderFolderTree() {
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		ImGui::PushID(assetsPath.string().c_str());
		renderFolderRow(assetsPath, assetsPath, title, true);
		ImGui::PopID();
	}
	m_treeSyncedPath = m_currentPath;
}

// NOLINTNEXTLINE(misc-no-recursion)
void ContentBrowser::renderFolderRow(const std::filesystem::path& iPath, const std::filesystem::path& iRoot,
									 const std::string& iLabel, const bool iIsRoot) {
	const auto& theme = gui::UiLayer::getTheme();
	const auto& children = getSubFolders(iPath);
	const bool selected = iPath == m_currentPath;
	// Unfold the ancestors of the current folder once each time it changes (breadcrumb, double-click).
	if (m_treeSyncedPath != m_currentPath) {
		const auto relative = m_currentPath.lexically_relative(iPath);
		if (!relative.empty() && !relative.string().starts_with("..") && relative != ".")
			ImGui::SetNextItemOpen(true);
	}
	const auto row = gui::widgets::treeRow(iPath.filename().string().c_str(),
										   {.label = iLabel.c_str(),
											.glyph = selected ? gui::icon::folderOpen : gui::icon::folder,
											.glyphColor = iIsRoot ? theme.accent : theme.textMuted,
											.selected = selected,
											.hasChildren = !children.empty(),
											.defaultOpen = iIsRoot && iRoot == m_currentRootPath});
	if (row.clicked && !selected) {
		m_currentRootPath = iRoot;
		m_currentPath = iPath;
		m_searchFilter.clear();
	}
	if (!row.open)
		return;
	for (const auto& child: children) renderFolderRow(child, iRoot, child.filename().string(), false);
	gui::widgets::endTreeRow();
}

void ContentBrowser::renderTopBand() {
	const auto& theme = gui::UiLayer::getTheme();
	if (gui::widgets::iconButton("back", {.glyph = gui::icon::arrowLeft,
										  .tooltip = "Parent folder",
										  .disabled = m_currentPath == m_currentRootPath}))
		m_currentPath = m_currentPath.parent_path();
	ImGui::SameLine();
	// Breadcrumb: the root, then every folder down to the current one, each clickable.
	std::vector<std::filesystem::path> crumbs;
	for (auto path = m_currentPath; !path.empty(); path = path.parent_path()) {
		crumbs.push_back(path);
		if (path == m_currentRootPath || path == path.parent_path())
			break;
	}
	std::ranges::reverse(crumbs);
	std::string rootTitle = m_currentRootPath.filename().string();
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (assetsPath == m_currentRootPath)
			rootTitle = title;
	}
	for (size_t i = 0; i < crumbs.size(); ++i) {
		const bool last = i + 1 == crumbs.size();
		const auto name = i == 0 ? rootTitle : crumbs[i].filename().string();
		ImGui::AlignTextToFramePadding();
		if (last) {
			gui::UiLayer::pushTextStyle(gui::TextStyle::Strong);
			ImGui::TextUnformatted(name.c_str());
			gui::UiLayer::popTextStyle();
		} else {
			ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(theme.textMuted));
			if (ImGui::Selectable(std::format("{}##crumb{}", name, i).c_str(), false, ImGuiSelectableFlags_None,
								  ImGui::CalcTextSize(name.c_str())))
				m_currentPath = crumbs[i];
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::TextColored(gui::vec(theme.textDisabled), "%s", gui::icon::chevronRight);
		}
		ImGui::SameLine();
	}
	// Search field, three buttons and the size slider, with the gaps between them.
	const float right = ImGui::GetFontSize() * 14.5f + ImGui::GetFrameHeight() * 3.f +
						ImGui::GetStyle().ItemSpacing.x * 5.f + ImGui::GetStyle().WindowPadding.x;
	ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - right));
	ImGui::BeginChild("##cbSearchBox", {ImGui::GetFontSize() * 10.f, ImGui::GetFrameHeight()}, ImGuiChildFlags_None,
					  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
	std::ignore = gui::widgets::searchField("##search", m_searchFilter, "Search this folder");
	ImGui::EndChild();
	ImGui::SameLine();
	if (gui::widgets::iconButton("grid",
								 {.glyph = gui::icon::layoutGrid, .tooltip = "Grid view", .selected = !m_listView}))
		m_listView = false;
	ImGui::SameLine();
	if (gui::widgets::iconButton("list", {.glyph = gui::icon::list, .tooltip = "List view", .selected = m_listView}))
		m_listView = true;
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.5f);
	ImGui::BeginDisabled(m_listView);
	ImGui::SliderFloat("##tileSize", &m_tileSize, 56.f, 160.f, "");
	ImGui::EndDisabled();
	gui::widgets::itemTooltip("Thumbnail size");
	ImGui::SameLine();
	if (gui::widgets::iconButton("meshStyle", {.glyph = gui::icon::box, .tooltip = "Mesh thumbnails"}))
		ImGui::OpenPopup("MeshThumbnailStyle");
	if (ImGui::BeginPopup("MeshThumbnailStyle")) {
		using Style = AssetThumbnails::MeshStyle;
		const auto style = m_previews.getMeshStyle();
		gui::widgets::menuHeader("Mesh thumbnails");
		const auto entry = [&](const char* iLabel, const Style iStyle) -> void {
			if (gui::widgets::menuItem({.glyph = style == iStyle ? gui::icon::check : nullptr, .label = iLabel}))
				m_previews.setMeshStyle(iStyle);
		};
		entry("Textured", Style::Textured);
		entry("Shaded", Style::Shaded);
		entry("Flat", Style::Flat);
		ImGui::EndPopup();
	}
	renderKindChips();
	ImGui::Separator();
}

void ContentBrowser::renderKindChips() {
	std::vector<std::pair<std::string, size_t>> kinds;
	for (const auto& entry: m_cachedEntries) {
		const std::string label = describeKind(entry.path(), entry.is_directory()).label;
		if (auto it = std::ranges::find(kinds, label, &std::pair<std::string, size_t>::first); it != kinds.end())
			++it->second;
		else
			kinds.emplace_back(label, 1);
	}
	if (!m_kindFilter.empty() &&
		std::ranges::find(kinds, m_kindFilter, &std::pair<std::string, size_t>::first) == kinds.end())
		m_kindFilter.clear();
	if (kinds.size() < 2)
		return;
	std::ranges::sort(kinds);
	if (gui::widgets::chip(std::format("All  {}###kindAll", m_cachedEntries.size()).c_str(), m_kindFilter.empty()))
		m_kindFilter.clear();
	for (const auto& [label, count]: kinds) {
		ImGui::SameLine(0.f, 4.f);
		const bool selected = m_kindFilter == label;
		if (gui::widgets::chip(std::format("{}  {}###kind{}", label, count, label).c_str(), selected))
			m_kindFilter = selected ? std::string{} : label;
	}
}

void ContentBrowser::openEntry(const std::filesystem::directory_entry& iEntry) {
	const auto& path = iEntry.path();
	if (iEntry.is_directory()) {
		m_currentPath /= path.filename();
		m_searchFilter.clear();
	} else if ((path.extension() == ".owl" || path.extension() == ".owlhud" || path.extension() == ".owlprefab") &&
			   m_sceneOpenCallback) {
		m_sceneOpenCallback(path);
	} else if ((path.extension() == ".owlflow" || path.extension() == ".owlvs" || path.extension() == ".owlpcg") &&
			   m_nodeGraphOpenCallback) {
		m_nodeGraphOpenCallback(path);
	} else if (path.extension() == ".owlanim" && m_animationOpenCallback) {
		m_animationOpenCallback(path);
	} else if (path.extension() == ".owltilemap" && m_tilemapOpenCallback) {
		m_tilemapOpenCallback(path);
	} else if (path.extension() == ".owltileset" && m_tilesetOpenCallback) {
		m_tilesetOpenCallback(path);
	} else if ((path.extension() == ".ttf" || path.extension() == ".otf" || path.extension() == ".owlfont") &&
			   m_fontOpenCallback) {
		m_fontOpenCallback(path);
	} else if (classifyAsset(path) == AssetKind::Image && m_imageOpenCallback) {
		m_imageOpenCallback(path);
	} else if (path.filename() == input::ActionMap::fileName && m_actionMapOpenCallback) {
		m_actionMapOpenCallback(path);
	} else if (path.extension() == ".owlmat" && m_materialOpenCallback) {
		m_materialOpenCallback(path);
	} else if ((path.extension() == ".owlsound" || classifyAsset(path) == AssetKind::Audio) && m_soundOpenCallback) {
		m_soundOpenCallback(path);
	} else if (path.extension() == ".owltable" && m_dataTableOpenCallback) {
		m_dataTableOpenCallback(path);
	} else if (path.extension() == ".owldlg" && m_dialogueOpenCallback) {
		m_dialogueOpenCallback(path);
	} else if (path.filename() == data::localisation::Localisation::fileName && m_localisationOpenCallback) {
		m_localisationOpenCallback(path);
	} else if (path.extension() == ".owlmacro" && m_macroRunCallback) {
		m_macroRunCallback(path);
	} else if (classifyAsset(path) == AssetKind::Mesh && m_meshPlaceCallback) {
		m_meshPlaceCallback(path);
	} else if (m_codeOpenCallback && classifyAsset(path) == AssetKind::Code) {
		m_codeOpenCallback(path);
	}
}

void ContentBrowser::renderContent() {
	m_previews.beginFrame();
	ImGui::BeginChild("##cbItems", {0.f, 0.f}, ImGuiChildFlags_None);
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float labelHeight = ImGui::GetTextLineHeight() * 2.f + ImGui::GetFontSize() * 0.9f + 8.f;
	const float tileWidth = m_listView ? ImGui::GetContentRegionAvail().x : m_tileSize;
	const float tileHeight = m_listView ? ImGui::GetFrameHeight() : m_tileSize + labelHeight;
	const int columns = m_listView ? 1
								   : std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) /
																  (tileWidth + spacing)));
	bool openContextMenu = false;
	int column = 0;
	size_t shown = 0;
	auto* draw = ImGui::GetWindowDrawList();
	for (const auto& directoryEntry: m_cachedEntries) {
		const auto& path = directoryEntry.path();
		const auto relativePath = relative(path, m_currentRootPath);
		const std::string name = path.filename().string();
		if (!gui::widgets::matchesFilter(name, m_searchFilter))
			continue;
		if (!m_kindFilter.empty() && describeKind(path, directoryEntry.is_directory()).label != m_kindFilter)
			continue;
		++shown;
		if (column > 0)
			ImGui::SameLine(0.f, spacing);
		ImGui::PushID(name.c_str());
		const bool clicked = ImGui::InvisibleButton("##tile", {tileWidth, tileHeight});
		const bool hovered = ImGui::IsItemHovered();
		const auto min = ImGui::GetItemRectMin();
		const ImVec2 max{min.x + tileWidth, min.y + tileHeight};
		const bool selected = path == m_selectedPath;
		if (clicked)
			m_selectedPath = path;
		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			m_selectedPath = path;
			m_renaming = false;
			openContextMenu = true;
		}
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", relativePath.string().c_str(),
									  relativePath.string().size() + 1);
			gui::widgets::dragPreview(directoryEntry.is_directory() ? gui::icon::folder : gui::icon::file, name.c_str(),
									  describeKind(path, directoryEntry.is_directory()).label);
			ImGui::EndDragDropSource();
		}
		if (directoryEntry.is_directory() && ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				const std::filesystem::path sourcePath = m_currentRootPath / static_cast<const char*>(payload->Data);
				if (exists(sourcePath) && sourcePath != path)
					moveItem(sourcePath, path);
			}
			ImGui::EndDragDropTarget();
		}
		if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			openEntry(directoryEntry);

		drawTile(draw, {.path = path,
						.name = name,
						.directory = directoryEntry.is_directory(),
						.preview = directoryEntry.is_directory() ? nullptr : m_previews.get(path),
						.min = min,
						.max = max,
						.width = tileWidth,
						.height = tileHeight,
						.thumbSize = m_tileSize,
						.list = m_listView,
						.hovered = hovered,
						.selected = selected});
		const auto kind = describeKind(path, directoryEntry.is_directory());
		gui::widgets::itemTooltip(name.c_str(), nullptr, kind.label);
		ImGui::PopID();
		column = (column + 1) % columns;
	}
	if (shown == 0)
		std::ignore = gui::widgets::emptyState(m_searchFilter.empty() ? gui::icon::folderOpen : gui::icon::search,
											   m_searchFilter.empty() ? "This folder is empty" : "Nothing matches",
											   m_searchFilter.empty()
													   ? "Drop files from your file manager, or right-click to import."
													   : "Check the spelling, or clear the search.");

	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		m_selectedPath.clear();
		m_renaming = false;
		openContextMenu = true;
	}
	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		m_selectedPath.clear();
	ImGui::EndChild();
	if (openContextMenu)
		ImGui::OpenPopup("ContentBrowserContextMenu");
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const std::filesystem::path sourcePath = m_currentRootPath / static_cast<const char*>(payload->Data);
			if (exists(sourcePath) && sourcePath.parent_path() != m_currentPath)
				moveItem(sourcePath, m_currentPath);
		}
		ImGui::EndDragDropTarget();
	}
}

void ContentBrowser::renderCreateItems() {
	if (gui::widgets::menuItem({.glyph = gui::icon::folderPlus, .label = "Create Folder"}))
		createFolder();
	if (m_materialCreateCallback && gui::widgets::menuItem({.glyph = gui::icon::paintbrush, .label = "New Material"}))
		createMaterial();
	if (m_soundCreateCallback && gui::widgets::menuItem({.glyph = gui::icon::volume2, .label = "New Sound Cue"}))
		createSoundCue();
	if (m_dataTableCreateCallback && gui::widgets::menuItem({.glyph = gui::icon::rows3, .label = "New Data Table"}))
		createDataTable();
	if (m_dialogueCreateCallback && gui::widgets::menuItem({.glyph = gui::icon::bookOpen, .label = "New Dialogue"}))
		createDialogue();
	if (gui::widgets::menuItem({.glyph = gui::icon::workflow, .label = "New Visual Script"}))
		createVisualScript();
	if (gui::widgets::menuItem({.glyph = gui::icon::sparkles, .label = "New PCG Graph"}))
		createPcgGraph();
	ImGui::Separator();
	if (gui::widgets::menuItem({.glyph = gui::icon::download, .label = "Import File..."}))
		importFiles();
	if (gui::widgets::menuItem({.glyph = gui::icon::folderOpen, .label = "Import Folder..."}))
		importFolder();
}

void ContentBrowser::renderContextMenu() {
	if (!ImGui::BeginPopup("ContentBrowserContextMenu"))
		return;
	if (m_selectedPath.empty()) {
		gui::widgets::menuHeader(m_currentPath.filename().string().c_str(), "Folder");
		renderCreateItems();
	} else {
		const bool isDir = is_directory(m_selectedPath);
		const std::string itemName = m_selectedPath.filename().string();
		gui::widgets::menuHeader(itemName.c_str(), describeKind(m_selectedPath, isDir).label);
		const bool isMesh = !isDir && classifyAsset(m_selectedPath) == AssetKind::Mesh;
		if (gui::widgets::menuItem(
					{.glyph = isDir ? gui::icon::folderOpen : gui::icon::externalLink, .label = "Open"})) {
			if (isMesh && m_meshOpenCallback)
				m_meshOpenCallback(m_selectedPath);
			else
				openEntry(std::filesystem::directory_entry{m_selectedPath});
		}
		if (isMesh && m_meshPlaceCallback &&
			gui::widgets::menuItem({.glyph = gui::icon::plus, .label = "Place in Scene"}))
			m_meshPlaceCallback(m_selectedPath);
		if (m_selectedPath.extension() == ".owlmacro" && m_codeOpenCallback &&
			gui::widgets::menuItem({.glyph = gui::icon::fileCode2, .label = "Edit as Text"}))
			m_codeOpenCallback(m_selectedPath);
		if (m_selectedPath.extension() == ".owlmacro" && m_macroExportCallback &&
			gui::widgets::menuItem({.glyph = gui::icon::code, .label = "Export to Lua"}))
			m_macroExportCallback(m_selectedPath);
		if (m_selectedPath.extension() == ".lua" && m_editorScriptRunCallback &&
			std::ranges::contains(m_selectedPath.parent_path(), std::filesystem::path{"editor"}) &&
			gui::widgets::menuItem({.glyph = gui::icon::play, .label = "Run as Editor Script"}))
			m_editorScriptRunCallback(m_selectedPath);
		if (const auto importer = isDir ? std::nullopt : data::assets::importerOf(m_selectedPath);
			(importer == data::assets::Importer::Texture || importer == data::assets::Importer::Mesh) &&
			m_importSettingsCallback &&
			gui::widgets::menuItem({.glyph = gui::icon::slidersHorizontal, .label = "Import Settings"}))
			m_importSettingsCallback(m_selectedPath);
		if (data::assets::importerOf(m_selectedPath) || isDir) {
			if (gui::widgets::menuItem({.glyph = gui::icon::package, .label = isDir ? "Cook Folder" : "Cook"}))
				std::ignore = runCommand("asset.cook", {{"path", m_selectedPath.string()}});
		}
		if (gui::widgets::menuItem({.glyph = gui::icon::pencil, .label = "Rename"})) {
			m_renaming = true;
			m_renameBuffer = itemName;
		}
		if (gui::widgets::menuItem(
					{.glyph = gui::icon::trash2, .label = isDir ? "Delete Folder" : "Delete", .danger = true}))
			m_pendingDelete = true;
		ImGui::Separator();
		renderCreateItems();
	}

	ImGui::EndPopup();

	// Rename dialog
	if (m_renaming && !m_selectedPath.empty()) {
		ImGui::OpenPopup("RenameItem");
	}

	if (ImGui::BeginPopupModal("RenameItem", &m_renaming, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Rename: %s", m_selectedPath.filename().string().c_str());
		const bool submitted =

				ImGui::InputText("##rename", &m_renameBuffer, ImGuiInputTextFlags_EnterReturnsTrue);

		ImGui::SameLine();
		const bool okClicked = gui::widgets::button("OK", {.glyph = gui::icon::check, .label = "OK", .primary = true});

		ImGui::SameLine();
		if (gui::widgets::button("Cancel", {.glyph = gui::icon::x, .label = "Cancel"})) {
			m_renaming = false;
			m_selectedPath.clear();

			ImGui::CloseCurrentPopup();
		}
		if (submitted || okClicked) {
			if (!m_renameBuffer.empty() && m_renameBuffer != m_selectedPath.filename().string())
				std::ignore = runCommand("asset.rename", {{"path", m_selectedPath.string()}, {"name", m_renameBuffer}});
			m_renaming = false;
			m_selectedPath.clear();

			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	// Delete confirmation popup
	if (m_pendingDelete && !m_selectedPath.empty()) {
		ImGui::OpenPopup("ConfirmDelete");
		m_pendingDelete = false;
	}
	if (ImGui::BeginPopupModal("ConfirmDelete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Delete '%s'?", m_selectedPath.filename().string().c_str());
		if (is_directory(m_selectedPath))

			ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.0f}, "This will delete all contents of the folder.");

		ImGui::Separator();
		const float buttonWidth = ImGui::CalcTextSize("Cancel").x + ImGui::GetFontSize() +

								  ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float totalWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;

		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);
		if (gui::widgets::button("Delete", {.glyph = gui::icon::trash2, .label = "Delete", .width = buttonWidth})) {
			deleteSelected();

			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (gui::widgets::button("Cancel", {.glyph = gui::icon::x, .label = "Cancel", .width = buttonWidth})) {
			m_selectedPath.clear();

			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void ContentBrowser::deleteSelected() {
	if (m_selectedPath.empty() || !exists(m_selectedPath))
		return;
	std::ignore = runCommand("asset.delete", {{"path", m_selectedPath.string()}});
	m_selectedPath.clear();
}

auto ContentBrowser::runCommand(const std::string& iName, const commands::CommandArgs& iArgs) -> bool {
	if (!m_commandCallback) {
		OWL_CORE_WARN("ContentBrowser: No command runner, {} ignored.", iName)
		return false;
	}
	const bool done = m_commandCallback(iName, iArgs);
	if (done)
		m_rescanRequested = true;
	return done;
}

void ContentBrowser::createFolder() {
	const auto baseName = m_currentPath / "New Folder";
	auto folderPath = baseName;
	uint32_t counter = 1;
	while (exists(folderPath)) {
		folderPath = m_currentPath / std::format("New Folder {}", counter);
		++counter;
	}

	std::error_code ec;
	std::filesystem::create_directory(folderPath, ec);
	if (ec)
		OWL_CORE_ERROR("Failed to create folder: {}.", ec.message())
	else
		m_rescanRequested = true;
}

void ContentBrowser::createMaterial() {
	auto materialPath = m_currentPath / "New Material.owlmat";
	for (uint32_t counter = 1; exists(materialPath); ++counter)
		materialPath = m_currentPath / std::format("New Material {}.owlmat", counter);
	m_materialCreateCallback(materialPath);
	m_rescanRequested = true;
}

void ContentBrowser::createSoundCue() {
	auto cuePath = m_currentPath / "New Sound Cue.owlsound";
	for (uint32_t counter = 1; exists(cuePath); ++counter)
		cuePath = m_currentPath / std::format("New Sound Cue {}.owlsound", counter);
	m_soundCreateCallback(cuePath);
	m_rescanRequested = true;
}

void ContentBrowser::createDataTable() {
	auto tablePath = m_currentPath / "New Data Table.owltable";
	for (uint32_t counter = 1; exists(tablePath); ++counter)
		tablePath = m_currentPath / std::format("New Data Table {}.owltable", counter);
	m_dataTableCreateCallback(tablePath);
	m_rescanRequested = true;
}

void ContentBrowser::createDialogue() {
	auto dialoguePath = m_currentPath / "New Dialogue.owldlg";
	for (uint32_t counter = 1; exists(dialoguePath); ++counter)
		dialoguePath = m_currentPath / std::format("New Dialogue {}.owldlg", counter);
	m_dialogueCreateCallback(dialoguePath);
	m_rescanRequested = true;
}

void ContentBrowser::createVisualScript() {
	auto scriptPath = m_currentPath / "New Visual Script.owlvs";
	for (uint32_t counter = 1; exists(scriptPath); ++counter)
		scriptPath = m_currentPath / std::format("New Visual Script {}.owlvs", counter);
	if (!platform::writeFileAtomic(scriptPath, script::visual::serialize(VisualScriptDocument::starterScript()))) {
		OWL_ERROR("ContentBrowser: Cannot create the visual script '{}'.", scriptPath.string())
		return;
	}
	m_rescanRequested = true;
	if (m_nodeGraphOpenCallback)
		m_nodeGraphOpenCallback(scriptPath);
}

void ContentBrowser::createPcgGraph() {
	auto graphPath = m_currentPath / "New PCG Graph.owlpcg";
	for (uint32_t counter = 1; exists(graphPath); ++counter)
		graphPath = m_currentPath / std::format("New PCG Graph {}.owlpcg", counter);
	if (!scene::pcg::saveToFile(PcgGraphDocument::starterGraph(), graphPath)) {
		OWL_ERROR("ContentBrowser: Cannot create the PCG graph '{}'.", graphPath.string())
		return;
	}
	m_rescanRequested = true;
	if (m_nodeGraphOpenCallback)
		m_nodeGraphOpenCallback(graphPath);
}

void ContentBrowser::importFiles() {
	const auto file = platform::FileDialog::openFile("");
	if (file.empty())
		return;

	const auto destPath = m_currentPath / file.filename();
	std::error_code ec;
	std::filesystem::copy(file, destPath, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec)
		OWL_CORE_ERROR("Failed to import '{}': {}.", file.filename().string(), ec.message())
	else
		m_rescanRequested = true;
}

void ContentBrowser::importFolder() {
	const auto folder = platform::FileDialog::pickFolder();
	if (folder.empty())
		return;

	const auto destPath = m_currentPath / folder.filename();
	std::error_code ec;
	std::filesystem::copy(folder, destPath, std::filesystem::copy_options::recursive, ec);
	if (ec)
		OWL_CORE_ERROR("Failed to import folder '{}': {}.", folder.filename().string(), ec.message())
	else
		m_rescanRequested = true;
}

void ContentBrowser::handleFileDrop(const std::vector<std::filesystem::path>& iPaths) {
	for (const auto& sourcePath: iPaths) {
		if (!exists(sourcePath))
			continue;

		const auto destPath = m_currentPath / sourcePath.filename();
		std::error_code ec;
		if (is_directory(sourcePath)) {
			std::filesystem::copy(sourcePath, destPath, std::filesystem::copy_options::recursive, ec);
		} else {
			std::filesystem::copy(sourcePath, destPath, std::filesystem::copy_options::overwrite_existing, ec);
		}
		if (ec)
			OWL_CORE_ERROR("Failed to import dropped file '{}': {}.", sourcePath.filename().string(), ec.message())
		else
			m_rescanRequested = true;
	}
}

void ContentBrowser::moveItem(const std::filesystem::path& iSource, const std::filesystem::path& iDestDir) {
	if (iSource.parent_path() == iDestDir || iSource == iDestDir)
		return;
	std::ignore = runCommand("asset.move", {{"path", iSource.string()}, {"folder", iDestDir.string()}});
}

}// namespace owl::nest::panel
