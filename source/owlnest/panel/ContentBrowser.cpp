/**
 * @file ContentBrowser.cpp
 * @author Silmaen
 * @date 10/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ContentBrowser.h"

namespace owl::panel {

static std::filesystem::path s_assetPath;

ContentBrowser::ContentBrowser() {
	OWL_SCOPE_UNTRACK
	s_assetPath = core::Application::get().getAssetDirectory();
	currentPath = s_assetPath;
	if (const auto fileIconPath = s_assetPath / "icons" / "FileIcon.png"; exists(fileIconPath)) {
		fileIcon = renderer::Texture2D::create(fileIconPath);
	} else { OWL_CORE_WARN("Unable to find file icon at {}", fileIconPath.string()) }
	if (const auto dirIconPath = s_assetPath / "icons" / "DirectoryIcon.png"; exists(dirIconPath)) {
		dirIcon = renderer::Texture2D::create(dirIconPath);
	} else { OWL_CORE_WARN("Unable to find directory icon at {}", dirIconPath.string()) }
}

void ContentBrowser::onImGuiRender() {
	ImGui::Begin("Content Browser");

	if (currentPath != std::filesystem::path(s_assetPath)) {
		if (ImGui::Button("<-")) { currentPath = currentPath.parent_path(); }
	}

	static float padding = 30.0f;
	static float thumbnailSize = 100.0f;
	const float cellSize = thumbnailSize + padding;

	// setup array of icons
	const float panelWidth = ImGui::GetContentRegionAvail().x;
	int columnCount = static_cast<int>(panelWidth / cellSize);
	if (columnCount < 1)
		columnCount = 1;
	ImGui::Columns(columnCount, nullptr, false);

	for (auto &directoryEntry: std::filesystem::directory_iterator(currentPath)) {
		const auto &path = directoryEntry.path();
		auto relativePath = std::filesystem::relative(path, s_assetPath);
		std::string filenameString = relativePath.filename().string();
		ImGui::PushID(filenameString.c_str());
		shared<renderer::Texture2D> icon = directoryEntry.is_directory() ? dirIcon : fileIcon;
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		if (icon) {
			ImGui::ImageButton(reinterpret_cast<ImTextureID>(icon->getRendererId()), {thumbnailSize, thumbnailSize},
							   {0, 1}, {1, 0});
			if (ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", relativePath.string().c_str(),
										  relativePath.string().size() + 1);
				ImGui::EndDragDropSource();
			}

			ImGui::PopStyleColor();

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				if (directoryEntry.is_directory())
					currentPath /= path.filename();
			}
			ImGui::TextWrapped("%s", filenameString.c_str());
		} else {
			if (directoryEntry.is_directory()) {
				if (ImGui::Button(filenameString.c_str())) { currentPath /= path.filename(); }
			} else { if (ImGui::Button(filenameString.c_str())) {} }
		}
		ImGui::NextColumn();
		ImGui::PopID();
	}
	ImGui::Columns(1);
	ImGui::SliderFloat("Thumbnail Size", &thumbnailSize, 16, 512);
	ImGui::SliderFloat("Padding", &padding, 0, 32);
	ImGui::End();
}

}// namespace owl::panel
