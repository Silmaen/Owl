/**
 * @file SoundDocument.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SoundDocument.h"

#include "EditorLayer.h"

#include <data/assets/AssetSearchPaths.h>
#include <gui/IconFont.h>
#include <gui/Theme.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/AssetField.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Tooltip.h>
#include <gui/widgets/TreeRow.h>
#include <sound/SoundCommand.h>
#include <sound/SoundData.h>
#include <sound/SoundHelper.h>
#include <sound/Waveform.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <random>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest {

namespace {

constexpr size_t g_waveformColumns = 600;

// Every field of a take, as `sound.set_take` reads them.
auto getTakeArgs(const size_t iIndex, const sound::SoundVariant& iVariant) -> commands::CommandArgs {
	return {{"take", static_cast<int64_t>(iIndex)},
			{"file", iVariant.file},
			{"trim_start", static_cast<double>(iVariant.trimStart)},
			{"trim_end", static_cast<double>(iVariant.trimEnd)},
			{"loop_start", static_cast<double>(iVariant.loopStart)},
			{"loop_end", static_cast<double>(iVariant.loopEnd)},
			{"weight", static_cast<double>(iVariant.weight)}};
}
constexpr float g_waveformHeight = 140.f;
constexpr float g_markerPick = 6.f;
constexpr float g_minPart = 0.01f;

class ModifySoundCommand final : public UndoCommand<sound::SoundCue> {
public:
	ModifySoundCommand(sound::SoundCue iBefore, sound::SoundCue iAfter, std::string iDescription, const bool iMerge)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)},
		  m_merge{iMerge} {}

	void undo(sound::SoundCue& ioTarget) override { ioTarget = m_before; }

	void redo(sound::SoundCue& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return m_merge ? 1 : 0; }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto& other = static_cast<const ModifySoundCommand&>(iOther);
		if (other.m_description != m_description)
			return false;
		m_after = other.m_after;
		return true;
	}

private:
	sound::SoundCue m_before;
	sound::SoundCue m_after;
	std::string m_description;
	bool m_merge;
};

auto isAudioFile(const std::filesystem::path& iPath) -> bool {
	return std::ranges::contains(sound::SoundData::extension(), iPath.extension().string());
}

auto findOnDisk(const std::string& iFile) -> std::filesystem::path {
	if (const std::filesystem::path path{iFile}; path.is_absolute())
		return path;
	for (const auto& directory: data::assets::getAssetSearchPaths())
		if (auto candidate = directory / iFile; exists(candidate))
			return candidate;
	return {};
}

// The asset reference of a cue in the asset folders, as scenes and scripts name it.
auto toAssetName(const std::filesystem::path& iPath) -> std::string {
	for (const auto& directory: data::assets::getAssetSearchPaths())
		if (data::assets::isInDirectory(iPath, directory))
			return std::filesystem::relative(iPath, directory).generic_string();
	return iPath.generic_string();
}

auto sortedUnique(std::span<const size_t> iIndices, const size_t iCount) -> std::vector<size_t> {
	std::vector<size_t> sorted;
	for (const auto index: iIndices)
		if (index < iCount)
			sorted.push_back(index);
	std::ranges::sort(sorted);
	const auto [first, last] = std::ranges::unique(sorted);
	sorted.erase(first, last);
	return sorted;
}

void setMarker(sound::SoundVariant& ioVariant, const SoundMarker iMarker, const float iSeconds) {
	switch (iMarker) {
		case SoundMarker::TrimStart:
			ioVariant.trimStart = iSeconds;
			break;
		case SoundMarker::TrimEnd:
			ioVariant.trimEnd = iSeconds;
			break;
		case SoundMarker::LoopStart:
			ioVariant.loopStart = iSeconds;
			break;
		case SoundMarker::LoopEnd:
			ioVariant.loopEnd = iSeconds;
			break;
	}
}

}// namespace

auto orderMarkers(sound::SoundVariant iVariant, const float iDuration) -> sound::SoundVariant {
	const float limit = iDuration > 0.f ? iDuration : std::numeric_limits<float>::max();
	iVariant.trimStart = std::clamp(iVariant.trimStart, 0.f, limit);
	if (iVariant.trimEnd > 0.f)
		iVariant.trimEnd = std::clamp(iVariant.trimEnd, std::min(iVariant.trimStart + g_minPart, limit), limit);
	const float end = iVariant.trimEnd > 0.f ? iVariant.trimEnd : limit;
	if (iVariant.loopStart > 0.f || iVariant.loopEnd > 0.f) {
		iVariant.loopStart = std::clamp(iVariant.loopStart, iVariant.trimStart, end);
		if (iVariant.loopEnd > 0.f)
			iVariant.loopEnd = std::clamp(iVariant.loopEnd, std::min(iVariant.loopStart + g_minPart, end), end);
	}
	iVariant.weight = std::max(0.f, iVariant.weight);
	return iVariant;
}

SoundDocument::SoundDocument() { m_undo.markSaved(); }

SoundDocument::~SoundDocument() {
	stop();
	m_previewCache.clear();
}

auto SoundDocument::title() const -> std::string {
	return m_path.empty() ? std::string{"Untitled"} : m_path.filename().string();
}

void SoundDocument::onAttach(EditorLayer* iEditor) { mp_editorLayer = iEditor; }

void SoundDocument::onDetach() {
	stop();
	m_previewCache.clear();
	mp_editorLayer = nullptr;
}

void SoundDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {
	commitPreview();
	// A finished source still holds its buffer: delete it before the sound data goes.
	if (m_previewHandle != sound::invalidSoundHandle && !isPlaying())
		stop();
}

void SoundDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto SoundDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto loaded = sound::SoundCue::load(iPath);
	if (!loaded) {
		OWL_CORE_WARN("SoundDocument: {} is not a sound cue.", iPath.string())
		return false;
	}
	m_cue = std::move(*loaded);
	m_path = iPath;
	m_undo.clear();
	m_undo.markSaved();
	setSelection(m_cue.variants.empty() ? std::vector<size_t>{} : std::vector<size_t>{0});
	return true;
}

void SoundDocument::startFromAudio(const std::filesystem::path& iAudioFile) {
	m_cue = {.variants = {{.file = toAssetName(iAudioFile),
						   .trimStart = 0.f,
						   .trimEnd = 0.f,
						   .loopStart = 0.f,
						   .loopEnd = 0.f,
						   .weight = 1.f}},
			 .volume = 1.f,
			 .volumeJitter = 0.f,
			 .pitch = 1.f,
			 .pitchJitter = 0.f,
			 .loop = false};
	m_path = std::filesystem::path{iAudioFile}.replace_extension(sound::SoundCue::fileExtension());
	m_undo.clear();
	m_undo.markUnsaved();
	setSelection({0});
}

auto SoundDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto SoundDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (!m_cue.saveToFile(iPath)) {
		OWL_CORE_ERROR("SoundDocument: Failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_undo.markSaved();
	sound::SoundHelper::forgetCue(toAssetName(iPath));
	return true;
}

auto SoundDocument::recoverySnapshot() const -> std::optional<std::string> { return m_cue.serializeToString(); }

auto SoundDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	auto cue = sound::SoundCue::deserializeFromString(iSnapshot);
	if (!cue) {
		OWL_WARN("Recovery: Cannot restore the autosave of sound '{}'.", title())
		return false;
	}
	m_cue = std::move(*cue);
	m_undo.clear();
	m_undo.markUnsaved();
	setSelection(m_selection);
	return true;
}

void SoundDocument::performUndo() {
	cancelPreview();
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_cue);
	setSelection(m_selection);
}

void SoundDocument::performRedo() {
	cancelPreview();
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_cue);
	setSelection(m_selection);
}

auto SoundDocument::commit(sound::SoundCue iNext, const std::string& iDescription, const bool iMerge) -> bool {
	if (iNext == m_cue)
		return false;
	auto before = std::move(m_cue);
	m_cue = iNext;
	m_undo.push(mkUniq<ModifySoundCommand>(std::move(before), std::move(iNext), iDescription, iMerge));
	setSelection(m_selection);
	return true;
}

void SoundDocument::runCommand(const std::string_view iName, commands::CommandArgs iArgs) {
	commitPreview(true);
	sendCommand(iName, std::move(iArgs));
}

void SoundDocument::sendCommand(const std::string_view iName, commands::CommandArgs iArgs) const {
	if (mp_editorLayer == nullptr) {
		OWL_WARN("SoundDocument: {} ignored, no editor to run it.", iName)
		return;
	}
	iArgs.set("path", m_path.generic_string());
	std::ignore = mp_editorLayer->execute(iName, iArgs, commands::CommandSource::Gesture);
}

void SoundDocument::preview(sound::SoundCue iNext) {
	if (!m_previewBefore.has_value())
		m_previewBefore = m_cue;
	m_cue = std::move(iNext);
}

void SoundDocument::cancelPreview() {
	if (!m_previewBefore.has_value())
		return;
	m_cue = std::move(*m_previewBefore);
	m_previewBefore.reset();
}

void SoundDocument::commitPreview(const bool iForce) {
	if (!m_previewBefore.has_value() || (!iForce && ImGui::IsAnyItemActive()))
		return;
	const auto edited = std::move(m_cue);
	cancelPreview();
	auto before = m_cue;
	before.variants.clear();
	auto after = edited;
	after.variants.clear();
	if (after != before)
		sendCommand("sound.set", {{"volume", static_cast<double>(edited.volume)},
								  {"volume_change", static_cast<double>(edited.volumeJitter)},
								  {"pitch", static_cast<double>(edited.pitch)},
								  {"pitch_change", static_cast<double>(edited.pitchJitter)},
								  {"loop", edited.loop}});
	for (size_t i = 0; i < std::min(edited.variants.size(), m_cue.variants.size()); ++i) {
		if (edited.variants[i] != m_cue.variants[i])
			sendCommand("sound.set_take", getTakeArgs(i, edited.variants[i]));
	}
}

auto SoundDocument::setProperties(const sound::SoundCue& iCue, const bool iMerge) -> bool {
	auto next = m_cue;
	next.volume = std::max(0.f, iCue.volume);
	next.volumeJitter = std::clamp(iCue.volumeJitter, 0.f, 1.f);
	next.pitch = std::max(0.01f, iCue.pitch);
	next.pitchJitter = std::clamp(iCue.pitchJitter, 0.f, 0.9f);
	next.loop = iCue.loop;
	return commit(std::move(next), "Edit sound", iMerge);
}

auto SoundDocument::addVariant(const std::string& iFile) -> bool {
	if (iFile.empty() || !isAudioFile(iFile)) {
		OWL_CORE_WARN("SoundDocument: {} is not an audio file.", iFile)
		return false;
	}
	auto next = m_cue;
	next.variants.push_back(
			{.file = iFile, .trimStart = 0.f, .trimEnd = 0.f, .loopStart = 0.f, .loopEnd = 0.f, .weight = 1.f});
	const auto added = next.variants.size() - 1;
	if (!commit(std::move(next), "Add take", false))
		return false;
	setSelection({added});
	return true;
}

auto SoundDocument::setVariant(const size_t iIndex, const sound::SoundVariant& iVariant, const bool iMerge) -> bool {
	if (iIndex >= m_cue.variants.size()) {
		OWL_CORE_WARN("SoundDocument: No take {}.", iIndex)
		return false;
	}
	if (!iVariant.file.empty() && !isAudioFile(iVariant.file)) {
		OWL_CORE_WARN("SoundDocument: {} is not an audio file.", iVariant.file)
		return false;
	}
	auto next = m_cue;
	next.variants[iIndex] = orderMarkers(iVariant, getWaveform(iVariant.file).duration);
	return commit(std::move(next), "Edit take", iMerge);
}

auto SoundDocument::removeVariants(const std::span<const size_t> iIndices) -> bool {
	const auto indices = sortedUnique(iIndices, m_cue.variants.size());
	if (indices.empty())
		return false;
	auto next = m_cue;
	for (const auto index: indices | std::views::reverse)
		next.variants.erase(next.variants.begin() + static_cast<std::ptrdiff_t>(index));
	if (!commit(std::move(next), indices.size() > 1 ? "Remove takes" : "Remove take", false))
		return false;
	setSelection(m_cue.variants.empty() ? std::vector<size_t>{}
										: std::vector{std::min(indices.front(), m_cue.variants.size() - 1)});
	return true;
}

auto SoundDocument::duplicateVariants(const std::span<const size_t> iIndices) -> bool {
	const auto indices = sortedUnique(iIndices, m_cue.variants.size());
	if (indices.empty())
		return false;
	auto next = m_cue;
	const auto insertAt = indices.back() + 1;
	std::vector<sound::SoundVariant> copies;
	copies.reserve(indices.size());
	for (const auto index: indices) copies.push_back(m_cue.variants[index]);
	next.variants.insert(next.variants.begin() + static_cast<std::ptrdiff_t>(insertAt), copies.begin(), copies.end());
	if (!commit(std::move(next), "Duplicate takes", false))
		return false;
	std::vector<size_t> selection(copies.size());
	for (size_t i = 0; i < copies.size(); ++i) selection[i] = insertAt + i;
	setSelection(std::move(selection));
	return true;
}

auto SoundDocument::moveVariants(const std::span<const size_t> iIndices, const bool iDown) -> bool {
	const auto indices = sortedUnique(iIndices, m_cue.variants.size());
	if (indices.empty() || (iDown ? indices.back() + 1 >= m_cue.variants.size() : indices.front() == 0))
		return false;
	auto next = m_cue;
	if (iDown) {
		for (const auto index: indices | std::views::reverse) std::swap(next.variants[index], next.variants[index + 1]);
	} else {
		for (const auto index: indices) std::swap(next.variants[index], next.variants[index - 1]);
	}
	if (!commit(std::move(next), "Move takes", false))
		return false;
	std::vector<size_t> selection;
	selection.reserve(indices.size());
	for (const auto index: indices) selection.push_back(iDown ? index + 1 : index - 1);
	setSelection(std::move(selection));
	return true;
}

void SoundDocument::setSelection(std::vector<size_t> iIndices) {
	m_selection = sortedUnique(iIndices, m_cue.variants.size());
}

void SoundDocument::play(const sound::SoundVariant& iVariant, const float iVolume, const float iPitch) {
	stop();
	if (sound::SoundCommand::getState() != sound::SoundAPI::State::Ready) {
		OWL_CORE_WARN("SoundDocument: The sound system is not ready, nothing plays.")
		return;
	}
	auto& cached = m_previewCache[sound::getVariantKey(iVariant)];
	if (!cached)
		cached = sound::SoundData::create(sound::getVariantSpecification(iVariant));
	m_previewData = cached;
	if (!m_previewData)
		return;
	m_previewHandle = sound::SoundCommand::play(m_previewData, {.volume = iVolume,
																.pitch = iPitch,
																.loop = m_cue.loop,
																.spatial = false,
																.position = {0.f, 0.f, 0.f},
																.maxDistance = 50.f,
																.rolloff = 1.f});
}

void SoundDocument::playCue() {
	static std::mt19937_64 s_engine{std::random_device{}()};
	if (const auto pick = m_cue.pick(s_engine()); pick.has_value()) {
		setSelection({pick->variant});
		play(m_cue.variants[pick->variant], pick->volume, pick->pitch);
	}
}

void SoundDocument::playVariant(const size_t iIndex) {
	if (iIndex < m_cue.variants.size())
		play(m_cue.variants[iIndex], m_cue.volume, m_cue.pitch);
}

void SoundDocument::stop() {
	if (m_previewHandle != sound::invalidSoundHandle &&
		sound::SoundCommand::getState() == sound::SoundAPI::State::Ready)
		sound::SoundCommand::stop(m_previewHandle);
	m_previewHandle = sound::invalidSoundHandle;
	m_previewData.reset();
}

auto SoundDocument::isPlaying() const -> bool {
	return m_previewHandle != sound::invalidSoundHandle &&
		   sound::SoundCommand::getState() == sound::SoundAPI::State::Ready &&
		   sound::SoundCommand::isPlaying(m_previewHandle);
}

auto SoundDocument::getWaveform(const std::string& iFile) -> const WaveformView& {
	if (const auto found = m_waveforms.find(iFile); found != m_waveforms.end())
		return found->second;
	WaveformView view;
	if (const auto path = findOnDisk(iFile); !path.empty()) {
		view.peaks = sound::readWaveform(path, g_waveformColumns);
		view.duration = sound::readSoundDuration(path).value_or(0.f);
	}
	return m_waveforms.emplace(iFile, std::move(view)).first->second;
}

void SoundDocument::onImGuiRender() {
	const auto winTitle = documentWindowTitle(DocumentType::Sound, title(), static_cast<uint64_t>(id()));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;
	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);
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
		const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (windowFocused && !m_wasFocused && mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
		m_wasFocused = windowFocused;
		const auto& io = ImGui::GetIO();
		if (windowFocused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
			std::ignore = save();
		if (windowFocused && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
			if (isPlaying())
				stop();
			else
				playCue();
		}
		renderVariantEditor();
	}
	ImGui::End();
}

void SoundDocument::renderPropertiesPanel() {
	if (!gui::widgets::sectionHeader("Sound cue", "comp_sound", false).open)
		return;
	const bool playing = isPlaying();
	if (gui::widgets::button("##playCue", {.glyph = playing ? gui::icon::square : gui::icon::play,
										   .label = playing ? "Stop" : "Play",
										   .tooltip = playing ? "Stop the preview" : "Play the cue",
										   .shortcut = "Space",
										   .description = "A random take, with a random volume and pitch.",
										   .primary = !playing,
										   .disabled = m_cue.variants.empty()})) {
		if (playing)
			stop();
		else
			playCue();
	}
	if (gui::widgets::beginProperties("##cueProperties")) {
		auto next = m_cue;
		bool changed = gui::widgets::propertyFloat("Volume", next.volume, 1.f, 0.01f, "%.2f",
												   "Base volume multiplier of every take.");
		float volumeJitter = next.volumeJitter * 100.f;
		if (gui::widgets::propertyFloat("Volume change", volumeJitter, 0.f, 0.5f, "%.0f %%",
										"Random change of the volume at each play, in percent.")) {
			next.volumeJitter = volumeJitter / 100.f;
			changed = true;
		}
		changed |= gui::widgets::propertyFloat("Pitch", next.pitch, 1.f, 0.01f, "%.2f",
											   "Base pitch multiplier of every take.");
		float pitchJitter = next.pitchJitter * 100.f;
		if (gui::widgets::propertyFloat("Pitch change", pitchJitter, 0.f, 0.5f, "%.0f %%",
										"Random change of the pitch at each play, in percent.")) {
			next.pitchJitter = pitchJitter / 100.f;
			changed = true;
		}
		changed |= gui::widgets::propertyBool("Loop", next.loop, false,
											  "Loop the take, between its loop points when it has some.");
		gui::widgets::endProperties();
		if (changed)
			preview(std::move(next));
	}
	gui::widgets::endSection();
}

void SoundDocument::renderVariantToolbar() {
	const bool none = m_selection.empty();
	if (gui::widgets::iconButton("##upTakes", {.glyph = gui::icon::arrowUp,
											   .tooltip = "Move up",
											   .description = "Move the selected takes one row up.",
											   .disabled = none}))
		runCommand("sound.move_takes", {{"takes", commands::formatIndices(m_selection)}, {"down", false}});
	ImGui::SameLine();
	if (gui::widgets::iconButton("##downTakes", {.glyph = gui::icon::arrowDown,
												 .tooltip = "Move down",
												 .description = "Move the selected takes one row down.",
												 .disabled = none}))
		runCommand("sound.move_takes", {{"takes", commands::formatIndices(m_selection)}, {"down", true}});
	ImGui::SameLine();
	if (gui::widgets::iconButton("##copyTakes", {.glyph = gui::icon::copy,
												 .tooltip = "Duplicate",
												 .shortcut = "Ctrl+D",
												 .description = "Copy the selected takes.",
												 .disabled = none}))
		runCommand("sound.duplicate_takes", {{"takes", commands::formatIndices(m_selection)}});
	ImGui::SameLine();
	if (gui::widgets::iconButton("##removeTakes", {.glyph = gui::icon::trash2,
												   .tooltip = "Remove",
												   .shortcut = "Delete",
												   .description = "Remove the selected takes.",
												   .disabled = none}))
		runCommand("sound.remove_takes", {{"takes", commands::formatIndices(m_selection)}});
}

void SoundDocument::clickVariant(const size_t iIndex) {
	const auto& io = ImGui::GetIO();
	const bool selected = std::ranges::contains(m_selection, iIndex);
	if (io.KeyCtrl) {
		auto selection = m_selection;
		if (selected)
			std::erase(selection, iIndex);
		else
			selection.push_back(iIndex);
		setSelection(std::move(selection));
	} else if (io.KeyShift && !m_selection.empty()) {
		std::vector<size_t> range;
		for (size_t j = std::min(m_selection.front(), iIndex); j <= std::max(m_selection.back(), iIndex); ++j)
			range.push_back(j);
		setSelection(std::move(range));
	} else {
		setSelection({iIndex});
	}
	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		playVariant(iIndex);
}

void SoundDocument::renderHierarchyPanel() {
	if (!gui::widgets::sectionHeader("Takes", "wav_icon", false).open)
		return;
	renderVariantToolbar();
	const bool listFocused = ImGui::IsWindowFocused();
	bool openMenu = false;
	for (size_t i = 0; i < m_cue.variants.size(); ++i) {
		const auto& variant = m_cue.variants[i];
		const auto name = variant.file.empty() ? std::string{"(no file)"}
											   : std::filesystem::path{variant.file}.filename().string();
		const auto weight = std::format("x{:.2f}", static_cast<double>(variant.weight));
		const auto row = gui::widgets::treeRow(std::format("##take{}", i).c_str(),
											   {.label = name.c_str(),
												.glyph = gui::icon::audioLines,
												.trailing = weight.c_str(),
												.selected = std::ranges::contains(m_selection, i)});
		if (row.clicked)
			clickVariant(i);
		if (row.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			if (!std::ranges::contains(m_selection, i))
				setSelection({i});
			openMenu = true;
		}
		if (row.hovered)
			gui::widgets::itemTooltip(variant.file.c_str());
	}
	if (openMenu)
		ImGui::OpenPopup("##takeMenu");
	if (ImGui::BeginPopup("##takeMenu")) {
		gui::widgets::menuHeader(m_selection.size() > 1 ? "Takes" : "Take", "Sound cue");
		if (gui::widgets::menuItem({.glyph = gui::icon::play, .label = "Play", .enabled = !m_selection.empty()}))
			playVariant(m_selection.front());
		if (gui::widgets::menuItem({.glyph = gui::icon::copy, .label = "Duplicate", .shortcut = "Ctrl+D"}))
			runCommand("sound.duplicate_takes", {{"takes", commands::formatIndices(m_selection)}});
		if (gui::widgets::menuItem(
					{.glyph = gui::icon::trash2, .label = "Remove", .shortcut = "Delete", .danger = true}))
			runCommand("sound.remove_takes", {{"takes", commands::formatIndices(m_selection)}});
		ImGui::EndPopup();
	}
	if (listFocused && !ImGui::GetIO().WantTextInput) {
		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			runCommand("sound.remove_takes", {{"takes", commands::formatIndices(m_selection)}});
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
			runCommand("sound.duplicate_takes", {{"takes", commands::formatIndices(m_selection)}});
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
			std::vector<size_t> all(m_cue.variants.size());
			for (size_t i = 0; i < all.size(); ++i) all[i] = i;
			setSelection(std::move(all));
		}
	}
	if (m_cue.variants.empty())
		std::ignore = gui::widgets::emptyState(gui::icon::audioLines, "No take",
											   "Drop audio files from the Content Browser here.");
	ImGui::Dummy({ImGui::GetContentRegionAvail().x,
				  std::max(ImGui::GetFrameHeight() * 2.f, ImGui::GetContentRegionAvail().y)});
	if (std::filesystem::path dropped;
		gui::widgets::assetDropTarget(gui::widgets::AssetKind::Sound, dropped) && !sound::SoundCue::isCue(dropped))
		runCommand("sound.add_take", {{"file", dropped.generic_string()}});
	gui::widgets::endSection();
}

void SoundDocument::renderVariantEditor() {
	if (m_selection.empty()) {
		std::ignore =
				gui::widgets::emptyState(gui::icon::audioLines, m_cue.variants.empty() ? "No take" : "No take selected",
										 m_cue.variants.empty() ? "Drop an audio file on the Takes panel to start."
																: "Select a take to see its waveform.");
		return;
	}
	const auto index = m_selection.front();
	auto variant = m_cue.variants[index];
	const auto name = std::filesystem::path{variant.file}.filename().string();
	if (!gui::widgets::sectionHeader(name.c_str(), "wav_icon", false).open)
		return;
	const bool playing = isPlaying();
	if (gui::widgets::button("##playTake", {.glyph = playing ? gui::icon::square : gui::icon::play,
											.label = playing ? "Stop" : "Play take",
											.tooltip = playing ? "Stop the preview" : "Play the take",
											.description = "The trimmed part, with the base volume and pitch."}))
		playing ? stop() : playVariant(index);
	ImGui::SameLine();
	ImGui::TextDisabled("%s", variant.file.c_str());
	if (std::filesystem::path dropped;
		gui::widgets::assetDropTarget(gui::widgets::AssetKind::Sound, dropped) && !sound::SoundCue::isCue(dropped)) {
		runCommand("sound.set_take", {{"take", static_cast<int64_t>(index)}, {"file", dropped.generic_string()}});
		gui::widgets::endSection();
		return;
	}
	gui::widgets::itemTooltip("Drop another audio file here to replace the file of the take.");
	const auto& view = getWaveform(variant.file);
	renderWaveform(index, view);
	if (gui::widgets::beginProperties("##takeProperties")) {
		auto next = m_cue.variants[index];
		bool changed = gui::widgets::propertyFloat("Trim start", next.trimStart, 0.f, 0.005f, "%.3f s",
												   "First played second.");
		changed |= gui::widgets::propertyFloat("Trim end", next.trimEnd, 0.f, 0.005f,
											   next.trimEnd > 0.f ? "%.3f s" : "end",
											   "Last played second; 0 plays to the end of the file.");
		changed |= gui::widgets::propertyFloat("Loop start", next.loopStart, 0.f, 0.005f, "%.3f s",
											   "Start of the part a looping cue repeats.");
		changed |= gui::widgets::propertyFloat("Loop end", next.loopEnd, 0.f, 0.005f,
											   next.loopEnd > 0.f ? "%.3f s" : "end",
											   "End of the repeated part; both loop points at 0 loop the whole take.");
		changed |= gui::widgets::propertyFloat("Weight", next.weight, 1.f, 0.05f, "%.2f",
											   "Relative chance of this take being picked.");
		gui::widgets::endProperties();
		if (changed) {
			auto cue = m_cue;
			cue.variants[index] = next;
			preview(std::move(cue));
		}
	}
	if (view.duration > 0.f)
		ImGui::TextDisabled("Length %.3f s. Drag the markers on the waveform; double-click one to reset it.",
							static_cast<double>(view.duration));
	gui::widgets::endSection();
}

void SoundDocument::renderWaveform(const size_t iIndex, const WaveformView& iView) {
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 size{std::max(ImGui::GetContentRegionAvail().x, 50.f), g_waveformHeight};
	ImGui::InvisibleButton("##waveform", size);
	auto* draw = ImGui::GetWindowDrawList();
	const auto& theme = gui::UiLayer::getTheme();
	draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, ImGui::GetColorU32(gui::vec(theme.well)));
	if (iView.duration <= 0.f || iView.peaks.empty()) {
		draw->AddText({origin.x + 8.f, origin.y + 8.f}, ImGui::GetColorU32(gui::vec(theme.textMuted)),
					  "The file cannot be read.");
		return;
	}
	const auto& variant = m_cue.variants[iIndex];
	const auto toX = [&](const float iSeconds) -> float { return origin.x + iSeconds / iView.duration * size.x; };
	const float middle = origin.y + size.y * 0.5f;
	const float end = variant.trimEnd > 0.f ? variant.trimEnd : iView.duration;
	const auto wave = ImGui::GetColorU32(gui::vec(theme.accent));
	const auto muted = ImGui::GetColorU32(gui::vec(theme.textMuted));
	const float column = size.x / static_cast<float>(iView.peaks.size());
	for (size_t i = 0; i < iView.peaks.size(); ++i) {
		const float x = origin.x + static_cast<float>(i) * column;
		const float seconds = static_cast<float>(i) / static_cast<float>(iView.peaks.size()) * iView.duration;
		const float half = std::max(0.5f, iView.peaks[i] * size.y * 0.48f);
		draw->AddRectFilled({x, middle - half}, {x + std::max(1.f, column - 0.5f), middle + half},
							seconds >= variant.trimStart && seconds <= end ? wave : muted);
	}
	const bool looped = variant.loopStart > 0.f || variant.loopEnd > 0.f;
	const float loopEnd = variant.loopEnd > 0.f ? variant.loopEnd : end;
	if (looped)
		draw->AddRectFilled({toX(variant.loopStart), origin.y}, {toX(loopEnd), origin.y + size.y},
							ImGui::GetColorU32(gui::vec(gui::Theme::withAlpha(theme.success, 0.15f))));
	const std::array markers{std::pair{SoundMarker::TrimStart, variant.trimStart}, std::pair{SoundMarker::TrimEnd, end},
							 std::pair{SoundMarker::LoopStart, looped ? variant.loopStart : variant.trimStart},
							 std::pair{SoundMarker::LoopEnd, loopEnd}};
	const float mouseX = ImGui::GetIO().MousePos.x;
	std::optional<SoundMarker> hovered;
	float best = g_markerPick;
	for (const auto& [marker, seconds]: markers) {
		const bool loop = marker == SoundMarker::LoopStart || marker == SoundMarker::LoopEnd;
		const auto color = ImGui::GetColorU32(
				gui::vec(gui::Theme::withAlpha(loop ? theme.success : theme.text, loop && !looped ? 0.35f : 1.f)));
		const float x = toX(seconds);
		const float top = loop ? origin.y + size.y * 0.5f : origin.y;
		draw->AddLine({x, top}, {x, origin.y + size.y}, color, 2.f);
		draw->AddTriangleFilled({x - 5.f, top}, {x + 5.f, top}, {x, top + 7.f}, color);
		if (ImGui::IsItemHovered() && std::abs(mouseX - x) < best) {
			best = std::abs(mouseX - x);
			hovered = marker;
		}
	}
	if (hovered.has_value() || m_draggedMarker.has_value())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	if (ImGui::IsItemActivated())
		m_draggedMarker = hovered;
	if (hovered.has_value() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
		auto next = variant;
		setMarker(next, *hovered, 0.f);
		runCommand("sound.set_take", getTakeArgs(iIndex, next));
		m_draggedMarker.reset();
		return;
	}
	if (m_draggedMarker.has_value() && ImGui::IsItemActive()) {
		const float seconds = std::clamp((mouseX - origin.x) / size.x * iView.duration, 0.f, iView.duration);
		auto next = variant;
		setMarker(next, *m_draggedMarker, seconds);
		auto cue = m_cue;
		cue.variants[iIndex] = next;
		preview(std::move(cue));
	}
	if (!ImGui::IsItemActive())
		m_draggedMarker.reset();
	if (ImGui::IsItemHovered() && !m_draggedMarker.has_value() && hovered.has_value())
		gui::widgets::itemTooltip("Drag to move, double-click to reset.");
}

}// namespace owl::nest
