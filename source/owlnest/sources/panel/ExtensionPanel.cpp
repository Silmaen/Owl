/**
 * @file ExtensionPanel.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ExtensionPanel.h"

#include "../DockLayout.h"
#include "../commands/ExtensionCommands.h"

#include <data/assets/pack/ExtensionPack.h>
#include <data/assets/pack/ExtensionRegistry.h>
#include <gui/IconFont.h>
#include <gui/utils.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Toggle.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest::panel {

namespace {

constexpr std::array<const char*, 2> g_kindNames{"Data", "Script"};
constexpr std::array<const char*, 2> g_kindLabels{"Data (assets only)", "Script (assets and scripts)"};

auto formatDependencies(const std::vector<data::assets::pack::ExtensionDependency>& iDependencies,
						const size_t iSkipped) -> std::string {
	std::string text;
	for (size_t index = 0; index < iDependencies.size(); ++index) {
		if (index == iSkipped)
			continue;
		text += std::format("{}{}@{}", text.empty() ? "" : ", ", iDependencies[index].id,
							iDependencies[index].minVersion.toString());
	}
	return text;
}

}// namespace

void ExtensionPanel::setProjectDirectory(const std::filesystem::path& iProjectDirectory) {
	if (iProjectDirectory == m_projectDirectory)
		return;
	m_projectDirectory = iProjectDirectory;
	m_selected.reset();
	m_dirty = true;
}

void ExtensionPanel::refresh() {
	const std::string selectedId = getSelected() != nullptr ? getSelected()->manifest.id : std::string{};
	m_entries.clear();
	m_selected.reset();
	m_dirty = false;
	if (m_projectDirectory.empty())
		return;
	for (const auto& source:
		 data::assets::pack::ExtensionRegistry::listSources(commands::getExtensionsFolder(m_projectDirectory))) {
		if (auto manifest = data::assets::pack::ExtensionPack::readManifest(source); manifest)
			m_entries.push_back({.folder = source, .manifest = std::move(*manifest)});
	}
	if (!selectedId.empty())
		std::ignore = select(selectedId);
	loadDrafts();
}

auto ExtensionPanel::select(const std::string& iId) -> bool {
	for (size_t index = 0; index < m_entries.size(); ++index) {
		if (m_entries[index].manifest.id == iId) {
			m_selected = index;
			loadDrafts();
			return true;
		}
	}
	return false;
}

auto ExtensionPanel::getSelected() const -> const Entry* {
	return m_selected && *m_selected < m_entries.size() ? &m_entries[*m_selected] : nullptr;
}

void ExtensionPanel::loadDrafts() {
	const auto* entry = getSelected();
	if (entry == nullptr)
		return;
	m_name = entry->manifest.name;
	m_version = entry->manifest.version.toString();
	m_author = entry->manifest.author;
	m_description = entry->manifest.description;
	std::error_code error;
	m_manifestTime =
			std::filesystem::last_write_time(entry->folder / data::assets::pack::g_extensionManifestEntry, error);
}

auto ExtensionPanel::makeContext() const -> commands::CommandContext {
	if (m_contextProvider)
		return m_contextProvider();
	return {.scene = m_scene, .undo = mp_undoManager, .playing = false, .projectDirectory = m_projectDirectory};
}

auto ExtensionPanel::isEnabled(const std::string& iId) const -> bool {
	return mp_disabled == nullptr || !std::ranges::contains(*mp_disabled, iId);
}

auto ExtensionPanel::run(const std::string& iName, commands::CommandArgs iArgs) -> bool {
	if (mp_commands == nullptr)
		return false;
	if (const auto* entry = getSelected(); entry != nullptr && iName != "extension.create" && !iArgs.has("path"))
		iArgs.set("path", entry->folder.string());
	const auto context = makeContext();
	const auto* spec = mp_commands->find(iName);
	if (spec != nullptr && spec->needsScene && (!context.scene || context.undo == nullptr)) {
		m_status = "Open a scene: its history records the manifest edits.";
		return false;
	}
	const auto result = mp_commands->execute(iName, iArgs, context);
	m_status = result ? result->description : result.error().message;
	if (!result)
		OWL_WARN("Extensions: '{}' failed: {}.", iName, result.error().message)
	if (result && iName == "extension.set" && context.onExtensionsChanged)
		context.onExtensionsChanged();
	refresh();
	if (result && iName == "extension.create")
		std::ignore = select(iArgs.getText("id"));
	return result.has_value();
}

void ExtensionPanel::renderList() {
	if (!gui::widgets::sectionHeader("Packs", gui::icon::puzzle, false).open)
		return;
	if (m_entries.empty())
		ImGui::TextDisabled("No pack in extensions/ yet.");
	for (size_t index = 0; index < m_entries.size(); ++index) {
		const auto& manifest = m_entries[index].manifest;
		ImGui::PushID(static_cast<int>(index));
		if (bool enabled = isEnabled(manifest.id); gui::widgets::checkbox("##enabled", &enabled))
			std::ignore = run("extension.enable", {{"path", m_entries[index].folder.string()}, {"enabled", enabled}});
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Enabled: mounted as a plugin in Owl Nest, embedded as a mod by Pack Game.");
		ImGui::PopID();
		ImGui::SameLine();
		const auto label = std::format("{}  {}  ({})##pack{}", manifest.getDisplayName(), manifest.version.toString(),
									   data::assets::pack::getKindName(manifest.kind), index);
		if (ImGui::Selectable(label.c_str(), m_selected == index)) {
			m_selected = index;
			loadDrafts();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", m_entries[index].folder.string().c_str());
	}
	if (gui::widgets::beginProperties("##newPack")) {
		std::ignore = gui::widgets::propertyText("New pack", m_newId, "my.mod",
												 "Id of the pack: lower-case letters, digits, `.`, `_`, `-`.");
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::endProperties();
	}
	if (gui::widgets::button("##createPack", {.glyph = gui::icon::packagePlus,
											  .label = "Create",
											  .tooltip = "Create the pack",
											  .description = "A folder in extensions/ and its manifest.",
											  .disabled = m_newId.empty()}) &&
		run("extension.create", {{"id", m_newId}}))
		m_newId.clear();
	gui::widgets::endSection();
}

void ExtensionPanel::renderManifest(const Entry& iEntry) {
	if (!gui::widgets::sectionHeader("Manifest", gui::icon::fileCog, false).open)
		return;
	const auto& manifest = iEntry.manifest;
	if (gui::widgets::beginProperties("##manifest")) {
		gui::widgets::propertyLabel("Id", "What the dependencies of other packs name; the folder of the pack.");
		ImGui::TextUnformatted(manifest.id.c_str());
		std::ignore = gui::widgets::propertyReset(false);
		const auto textField = [this](const char* iLabel, std::string& ioBuffer, const char* iArg, const char* iHint,
									  const char* iTooltip) -> void {
			std::ignore = gui::widgets::propertyText(iLabel, ioBuffer, iHint, iTooltip);
			if (ImGui::IsItemDeactivatedAfterEdit())
				std::ignore = run("extension.set", {{iArg, ioBuffer}});
		};
		textField("Name", m_name, "name", manifest.id.c_str(), "Name shown to the player and in the editor.");
		textField("Version", m_version, "version", "1.0.0", "Version, `major.minor.patch`.");
		int kind = static_cast<int>(manifest.kind);
		gui::widgets::propertyLabel("Kind", "A Data pack mounts its assets only; a Script pack its scripts too.");
		if (gui::widgets::combo("##kind", &kind, g_kindLabels))
			std::ignore = run("extension.set", {{"kind", std::string{g_kindNames[static_cast<size_t>(kind)]}}});
		if (gui::widgets::propertyReset(manifest.kind != data::assets::pack::ExtensionKind::Data))
			std::ignore = run("extension.set", {{"kind", std::string{"Data"}}});
		int loadOrder = manifest.loadOrder;
		gui::widgets::propertyLabel("Load order",
									"Rank among the packs that do not depend on each other: lower first, the later one "
									"wins an override.");
		ImGui::DragInt("##loadOrder", &loadOrder, 0.2f, -1000, 1000);
		if (ImGui::IsItemDeactivatedAfterEdit())
			std::ignore = run("extension.set", {{"load_order", static_cast<int64_t>(loadOrder)}});
		if (gui::widgets::propertyReset(manifest.loadOrder != 0))
			std::ignore = run("extension.set", {{"load_order", int64_t{0}}});
		textField("Author", m_author, "author", nullptr, "Author or studio.");
		textField("Description", m_description, "description", nullptr, "One sentence about the pack.");
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

void ExtensionPanel::renderDependencies(const Entry& iEntry) {
	if (!gui::widgets::sectionHeader("Dependencies", gui::icon::layers, false).open)
		return;
	const auto& dependencies = iEntry.manifest.dependencies;
	if (dependencies.empty())
		ImGui::TextDisabled("Mounted without any other pack.");
	for (size_t index = 0; index < dependencies.size(); ++index) {
		ImGui::PushID(static_cast<int>(index));
		if (gui::widgets::iconButton("##removeDependency",
									 {.glyph = gui::icon::trash2, .tooltip = "Remove the dependency"}))
			std::ignore = run("extension.set", {{"dependencies", formatDependencies(dependencies, index)}});
		ImGui::SameLine();
		ImGui::Text("%s  >= %s", dependencies[index].id.c_str(), dependencies[index].minVersion.toString().c_str());
		ImGui::PopID();
	}
	if (gui::widgets::beginProperties("##newDependency")) {
		std::ignore = gui::widgets::propertyText("Pack", m_dependencyId, "other.mod", "Id of the pack needed.");
		std::ignore = gui::widgets::propertyReset(false);
		std::ignore = gui::widgets::propertyText("Version", m_dependencyVersion, "0.0.0", "Lowest version accepted.");
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::endProperties();
	}
	if (gui::widgets::button("##addDependency", {.glyph = gui::icon::plus,
												 .label = "Add",
												 .tooltip = "Add the dependency",
												 .disabled = m_dependencyId.empty()})) {
		auto text = formatDependencies(dependencies, dependencies.size());
		text += std::format("{}{}{}{}", text.empty() ? "" : ", ", m_dependencyId,
							m_dependencyVersion.empty() ? "" : "@", m_dependencyVersion);
		if (run("extension.set", {{"dependencies", text}})) {
			m_dependencyId.clear();
			m_dependencyVersion.clear();
		}
	}
	gui::widgets::endSection();
}

void ExtensionPanel::onImGuiRender() {
	if (!m_visible)
		return;
	if (m_focus) {
		ImGui::SetNextWindowFocus();
		m_focus = false;
	}
	if (!ImGui::Begin(panelWindow::title(gui::icon::puzzle, "Extensions", panelWindow::extensions).c_str(),
					  &m_visible)) {
		ImGui::End();
		return;
	}
	if (m_projectDirectory.empty()) {
		gui::widgets::emptyState(gui::icon::puzzle, "No project", "Open a project to list its extension packs.");
		ImGui::End();
		return;
	}
	if (const auto* entry = getSelected(); entry != nullptr) {
		std::error_code error;
		if (std::filesystem::last_write_time(entry->folder / data::assets::pack::g_extensionManifestEntry, error) !=
			m_manifestTime)
			m_dirty = true;
	}
	if (m_dirty)
		refresh();
	renderList();
	if (const auto* entry = getSelected(); entry != nullptr) {
		renderManifest(*entry);
		renderDependencies(*entry);
		if (gui::widgets::button("##exportPack",
								 {.glyph = gui::icon::upload,
								  .label = "Export .owlmod",
								  .tooltip = "Export the pack",
								  .description = "Write exports/<id>.owlmod: drop it in the mods folder of a game.",
								  .primary = true}))
			std::ignore = run("extension.export", {});
		ImGui::SameLine();
		if (gui::widgets::button("##refreshPacks", {.glyph = gui::icon::refreshCw, .label = "Refresh"}))
			refresh();
	}
	if (!m_status.empty())
		ImGui::TextDisabled("%s", m_status.c_str());
	ImGui::End();
}

}// namespace owl::nest::panel
