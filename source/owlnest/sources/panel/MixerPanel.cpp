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
#include <gui/widgets/Tooltip.h>
#include <sound/SoundCommand.h>

#include <imgui.h>

#include <format>
#include <string>
#include <tuple>
#include <utility>

namespace owl::nest::panel {

namespace {

constexpr float g_faderHeight = 140.f;

auto busArg(const sound::Bus iBus) -> commands::ArgValue { return std::string{sound::getBusName(iBus)}; }

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
	const float width = ImGui::GetFrameHeight() * 2.4f;
	ImGui::TextUnformatted(name.data(), name.data() + name.size());
	float volume = settings.volume;
	ImGui::VSliderFloat("##volume", {width, g_faderHeight}, &volume, 0.f, 2.f, "%.2f");
	if (ImGui::IsItemActive())
		m_dragVolume = volume;
	if (ImGui::IsItemDeactivatedAfterEdit() && m_dragVolume) {
		runBusCommand(iBus, iPlaying, {{"volume", static_cast<double>(*m_dragVolume)}});
		m_dragVolume.reset();
	}
	gui::widgets::itemTooltip(std::format("{} volume", name).c_str(), nullptr,
							  "Designer volume of the bus, multiplied by the player's `volume_*` setting.");
	const float gain = mixer != nullptr ? mixer->getBusGain(iBus) : 0.f;
	ImGui::ProgressBar(gain * 0.5f, {width, ImGui::GetFrameHeight() * 0.5f}, "");
	gui::widgets::itemTooltip(std::format("Gain {:.2f}", gain).c_str(), nullptr,
							  "Effective gain of the bus now: its volumes and Master's, mute and solo applied.");
	const float half = (width - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	const bool muted = settings.muted;
	if (muted)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram));
	if (ImGui::Button("M", {half, 0.f}))
		runBusCommand(iBus, iPlaying, {{"muted", !muted}});
	if (muted)
		ImGui::PopStyleColor();
	gui::widgets::itemTooltip("Mute", nullptr, "Silence the bus.");
	ImGui::SameLine();
	ImGui::BeginDisabled(iBus == sound::Bus::Master);
	const bool solo = settings.solo;
	if (solo)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_PlotLines));
	if (ImGui::Button("S", {half, 0.f}))
		runBusCommand(iBus, iPlaying, {{"solo", !solo}});
	if (solo)
		ImGui::PopStyleColor();
	ImGui::EndDisabled();
	gui::widgets::itemTooltip("Solo", nullptr, "Hear only the soloed buses.");
	ImGui::EndGroup();
	ImGui::PopID();
}

void MixerPanel::onImGuiRender() {
	if (!m_visible)
		return;
	if (!ImGui::Begin(panelWindow::title(gui::icon::audioLines, "Mixer", panelWindow::mixer).c_str(), &m_visible)) {
		ImGui::End();
		return;
	}
	if (!m_scene) {
		ImGui::TextDisabled("Open a scene to edit its mix.");
		ImGui::End();
		return;
	}
	const bool playing = m_scene->status != scene::Scene::Status::Editing;
	ImGui::TextDisabled(playing ? "Live mixer: the changes end with Play." : "Mix of the scene, saved with it.");
	for (const sound::Bus bus: sound::g_allBuses) {
		if (bus != sound::Bus::Master)
			ImGui::SameLine();
		drawStrip(bus, playing);
	}
	ImGui::BeginDisabled(playing);
	float doppler = m_scene->getMixSettings().dopplerFactor;
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.f);
	ImGui::DragFloat("Doppler factor", &doppler, 0.01f, 0.f, 10.f, "%.2f");
	if (ImGui::IsItemDeactivatedAfterEdit() && mp_commands != nullptr) {
		const commands::CommandTarget target{.registry = mp_commands, .scene = m_scene, .undo = mp_undoManager};
		if (target.isValid())
			std::ignore = target.execute("scene.set_mixer", {{"doppler_factor", static_cast<double>(doppler)}});
	}
	ImGui::EndDisabled();
	gui::widgets::itemTooltip("Doppler factor", nullptr,
							  "Scale of the pitch shift of moving sounds: 0 disables it, 1 is physical.");
	ImGui::End();
}

}// namespace owl::nest::panel
