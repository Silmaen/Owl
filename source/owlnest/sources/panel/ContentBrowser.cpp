/**
 * @file ContentBrowser.cpp
 * @author Silmaen
 * @date 10/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ContentBrowser.h"

#include <algorithm>
#include <core/utils/FileDialog.h>
#include <gui/IconBank.h>
#include <gui/utils.h>
#include <imgui_internal.h>

namespace owl::nest::panel {

namespace {

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
	if (iPath.extension() == ".png")
		return iconBank.getIcon("png_icon");
	if (iPath.extension() == ".svg")
		return iconBank.getIcon("svg_icon");
	if (iPath.extension() == ".ttf")
		return iconBank.getIcon("ttf_icon");
	if (iPath.extension() == ".yml" || iPath.extension() == ".yaml")
		return iconBank.getIcon("yml_icon");
	return iconBank.getIcon("text_icon");
}

}// namespace

ContentBrowser::ContentBrowser() = default;

void ContentBrowser::detach() {}

void ContentBrowser::attach() {
	m_currentRootPath = core::Application::get().getAssetDirectories().front().assetsPath;
	m_currentPath = m_currentRootPath;
}

void ContentBrowser::onImGuiRender() {
	ImGui::Begin("Content Browser");

	// ----------------------------------------
	// Top band
	renderTopBand();

	// ---------------------------------------
	// Content band
	renderContent();

	// Context menu (popup)
	renderContextMenu();

	ImGui::End();
}

void ContentBrowser::renderTopBand() {
	if (m_currentPath != m_currentRootPath) {
		const auto& iconBank = gui::IconBank::instance();
		bool clicked = false;
		if (const auto iconInfo = iconBank.getIcon("back")) {
			constexpr float btnSize = 20.0f;
			ImGui::PushStyleColor(ImGuiCol_Button, gui::vec(math::vec4{0.f, 0.f, 0.f, 0.f}));
			clicked = ImGui::ImageButton("##back", static_cast<ImTextureID>(iconInfo->textureId),
									   gui::vec(math::vec2{btnSize, btnSize}), gui::vec(iconInfo->uv0),
									   gui::vec(iconInfo->uv1));
			ImGui::PopStyleColor();
		} else {
			clicked = ImGui::Button("Back");
		}
		if (clicked)
			m_currentPath = m_currentPath.parent_path();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Back");
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

	bool openContextMenu = false;
	uint32_t item = 0;
	for (const auto& directoryEntry: std::filesystem::directory_iterator(m_currentPath)) {
		++item;
		const auto& path = directoryEntry.path();
		auto relativePath = relative(path, m_currentRootPath);
		const std::string filenameString = relativePath.filename().string();
		ImGui::PushID(filenameString.c_str());
		ImGui::PushStyleColor(ImGuiCol_Button, gui::vec(math::vec4{0.f, 0.f, 0.f, 0.f}));
		const auto iconInfo = getFileIcon(path);
		constexpr auto thumbSizeVec = gui::vec(math::vec2{thumbnailSize, thumbnailSize});
		if (iconInfo.has_value()) {
			ImGui::ImageButton(std::format("content_btn_{}", item).c_str(),
							   static_cast<ImTextureID>(iconInfo->textureId), thumbSizeVec,
							   gui::vec(iconInfo->uv0), gui::vec(iconInfo->uv1));
		} else {
			ImGui::Button(filenameString.c_str(), thumbSizeVec);
		}

		// Right-click context menu — defer OpenPopup to outside the PushID scope
		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			m_selectedPath = path;
			m_renaming = false;
			openContextMenu = true;
		}

		// Drag source (existing)
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", relativePath.string().c_str(),
									  relativePath.string().size() + 1);
			ImGui::EndDragDropSource();
		}

		// Drop target: folders accept items
		if (directoryEntry.is_directory() && ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				const auto* droppedRelPath = static_cast<const char*>(payload->Data);
				const std::filesystem::path sourcePath = m_currentRootPath / droppedRelPath;
				if (exists(sourcePath) && sourcePath != path) {
					moveItem(sourcePath, path);
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			if (directoryEntry.is_directory())
				m_currentPath /= path.filename();
		}
		if (iconInfo.has_value())
			ImGui::TextWrapped("%s", filenameString.c_str());
		ImGui::NextColumn();
		ImGui::PopID();
	}
	ImGui::Columns(1);

	// Right-click on empty space
	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		m_selectedPath.clear();
		m_renaming = false;
		openContextMenu = true;
	}

	// Open the popup at window level (outside any PushID scope)
	if (openContextMenu)
		ImGui::OpenPopup("ContentBrowserContextMenu");

	// Drop target for the content area background (move items to current dir)
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const auto* droppedRelPath = static_cast<const char*>(payload->Data);
			const std::filesystem::path sourcePath = m_currentRootPath / droppedRelPath;
			if (exists(sourcePath) && sourcePath.parent_path() != m_currentPath) {
				moveItem(sourcePath, m_currentPath);
			}
		}
		ImGui::EndDragDropTarget();
	}
}

void ContentBrowser::renderContextMenu() {
	if (!ImGui::BeginPopup("ContentBrowserContextMenu"))
		return;

	const auto& iconBank = gui::IconBank::instance();

	if (m_selectedPath.empty()) {
		// Background context menu
		if (iconBank.menuItem("new_folder", "Create Folder")) {
			createFolder();
		}
		ImGui::Separator();
		if (iconBank.menuItem("import_file", "Import File...")) {
			importFiles();
		}
		if (iconBank.menuItem("import_folder", "Import Folder...")) {
			importFolder();
		}
	} else {
		// Item context menu
		const bool isDir = is_directory(m_selectedPath);
		const std::string itemName = m_selectedPath.filename().string();

		ImGui::TextDisabled("%s", itemName.c_str());
		ImGui::Separator();

		if (iconBank.menuItem("rename", "Rename")) {
			m_renaming = true;
			m_renameBuffer = itemName;
		}

		if (iconBank.menuItem("delete", isDir ? "Delete Folder" : "Delete")) {
			deleteSelected();
			ImGui::EndPopup();
			return;
		}

		ImGui::Separator();

		if (iconBank.menuItem("new_folder", "Create Folder")) {
			createFolder();
		}
		ImGui::Separator();
		if (iconBank.menuItem("import_file", "Import File...")) {
			importFiles();
		}
		if (iconBank.menuItem("import_folder", "Import Folder...")) {
			importFolder();
		}
	}

	ImGui::EndPopup();

	// Rename dialog
	if (m_renaming && !m_selectedPath.empty()) {
		ImGui::OpenPopup("RenameItem");
	}

	if (ImGui::BeginPopupModal("RenameItem", &m_renaming, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Rename: %s", m_selectedPath.filename().string().c_str());
		m_renameBuffer.resize(256);
		if (ImGui::InputText("##rename", m_renameBuffer.data(), m_renameBuffer.capacity(),
							 ImGuiInputTextFlags_EnterReturnsTrue)) {
			if (const std::string newName(m_renameBuffer); !newName.empty()) {
				const auto newPath = m_selectedPath.parent_path() / newName;
				std::error_code ec;
				std::filesystem::rename(m_selectedPath, newPath, ec);
				if (ec)
					OWL_CORE_ERROR("Failed to rename '{}': {}", m_selectedPath.filename().string(), ec.message())
			}
			m_renaming = false;
			m_selectedPath.clear();
		}
		if (ImGui::Button("Cancel")) {
			m_renaming = false;
			m_selectedPath.clear();
		}
		ImGui::EndPopup();
	}
}

void ContentBrowser::deleteSelected() {
	if (m_selectedPath.empty() || !exists(m_selectedPath))
		return;

	std::error_code ec;
	if (is_directory(m_selectedPath)) {
		std::filesystem::remove_all(m_selectedPath, ec);
	} else {
		std::filesystem::remove(m_selectedPath, ec);
	}

	if (ec)
		OWL_CORE_ERROR("Failed to delete '{}': {}", m_selectedPath.filename().string(), ec.message())

	m_selectedPath.clear();
}

void ContentBrowser::createFolder() const {
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
		OWL_CORE_ERROR("Failed to create folder: {}", ec.message())
}

void ContentBrowser::importFiles() const {
	const auto file = core::utils::FileDialog::openFile("");
	if (file.empty())
		return;

	const auto destPath = m_currentPath / file.filename();
	std::error_code ec;
	std::filesystem::copy(file, destPath, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec)
		OWL_CORE_ERROR("Failed to import '{}': {}", file.filename().string(), ec.message())
}

void ContentBrowser::importFolder() const {
	const auto folder = core::utils::FileDialog::pickFolder();
	if (folder.empty())
		return;

	const auto destPath = m_currentPath / folder.filename();
	std::error_code ec;
	std::filesystem::copy(folder, destPath, std::filesystem::copy_options::recursive, ec);
	if (ec)
		OWL_CORE_ERROR("Failed to import folder '{}': {}", folder.filename().string(), ec.message())
}

void ContentBrowser::handleFileDrop(const std::vector<std::filesystem::path>& iPaths) const {
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
			OWL_CORE_ERROR("Failed to import dropped file '{}': {}", sourcePath.filename().string(), ec.message())
	}
}

void ContentBrowser::moveItem(const std::filesystem::path& iSource, const std::filesystem::path& iDestDir) {
	const auto destPath = iDestDir / iSource.filename();
	if (iSource == destPath)
		return;

	std::error_code ec;
	std::filesystem::rename(iSource, destPath, ec);
	if (ec) {
		// rename fails across filesystems, fall back to copy + delete
		std::filesystem::copy(iSource, destPath,
							  std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
							  ec);
		if (!ec) {
			std::filesystem::remove_all(iSource, ec);
		}
		if (ec)
			OWL_CORE_ERROR("Failed to move '{}': {}", iSource.filename().string(), ec.message())
	}
}

}// namespace owl::nest::panel
