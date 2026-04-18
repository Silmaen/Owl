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
#include <imgui_stdlib.h>

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
	if (iPath.extension() == ".owlprefab")
		return iconBank.getIcon("prefab_icon");
	if (iPath.extension() == ".png")
		return iconBank.getIcon("png_icon");
	if (iPath.extension() == ".svg")
		return iconBank.getIcon("svg_icon");
	if (iPath.extension() == ".ttf")
		return iconBank.getIcon("ttf_icon");
	if (iPath.extension() == ".yml" || iPath.extension() == ".yaml")
		return iconBank.getIcon("yml_icon");
	if (iPath.extension() == ".lua")
		return iconBank.getIcon("lua_icon");
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

}// namespace

ContentBrowser::ContentBrowser() = default;

void ContentBrowser::detach() {}

void ContentBrowser::attach() {
	m_currentRootPath = core::Application::get().getAssetDirectories().front().assetsPath;
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
	core::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[buffer, iPath]() {
				if (!exists(iPath) || !is_directory(iPath))
					return;
				std::error_code ec;
				for (const auto& entry: std::filesystem::directory_iterator(iPath, ec))
					buffer->push_back(entry);
				std::ranges::sort(*buffer, [](const auto& iA, const auto& iB) -> auto {
					if (iA.is_directory() != iB.is_directory())
						return iA.is_directory();
					return iA.path().filename().string() < iB.path().filename().string();
				});
			},
			[flag]() { flag->store(false); }));
}

void ContentBrowser::onImGuiRender() {
	ImGui::Begin("Content Browser");

	// Swap in the latest background scan when it completes.
	if (!m_scanInProgress.load() && m_pendingEntries) {
		m_cachedEntries = std::move(*m_pendingEntries);
		m_cachedPath = m_pendingScanPath;
		m_pendingEntries.reset();
	}

	// Navigation or external trigger may require a rescan.
	if (m_currentPath != m_cachedPath && m_currentPath != m_pendingScanPath)
		requestScan(m_currentPath);
	else if (m_rescanRequested) {
		m_rescanRequested = false;
		requestScan(m_currentPath);
	}

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
	for (const auto& directoryEntry: m_cachedEntries) {
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
			if (directoryEntry.is_directory()) {
				m_currentPath /= path.filename();
			} else if (path.extension() == ".owl" && m_sceneOpenCallback) {
				m_sceneOpenCallback(path);
			}
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

		// Open scene file
		if (!isDir && m_selectedPath.extension() == ".owl" && m_sceneOpenCallback) {
			if (iconBank.menuItem("open", "Open Scene")) {
				m_sceneOpenCallback(m_selectedPath);
			}
			ImGui::Separator();
		}

		if (iconBank.menuItem("rename", "Rename")) {
			m_renaming = true;
			m_renameBuffer = itemName;
		}

		if (iconBank.menuItem("delete", isDir ? "Delete Folder" : "Delete")) {
			m_pendingDelete = true;
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
		const bool submitted =
				ImGui::InputText("##rename", &m_renameBuffer, ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		const auto& buttonBank = gui::IconBank::instance();
		const bool okClicked = buttonBank.iconButton("rename", "OK");
		ImGui::SameLine();
		if (buttonBank.iconButton("close", "Cancel")) {
			m_renaming = false;
			m_selectedPath.clear();
			ImGui::CloseCurrentPopup();
		}
		if (submitted || okClicked) {
			if (!m_renameBuffer.empty()) {
				const auto newPath = m_selectedPath.parent_path() / m_renameBuffer;
				std::error_code ec;
				std::filesystem::rename(m_selectedPath, newPath, ec);
				if (ec)
					OWL_CORE_ERROR("Failed to rename '{}': {}", m_selectedPath.filename().string(), ec.message())
				else
					m_rescanRequested = true;
			}
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
		const auto& buttonBank = gui::IconBank::instance();
		const float buttonWidth = ImGui::CalcTextSize("Cancel").x + ImGui::GetFontSize() +
								  ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float totalWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);
		if (buttonBank.iconButton("delete", "Delete", {buttonWidth, 0})) {
			deleteSelected();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (buttonBank.iconButton("close", "Cancel", {buttonWidth, 0})) {
			m_selectedPath.clear();
			ImGui::CloseCurrentPopup();
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
	m_rescanRequested = true;
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
		OWL_CORE_ERROR("Failed to create folder: {}", ec.message())
	else
		m_rescanRequested = true;
}

void ContentBrowser::importFiles() {
	const auto file = core::utils::FileDialog::openFile("");
	if (file.empty())
		return;

	const auto destPath = m_currentPath / file.filename();
	std::error_code ec;
	std::filesystem::copy(file, destPath, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec)
		OWL_CORE_ERROR("Failed to import '{}': {}", file.filename().string(), ec.message())
	else
		m_rescanRequested = true;
}

void ContentBrowser::importFolder() {
	const auto folder = core::utils::FileDialog::pickFolder();
	if (folder.empty())
		return;

	const auto destPath = m_currentPath / folder.filename();
	std::error_code ec;
	std::filesystem::copy(folder, destPath, std::filesystem::copy_options::recursive, ec);
	if (ec)
		OWL_CORE_ERROR("Failed to import folder '{}': {}", folder.filename().string(), ec.message())
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
			OWL_CORE_ERROR("Failed to import dropped file '{}': {}", sourcePath.filename().string(), ec.message())
		else
			m_rescanRequested = true;
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
	if (!ec)
		m_rescanRequested = true;
}

}// namespace owl::nest::panel
