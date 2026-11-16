/**
 * @file NetworkPanel.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "NetworkPanel.h"

#include "../DockLayout.h"

#include <app/EngineContext.h>
#include <gui/IconFont.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <network/GameSession.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <tuple>

namespace owl::nest::panel {

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
auto sameConditions(const network::LinkConditions& iLeft, const network::LinkConditions& iRight) -> bool {
	return iLeft.latencyMs == iRight.latencyMs && iLeft.jitterMs == iRight.jitterMs && iLeft.loss == iRight.loss &&
		   iLeft.duplicate == iRight.duplicate && iLeft.seed == iRight.seed;
}
OWL_DIAG_POP

void valueRow(const char* iLabel, const std::string& iValue, const char* iTooltip = nullptr) {
	gui::widgets::propertyLabel(iLabel, iTooltip);
	ImGui::TextUnformatted(iValue.c_str());
}

}// namespace

void NetworkPanel::onImGuiRender(app::EngineContext& ioContext) {
	if (ioContext.hasNetworkSession()) {
		auto& session = ioContext.getNetworkSession();
		if (!sameConditions(session.getLinkConditions(), m_conditions))
			session.setLinkConditions(m_conditions);
	}
	if (!m_visible)
		return;
	if (m_focus)
		ImGui::SetNextWindowFocus();
	m_focus = false;
	if (!ImGui::Begin(panelWindow::title(gui::icon::globe, "Network", panelWindow::network).c_str(), &m_visible)) {
		ImGui::End();
		return;
	}
	drawSession(ioContext);
	drawConditions();
	if (ioContext.hasNetworkSession())
		drawFigures(ioContext);
	ImGui::End();
}

void NetworkPanel::drawSession(app::EngineContext& ioContext) {
	if (!gui::widgets::subHeader("Session", true))
		return;
	const bool online =
			ioContext.hasNetworkSession() && ioContext.getNetworkSession().getRole() != network::NetRole::Offline;
	ImGui::TextDisabled("Scripts open a session with net.host and net.join; it lives while the scene plays.");
	if (online) {
		auto& session = ioContext.getNetworkSession();
		if (gui::widgets::beginProperties("##netSession")) {
			valueRow("Role", std::string{magic_enum::enum_name(session.getRole())});
			valueRow("State", std::string{magic_enum::enum_name(session.getMatchState())});
			valueRow("Local Player", std::to_string(session.getLocalPlayer()));
			if (session.getRole() == network::NetRole::Host)
				valueRow("Port", std::to_string(session.getPort()));
			gui::widgets::endProperties();
		}
		if (gui::widgets::button("##netLeave", {.glyph = gui::icon::x,
												.label = "Leave",
												.tooltip = "Close the session (the host closes it for every player)"}))
			session.leave();
		return;
	}
	if (gui::widgets::beginProperties("##netJoin")) {
		std::ignore = gui::widgets::propertyInt("Host Port", m_hostPort, 7777, 0, 65535, "%d",
												"Port to listen on; 0 for one the system chooses.");
		gui::widgets::propertyLabel("Address", "Address of the host to join.");
		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputText("##netAddress", &m_joinAddress);
		std::ignore = gui::widgets::propertyInt("Join Port", m_joinPort, 7777, 1, 65535, "%d", "Port of the host.");
		gui::widgets::endProperties();
	}
	if (gui::widgets::button("##netHost", {.glyph = gui::icon::globe,
										   .label = "Host",
										   .tooltip = "Open a lobby on this machine, as a script's net.host does"}))
		std::ignore = ioContext.getNetworkSession().host(static_cast<uint16_t>(m_hostPort));
	ImGui::SameLine();
	if (gui::widgets::button("##netJoinButton",
							 {.glyph = gui::icon::play,
							  .label = "Join",
							  .tooltip = "Join the lobby of a host (an OwlRunner playing the game, another Owl Nest)"}))
		std::ignore = ioContext.getNetworkSession().join(m_joinAddress, static_cast<uint16_t>(m_joinPort));
}

void NetworkPanel::drawConditions() {
	if (!gui::widgets::subHeader("Simulated Link", true))
		return;
	ImGui::TextDisabled("Applied to what this machine sends, in every session of the editor.");
	if (!gui::widgets::beginProperties("##netConditions"))
		return;
	auto latency = static_cast<int>(m_conditions.latencyMs);
	if (gui::widgets::propertyInt("Latency", latency, 0, 0, 2000, "%d ms", "Delay added to every message."))
		m_conditions.latencyMs = static_cast<uint32_t>(latency);
	auto jitter = static_cast<int>(m_conditions.jitterMs);
	if (gui::widgets::propertyInt("Jitter", jitter, 0, 0, 1000, "%d ms", "Random delay added on top of the latency."))
		m_conditions.jitterMs = static_cast<uint32_t>(jitter);
	std::ignore =
			gui::widgets::propertyFloat("Loss", m_conditions.loss, 0.f, 0.005f, "%.3f",
										"Share of the unreliable messages dropped (snapshots, unreliable calls).");
	std::ignore = gui::widgets::propertyFloat("Duplication", m_conditions.duplicate, 0.f, 0.005f, "%.3f",
											  "Share of the unreliable messages sent twice.");
	m_conditions.loss = std::clamp(m_conditions.loss, 0.f, 1.f);
	m_conditions.duplicate = std::clamp(m_conditions.duplicate, 0.f, 1.f);
	gui::widgets::endProperties();
}

void NetworkPanel::drawFigures(app::EngineContext& iContext) {
	const auto& session = iContext.getNetworkSession();
	const auto& stats = session.getStats();
	if (gui::widgets::subHeader("Figures", true) && gui::widgets::beginProperties("##netFigures")) {
		valueRow("Round Trip", std::format("{} ms", stats.roundTripMs), "Mean round trip to the host or the clients.");
		valueRow("Packet Loss", std::format("{:.1f} %", stats.packetLoss * 100.f));
		valueRow("Sent", std::format("{:.2f} kB/s", stats.sentPerSecond / 1000.f));
		valueRow("Received", std::format("{:.2f} kB/s", stats.receivedPerSecond / 1000.f));
		valueRow("Replicated Entities", std::to_string(stats.replicatedEntities));
		valueRow("Tick", std::to_string(stats.tick));
		valueRow("Stalled Frames", std::to_string(stats.stalledFrames),
				 "Lockstep: frames that waited for the input of another player.");
		valueRow("Desync", stats.desyncTick.has_value() ? std::format("at tick {}", *stats.desyncTick) : "none",
				 "Lockstep: the first tick whose world hash differed between two machines.");
		valueRow("Remote Calls", std::format("{} sent, {} received", stats.rpcSent, stats.rpcReceived));
		gui::widgets::endProperties();
	}
	if (!gui::widgets::subHeader("Players", true))
		return;
	if (!ImGui::BeginTable("##netPlayers", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
		return;
	for (const char* column: {"#", "Name", "Ready", "Input", "Machine"}) ImGui::TableSetupColumn(column);
	ImGui::TableHeadersRow();
	for (const auto& player: session.getPlayers()) {
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%u%s", player.id, player.id == session.getLocalPlayer() ? " (you)" : "");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(player.connected ? player.name.c_str() : std::format("{} (lost)", player.name).c_str());
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(player.ready ? gui::icon::check : "");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(std::string{magic_enum::enum_name(player.inputType)}.c_str());
		ImGui::TableNextColumn();
		const bool meets = player.profile.meets(session.getRules().minimum);
		ImGui::Text("%u threads, %.1f ms%s", player.profile.cpuThreads, static_cast<double>(player.profile.frameMs),
					meets ? "" : " (below the minimum)");
	}
	ImGui::EndTable();
}

}// namespace owl::nest::panel
