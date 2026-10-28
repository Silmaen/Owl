/**
 * @file ScriptDebugPanel.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ScriptDebugPanel.h"

#include "../DockLayout.h"

#include <gui/IconFont.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SearchField.h>
#include <gui/widgets/SectionHeader.h>
#include <imgui.h>
#include <scene/Scene.h>
#include <scene/component/LuaScript.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::panel {

namespace {

auto lowered(std::string_view iText) -> std::string {
	std::string out{iText};
	std::ranges::transform(out, out.begin(),
						   [](const unsigned char iChar) -> char { return static_cast<char>(std::tolower(iChar)); });
	return out;
}

void renderValues(const char* iId, const std::vector<script::WatchValue>& iValues, const std::string& iFilter) {
	if (!gui::widgets::beginProperties(iId))
		return;
	const auto filter = lowered(iFilter);
	for (const auto& value: iValues) {
		if (!filter.empty() && !lowered(value.name).contains(filter))
			continue;
		gui::widgets::propertyLabel(value.name.c_str(), value.type.c_str());
		ImGui::TextUnformatted(value.value.c_str());
		if (ImGui::IsItemHovered() && value.value.size() > 24)
			ImGui::SetTooltip("%s", value.value.c_str());
	}
	gui::widgets::endProperties();
}

void renderScript(const std::string& iTitle, const scene::component::LuaScript& iScript, const std::string& iFilter) {
	if (!iScript.instance || !iScript.instance->isValid())
		return;
	const auto section = gui::widgets::sectionHeader(iTitle.c_str(), gui::icon::fileCode2, false, true);
	if (!section.open)
		return;
	if (iScript.instance->isDisabled())
		gui::widgets::notice("Disabled: the script exceeded a quota.", gui::widgets::BadgeKind::Error);
	ImGui::TextDisabled("%s", iScript.scriptPath.c_str());
	const auto values = iScript.instance->getWatchValues();
	if (values.empty())
		ImGui::TextDisabled("No variable.");
	else
		renderValues(iTitle.c_str(), values, iFilter);
	gui::widgets::endSection();
}

auto runningInstance(const scene::component::LuaScript& iScript, const std::string& iPath)
		-> const script::ScriptInstance* {
	if (!iScript.instance || !iScript.instance->isValid() ||
		!script::ScriptDebugger::isSameScript(iScript.scriptPath, iPath))
		return nullptr;
	return iScript.instance.get();
}

}// namespace

auto findScriptInstance(scene::Scene& iScene, const scene::Entity& iSelected, const std::filesystem::path& iScriptPath)
		-> const script::ScriptInstance* {
	const auto path = iScriptPath.string();
	if (iSelected && iSelected.hasComponent<scene::component::LuaScript>())
		if (const auto* instance = runningInstance(iSelected.getComponent<scene::component::LuaScript>(), path))
			return instance;
	if (const auto* instance = runningInstance(iScene.getSceneScript(), path))
		return instance;
	for (const auto& entity: iScene.getAllEntities()) {
		if (!entity.hasComponent<scene::component::LuaScript>())
			continue;
		if (const auto* instance = runningInstance(entity.getComponent<scene::component::LuaScript>(), path))
			return instance;
	}
	return nullptr;
}

void ScriptDebugPanel::setHit(script::BreakpointHit iHit) {
	m_hit = std::move(iHit);
	m_focus = true;
}

auto ScriptDebugPanel::onImGuiRender(const Context& iContext) -> Request {
	if (m_focus) {
		ImGui::SetNextWindowFocus();
		m_focus = false;
	}
	const bool visible =
			ImGui::Begin(panelWindow::title(gui::icon::bug, "Script Debug", panelWindow::scriptDebug).c_str());
	Request request = Request::None;
	if (visible) {
		if (m_hit.has_value())
			request = renderHit(iContext.paused);
		renderWatch(iContext);
	}
	ImGui::End();
	return request;
}

auto ScriptDebugPanel::renderHit(const bool iPaused) -> Request {
	Request request = Request::None;
	const auto title =
			std::format("Breakpoint: {}:{}", std::filesystem::path{m_hit->script}.filename().string(), m_hit->line);
	if (!gui::widgets::sectionHeader(title.c_str(), gui::icon::circleDot, false, true).open)
		return request;
	if (gui::widgets::button("##continue", {.glyph = gui::icon::play,
											.label = "Continue",
											.tooltip = "Resume the game",
											.description = "The game runs on until the next breakpoint.",
											.primary = true,
											.disabled = !iPaused}))
		request = Request::Continue;
	ImGui::SameLine();
	if (gui::widgets::button("##open", {.glyph = gui::icon::externalLink,
										.label = "Open",
										.tooltip = "Open the script at the line",
										.description = "A visual script opens on its graph, with the node selected."}))
		request = Request::OpenHit;
	if (!m_hit->context.empty())
		ImGui::TextDisabled("%s", m_hit->context.c_str());
	if (m_hit->locals.empty())
		ImGui::TextDisabled("No local at this line.");
	else
		renderValues("##locals", m_hit->locals, {});
	gui::widgets::endSection();
	return request;
}

void ScriptDebugPanel::renderWatch(const Context& iContext) {
	if (iContext.scene == nullptr) {
		ImGui::TextDisabled("Play a scene to watch its scripts.");
		return;
	}
	std::ignore = gui::widgets::searchField("##watchFilter", m_filter, "Filter variables");
	renderScript("Scene Script", iContext.scene->getSceneScript(), m_filter);
	auto entities = iContext.scene->getAllEntities();
	std::erase_if(entities, [](const scene::Entity& iEntity) -> bool {
		return !iEntity.hasComponent<scene::component::LuaScript>();
	});
	if (iContext.selected) {
		const auto selected = std::ranges::find(entities, iContext.selected);
		if (selected != entities.end())
			std::rotate(entities.begin(), selected, selected + 1);
	}
	for (const auto& entity: entities) {
		ImGui::PushID(static_cast<int>(static_cast<uint64_t>(entity.getUUID()) & 0x7fffffffU));
		renderScript(entity.getName(), entity.getComponent<scene::component::LuaScript>(), m_filter);
		ImGui::PopID();
	}
}

}// namespace owl::nest::panel
