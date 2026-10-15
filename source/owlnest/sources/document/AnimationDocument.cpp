/**
 * @file AnimationDocument.cpp
 * @author Silmaen
 * @date 27/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AnimationDocument.h"

#include "EditorLayer.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/AssetField.h>
#include <gui/widgets/CurveEditor.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Tooltip.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <cstdint>
#include <format>
#include <tuple>
#include <vector>

namespace owl::nest {

namespace {
constexpr float g_previewSize = 220.f;
constexpr float g_timelineHeight = 110.f;

}// namespace

AnimationDocument::AnimationDocument() = default;

AnimationDocument::~AnimationDocument() = default;

auto AnimationDocument::title() const -> std::string {
	if (!m_path.empty())
		return m_path.stem().string();
	return "Untitled";
}

auto AnimationDocument::isDirty() const -> bool {
	return m_clip.serializeToString(m_path.empty() ? std::string{"untitled"} : m_path.stem().string()) !=
		   m_savedSnapshot;
}

auto AnimationDocument::recoverySnapshot() const -> std::optional<std::string> {
	return m_clip.serializeToString(m_path.empty() ? std::string{"untitled"} : m_path.stem().string());
}

auto AnimationDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	if (!m_clip.deserializeFromString(iSnapshot)) {
		OWL_WARN("Recovery: Cannot restore the autosave of animation '{}'.", title())
		return false;
	}
	return true;
}

void AnimationDocument::onAttach(EditorLayer* iEditor) {
	mp_editorLayer = iEditor;
	refreshSavedSnapshot();
	m_currentFrame = m_clip.firstFrame;
}

void AnimationDocument::onDetach() { mp_editorLayer = nullptr; }

void AnimationDocument::onUpdate(const core::Timestep& iTimeStep) { advancePlayback(iTimeStep.getSeconds()); }

void AnimationDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {
	// ImGui consumes its own events; nothing to dispatch at the document level.
}

void AnimationDocument::onImGuiRender() {
	const auto winTitle = documentWindowTitle(DocumentType::Animation, title(), static_cast<uint64_t>(id()));

	ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
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
	if (wantFocus)
		ImGui::SetWindowFocus();
	if (open) {
		if (m_currentFrame < m_clip.firstFrame || m_currentFrame > m_clip.lastFrame)
			m_currentFrame = m_clip.firstFrame;

		const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (windowFocused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
			std::ignore = save();

		const auto avail = ImGui::GetContentRegionAvail();
		// The timeline section: its header, the sequencer and the section frame, kept whole below the panels.
		const float timelineHeight = g_timelineHeight + 2.f * ImGui::GetFrameHeightWithSpacing();
		const float topHeight = std::max(180.f, avail.y - timelineHeight - ImGui::GetStyle().ItemSpacing.y);
		const float leftWidth = std::max(g_previewSize + 16.f, avail.x * 0.45f);

		ImGui::BeginChild("##animation_preview", ImVec2{leftWidth, topHeight}, ImGuiChildFlags_Borders);
		renderPreview();
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##animation_props", ImVec2{0.f, topHeight}, ImGuiChildFlags_Borders);
		renderProperties();
		ImGui::EndChild();

		ImGui::BeginChild("##animation_timeline", ImVec2{0.f, 0.f}, ImGuiChildFlags_None,
						  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		renderTimeline();
		ImGui::EndChild();
	}
	ImGui::End();
}

auto AnimationDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto AnimationDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	const auto displayName = iPath.stem().string();
	if (!m_clip.saveToFile(iPath, displayName)) {
		OWL_CORE_ERROR("AnimationDocument: failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	refreshSavedSnapshot();
	return true;
}

auto AnimationDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	if (!m_clip.loadFromFile(iPath)) {
		OWL_CORE_ERROR("AnimationDocument: failed to load '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	refreshSavedSnapshot();
	m_currentFrame = m_clip.firstFrame;
	m_elapsedTime = 0.f;
	m_playing = true;
	return true;
}

void AnimationDocument::stop() {
	m_playing = false;
	m_currentFrame = m_clip.firstFrame;
	m_elapsedTime = 0.f;
}

void AnimationDocument::stepNext() {
	m_playing = false;
	if (m_clip.lastFrame > m_clip.firstFrame) {
		if (m_currentFrame >= m_clip.lastFrame)
			m_currentFrame = m_clip.firstFrame;
		else
			++m_currentFrame;
	}
	m_elapsedTime = 0.f;
}

void AnimationDocument::stepPrevious() {
	m_playing = false;
	if (m_clip.lastFrame > m_clip.firstFrame) {
		if (m_currentFrame <= m_clip.firstFrame)
			m_currentFrame = m_clip.lastFrame;
		else
			--m_currentFrame;
	}
	m_elapsedTime = 0.f;
}

void AnimationDocument::renderProperties() {
	const scene::AnimationClip defaults;
	if (gui::widgets::sectionHeader("Spritesheet", gui::icon::image, false).open) {
		static constexpr ImVec2 thumbnailSize{96.f, 96.f};
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		const ImVec2 thumbnail{cursor.x + ImGui::GetStyle().FramePadding.x,
							   cursor.y + ImGui::GetStyle().FramePadding.y};
		std::ignore = gui::widgets::textureField("##spritesheet", m_clip.texture, thumbnailSize);
		gui::widgets::itemTooltip("Spritesheet", nullptr, "Drop a texture from the Content Browser.");
		if (m_frameOutlined && m_clip.texture) {
			const auto [topLeft, bottomRight] = currentFrameRect();
			const auto corner = [&thumbnail](const math::vec2& iPoint) -> ImVec2 {
				return {thumbnail.x + iPoint.x() * thumbnailSize.x, thumbnail.y + iPoint.y() * thumbnailSize.y};
			};
			ImGui::GetWindowDrawList()->AddRect(corner(topLeft), corner(bottomRight),
												ImGui::GetColorU32(gui::vec(gui::UiLayer::getTheme().accent)), 0.f, 0,
												2.f);
		}
		// The grid of the sheet beside its thumbnail.
		ImGui::SameLine();
		ImGui::BeginGroup();
		if (gui::widgets::beginProperties("##sheet")) {
			int cols = static_cast<int>(m_clip.columns);
			if (gui::widgets::propertyInt("Columns", cols, static_cast<int>(defaults.columns), 1, 256, "%d",
										  "Frames per row of the sheet."))
				m_clip.columns = static_cast<uint32_t>(std::max(cols, 1));
			int rows = static_cast<int>(m_clip.rows);
			if (gui::widgets::propertyInt("Rows", rows, static_cast<int>(defaults.rows), 1, 256, "%d",
										  "Rows of frames in the sheet."))
				m_clip.rows = static_cast<uint32_t>(std::max(rows, 1));
			std::ignore = gui::widgets::propertyBool("Show Current Frame", m_frameOutlined, true,
													 "Outline the current frame on the spritesheet.");
			gui::widgets::endProperties();
		}
		ImGui::EndGroup();
		gui::widgets::endSection();
	}
	const int maxFrame = static_cast<int>(std::max(m_clip.columns * m_clip.rows, 1u) - 1);
	if (gui::widgets::sectionHeader("Playback", gui::icon::film, false).open) {
		if (gui::widgets::beginProperties("##playback")) {
			int first = static_cast<int>(m_clip.firstFrame);
			if (gui::widgets::propertyInt("First Frame", first, static_cast<int>(defaults.firstFrame), 0, maxFrame,
										  "%d", "First frame of the clip, counted row by row from the top left."))
				m_clip.firstFrame = static_cast<uint32_t>(std::clamp(first, 0, maxFrame));
			int last = static_cast<int>(m_clip.lastFrame);
			if (gui::widgets::propertyInt("Last Frame", last, std::max(static_cast<int>(defaults.lastFrame), first),
										  static_cast<int>(m_clip.firstFrame), maxFrame, "%d",
										  "Last frame of the clip, included."))
				m_clip.lastFrame =
						static_cast<uint32_t>(std::clamp(last, static_cast<int>(m_clip.firstFrame), maxFrame));
			if (gui::widgets::propertyFloat("Frame Duration", m_clip.frameDuration, defaults.frameDuration, 0.01f,
											"%.3f s", "Time each frame stays on screen, before the speed curve."))
				m_clip.frameDuration = std::clamp(m_clip.frameDuration, 0.001f, 10.f);
			std::ignore = gui::widgets::propertyBool("Loop", m_clip.loop, defaults.loop,
													 "Start again from the first frame after the last one.");
			gui::widgets::endProperties();
		}
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Speed Curve", gui::icon::timer, false).open) {
		gui::widgets::curveEditor("##animSpeedCurve", m_clip.speedCurve);
		gui::widgets::endSection();
	}
}

void AnimationDocument::renderPreview() {
	if (!gui::widgets::sectionHeader("Preview", gui::icon::play, false).open)
		return;
	const auto previewArea = ImGui::GetContentRegionAvail();
	const float side =
			std::max(80.f, std::min(previewArea.x, previewArea.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
	const ImVec2 previewSize{side, side};

	if (auto im = gui::imTexture(m_clip.texture); im.has_value() && m_clip.texture) {
		const auto [uv0, uv1] = currentFrameUv();
		ImGui::Image(im.value(), previewSize, gui::vec(uv0), gui::vec(uv1));
	} else {
		ImGui::BeginChild("##noSheet", previewSize);
		std::ignore = gui::widgets::emptyState(gui::icon::image, "No spritesheet",
											   "Drop a texture on the Spritesheet field to preview the clip.");
		ImGui::EndChild();
	}

	ImGui::Spacing();
	if (gui::widgets::iconButton("##previous", {.glyph = gui::icon::skipBack, .tooltip = "Previous frame"}))
		stepPrevious();
	ImGui::SameLine();
	if (gui::widgets::iconButton("##play", {.glyph = m_playing ? gui::icon::pause : gui::icon::play,
											.tooltip = m_playing ? "Pause" : "Play",
											.selected = m_playing}))
		m_playing = !m_playing;
	ImGui::SameLine();
	if (gui::widgets::iconButton(
				"##stop",
				{.glyph = gui::icon::square, .tooltip = "Stop", .description = "Stop and go back to the first frame."}))
		stop();
	ImGui::SameLine();
	if (gui::widgets::iconButton("##next", {.glyph = gui::icon::skipForward, .tooltip = "Next frame"}))
		stepNext();
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("Frame %u  (%u to %u)%s", m_currentFrame, m_clip.firstFrame, m_clip.lastFrame,
						m_clip.loop ? ", loop" : "");
	gui::widgets::endSection();
}

void AnimationDocument::renderTimeline() {
	if (!gui::widgets::sectionHeader("Timeline", gui::icon::clapperboard, false).open)
		return;

	const int totalGrid = static_cast<int>(std::max(m_clip.columns * m_clip.rows, 1u));
	gui::widgets::SequencerOptions opts;
	opts.frameMin = 0;
	opts.frameMax = totalGrid - 1;
	opts.allowEditStartEnd = true;
	opts.showCurrentFrame = true;

	std::vector<gui::widgets::SequencerEntry> entries;
	entries.push_back(gui::widgets::SequencerEntry{
			.label = "Range",
			.startFrame = static_cast<int32_t>(m_clip.firstFrame),
			.endFrame = static_cast<int32_t>(m_clip.lastFrame),
			.color = gui::UiLayer::getTheme().accent,
	});

	auto currentFrame = static_cast<int32_t>(m_currentFrame);
	const bool changed = gui::widgets::sequencer("##animTimeline", entries, currentFrame, m_seqFirstVisibleFrame, opts,
												 {0.f, g_timelineHeight});

	// Clamp + write back to the clip when the user dragged the bar handles.
	const auto first = std::clamp(entries[0].startFrame, opts.frameMin, opts.frameMax);
	const auto last = std::clamp(entries[0].endFrame, first, opts.frameMax);
	m_clip.firstFrame = static_cast<uint32_t>(first);
	m_clip.lastFrame = static_cast<uint32_t>(last);

	if (changed) {
		// Manual playhead scrub pauses playback so the user sees the frame they picked.
		const auto requestedFrame = std::clamp(currentFrame, first, last);
		if (static_cast<uint32_t>(requestedFrame) != m_currentFrame) {
			m_currentFrame = static_cast<uint32_t>(requestedFrame);
			m_elapsedTime = 0.f;
			m_playing = false;
		}
	}
	gui::widgets::endSection();
}

void AnimationDocument::refreshSavedSnapshot() {
	const auto displayName = m_path.empty() ? std::string{"untitled"} : m_path.stem().string();
	m_savedSnapshot = m_clip.serializeToString(displayName);
}

void AnimationDocument::advancePlayback(const float iSeconds) {
	if (!m_playing)
		return;
	if (m_clip.frameDuration <= 0.f)
		return;
	const uint32_t totalFrames =
			(m_clip.lastFrame >= m_clip.firstFrame) ? (m_clip.lastFrame - m_clip.firstFrame + 1u) : 0u;
	if (totalFrames <= 1u)
		return;

	float dt = iSeconds;
	if (!m_clip.speedCurve.empty()) {
		const float progress = totalFrames > 1u ? static_cast<float>(m_currentFrame - m_clip.firstFrame) /
														  static_cast<float>(totalFrames - 1u)
												: 0.f;
		dt *= m_clip.speedCurve.evaluate(progress);
	}
	m_elapsedTime += dt;
	const auto framesToAdvance = static_cast<uint32_t>(m_elapsedTime / m_clip.frameDuration);
	if (framesToAdvance == 0u)
		return;
	m_elapsedTime -= static_cast<float>(framesToAdvance) * m_clip.frameDuration;

	if (m_clip.loop) {
		const auto offset = (m_currentFrame - m_clip.firstFrame + framesToAdvance) % totalFrames;
		m_currentFrame = m_clip.firstFrame + offset;
	} else {
		const auto next = m_currentFrame + framesToAdvance;
		if (next >= m_clip.lastFrame) {
			m_currentFrame = m_clip.lastFrame;
			m_playing = false;
		} else {
			m_currentFrame = next;
		}
	}
}

auto AnimationDocument::currentFrameRect() const -> std::pair<math::vec2, math::vec2> {
	if (m_clip.columns == 0u || m_clip.rows == 0u)
		return {{0.f, 0.f}, {1.f, 1.f}};
	const uint32_t frame = std::clamp(m_currentFrame, 0u, m_clip.columns * m_clip.rows - 1u);
	const uint32_t colIdx = frame % m_clip.columns;
	const uint32_t rowIdx = frame / m_clip.columns;
	const auto col = static_cast<float>(colIdx);
	const auto row = static_cast<float>(rowIdx);
	const float invCols = 1.f / static_cast<float>(m_clip.columns);
	const float invRows = 1.f / static_cast<float>(m_clip.rows);
	return {{col * invCols, row * invRows}, {(col + 1.f) * invCols, (row + 1.f) * invRows}};
}

auto AnimationDocument::currentFrameUv() const -> std::pair<math::vec2, math::vec2> {
	// Engine textures are loaded bottom-up: the top row of the sheet sits at V = 1.
	const auto [topLeft, bottomRight] = currentFrameRect();
	return {{topLeft.x(), 1.f - topLeft.y()}, {bottomRight.x(), 1.f - bottomRight.y()}};
}

}// namespace owl::nest
