/**
 * @file AnimationDocument.cpp
 * @author Silmaen
 * @date 27/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AnimationDocument.h"

#include "EditorLayer.h"

#include <gui/utils.h>
#include <gui/widgets/AssetField.h>
#include <gui/widgets/CurveEditor.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

namespace owl::nest {

namespace {
constexpr float g_previewSize = 220.f;
constexpr float g_timelineHeight = 200.f;

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
	const auto winTitle = std::format("{}##animation_{:x}", title(), static_cast<uint64_t>(id()));

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
	if (wantFocus)
		ImGui::SetWindowFocus();
	if (open) {
		// Clamp the playhead inside the configured range whenever the user changes
		// firstFrame / lastFrame — keeps the preview consistent without a stale frame.
		if (m_currentFrame < m_clip.firstFrame || m_currentFrame > m_clip.lastFrame)
			m_currentFrame = m_clip.firstFrame;

		const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (windowFocused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
			std::ignore = save();

		const auto avail = ImGui::GetContentRegionAvail();
		const float topHeight = std::max(180.f, avail.y - g_timelineHeight - 24.f);
		const float leftWidth = std::max(g_previewSize + 16.f, avail.x * 0.45f);

		ImGui::BeginChild("##animation_preview", ImVec2{leftWidth, topHeight}, ImGuiChildFlags_Borders);
		renderPreview();
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##animation_props", ImVec2{0.f, topHeight}, ImGuiChildFlags_Borders);
		renderProperties();
		ImGui::EndChild();

		renderTimeline();
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
	ImGui::TextDisabled("Animation Clip");
	ImGui::Separator();

	gui::widgets::textureField("Spritesheet", m_clip.texture);

	const int maxFrame = static_cast<int>(std::max(m_clip.columns * m_clip.rows, 1u) - 1);

	int cols = static_cast<int>(m_clip.columns);
	if (ImGui::DragInt("Columns", &cols, 1, 1, 256))
		m_clip.columns = static_cast<uint32_t>(std::max(cols, 1));
	int rows = static_cast<int>(m_clip.rows);
	if (ImGui::DragInt("Rows", &rows, 1, 1, 256))
		m_clip.rows = static_cast<uint32_t>(std::max(rows, 1));
	int first = static_cast<int>(m_clip.firstFrame);
	if (ImGui::DragInt("First Frame", &first, 1, 0, maxFrame))
		m_clip.firstFrame = static_cast<uint32_t>(std::clamp(first, 0, maxFrame));
	int last = static_cast<int>(m_clip.lastFrame);
	if (ImGui::DragInt("Last Frame", &last, 1, static_cast<int>(m_clip.firstFrame), maxFrame))
		m_clip.lastFrame = static_cast<uint32_t>(std::clamp(last, static_cast<int>(m_clip.firstFrame), maxFrame));
	ImGui::DragFloat("Frame Duration", &m_clip.frameDuration, 0.01f, 0.001f, 10.f, "%.3f s");
	ImGui::Checkbox("Loop", &m_clip.loop);

	ImGui::Spacing();
	ImGui::Text("Current Frame: %u", m_currentFrame);

	if (ImGui::CollapsingHeader("Speed Curve")) {
		gui::widgets::curveEditor("##animSpeedCurve", m_clip.speedCurve);
	}
}

void AnimationDocument::renderPreview() {
	ImGui::TextDisabled("Preview");
	ImGui::Separator();

	const auto previewArea = ImGui::GetContentRegionAvail();
	const float side = std::max(80.f, std::min(previewArea.x, previewArea.y - 36.f));
	const ImVec2 previewSize{side, side};

	if (auto im = gui::imTexture(m_clip.texture); im.has_value() && m_clip.texture) {
		const auto [uv0, uv1] = currentFrameUv();
		ImGui::Image(im.value(), previewSize, gui::vec(uv0), gui::vec(uv1));
	} else {
		// Placeholder when no spritesheet is set.
		const auto cursor = ImGui::GetCursorScreenPos();
		auto* drawList = ImGui::GetWindowDrawList();
		const ImU32 col = ImGui::GetColorU32(ImGuiCol_FrameBg);
		drawList->AddRectFilled(cursor, ImVec2{cursor.x + previewSize.x, cursor.y + previewSize.y}, col, 4.f);
		const auto* msg = "Drop a spritesheet";
		const auto txtSize = ImGui::CalcTextSize(msg);
		drawList->AddText(
				ImVec2{cursor.x + (previewSize.x - txtSize.x) * 0.5f, cursor.y + (previewSize.y - txtSize.y) * 0.5f},
				ImGui::GetColorU32(ImGuiCol_TextDisabled), msg);
		ImGui::Dummy(previewSize);
	}

	ImGui::Spacing();
	if (ImGui::Button(m_playing ? "Pause" : "Play"))
		m_playing = !m_playing;
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
		stop();
	ImGui::SameLine();
	if (ImGui::Button("|<"))
		stepPrevious();
	ImGui::SameLine();
	if (ImGui::Button(">|"))
		stepNext();
}

void AnimationDocument::renderTimeline() {
	ImGui::TextDisabled("Timeline");
	ImGui::Separator();

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
			.color = {0.30f, 0.70f, 1.0f, 1.0f},
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

auto AnimationDocument::currentFrameUv() const -> std::pair<math::vec2, math::vec2> {
	if (m_clip.columns == 0u || m_clip.rows == 0u)
		return {{0.f, 0.f}, {1.f, 1.f}};
	const uint32_t frame = std::clamp(m_currentFrame, 0u, m_clip.columns * m_clip.rows - 1u);
	const uint32_t colIdx = frame % m_clip.columns;
	const uint32_t rowIdx = frame / m_clip.columns;
	const float invCols = 1.f / static_cast<float>(m_clip.columns);
	const float invRows = 1.f / static_cast<float>(m_clip.rows);
	const auto col = static_cast<float>(colIdx);
	const auto row = static_cast<float>(rowIdx);
	// Row-major from the top, V-flipped to display the same way Renderer2D draws the sub-texture.
	const math::vec2 uv0{col * invCols, row * invRows};
	const math::vec2 uv1{(col + 1.f) * invCols, (row + 1.f) * invRows};
	return {uv0, uv1};
}

}// namespace owl::nest
