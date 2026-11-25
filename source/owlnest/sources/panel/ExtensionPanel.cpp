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

#include <app/Application.h>
#include <data/assets/pack/ExtensionPack.h>
#include <data/assets/pack/ExtensionRegistry.h>
#include <gui/IconFont.h>
#include <gui/component/render.h>
#include <gui/utils.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Toggle.h>
#include <platform/FileUtils.h>

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

// Game events a mod handles in its entry script, in the order the panel lists them.
constexpr std::array<const char*, 6> g_modHooks{"on_create",         "on_scene_load", "on_entity_spawn",
												"on_entity_destroy", "on_update",     "on_tick"};

// Functions and components of the entry script of a pack, read from its folder.
void readEntryScript(ExtensionPanel::Entry& ioEntry) {
	ioEntry.functions.clear();
	ioEntry.components.clear();
	ioEntry.scriptTime = {};
	if (ioEntry.manifest.script.empty())
		return;
	const auto file = ioEntry.folder / ioEntry.manifest.script;
	std::error_code error;
	if (!std::filesystem::is_regular_file(file, error))
		return;
	ioEntry.scriptTime = std::filesystem::last_write_time(file, error);
	const auto text = platform::fileToString(file);
	const std::vector<uint8_t> source{text.begin(), text.end()};
	ioEntry.functions = script::ScriptEngine::extractFunctionsFromBuffer(source, ioEntry.manifest.script);
	ioEntry.components = script::ScriptEngine::extractComponentsFromBuffer(source, ioEntry.manifest.script);
}

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
		if (auto manifest = data::assets::pack::ExtensionPack::readManifest(source); manifest) {
			m_entries.push_back({.folder = source,
								 .manifest = std::move(*manifest),
								 .functions = {},
								 .components = {},
								 .scriptTime = {}});
			readEntryScript(m_entries.back());
		}
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
	m_script = entry->manifest.script;
	m_editorScript = entry->manifest.editorScript;
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
	if (const auto* entry = getSelected();
		entry != nullptr && iName != "extension.create" && !iName.starts_with("plugin.") && !iArgs.has("path"))
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
		if (manifest.kind == data::assets::pack::ExtensionKind::Script)
			textField("Entry script", m_script, "script", "scripts/<id>/main.lua",
					  "Script run while a scene plays: the game events of the mod and its components, an asset path "
					  "inside the pack.");
		if (manifest.kind == data::assets::pack::ExtensionKind::Script)
			textField("Editor script", m_editorScript, "editor_script", "editor/<id>/main.lua",
					  "Script of the Owl Nest plugin: run by the editor, it adds commands, panels, inspector drawers, "
					  "document types and importers; an asset path inside the pack.");
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

void ExtensionPanel::renderScript(const Entry& iEntry) {
	if (iEntry.manifest.kind != data::assets::pack::ExtensionKind::Script)
		return;
	if (!gui::widgets::sectionHeader("Entry script", gui::icon::fileCode2, false).open)
		return;
	const auto file = iEntry.folder / iEntry.manifest.script;
	std::error_code error;
	if (iEntry.manifest.script.empty() || !std::filesystem::is_regular_file(file, error)) {
		gui::widgets::notice("No entry script: the pack runs no code while a scene plays.");
		if (gui::widgets::button("##createScript", {.glyph = gui::icon::filePlus,
													.label = "Create",
													.tooltip = "Create the entry script",
													.description = "A script with the game events and a components "
																   "table, named by the manifest."}))
			std::ignore = run("extension.script", {});
		gui::widgets::endSection();
		return;
	}
	if (gui::widgets::button("##openScript", {.glyph = gui::icon::externalLink,
											  .label = "Open",
											  .tooltip = "Open the entry script in the code editor"}))
		std::ignore = gui::component::openAsset(file);
	ImGui::SameLine();
	ImGui::TextDisabled("%s", iEntry.manifest.script.c_str());
	for (const auto* hook: g_modHooks) {
		ImGui::PushID(hook);
		const auto found = std::ranges::find(iEntry.functions, std::string{hook}, &script::ScriptFunction::name);
		const bool defined = found != iEntry.functions.end();
		if (gui::widgets::iconButton("##openHook",
									 {.glyph = gui::icon::externalLink,
									  .tooltip = defined ? "Open the code editor at the function" : "Not defined",
									  .disabled = !defined}))
			std::ignore = gui::component::openAssetAtLine(file, found->line);
		ImGui::SameLine();
		if (defined)
			ImGui::Text("%s  (line %u)", hook, found->line);
		else
			ImGui::TextDisabled("%s  (not defined)", hook);
		ImGui::PopID();
	}
	if (!iEntry.components.empty()) {
		ImGui::SeparatorText("Components");
		for (const auto& component: iEntry.components) {
			std::string fields;
			for (const auto& field: component.fields)
				fields += std::format("{}{}: {}", fields.empty() ? "" : ", ", field.name,
									  script::ScriptEngine::propertyTypeName(field.type));
			ImGui::BulletText("%s", component.key.c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", fields.empty() ? "No field." : fields.c_str());
			if (!component.key.starts_with(iEntry.manifest.id + ".")) {
				ImGui::SameLine();
				ImGui::TextDisabled("(not registered: the key must start with %s.)", iEntry.manifest.id.c_str());
			}
		}
	}
	gui::widgets::endSection();
}

void ExtensionPanel::renderPlugin(const Entry& iEntry) {
	if (iEntry.manifest.kind != data::assets::pack::ExtensionKind::Script)
		return;
	if (!gui::widgets::sectionHeader("Owl Nest plugin", gui::icon::panelsTopLeft, false).open)
		return;
	const auto file = iEntry.folder / iEntry.manifest.editorScript;
	std::error_code error;
	if (iEntry.manifest.editorScript.empty() || !std::filesystem::is_regular_file(file, error)) {
		gui::widgets::notice("No editor script: the pack adds nothing to Owl Nest.", gui::widgets::BadgeKind::Info);
		if (gui::widgets::button("##createEditorScript",
								 {.glyph = gui::icon::filePlus,
								  .label = "Create",
								  .tooltip = "Create the editor script",
								  .description = "A script declaring a command and a panel, named by the manifest."}))
			std::ignore = run("extension.script", {{"editor", true}});
		gui::widgets::endSection();
		return;
	}
	if (gui::widgets::button("##openEditorScript", {.glyph = gui::icon::externalLink,
													.label = "Open",
													.tooltip = "Open the editor script in the code editor"}))
		std::ignore = gui::component::openAsset(file);
	ImGui::SameLine();
	if (gui::widgets::button("##reloadPlugins", {.glyph = gui::icon::refreshCw,
												 .label = "Reload",
												 .tooltip = "Load the plugins again",
												 .description = "Run the editor scripts of the enabled packs again, "
																"after a change."}))
		std::ignore = run("plugin.reload", {});
	ImGui::SameLine();
	ImGui::TextDisabled("%s", iEntry.manifest.editorScript.c_str());
	const auto* plugin = mp_plugins != nullptr ? mp_plugins->find(iEntry.manifest.id) : nullptr;
	if (plugin == nullptr) {
		gui::widgets::notice("Not loaded: the pack is disabled, or left out by its dependencies.",
							 gui::widgets::BadgeKind::Info);
		gui::widgets::endSection();
		return;
	}
	for (const auto& message: plugin->errors) gui::widgets::notice(message.c_str(), gui::widgets::BadgeKind::Error);
	if (plugin->contributions.empty() && plugin->errors.empty())
		ImGui::TextDisabled("The script declares nothing.");
	// The panel toggles run a command that may reload the plugins: the list is drawn from a copy.
	const auto contributions = plugin->contributions;
	for (size_t index = 0; index < contributions.size(); ++index) {
		const auto& contribution = contributions[index];
		ImGui::PushID(static_cast<int>(index));
		const bool defined = contribution.line > 0;
		if (gui::widgets::iconButton("##openHandler",
									 {.glyph = gui::icon::externalLink,
									  .tooltip = defined ? "Open the code editor at the function" : "Not defined",
									  .disabled = !defined}))
			std::ignore = gui::component::openAssetAtLine(file, contribution.line);
		ImGui::SameLine();
		if (contribution.kind == session::PluginContributionKind::Panel) {
			if (bool visible = contribution.visible; gui::widgets::checkbox("##shown", &visible))
				std::ignore = run("plugin.show_panel", {{"id", contribution.id}, {"visible", visible}});
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Show the panel.");
			ImGui::SameLine();
		}
		const auto kind = session::getContributionKindName(contribution.kind);
		const auto extension =
				contribution.extension.empty() ? std::string{} : std::format(" ({})", contribution.extension);
		if (defined)
			ImGui::Text("%s", std::format("{}  {}{}  -  {} (line {})", kind, contribution.id, extension,
										  contribution.handler, contribution.line)
									  .c_str());
		else
			ImGui::TextDisabled("%s", std::format("{}  {}{}  -  {} (not defined)", kind, contribution.id, extension,
												  contribution.handler)
											  .c_str());
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", contribution.title.c_str());
		ImGui::PopID();
	}
	gui::widgets::endSection();
}

void ExtensionPanel::renderConflicts() {
	if (!app::Application::instanced())
		return;
	const auto conflicts = app::Application::get().getExtensions().getConflicts();
	if (conflicts.empty() || !gui::widgets::sectionHeader("Conflicts", gui::icon::triangleAlert, false).open)
		return;
	ImGui::TextWrapped("These assets are held by several enabled packs: the last one mounted serves each.");
	for (const auto& conflict: conflicts) {
		std::string others;
		for (size_t index = 0; index + 1 < conflict.providers.size(); ++index)
			others += std::format("{}{}", others.empty() ? "" : ", ", conflict.providers[index]);
		ImGui::BulletText("%s: %s (over %s)", conflict.path.c_str(), conflict.providers.back().c_str(), others.c_str());
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
		if (!entry->manifest.script.empty() &&
			std::filesystem::last_write_time(entry->folder / entry->manifest.script, error) != entry->scriptTime)
			m_dirty = true;
	}
	if (m_dirty)
		refresh();
	renderList();
	if (const auto* entry = getSelected(); entry != nullptr) {
		renderManifest(*entry);
		renderScript(*entry);
		renderPlugin(*entry);
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
	renderConflicts();
	if (!m_status.empty())
		ImGui::TextDisabled("%s", m_status.c_str());
	ImGui::End();
}

}// namespace owl::nest::panel
