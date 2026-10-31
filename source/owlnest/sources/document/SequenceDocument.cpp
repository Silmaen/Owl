/**
 * @file SequenceDocument.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SequenceDocument.h"

#include "DockLayout.h"
#include "EditorLayer.h"
#include "SceneDocument.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <gui/IconFont.h>
#include <gui/Theme.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <math/trigonometry.h>
#include <scene/Scene.h>
#include <scene/component/SequencePlayer.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace owl::nest {

namespace {

using data::sequence::CameraKey;
using data::sequence::EventKey;
using data::sequence::Sequence;

class ModifySequenceCommand final : public UndoCommand<Sequence> {
public:
	ModifySequenceCommand(Sequence iBefore, Sequence iAfter, std::string iDescription, const bool iMerge)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)},
		  m_merge{iMerge} {}

	void undo(Sequence& ioTarget) override { ioTarget = m_before; }

	void redo(Sequence& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return m_merge ? 1 : 0; }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto& other = static_cast<const ModifySequenceCommand&>(iOther);
		if (other.m_description != m_description)
			return false;
		m_after = other.m_after;
		return true;
	}

private:
	Sequence m_before;
	Sequence m_after;
	std::string m_description;
	bool m_merge;
};

constexpr float g_headerWidth = 160.f;
constexpr float g_rowHeight = 24.f;
constexpr float g_rulerHeight = 22.f;
constexpr float g_keyRadius = 6.f;
constexpr float g_snapStep = 0.1f;
// Seconds between two searches of a scene playing the sequence while it has none.
constexpr float g_previewSearchPeriod = 0.5f;

auto color(const math::vec4& iColor) -> ImU32 { return ImGui::GetColorU32(gui::vec(iColor)); }

auto theme() -> const gui::Theme& { return gui::UiLayer::getTheme(); }

// Colour of a camera track in the path view, cycling through the theme's.
auto trackColor(const size_t iTrack) -> math::vec4 {
	const auto& colors = theme();
	const std::array palette{colors.accent, colors.info, colors.success, colors.warning, colors.axisX, colors.play};
	return palette[iTrack % palette.size()];
}

// Seconds between two ticks of the ruler at a zoom.
auto rulerStep(const float iZoom) -> float {
	if (iZoom < 30.f)
		return 5.f;
	return iZoom < 60.f ? 2.f : 1.f;
}

// Colour of a key on the timeline.
auto keyColor(const bool iPicked, const bool iMuted) -> math::vec4 {
	if (iPicked)
		return theme().text;
	return iMuted ? theme().textDisabled : theme().accent;
}

auto snapped(const float iTime, const bool iSnap) -> float {
	return iSnap ? std::round(iTime / g_snapStep) * g_snapStep : iTime;
}

}// namespace

SequenceDocument::SequenceDocument() { m_undo.markSaved(); }

SequenceDocument::~SequenceDocument() = default;

auto SequenceDocument::title() const -> std::string {
	return m_path.empty() ? std::string{"Sequence"} : m_path.stem().string();
}

void SequenceDocument::onAttach(EditorLayer* iEditor) { mp_editorLayer = iEditor; }

void SequenceDocument::onDetach() { mp_editorLayer = nullptr; }

void SequenceDocument::onUpdate(const core::Timestep& iTimeStep) {
	m_previewSearch -= iTimeStep.getSeconds();
	if (m_previewSearch <= 0.f) {
		m_previewSearch = g_previewSearchPeriod;
		if (const auto player = getPreviewPlayer(); !isPlayerOf(player, m_path))
			resolvePreview(nullptr, {});
	}
	if (!m_playing)
		return;
	m_time += iTimeStep.getSeconds();
	if (m_time >= m_sequence.duration) {
		m_time = m_sequence.duration;
		m_playing = false;
	}
}

void SequenceDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto SequenceDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto loaded = Sequence::loadFile(iPath);
	if (!loaded) {
		OWL_CORE_WARN("SequenceDocument: {} is not a sequence.", iPath.string())
		return false;
	}
	m_sequence = std::move(*loaded);
	m_path = iPath;
	m_undo.clear();
	m_undo.markSaved();
	m_selection = {};
	m_time = 0.f;
	return true;
}

void SequenceDocument::onImGuiRender() {
	const auto winTitle = documentWindowTitle(DocumentType::Sequence, title(), static_cast<uint64_t>(id()));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;
	// Docked with the bottom panels, under the viewport of the scene it previews on.
	if (const auto* browser = ImGui::FindWindowByName(panelWindow::contentBrowser);
		browser != nullptr && browser->DockId != 0)
		ImGui::SetNextWindowDockID(browser->DockId, ImGuiCond_FirstUseEver);
	const bool wantFocus = consumeFocusRequest();
	if (wantFocus)
		ImGui::SetNextWindowFocus();
	const bool open = ImGui::Begin(winTitle.c_str(), &m_pOpen, flags);
	if (mp_editorLayer != nullptr)
		mp_editorLayer->renderDocumentTabMenu(*this);
	if (wantFocus) {
		ImGui::SetWindowFocus();
		if (mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
	}
	if (open) {
		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (focused && !m_wasFocused && mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
		m_wasFocused = focused;
		const auto& io = ImGui::GetIO();
		if (focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
			std::ignore = save();
		if (focused && !io.WantTextInput) {
			if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
				std::ignore = deleteSelectedKeys();
			if (ImGui::IsKeyPressed(ImGuiKey_K, false))
				std::ignore = addKeyAtPlayhead();
			if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
				setPlaying(!m_playing);
		}
		renderToolbar();
		renderTimeline();
	}
	ImGui::End();
}

auto SequenceDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto SequenceDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (!m_sequence.saveToFile(iPath)) {
		OWL_CORE_ERROR("SequenceDocument: Failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_undo.markSaved();
	// The players read the saved file again.
	if (app::Application::instanced())
		app::Application::get().getEngineContext().getSequences().clear();
	return true;
}

auto SequenceDocument::recoverySnapshot() const -> std::optional<std::string> { return m_sequence.serializeToString(); }

auto SequenceDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	auto restored = Sequence::deserializeFromString(iSnapshot);
	if (!restored) {
		OWL_WARN("Recovery: Cannot restore the autosave of the sequence.")
		return false;
	}
	m_sequence = std::move(*restored);
	m_undo.clear();
	m_undo.markUnsaved();
	setSelection(m_selection);
	return true;
}

void SequenceDocument::performUndo() {
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_sequence);
	setSelection(m_selection);
}

void SequenceDocument::performRedo() {
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_sequence);
	setSelection(m_selection);
}

auto SequenceDocument::apply(Sequence iNext, const std::string& iDescription, const bool iMerge) -> bool {
	if (iNext == m_sequence)
		return false;
	auto before = std::move(m_sequence);
	m_sequence = iNext;
	m_undo.push(mkUniq<ModifySequenceCommand>(std::move(before), std::move(iNext), iDescription, iMerge));
	setSelection(m_selection);
	return true;
}

void SequenceDocument::setSelection(SequenceSelection iSelection) {
	const size_t tracks = iSelection.camera ? m_sequence.cameraTracks.size() : m_sequence.eventTracks.size();
	if (iSelection.track >= tracks) {
		m_selection = {.camera = iSelection.camera, .track = tracks == 0 ? 0 : tracks - 1, .keys = {}};
		return;
	}
	const size_t keys = iSelection.camera ? m_sequence.cameraTracks[iSelection.track].keys.size()
										  : m_sequence.eventTracks[iSelection.track].keys.size();
	std::erase_if(iSelection.keys, [keys](const size_t iKey) -> bool { return iKey >= keys; });
	std::ranges::sort(iSelection.keys);
	const auto [first, last] = std::ranges::unique(iSelection.keys);
	iSelection.keys.erase(first, last);
	m_selection = std::move(iSelection);
}

void SequenceDocument::setTime(const float iTime) { m_time = std::clamp(iTime, 0.f, m_sequence.duration); }

void SequenceDocument::setPlaying(const bool iPlaying) {
	if (iPlaying && m_time >= m_sequence.duration)
		m_time = 0.f;
	m_playing = iPlaying;
}

auto SequenceDocument::addKeyAtPlayhead() -> bool {
	auto args = trackArgs();
	args.set("time", static_cast<double>(m_time));
	const auto result = sendCommand("sequence.add_key", std::move(args));
	if (!result)
		return false;
	if (const auto found = result->data.find("key"); found != result->data.end())
		if (const auto* key = std::get_if<int64_t>(&found->second); key != nullptr)
			setSelection(
					{.camera = m_selection.camera, .track = m_selection.track, .keys = {static_cast<size_t>(*key)}});
	return true;
}

auto SequenceDocument::deleteSelectedKeys() -> bool {
	if (m_selection.keys.empty())
		return false;
	auto args = trackArgs();
	args.set("keys", selectedKeysText());
	if (!sendCommand("sequence.remove_keys", std::move(args)))
		return false;
	setSelection({.camera = m_selection.camera, .track = m_selection.track, .keys = {}});
	return true;
}

auto SequenceDocument::addTrack(const bool iCamera) -> bool {
	const auto result = sendCommand("sequence.add_track", {{"kind", std::string{iCamera ? "Camera" : "Event"}}});
	if (!result)
		return false;
	setSelection({.camera = iCamera,
				  .track = iCamera ? m_sequence.cameraTracks.size() - 1 : m_sequence.eventTracks.size() - 1,
				  .keys = {}});
	return true;
}

auto SequenceDocument::sendCommand(const std::string_view iName, commands::CommandArgs iArgs)
		-> std::optional<commands::CommandResult> {
	if (mp_editorLayer == nullptr) {
		OWL_WARN("SequenceDocument: {} ignored, no editor to run it.", iName)
		return std::nullopt;
	}
	iArgs.set("path", m_path.generic_string());
	auto result = mp_editorLayer->execute(iName, iArgs, commands::CommandSource::Gesture);
	if (!result)
		return std::nullopt;
	return std::move(*result);
}

auto SequenceDocument::trackArgs() const -> commands::CommandArgs {
	return {{"kind", std::string{m_selection.camera ? "Camera" : "Event"}},
			{"track", static_cast<int64_t>(m_selection.track)}};
}

auto SequenceDocument::selectedKeysText() const -> std::string {
	std::string text;
	for (const auto key: m_selection.keys) text += std::format("{}{}", text.empty() ? "" : ",", key);
	return text;
}

auto SequenceDocument::keyTime(const bool iCamera, const size_t iTrack, const size_t iKey) const -> float {
	return iCamera ? m_sequence.cameraTracks[iTrack].keys[iKey].time : m_sequence.eventTracks[iTrack].keys[iKey].time;
}

auto SequenceDocument::timeToX(const float iTime) const -> float { return m_originX + iTime * m_zoom; }

auto SequenceDocument::xToTime(const float iX) const -> float { return (iX - m_originX) / m_zoom; }

void SequenceDocument::renderToolbar() {
	if (gui::widgets::iconButton("##play", {.glyph = m_playing ? gui::icon::pause : gui::icon::play,
											.tooltip = m_playing ? "Pause" : "Play",
											.shortcut = "Space",
											.description = "Play the sequence: the viewport of the scene follows it."}))
		setPlaying(!m_playing);
	ImGui::SameLine();
	if (gui::widgets::iconButton("##rewind", {.glyph = gui::icon::skipBack, .tooltip = "Back to the start"})) {
		m_playing = false;
		m_time = 0.f;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.f);
	float time = m_time;
	if (ImGui::DragFloat("##time", &time, 0.01f, 0.f, m_sequence.duration, "%.2f s"))
		setTime(time);
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	ImGui::TextUnformatted("Duration");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.f);
	float duration = m_sequence.duration;
	if (ImGui::DragFloat("##duration", &duration, 0.05f, 0.f, 3600.f, "%.2f s"))
		std::ignore = sendCommand("sequence.set_duration",
								  {{"duration", static_cast<double>(duration)}, {"merge", m_durationTyping}});
	m_durationTyping = ImGui::IsItemActive();
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	if (gui::widgets::iconButton("##addShot", {.glyph = gui::icon::video,
											   .tooltip = "Add a camera track",
											   .description = "A shot: its keys drive the camera between them."}))
		std::ignore = addTrack(true);
	ImGui::SameLine();
	if (gui::widgets::iconButton("##addEvents", {.glyph = gui::icon::zap,
												 .tooltip = "Add an event track",
												 .description = "Its keys call Lua functions during the playback."}))
		std::ignore = addTrack(false);
	ImGui::SameLine();
	if (gui::widgets::iconButton("##addKey", {.glyph = gui::icon::plus,
											  .tooltip = "Add a key",
											  .shortcut = "K",
											  .description = "A key at the playhead on the selected track."}))
		std::ignore = addKeyAtPlayhead();
	ImGui::SameLine();
	if (gui::widgets::iconButton("##removeKeys", {.glyph = gui::icon::trash2,
												  .tooltip = "Remove the keys",
												  .shortcut = "Delete",
												  .description = "Remove the selected keys.",
												  .disabled = m_selection.keys.empty()}))
		std::ignore = deleteSelectedKeys();
	ImGui::SameLine();
	if (gui::widgets::iconButton("##snap", {.glyph = gui::icon::scan,
											.tooltip = "Snap",
											.description = "Keys and playhead snap to a tenth of a second.",
											.selected = m_snap}))
		m_snap = !m_snap;
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	ImGui::SliderFloat("##zoom", &m_zoom, 10.f, 400.f, "%.0f px/s", ImGuiSliderFlags_Logarithmic);
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	const auto player = getPreviewPlayer();
	if (gui::widgets::iconButton("##lookThrough",
								 {.glyph = gui::icon::eye,
								  .tooltip = "Look through",
								  .description = "The viewport of the scene shows the sequence camera at the playhead; "
												 "turned off, the view stays there to be moved and keyed.",
								  .selected = m_lookThrough,
								  .disabled = !player}))
		m_lookThrough = !m_lookThrough;
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	if (const auto* document = getPreviewDocument(); document != nullptr && player)
		ImGui::TextDisabled(
				"%s", std::format("{} {} in {}", gui::icon::clapperboard, player.getName(), document->title()).c_str());
	else
		ImGui::TextDisabled("No open scene plays this sequence: open one to preview it.");
}

void SequenceDocument::renderTimeline() {
	auto* draw = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float width = std::max(ImGui::GetContentRegionAvail().x, g_headerWidth + 50.f);
	const size_t rows = m_sequence.cameraTracks.size() + m_sequence.eventTracks.size();
	const float height = g_rulerHeight + static_cast<float>(std::max<size_t>(rows, 1)) * g_rowHeight;
	m_originX = origin.x + g_headerWidth + g_keyRadius;
	m_lanesX = origin.x + g_headerWidth;
	m_rowsTop = origin.y + g_rulerHeight;
	const auto& colors = theme();
	ImGui::InvisibleButton("##timeline", {width, height}, ImGuiButtonFlags_MouseButtonLeft);
	const bool hovered = ImGui::IsItemHovered();
	draw->AddRectFilled(origin, {origin.x + width, origin.y + height}, color(colors.well));
	draw->PushClipRect({m_lanesX, origin.y}, {origin.x + width, origin.y + height}, true);
	draw->AddRectFilled({timeToX(m_sequence.duration), origin.y}, {origin.x + width, origin.y + height},
						color(gui::Theme::withAlpha(colors.surface1, 0.7f)));
	const float step = rulerStep(m_zoom);
	for (int tick = 0; timeToX(static_cast<float>(tick) * step) < origin.x + width; ++tick) {
		const float second = static_cast<float>(tick) * step;
		const float x = timeToX(second);
		draw->AddLine({x, origin.y}, {x, origin.y + height}, color(colors.lineSoft));
		draw->AddText({x + 3.f, origin.y + 3.f}, color(colors.textMuted), std::format("{:g}", second).c_str());
	}
	draw->PopClipRect();
	draw->AddLine({origin.x, m_rowsTop}, {origin.x + width, m_rowsTop}, color(colors.line));
	size_t row = 0;
	for (size_t track = 0; track < m_sequence.cameraTracks.size(); ++track, ++row)
		renderTimelineRow(true, track, m_rowsTop + static_cast<float>(row) * g_rowHeight);
	for (size_t track = 0; track < m_sequence.eventTracks.size(); ++track, ++row)
		renderTimelineRow(false, track, m_rowsTop + static_cast<float>(row) * g_rowHeight);
	if (rows == 0)
		draw->AddText({origin.x + 8.f, m_rowsTop + 4.f}, color(colors.textMuted),
					  "No track: add one from the Tracks panel.");
	const float playhead = timeToX(m_time);
	draw->AddLine({playhead, origin.y}, {playhead, origin.y + height}, color(colors.accent), 2.f);
	if (hovered)
		handleTimelineClicks();
	if (m_scrubbing) {
		setTime(snapped(xToTime(ImGui::GetIO().MousePos.x), m_snap));
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
			m_scrubbing = false;
	}
	if (m_dragging)
		updateKeyDrag();
	renderTrackMenu();
}

auto SequenceDocument::rowAt(const float iY) const -> std::optional<std::pair<bool, size_t>> {
	const float local = iY - m_rowsTop;
	if (local < 0.f)
		return std::nullopt;
	const auto index = static_cast<size_t>(local / g_rowHeight);
	if (index < m_sequence.cameraTracks.size())
		return std::pair{true, index};
	if (index - m_sequence.cameraTracks.size() < m_sequence.eventTracks.size())
		return std::pair{false, index - m_sequence.cameraTracks.size()};
	return std::nullopt;
}

auto SequenceDocument::keyAt(const bool iCamera, const size_t iTrack, const float iX) const -> std::optional<size_t> {
	const size_t count =
			iCamera ? m_sequence.cameraTracks[iTrack].keys.size() : m_sequence.eventTracks[iTrack].keys.size();
	for (size_t key = count; key-- > 0;)
		if (std::abs(timeToX(keyTime(iCamera, iTrack, key)) - iX) <= g_keyRadius + 2.f)
			return key;
	return std::nullopt;
}

void SequenceDocument::pickKey(const bool iCamera, const size_t iTrack, const size_t iKey) {
	auto keys = m_selection.camera == iCamera && m_selection.track == iTrack ? m_selection.keys : std::vector<size_t>{};
	const bool picked = std::ranges::contains(keys, iKey);
	if (ImGui::GetIO().KeyCtrl) {
		if (picked)
			std::erase(keys, iKey);
		else
			keys.push_back(iKey);
	} else if (!picked) {
		keys = {iKey};
	}
	setSelection({.camera = iCamera, .track = iTrack, .keys = std::move(keys)});
	m_dragging = !m_selection.keys.empty();
	m_dragStart = xToTime(ImGui::GetIO().MousePos.x);
	m_dragOffset = 0.f;
	setTime(keyTime(iCamera, iTrack, iKey));
}

void SequenceDocument::handleTimelineClicks() {
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const auto at = rowAt(mouse.y);
	const bool onLanes = mouse.x > m_lanesX;
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		if (!at) {
			m_scrubbing = true;
		} else if (!onLanes) {
			setSelection({.camera = at->first, .track = at->second, .keys = {}});
		} else if (const auto key = keyAt(at->first, at->second, mouse.x); key) {
			pickKey(at->first, at->second, *key);
		} else {
			setSelection({.camera = at->first, .track = at->second, .keys = {}});
			m_scrubbing = true;
		}
	}
	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && onLanes && at && !keyAt(at->first, at->second, mouse.x)) {
		m_scrubbing = false;
		setSelection({.camera = at->first, .track = at->second, .keys = {}});
		setTime(snapped(xToTime(mouse.x), m_snap));
		std::ignore = addKeyAtPlayhead();
	}
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && at) {
		setSelection({.camera = at->first, .track = at->second, .keys = {}});
		if (onLanes)
			setTime(snapped(xToTime(mouse.x), m_snap));
		ImGui::OpenPopup("##trackMenu");
	}
}

void SequenceDocument::updateKeyDrag() {
	float earliest = std::numeric_limits<float>::max();
	for (const auto key: m_selection.keys)
		earliest = std::min(earliest, keyTime(m_selection.camera, m_selection.track, key));
	m_dragOffset = std::max(-earliest, snapped(xToTime(ImGui::GetIO().MousePos.x) - m_dragStart, m_snap));
	if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		return;
	m_dragging = false;
	const float offset = std::exchange(m_dragOffset, 0.f);
	if (std::abs(offset) <= 1e-4f)
		return;
	std::vector<float> targets;
	targets.reserve(m_selection.keys.size());
	for (const auto key: m_selection.keys)
		targets.push_back(keyTime(m_selection.camera, m_selection.track, key) + offset);
	auto args = trackArgs();
	args.set("keys", selectedKeysText());
	args.set("offset", static_cast<double>(offset));
	if (!sendCommand("sequence.move_keys", std::move(args)))
		return;
	std::vector<size_t> moved;
	const size_t count = m_selection.camera ? m_sequence.cameraTracks[m_selection.track].keys.size()
											: m_sequence.eventTracks[m_selection.track].keys.size();
	for (size_t key = 0; key < count; ++key)
		if (std::ranges::any_of(targets, [&](const float iTime) -> bool {
				return std::abs(keyTime(m_selection.camera, m_selection.track, key) - iTime) < 1e-4f;
			}))
			moved.push_back(key);
	setSelection({.camera = m_selection.camera, .track = m_selection.track, .keys = std::move(moved)});
}

void SequenceDocument::renderTrackMenu() {
	if (!ImGui::BeginPopup("##trackMenu"))
		return;
	const bool hasTrack =
			m_selection.track < (m_selection.camera ? m_sequence.cameraTracks.size() : m_sequence.eventTracks.size());
	if (ImGui::MenuItem("Add key at playhead", "K", false, hasTrack))
		std::ignore = addKeyAtPlayhead();
	if (hasTrack) {
		const bool muted = m_selection.camera ? m_sequence.cameraTracks[m_selection.track].muted
											  : m_sequence.eventTracks[m_selection.track].muted;
		if (ImGui::MenuItem(muted ? "Unmute" : "Mute")) {
			auto args = trackArgs();
			args.set("muted", !muted);
			std::ignore = sendCommand("sequence.set_track", std::move(args));
		}
		if (ImGui::MenuItem("Move up") && sendCommand("sequence.move_track", trackArgs()) && m_selection.track > 0)
			setSelection({.camera = m_selection.camera, .track = m_selection.track - 1, .keys = {}});
		if (ImGui::MenuItem("Move down")) {
			auto args = trackArgs();
			args.set("down", true);
			if (sendCommand("sequence.move_track", std::move(args)))
				setSelection({.camera = m_selection.camera, .track = m_selection.track + 1, .keys = {}});
		}
		if (ImGui::MenuItem("Remove track"))
			std::ignore = sendCommand("sequence.remove_track", trackArgs());
	}
	ImGui::EndPopup();
}

void SequenceDocument::renderTimelineRow(const bool iCamera, const size_t iTrack, const float iTop) {
	auto* draw = ImGui::GetWindowDrawList();
	const auto& colors = theme();
	const float left = ImGui::GetItemRectMin().x;
	const float right = ImGui::GetItemRectMax().x;
	const bool selectedTrack = m_selection.camera == iCamera && m_selection.track == iTrack;
	const std::string& name = iCamera ? m_sequence.cameraTracks[iTrack].name : m_sequence.eventTracks[iTrack].name;
	const bool muted = iCamera ? m_sequence.cameraTracks[iTrack].muted : m_sequence.eventTracks[iTrack].muted;
	if (selectedTrack)
		draw->AddRectFilled({left, iTop}, {right, iTop + g_rowHeight}, color(colors.getSelection()));
	draw->AddLine({left, iTop + g_rowHeight}, {right, iTop + g_rowHeight}, color(colors.lineSoft));
	const auto label = std::format("{} {}", iCamera ? gui::icon::video : gui::icon::zap, name);
	draw->PushClipRect({left, iTop}, {left + g_headerWidth - 4.f, iTop + g_rowHeight}, true);
	draw->AddText({left + 6.f, iTop + 4.f}, color(muted ? colors.textDisabled : colors.text), label.c_str());
	draw->PopClipRect();
	draw->PushClipRect({left + g_headerWidth, iTop}, {right, iTop + g_rowHeight}, true);
	const float middle = iTop + g_rowHeight * 0.5f;
	const auto shown = [&](const size_t iKey) -> float {
		const float time = keyTime(iCamera, iTrack, iKey);
		return selectedTrack && m_dragging && std::ranges::contains(m_selection.keys, iKey) ? time + m_dragOffset
																							: time;
	};
	const size_t count =
			iCamera ? m_sequence.cameraTracks[iTrack].keys.size() : m_sequence.eventTracks[iTrack].keys.size();
	if (iCamera && count > 0) {
		const auto live = m_sequence.sampleCamera(m_time);
		const bool active = live && live->track == iTrack;
		draw->AddRectFilled({timeToX(shown(0)), middle - 4.f}, {timeToX(shown(count - 1)), middle + 4.f},
							color(gui::Theme::withAlpha(trackColor(iTrack), active ? 0.55f : 0.25f)), 3.f);
	}
	for (size_t key = 0; key < count; ++key) {
		const float x = timeToX(shown(key));
		const bool picked = selectedTrack && std::ranges::contains(m_selection.keys, key);
		const ImU32 fill = color(keyColor(picked, muted));
		if (iCamera)
			draw->AddQuadFilled({x, middle - g_keyRadius}, {x + g_keyRadius, middle}, {x, middle + g_keyRadius},
								{x - g_keyRadius, middle}, fill);
		else
			draw->AddCircleFilled({x, middle}, g_keyRadius - 1.f, fill);
		if (!iCamera)
			draw->AddText({x + g_keyRadius + 2.f, iTop + 4.f}, color(colors.textMuted),
						  m_sequence.eventTracks[iTrack].keys[key].function.c_str());
	}
	draw->PopClipRect();
}

void SequenceDocument::renderSequenceSettings() {
	if (!gui::widgets::sectionHeader("Sequence", gui::icon::clapperboard, false).open)
		return;
	if (gui::widgets::beginProperties("##sequence")) {
		float duration = m_sequence.duration;
		if (gui::widgets::propertyFloat("Duration", duration, Sequence{}.duration, 0.05f, "%.2f s",
										"Length of the cutscene; the player stops or loops there.")) {
			std::ignore =
					sendCommand("sequence.set_duration",
								{{"duration", static_cast<double>(std::max(duration, 0.f))}, {"merge", m_typing}});
			m_typing = true;
		}
		const auto player = getPreviewPlayer();
		const auto* document = getPreviewDocument();
		gui::widgets::propertyLabel("Preview", "The scene and the Sequence Player the viewport previews on; Open on a "
											   "Sequence Player picks another one.");
		ImGui::TextUnformatted(document != nullptr && player
									   ? std::format("{} in {}", player.getName(), document->title()).c_str()
									   : "No open scene plays it");
		std::ignore = gui::widgets::propertyReset(false);
		if (player) {
			const auto& component = player.getComponent<scene::component::SequencePlayer>();
			gui::widgets::propertyLabel("Playback", "Set on the Sequence Player, in the inspector of the scene.");
			ImGui::TextDisabled("%s", std::format("{}{}, speed {:g}", component.playOnStart ? "On start" : "From Lua",
												  component.loop ? ", loop" : "", component.speed)
											  .c_str());
			std::ignore = gui::widgets::propertyReset(false);
		}
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

void SequenceDocument::renderTrackSettings() {
	const bool camera = m_selection.camera;
	const char* icon = camera ? "comp_camera" : "comp_lua_script";
	if (!gui::widgets::sectionHeader(camera ? "Camera Track" : "Event Track", icon, false).open)
		return;
	if (gui::widgets::beginProperties("##track")) {
		std::string name = camera ? m_sequence.cameraTracks[m_selection.track].name
								  : m_sequence.eventTracks[m_selection.track].name;
		if (gui::widgets::propertyText("Name", name, "Track name")) {
			auto args = trackArgs();
			args.set("name", name);
			args.set("merge", m_typing);
			m_typing = true;
			std::ignore = sendCommand("sequence.set_track", std::move(args));
		}
		bool muted = camera ? m_sequence.cameraTracks[m_selection.track].muted
							: m_sequence.eventTracks[m_selection.track].muted;
		if (gui::widgets::propertyBool("Muted", muted, false,
									   camera ? "A muted shot never drives the camera."
											  : "The functions of a muted track are not called.")) {
			auto args = trackArgs();
			args.set("muted", muted);
			std::ignore = sendCommand("sequence.set_track", std::move(args));
		}
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

void SequenceDocument::renderPropertiesPanel() {
	if (!ImGui::IsAnyItemActive())
		m_typing = false;
	gui::UiLayer::pushTextStyle(gui::TextStyle::Label);
	renderSequenceSettings();
	const bool hasTrack =
			m_selection.track < (m_selection.camera ? m_sequence.cameraTracks.size() : m_sequence.eventTracks.size());
	if (!hasTrack) {
		std::ignore = gui::widgets::emptyState(gui::icon::film, "No track",
											   "Add a camera or an event track from the Sequencer toolbar.");
	} else {
		renderTrackSettings();
		if (m_selection.keys.size() != 1)
			std::ignore = gui::widgets::emptyState(
					gui::icon::mousePointerClick, m_selection.keys.empty() ? "No key selected" : "Several keys",
					m_selection.keys.empty() ? "Click a key on the timeline to edit it here."
											 : "Pick one key to edit it; drag moves them all.");
		else if (m_selection.camera)
			renderCameraKey(m_sequence.cameraTracks[m_selection.track].keys[m_selection.keys.front()]);
		else
			renderEventKey(m_sequence.eventTracks[m_selection.track].keys[m_selection.keys.front()]);
	}
	gui::UiLayer::popTextStyle();
}

void SequenceDocument::renderCameraKey(const CameraKey& iKey) {
	if (!gui::widgets::sectionHeader("Camera Key", "comp_camera", false).open)
		return;
	const CameraKey defaults;
	if (gui::widgets::beginProperties("##cameraKey")) {
		float time = iKey.time;
		if (gui::widgets::propertyFloat("Time", time, iKey.time, 0.01f, "%.2f s", "When the camera reaches this pose."))
			setSelectedKey({{"time", static_cast<double>(std::max(time, 0.f))}});
		math::vec3 position = iKey.position;
		if (gui::widgets::propertyVec3("Position", position, defaults.position, 0.05f,
									   "In the space of the Sequence Player (of its parent when it drives its own "
									   "camera)."))
			setSelectedKey({{"position", position}});
		math::vec3 rotation = math::degrees(iKey.rotation);
		if (gui::widgets::propertyVec3("Rotation", rotation, math::degrees(defaults.rotation), 0.5f,
									   "Euler angles in degrees."))
			setSelectedKey({{"rotation", rotation}});
		float fov = math::degrees(iKey.fov);
		if (gui::widgets::propertyFloat("Field of View", fov, math::degrees(defaults.fov), 0.2f, "%.1f°",
										"Vertical field of view of a perspective camera."))
			setSelectedKey({{"fov", static_cast<double>(std::clamp(fov, 1.f, 179.f))}});
		float size = iKey.size;
		if (gui::widgets::propertyFloat("Size", size, defaults.size, 0.05f, "%.2f",
										"Visible height of an orthographic camera."))
			setSelectedKey({{"size", static_cast<double>(std::max(size, 0.01f))}});
		gui::widgets::propertyLabel("Easing", "How the camera moves to the next key: Smooth curve, Linear, or Step (a "
											  "cut).");
		if (gui::widgets::beginCombo("##easing", std::string{data::sequence::getEasingName(iKey.easing)}.c_str())) {
			for (const auto easing: data::sequence::getEasings()) {
				const std::string label{data::sequence::getEasingName(easing)};
				if (ImGui::Selectable(label.c_str(), easing == iKey.easing))
					setSelectedKey({{"easing", label}});
			}
			ImGui::EndCombo();
		}
		if (gui::widgets::propertyReset(iKey.easing != defaults.easing))
			setSelectedKey({{"easing", std::string{data::sequence::getEasingName(defaults.easing)}}});
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

void SequenceDocument::renderEventKey(const EventKey& iKey) {
	if (!gui::widgets::sectionHeader("Event Key", "comp_lua_script", false).open)
		return;
	if (gui::widgets::beginProperties("##eventKey")) {
		float time = iKey.time;
		if (gui::widgets::propertyFloat("Time", time, iKey.time, 0.01f, "%.2f s", "When the function is called."))
			setSelectedKey({{"time", static_cast<double>(std::max(time, 0.f))}});
		std::string function = iKey.function;
		if (gui::widgets::propertyText("Function", function, "Function name",
									   "Lua function called at this time, in the script of the playing entity, else "
									   "the scene script."))
			setSelectedKey({{"function", function}});
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

void SequenceDocument::setSelectedKey(commands::CommandArgs iArgs) {
	if (m_selection.keys.size() != 1)
		return;
	for (const auto& [name, value]: trackArgs().getValues()) iArgs.set(name, value);
	iArgs.set("key", static_cast<int64_t>(m_selection.keys.front()));
	iArgs.set("merge", m_typing);
	m_typing = true;
	const auto result = sendCommand("sequence.set_key", std::move(iArgs));
	if (!result)
		return;
	if (const auto found = result->data.find("key"); found != result->data.end())
		if (const auto* key = std::get_if<int64_t>(&found->second); key != nullptr)
			setSelection(
					{.camera = m_selection.camera, .track = m_selection.track, .keys = {static_cast<size_t>(*key)}});
}

void SequenceDocument::setPreview(const core::UUID iDocument, const core::UUID iPlayer) {
	if (iDocument == m_previewDocument && iPlayer == m_previewPlayer)
		return;
	m_previewDocument = iDocument;
	m_previewPlayer = iPlayer;
	m_previewChanged = true;
	if (iPlayer == core::UUID{0})
		m_lookThrough = false;
}

void SequenceDocument::resolvePreview(SceneDocument* iPreferred, const scene::Entity& iSelected) {
	if (mp_editorLayer == nullptr)
		return;
	if (iPreferred != nullptr && isPlayerOf(iSelected, m_path)) {
		setPreview(iPreferred->id(), iSelected.getUUID());
		return;
	}
	if (isPlayerOf(getPreviewPlayer(), m_path))
		return;
	const auto bind = [this](const SceneDocument* iDocument) -> bool {
		if (iDocument == nullptr || !iDocument->getActiveScene())
			return false;
		const auto player = findSequencePlayer(*iDocument->getActiveScene(), m_path);
		if (player)
			setPreview(iDocument->id(), player.getUUID());
		return static_cast<bool>(player);
	};
	if (bind(iPreferred))
		return;
	for (const auto& document: mp_editorLayer->getDocumentManager().list())
		if (document && document->type() == DocumentType::Scene &&
			bind(static_cast<const SceneDocument*>(document.get())))
			return;
	setPreview(core::UUID{0}, core::UUID{0});
}

auto SequenceDocument::getPreviewDocument() const -> SceneDocument* {
	if (mp_editorLayer == nullptr || m_previewDocument == core::UUID{0})
		return nullptr;
	auto* document = mp_editorLayer->getDocumentManager().find(m_previewDocument);
	if (document == nullptr || document->type() != DocumentType::Scene)
		return nullptr;
	return static_cast<SceneDocument*>(document);
}

auto SequenceDocument::getPreviewPlayer() const -> scene::Entity {
	const auto* document = getPreviewDocument();
	if (document == nullptr || !document->getActiveScene())
		return {};
	const auto player = document->getActiveScene()->findEntityByUUID(m_previewPlayer);
	if (!player || !player.hasComponent<scene::component::SequencePlayer>())
		return {};
	return player;
}

auto SequenceDocument::setKeyAtPlayhead(const math::vec3& iPosition, const math::vec3& iRotation) -> bool {
	if (m_sequence.cameraTracks.empty() && !addTrack(true))
		return false;
	if (!m_selection.camera || m_selection.track >= m_sequence.cameraTracks.size())
		setSelection({.camera = true, .track = m_sequence.cameraTracks.size() - 1, .keys = {}});
	const auto& keys = m_sequence.cameraTracks[m_selection.track].keys;
	const auto existing = std::ranges::find_if(
			keys, [this](const CameraKey& iKey) -> bool { return std::abs(iKey.time - m_time) < 1e-3f; });
	auto args = trackArgs();
	args.set("position", iPosition);
	args.set("rotation", math::degrees(iRotation));
	if (existing != keys.end()) {
		const auto index = static_cast<size_t>(std::distance(keys.begin(), existing));
		args.set("key", static_cast<int64_t>(index));
		if (!sendCommand("sequence.set_key", std::move(args)))
			return false;
		setSelection({.camera = true, .track = m_selection.track, .keys = {index}});
		return true;
	}
	args.set("time", static_cast<double>(m_time));
	const auto result = sendCommand("sequence.add_key", std::move(args));
	if (!result)
		return false;
	if (const auto found = result->data.find("key"); found != result->data.end())
		if (const auto* key = std::get_if<int64_t>(&found->second); key != nullptr)
			setSelection({.camera = true, .track = m_selection.track, .keys = {static_cast<size_t>(*key)}});
	return true;
}

}// namespace owl::nest
