/**
 * @file ContentBrowser.cpp
 * @author Silmaen
 * @date 10/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ContentBrowser.h"

#include <algorithm>

namespace owl::panel {

namespace {
std::filesystem::path g_AssetPath;
}// namespace
ContentBrowser::ContentBrowser() { OWL_SCOPE_UNTRACK }


void ContentBrowser::detach() {
	fileIcon.reset();
	dirIcon.reset();
}

void ContentBrowser::attach() {
	g_AssetPath = core::Application::get().getAssetDirectory();
	currentPath = g_AssetPath;
	if (const auto fileIconPath = g_AssetPath / "icons" / "FileIcon.png"; exists(fileIconPath)) {
		fileIcon = renderer::Texture2D::create(fileIconPath);
	} else {
		OWL_CORE_WARN("Unable to find file icon at {}", fileIconPath.string())
	}
	if (const auto dirIconPath = g_AssetPath / "icons" / "DirectoryIcon.png"; exists(dirIconPath)) {
		dirIcon = renderer::Texture2D::create(dirIconPath);
	} else {
		OWL_CORE_WARN("Unable to find directory icon at {}", dirIconPath.string())
	}
}

void ContentBrowser::onImGuiRender() {
	ImGui::Begin("Content Browser");

	if (currentPath != std::filesystem::path(g_AssetPath)) {
		if (ImGui::Button("<-")) {
			currentPath = currentPath.parent_path();
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
	for (const auto& directoryEntry: std::filesystem::directory_iterator(currentPath)) {
		++item;
		const auto& path = directoryEntry.path();
		auto relativePath = relative(path, g_AssetPath);
		const std::string filenameString = relativePath.filename().string();
		ImGui::PushID(filenameString.c_str());
		const shared<renderer::Texture2D> icon = directoryEntry.is_directory() ? dirIcon : fileIcon;
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
					currentPath /= path.filename();
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
					currentPath /= path.filename();
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

}// namespace owl::panel
