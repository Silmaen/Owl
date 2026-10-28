/**
 * @file ConsolePanel.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ConsolePanel.h"

#include "../DockLayout.h"
#include "../commands/Macro.h"

#include <app/Application.h>
#include <gui/IconFont.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <platform/FileDialog.h>

#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::panel {

namespace {

constexpr std::array g_sources{commands::CommandSource::Gesture, commands::CommandSource::Console,
							   commands::CommandSource::Palette, commands::CommandSource::Mcp,
							   commands::CommandSource::Replay,  commands::CommandSource::Scenario,
							   commands::CommandSource::Script};

constexpr ImVec4 g_errorColor{0.92f, 0.38f, 0.38f, 1.f};
constexpr ImVec4 g_refusedColor{0.95f, 0.65f, 0.25f, 1.f};

auto sourceBit(const commands::CommandSource iSource) -> uint32_t { return 1u << static_cast<uint32_t>(iSource); }

auto outputColor(const session::ConsoleLineKind iKind) -> ImVec4 {
	switch (iKind) {
		case session::ConsoleLineKind::Error:
			return g_errorColor;
		case session::ConsoleLineKind::Refused:
			return g_refusedColor;
		case session::ConsoleLineKind::Input:
		case session::ConsoleLineKind::Result:
		case session::ConsoleLineKind::Help:
			break;
	}
	return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

auto inputCallback(ImGuiInputTextCallbackData* ioData) -> int {
	auto* console = static_cast<session::Console*>(ioData->UserData);
	std::optional<std::string> replacement;
	if (ioData->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
		replacement = ioData->EventKey == ImGuiKey_UpArrow ? console->previousHistory() : console->nextHistory();
	} else if (ioData->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
		const auto completion =
				console->complete(std::string_view{ioData->Buf, static_cast<size_t>(ioData->BufTextLen)});
		if (completion.completed != std::string_view{ioData->Buf, static_cast<size_t>(ioData->BufTextLen)})
			replacement = completion.completed;
	}
	if (replacement) {
		ioData->DeleteChars(0, ioData->BufTextLen);
		ioData->InsertChars(0, replacement->c_str());
	}
	return 0;
}

}// namespace

void ConsolePanel::onImGuiRender() {
	collectJournal();
	ImGui::Begin(panelWindow::title(gui::icon::terminal, "Console", panelWindow::console).c_str());
	renderToolbar();
	const float inputHeight = ImGui::GetFrameHeightWithSpacing();
	ImGui::BeginChild("##consoleItems", {0.f, -inputHeight}, ImGuiChildFlags_None);
	for (const auto& item: m_items) {
		if (item.journalIndex != 0) {
			renderEntry(item.journalIndex);
			continue;
		}
		ImGui::PushStyleColor(ImGuiCol_Text, outputColor(item.output.kind));
		ImGui::TextUnformatted(item.output.text.c_str());
		ImGui::PopStyleColor();
	}
	if (m_scrollToBottom)
		ImGui::SetScrollHereY(1.f);
	m_scrollToBottom = false;
	ImGui::EndChild();
	renderInput();
	ImGui::End();
}

void ConsolePanel::collectJournal() {
	for (const auto& entry: m_session.getJournal().getEntries()) {
		if (entry.index <= m_lastJournalIndex)
			continue;
		m_lastJournalIndex = entry.index;
		addItem({.journalIndex = entry.index, .output = {}});
	}
}

void ConsolePanel::addItem(Item iItem) {
	m_items.push_back(std::move(iItem));
	if (m_items.size() > maxItems)
		m_items.erase(m_items.begin());
	m_scrollToBottom = true;
}

void ConsolePanel::renderToolbar() {
	if (gui::widgets::chip("All##source", m_sourceMask == 0xff))
		m_sourceMask = 0xff;
	for (const auto source: g_sources) {
		ImGui::SameLine(0.f, 4.f);
		const bool shown = (m_sourceMask & sourceBit(source)) != 0;
		if (gui::widgets::chip(std::format("{}##source", commands::getSourceName(source)).c_str(),
							   shown && m_sourceMask != 0xff))
			m_sourceMask = m_sourceMask == 0xff ? sourceBit(source) : m_sourceMask ^ sourceBit(source);
	}
	if (m_sourceMask == 0)
		m_sourceMask = 0xff;
	ImGui::SameLine();
	if (gui::widgets::iconButton("luaMode", {.glyph = gui::icon::code,
											 .tooltip = "Lua",
											 .description = "Type Lua: variables kept between lines, the value of an "
															"expression printed, editor.* runs the commands.",
											 .selected = m_luaMode})) {
		m_luaMode = !m_luaMode;
		m_lua.cancelPending();
		m_refocus = true;
	}
	ImGui::SameLine();
	renderScriptsMenu();
	ImGui::SameLine();
	if (gui::widgets::iconButton(
				"saveMacro",
				{.glyph = gui::icon::save,
				 .tooltip = "Save as Macro",
				 .description = "Write the selected journal lines as a .owlmacro, replayed as one undo step.",
				 .disabled = m_selected.empty()}))
		saveSelection();
	ImGui::SameLine();
	if (gui::widgets::iconButton("clearSelection", {.glyph = gui::icon::x,
													.tooltip = "Clear Selection",
													.description = "Unselect every journal line.",
													.disabled = m_selected.empty()}))
		m_selected.clear();
	ImGui::SameLine();
	if (gui::widgets::iconButton("clearConsole",
								 {.glyph = gui::icon::eraser,
								  .tooltip = "Clear",
								  .description = "Empty the view; the journal and the `$n` numbers are kept."})) {
		m_items.clear();
		m_selected.clear();
	}
}

void ConsolePanel::renderEntry(const size_t iIndex) {
	const auto* entry = m_session.getJournal().find(iIndex);
	if (entry == nullptr || (m_sourceMask & sourceBit(entry->source)) == 0)
		return;
	const bool selected = m_selected.contains(iIndex);
	const auto label = std::format("${:<4} {:<8} {}###entry{}", iIndex, commands::getSourceName(entry->source),
								   entry->line, iIndex);
	if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowOverlap)) {
		if (selected)
			m_selected.erase(iIndex);
		else
			m_selected.insert(iIndex);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Click to select the line for a macro.");
	ImGui::Indent();
	if (entry->succeeded) {
		ImGui::TextDisabled("%s", entry->result.description.c_str());
	} else {
		const bool refused = entry->failure.error == commands::CommandError::RefusedDuringPlay;
		ImGui::PushStyleColor(ImGuiCol_Text, refused ? g_refusedColor : g_errorColor);
		ImGui::TextUnformatted(
				(refused ? std::format("refused during Play: {}", entry->failure.message) : entry->failure.message)
						.c_str());
		ImGui::PopStyleColor();
	}
	ImGui::Unindent();
}

void ConsolePanel::renderInput() {
	if (m_refocus)
		ImGui::SetKeyboardFocusHere();
	m_refocus = false;
	ImGui::SetNextItemWidth(-1.f);
	constexpr ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory |
										  ImGuiInputTextFlags_CallbackCompletion;
	const char* hint = "Command (Tab completes, help lists the commands)";
	if (m_luaMode)
		hint = m_lua.hasPending() ? ">> the block goes on (Esc cancels it)" : "Lua (help(editor) lists the commands)";
	if (m_luaMode && m_lua.hasPending() && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		m_lua.cancelPending();
	if (!ImGui::InputTextWithHint("##consoleInput", hint, &m_input, flags, inputCallback, &m_console))
		return;
	for (auto& output: m_luaMode ? m_lua.submit(m_input) : m_console.submit(m_input)) {
		if (output.journalIndex == 0)
			addItem({.journalIndex = 0, .output = std::move(output)});
	}
	collectJournal();
	m_input.clear();
	m_refocus = true;
}

void ConsolePanel::renderScriptsMenu() {
	if (gui::widgets::iconButton("editorScripts", {.glyph = gui::icon::scrollText,
												   .tooltip = "Editor Scripts",
												   .description = "Run a .lua of the editor/ folder of the project, as "
																  "one undo step."}))
		ImGui::OpenPopup("##editorScripts");
	if (!ImGui::BeginPopup("##editorScripts"))
		return;
	gui::widgets::menuHeader("Editor Scripts", "editor/*.lua");
	std::vector<std::filesystem::path> directories;
	if (app::Application::instanced())
		for (const auto& [title, directory]: app::Application::get().getAssetDirectories())
			directories.push_back(directory);
	const auto scripts = session::LuaConsole::listEditorScripts(directories);
	if (scripts.empty())
		ImGui::TextDisabled("No .lua under editor/ in the asset folders.");
	for (const auto& script: scripts)
		if (gui::widgets::menuItem({.glyph = gui::icon::play, .label = script.filename().string().c_str()}))
			runEditorScript(script);
	ImGui::EndPopup();
}

void ConsolePanel::runEditorScript(const std::filesystem::path& iPath) {
	for (auto& output: m_lua.runScript(iPath)) addItem({.journalIndex = 0, .output = std::move(output)});
	collectJournal();
}

void ConsolePanel::saveSelection() {
	std::vector<const commands::JournalEntry*> entries;
	for (const auto index: m_selected) {
		if (const auto* entry = m_session.getJournal().find(index); entry != nullptr)
			entries.push_back(entry);
	}
	const auto path = platform::FileDialog::saveFile("Owl Macro (*.owlmacro)|owlmacro\n", "macro.owlmacro");
	if (path.empty())
		return;
	const auto macro = commands::Macro::fromJournal(entries, m_session.getRegistry());
	if (const auto saved = macro.save(path); !saved) {
		addItem({.journalIndex = 0,
				 .output = {.kind = session::ConsoleLineKind::Error,
							.text = saved.error().message,
							.journalIndex = 0}});
		return;
	}
	addItem({.journalIndex = 0,
			 .output = {.kind = session::ConsoleLineKind::Help,
						.text = std::format("Saved {} commands to {}", macro.getLines().size(), path.string()),
						.journalIndex = 0}});
	m_selected.clear();
}

}// namespace owl::nest::panel
