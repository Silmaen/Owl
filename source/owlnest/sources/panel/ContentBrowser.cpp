/**
 * @file ContentBrowser.cpp
 * @author Silmaen
 * @date 10/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ContentBrowser.h"

#include <algorithm>
#include <imgui_internal.h>

namespace owl::nest::panel {

namespace {

void loadIcons() {
	auto& textureLibrary = renderer::Renderer::getTextureLibrary();
	textureLibrary.load("icons/files/folder_icon");
	textureLibrary.load("icons/files/glsl_icon");
	textureLibrary.load("icons/files/jpg_icon");
	textureLibrary.load("icons/files/json_icon");
	textureLibrary.load("icons/files/owl_icon");
	textureLibrary.load("icons/files/png_icon");
	textureLibrary.load("icons/files/svg_icon");
	textureLibrary.load("icons/files/text_icon");
	textureLibrary.load("icons/files/ttf_icon");
	textureLibrary.load("icons/files/yml_icon");
}

auto getFileIcon(const std::filesystem::path& iPath) -> std::optional<ImTextureID> {
	auto& textureLibrary = renderer::Renderer::getTextureLibrary();
	if (is_directory(iPath))
		return gui::imTexture(textureLibrary.get("icons/files/folder_icon"));
	if (iPath.extension() == ".glsl" || iPath.extension() == ".frag" || iPath.extension() == ".vert")
		return gui::imTexture(textureLibrary.get("icons/files/glsl_icon"));
	if (iPath.extension() == ".jpg")
		return gui::imTexture(textureLibrary.get("icons/files/jpg_icon"));
	if (iPath.extension() == ".json")
		return gui::imTexture(textureLibrary.get("icons/files/json_icon"));
	if (iPath.extension() == ".owl")
		return gui::imTexture(textureLibrary.get("icons/files/owl_icon"));
	if (iPath.extension() == ".png")
		return gui::imTexture(textureLibrary.get("icons/files/png_icon"));
	if (iPath.extension() == ".svg")
		return gui::imTexture(textureLibrary.get("icons/files/svg_icon"));
	if (iPath.extension() == ".ttf")
		return gui::imTexture(textureLibrary.get("icons/files/ttf_icon"));
	if (iPath.extension() == ".yml" || iPath.extension() == ".yaml")
		return gui::imTexture(textureLibrary.get("icons/files/yml_icon"));
	return gui::imTexture(textureLibrary.get("icons/files/text_file_icon"));
}

}// namespace

ContentBrowser::ContentBrowser() = default;

void ContentBrowser::detach() {}

void ContentBrowser::attach() {
	m_currentRootPath = core::Application::get().getAssetDirectories().front().assetsPath;
	m_currentPath = m_currentRootPath;
	loadIcons();
}

void ContentBrowser::onImGuiRender() {
	ImGui::Begin("Content Browser");

	// ----------------------------------------
	// Top band
	renderTopBand();

	// ---------------------------------------
	// Content band
	renderContent();

	// ---------------------------------------
	// bottom band
	//ImGui::SliderFloat("Thumbnail Size", &thumbnailSize, 16, 512);
	//ImGui::SliderFloat("Padding", &padding, 0, 32);
	ImGui::End();
}

void ContentBrowser::renderTopBand() {
	if (m_currentPath != m_currentRootPath) {
		if (ImGui::Button("Back")) {
			m_currentPath = m_currentPath.parent_path();
		}
	}
	for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
		ImGui::SameLine();
		if (assetsPath == m_currentRootPath) {
			ImGui::Text("%s", title.c_str());
			continue;
		}
		if (ImGui::Button(title.c_str())) {
			m_currentRootPath = assetsPath;
			m_currentPath = m_currentRootPath;
		}
	}
}

void ContentBrowser::renderContent() {
	constexpr float padding = 25.0f;
	constexpr float thumbnailSize = 80.0f;
	constexpr float cellSize = thumbnailSize + padding;

	// setup array of icons
	const float panelWidth = ImGui::GetContentRegionAvail().x;
	int columnCount = static_cast<int>(panelWidth / cellSize);
	columnCount = std::max(columnCount, 1);
	ImGui::Columns(columnCount, nullptr, false);

	uint32_t item = 0;
	for (const auto& directoryEntry: std::filesystem::directory_iterator(m_currentPath)) {
		++item;
		const auto& path = directoryEntry.path();
		auto relativePath = relative(path, m_currentRootPath);
		const std::string filenameString = relativePath.filename().string();
		ImGui::PushID(filenameString.c_str());
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		const auto tex = getFileIcon(path);
		if (tex.has_value()) {
			ImGui::ImageButton(std::format("content_btn_{}", item).c_str(), tex.value(), {thumbnailSize, thumbnailSize},
							   {0, 1}, {1, 0});
		} else {
			ImGui::Button(filenameString.c_str(), {thumbnailSize, thumbnailSize});
		}
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", relativePath.string().c_str(),
									  relativePath.string().size() + 1);
			ImGui::EndDragDropSource();
		}
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			if (directoryEntry.is_directory())
				m_currentPath /= path.filename();
		}
		if (tex.has_value())
			ImGui::TextWrapped("%s", filenameString.c_str());
		ImGui::NextColumn();
		ImGui::PopID();
	}
	ImGui::Columns(1);
}

}// namespace owl::nest::panel
