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
	textureLibrary.addFromStandardPath("icons/file_ext_jpg_icon");
	textureLibrary.addFromStandardPath("icons/file_ext_owl_icon");
	textureLibrary.addFromStandardPath("icons/file_ext_png_icon");
	textureLibrary.addFromStandardPath("icons/file_ext_ttf_icon");
	textureLibrary.addFromStandardPath("icons/file_ext_yml_icon");
	textureLibrary.addFromStandardPath("icons/folder_icon");
	textureLibrary.addFromStandardPath("icons/text_file_icon");
}

std::optional<ImTextureID> getFileIcon(const std::filesystem::path& iPath) {
	auto& textureLibrary = renderer::Renderer::getTextureLibrary();
	if (is_directory(iPath))
		return gui::imTexture(textureLibrary.get("icons/folder_icon"));
	if (iPath.extension() == ".png")
		return gui::imTexture(textureLibrary.get("icons/file_ext_png_icon"));
	if (iPath.extension() == ".jpg")
		return gui::imTexture(textureLibrary.get("icons/file_ext_jpg_icon"));
	if (iPath.extension() == ".owl")
		return gui::imTexture(textureLibrary.get("icons/file_ext_owl_icon"));
	if (iPath.extension() == ".ttf")
		return gui::imTexture(textureLibrary.get("icons/file_ext_ttf_icon"));
	if (iPath.extension() == ".yml")
		return gui::imTexture(textureLibrary.get("icons/file_ext_yml_icon"));
	return gui::imTexture(textureLibrary.get("icons/text_file_icon"));
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
			ImGui::ImageButton(fmt::format("content_btn_{}", item).c_str(), tex.value(), {thumbnailSize, thumbnailSize},
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
