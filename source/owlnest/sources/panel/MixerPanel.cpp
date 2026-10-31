/**
 * @file MixerPanel.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "MixerPanel.h"

#include "../DockLayout.h"

#include <gui/IconFont.h>
#include <gui/Theme.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Tooltip.h>
#include <sound/SoundCommand.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <tuple>
#include <utility>

namespace owl::nest::panel {

namespace {

auto busArg(const sound::Bus iBus) -> commands::ArgValue { return std::string{sound::getBusName(iBus)}; }

// Mute or solo switch of a strip: grey when off, the accent when on, like a toggled icon button.
auto stripToggle(const char* iLabel, const bool iOn, const float iWidth, const char* iTooltip, const char* iDescription)
		-> bool {
	const auto& theme = gui::UiLayer::getTheme();
	const bool clicked = ImGui::InvisibleButton(iLabel, {iWidth, ImGui::GetFrameHeight()});
	const bool hovered = ImGui::IsItemHovered();
	const bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	math::vec4 background = iOn ? theme.getSelection() : theme.surface3;
	if (hovered && !disabled)
		background = ImGui::IsItemActive() ? theme.surface4 : gui::Theme::mix(background, theme.surface4, 0.5f);
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(min, max, ImGui::GetColorU32(gui::vec(background)), theme.frameRounding);
	math::vec4 color = iOn ? theme.accent : theme.text;
	if (disabled)
		color = theme.textDisabled;
	const ImVec2 size = ImGui::CalcTextSize(iLabel, nullptr, true);
	draw->AddText({std::floor(min.x + (iWidth - size.x) * 0.5f), std::floor(min.y + (max.y - min.y - size.y) * 0.5f)},
				  ImGui::GetColorU32(gui::vec(color)), iLabel, ImGui::FindRenderedTextEnd(iLabel));
	gui::widgets::itemTooltip(iTooltip, nullptr, iDescription);
	return clicked;
}

// Effective gain of a bus as a thin meter: the well of a field, filled in the accent up to the gain (2 is full).
void gainMeter(const float iGain, const float iWidth) {
	const auto& theme = gui::UiLayer::getTheme();
	const float height = std::floor(ImGui::GetFontSize() * 0.4f);
	ImGui::Dummy({iWidth, height});
	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(min, max, ImGui::GetColorU32(gui::vec(theme.well)), height * 0.5f);
	if (const float fill = std::clamp(iGain * 0.5f, 0.f, 1.f); fill > 0.f)
		draw->AddRectFilled(min, {min.x + (max.x - min.x) * fill, max.y}, ImGui::GetColorU32(gui::vec(theme.accent)),
							height * 0.5f);
}

}// namespace

void MixerPanel::runBusCommand(const sound::Bus iBus, const bool iPlaying, commands::CommandArgs iArgs) const {
	if (mp_commands == nullptr)
		return;
	iArgs.set("bus", busArg(iBus));
	const commands::CommandTarget target{.registry = mp_commands, .scene = m_scene, .undo = mp_undoManager};
	if (iPlaying) {
		// Not a scene edit: the live mixer only, through the registry so the journal sees it.
		const commands::CommandContext context{.scene = m_scene, .undo = mp_undoManager, .playing = true};
		std::ignore = mp_commands->execute("sound.set_live_bus", iArgs, context);
		return;
	}
	if (target.isValid())
		std::ignore = target.execute("scene.set_mixer", iArgs);
}

void MixerPanel::drawStrip(const sound::Bus iBus, const bool iPlaying) {
	const auto& theme = gui::UiLayer::getTheme();
	const auto* mixer = sound::SoundCommand::getMixer();
	const auto& mix = m_scene->getMixSettings();
	const sound::BusSettings settings = iPlaying && mixer != nullptr
												? sound::BusSettings{.volume = mixer->getBusVolume(iBus),
																	 .muted = mixer->isBusMuted(iBus),
																	 .solo = mixer->isBusSolo(iBus)}
												: mix.getBus(iBus);
	const auto name = sound::getBusName(iBus);
	ImGui::PushID(static_cast<int>(sound::getBusIndex(iBus)));
	ImGui::BeginGroup();
	gui::UiLayer::pushTextStyle(gui::TextStyle::Caption);
	const float nameWidth = ImGui::CalcTextSize(name.data(), name.data() + name.size()).x;
	gui::UiLayer::popTextStyle();
	const float width = std::max(ImGui::GetFrameHeight() * 2.4f, nameWidth + ImGui::GetStyle().FramePadding.x * 2.f);
	gui::UiLayer::pushTextStyle(gui::TextStyle::Caption);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::floor((width - nameWidth) * 0.5f));
	ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(theme.textMuted));
	ImGui::TextUnformatted(name.data(), name.data() + name.size());
	ImGui::PopStyleColor();
	gui::UiLayer::popTextStyle();
	float volume = settings.volume;
	ImGui::PushStyleColor(ImGuiCol_FrameBg, gui::vec(theme.well));
	ImGui::VSliderFloat("##volume", {width, ImGui::GetFrameHeight() * 6.f}, &volume, 0.f, 2.f, "%.2f");
	ImGui::PopStyleColor();
	if (ImGui::IsItemActive())
		m_dragVolume = volume;
	if (ImGui::IsItemDeactivatedAfterEdit() && m_dragVolume) {
		runBusCommand(iBus, iPlaying, {{"volume", static_cast<double>(*m_dragVolume)}});
		m_dragVolume.reset();
	}
	gui::widgets::itemTooltip(std::format("{} Volume", name).c_str(), nullptr,
							  "Designer volume of the bus, multiplied by the player's `volume_*` setting.");
	const float gain = mixer != nullptr ? mixer->getBusGain(iBus) : 0.f;
	gainMeter(gain, width);
	gui::widgets::itemTooltip(std::format("Gain {:.2f}", gain).c_str(), nullptr,
							  "Effective gain of the bus now: its volumes and Master's, mute and solo applied.");
	const float half = (width - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	if (stripToggle("M##mute", settings.muted, half, "Mute", "Silence the bus."))
		runBusCommand(iBus, iPlaying, {{"muted", !settings.muted}});
	ImGui::SameLine();
	ImGui::BeginDisabled(iBus == sound::Bus::Master);
	if (stripToggle("S##solo", settings.solo, half, "Solo", "Hear only the soloed buses."))
		runBusCommand(iBus, iPlaying, {{"solo", !settings.solo}});
	ImGui::EndDisabled();
	ImGui::EndGroup();
	ImGui::PopID();
}

void MixerPanel::drawSpatial(const bool iPlaying) {
	if (!gui::widgets::beginProperties("##spatial"))
		return;
	ImGui::BeginDisabled(iPlaying);
	float doppler = m_dragDoppler.value_or(m_scene->getMixSettings().dopplerFactor);
	gui::widgets::propertyLabel("Doppler Factor",
								"Scale of the pitch shift of moving sounds: 0 disables it, 1 is physical.");
	ImGui::DragFloat("##doppler", &doppler, 0.01f, 0.f, 10.f, "%.2f");
	if (ImGui::IsItemActive())
		m_dragDoppler = doppler;
	bool commit = ImGui::IsItemDeactivatedAfterEdit();
	if (gui::widgets::propertyReset(std::abs(doppler - 1.f) > 1e-4f)) {
		doppler = 1.f;
		commit = true;
	}
	ImGui::EndDisabled();
	gui::widgets::endProperties();
	if (!commit)
		return;
	m_dragDoppler.reset();
	const commands::CommandTarget target{.registry = mp_commands, .scene = m_scene, .undo = mp_undoManager};
	if (mp_commands != nullptr && target.isValid())
		std::ignore = target.execute("scene.set_mixer", {{"doppler_factor", static_cast<double>(doppler)}});
}

void MixerPanel::onImGuiRender() {
	if (!m_visible)
		return;
	if (m_focus)
		ImGui::SetNextWindowFocus();
	m_focus = false;
	if (!ImGui::Begin(panelWindow::title(gui::icon::audioLines, "Mixer", panelWindow::mixer).c_str(), &m_visible)) {
		ImGui::End();
		return;
	}
	if (!m_scene) {
		std::ignore = gui::widgets::emptyState(gui::icon::audioLines, "No scene", "Open a scene to edit its mix.");
		ImGui::End();
		return;
	}
	const bool playing = m_scene->status != scene::Scene::Status::Editing;
	ImGui::TextDisabled(playing ? "Live mixer: the changes end with Play." : "Mix of the scene, saved with it.");
	if (gui::widgets::subHeader("Buses", true)) {
		for (const sound::Bus bus: sound::g_allBuses) {
			if (bus != sound::Bus::Master)
				ImGui::SameLine();
			drawStrip(bus, playing);
		}
	}
	if (gui::widgets::subHeader("Spatial", true))
		drawSpatial(playing);
	ImGui::End();
}

}// namespace owl::nest::panel
