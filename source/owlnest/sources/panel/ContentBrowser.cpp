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

namespace {}// namespace
ContentBrowser::ContentBrowser() { OWL_SCOPE_UNTRACK }


void ContentBrowser::detach() {
	m_fileIcon.reset();
	m_dirIcon.reset();
}

void ContentBrowser::attach() {
	m_currentRootPath = core::Application::get().getAssetDirectories().front().assetsPath;
	m_currentPath = m_currentRootPath;
	if (const auto fileIconPath = core::Application::get().getFullAssetPath("FileIcon.png", "icons");
		fileIconPath.has_value()) {
		m_fileIcon = renderer::Texture2D::create(fileIconPath.value());
	} else {
		OWL_CORE_WARN("Unable to find file icon.")
	}
	if (const auto dirIconPath = core::Application::get().getFullAssetPath("DirectoryIcon.png", "icons");
		dirIconPath.has_value()) {
		m_dirIcon = renderer::Texture2D::create(dirIconPath.value());
	} else {
		OWL_CORE_WARN("Unable to find directory icon.")
	}
}

void ContentBrowser::onImGuiRender() {
	ImGui::Begin("Content Browser");

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

	static float padding = 30.0f;
	static float thumbnailSize = 100.0f;
	const float cellSize = thumbnailSize + padding;

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
		const shared<renderer::Texture2D> icon = directoryEntry.is_directory() ? m_dirIcon : m_fileIcon;
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		uint64_t textureId = 0;
		if (icon)
			textureId = icon->getRendererId();
		if (textureId != 0) {
			// NOLINTBEGIN(performance-no-int-to-ptr)
			ImGui::ImageButton(fmt::format("content_btn_{}", item).c_str(), reinterpret_cast<ImTextureID>(textureId),
							   {thumbnailSize, thumbnailSize}, {0, 1}, {1, 0});
			// NOLINTEND(performance-no-int-to-ptr)
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
			ImGui::TextWrapped("%s", filenameString.c_str());
		} else {
			ImGui::Button(filenameString.c_str(), {thumbnailSize, thumbnailSize});
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
			ImGui::TextWrapped("%s", filenameString.c_str());
		}
		ImGui::NextColumn();
		ImGui::PopID();
	}
	ImGui::Columns(1);
	ImGui::SliderFloat("Thumbnail Size", &thumbnailSize, 16, 512);
	ImGui::SliderFloat("Padding", &padding, 0, 32);
	ImGui::End();
}

}// namespace owl::nest::panel
