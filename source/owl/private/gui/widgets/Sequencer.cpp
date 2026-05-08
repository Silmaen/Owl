/**
 * @file Sequencer.cpp
 * @author Silmaen
 * @date 27/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/widgets/Sequencer.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
OWL_DIAG_POP

#include <ImSequencer.h>

namespace owl::gui::widgets {

namespace {
/**
 * @brief
 *  Pack a `[0, 1]` RGBA colour into the IM_COL32 representation expected by ImSequencer.
 */
auto packColor(const math::vec4& iColor) -> uint32_t {
	const auto clamp01 = [](const float iV) -> float { return std::clamp(iV, 0.f, 1.f); };
	return IM_COL32(static_cast<int>(clamp01(iColor.x()) * 255.f), static_cast<int>(clamp01(iColor.y()) * 255.f),
					static_cast<int>(clamp01(iColor.z()) * 255.f), static_cast<int>(clamp01(iColor.w()) * 255.f));
}

/**
 * @brief
 *  Adapter exposing a `std::vector<SequencerEntry>` as an `ImSequencer::SequenceInterface`.
 *
 * ImSequencer hands back `int*` to start/end/colour so it can mutate them in place; we mirror
 * the entry data into stable side-by-side `int` vectors before each `Sequencer()` call and
 * sync changes back into the user's `SequencerEntry` vector when something was modified.
 */
class EntriesAdapter final : public ImSequencer::SequenceInterface {
public:
	EntriesAdapter(const EntriesAdapter&) = delete;

	EntriesAdapter(EntriesAdapter&&) = delete;

	auto operator=(const EntriesAdapter&) -> EntriesAdapter& = delete;

	auto operator=(EntriesAdapter&&) -> EntriesAdapter& = delete;

	~EntriesAdapter() override = default;

	EntriesAdapter(std::vector<SequencerEntry>& ioEntries, const SequencerOptions& iOpts)
		: m_entries{&ioEntries}, m_opts{iOpts} {
		const auto count = ioEntries.size();
		m_startBuf.resize(count);
		m_endBuf.resize(count);
		m_colorBuf.resize(count);
		m_typeBuf.resize(count, 0);
		for (size_t i = 0; i < count; ++i) {
			m_startBuf[i] = ioEntries[i].startFrame;
			m_endBuf[i] = ioEntries[i].endFrame;
			m_colorBuf[i] = packColor(ioEntries[i].color);
		}
	}

	[[nodiscard]] auto dirty() const -> bool { return m_dirty; }

	void syncBack() {
		auto& entries = *m_entries;
		for (size_t i = 0; i < entries.size(); ++i) {
			if (entries[i].startFrame != m_startBuf[i] || entries[i].endFrame != m_endBuf[i])
				m_dirty = true;
			entries[i].startFrame = m_startBuf[i];
			entries[i].endFrame = m_endBuf[i];
		}
	}

	[[nodiscard]] auto GetFrameMin() const -> int override { return m_opts.frameMin; }

	[[nodiscard]] auto GetFrameMax() const -> int override { return m_opts.frameMax; }

	[[nodiscard]] auto GetItemCount() const -> int override { return static_cast<int>(m_entries->size()); }

	[[nodiscard]] auto GetItemLabel(const int iIndex) const -> const char* override {
		if (iIndex < 0 || static_cast<size_t>(iIndex) >= m_entries->size())
			return "";
		return (*m_entries)[static_cast<size_t>(iIndex)].label.c_str();
	}

	void Get(const int iIndex, int** oStart, int** oEnd, int* oType, unsigned int* oColor) override {
		if (iIndex < 0 || static_cast<size_t>(iIndex) >= m_entries->size())
			return;
		const auto idx = static_cast<size_t>(iIndex);
		if (oStart != nullptr)
			*oStart = &m_startBuf[idx];
		if (oEnd != nullptr)
			*oEnd = &m_endBuf[idx];
		if (oType != nullptr)
			*oType = m_typeBuf[idx];
		if (oColor != nullptr)
			*oColor = m_colorBuf[idx];
	}

private:
	std::vector<SequencerEntry>* m_entries{nullptr};
	SequencerOptions m_opts;
	std::vector<int> m_startBuf;
	std::vector<int> m_endBuf;
	std::vector<int> m_typeBuf;
	std::vector<unsigned int> m_colorBuf;
	bool m_dirty{false};
};
}// namespace

auto sequencer(const char* iLabel, std::vector<SequencerEntry>& ioEntries, int32_t& ioCurrentFrame,
			   int32_t& ioFirstVisibleFrame, const SequencerOptions& iOpts, const math::vec2& iSize) -> bool {
	ImGui::PushID(iLabel);
	bool changed = false;

	int sequenceFlags = 0;
	if (iOpts.allowEditStartEnd)
		sequenceFlags |= ImSequencer::SEQUENCER_EDIT_STARTEND;
	if (iOpts.showCurrentFrame)
		sequenceFlags |= ImSequencer::SEQUENCER_CHANGE_FRAME;

	EntriesAdapter adapter(ioEntries, iOpts);

	int currentFrame = ioCurrentFrame;
	int firstFrame = ioFirstVisibleFrame;
	int selectedEntry = -1;
	bool expanded = true;

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const float widgetW = (iSize.x() > 0.f) ? iSize.x() : avail.x;
	const float widgetH = (iSize.y() > 0.f) ? iSize.y() : 200.f;

	ImGui::BeginChild("##owl_sequencer", ImVec2{widgetW, widgetH}, ImGuiChildFlags_Borders);
	ImSequencer::Sequencer(&adapter, &currentFrame, &expanded, &selectedEntry, &firstFrame, sequenceFlags);
	ImGui::EndChild();

	adapter.syncBack();
	if (adapter.dirty())
		changed = true;
	if (currentFrame != ioCurrentFrame) {
		ioCurrentFrame = currentFrame;
		changed = true;
	}
	if (firstFrame != ioFirstVisibleFrame)
		ioFirstVisibleFrame = firstFrame;

	ImGui::PopID();
	return changed;
}

}// namespace owl::gui::widgets
