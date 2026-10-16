/**
 * @file HudEditor.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HudEditor.h"

#include "../EditorLayer.h"
#include "../commands/HudCommands.h"
#include "../document/SceneDocument.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/Tooltip.h>
#include <renderer/CameraOrtho.h>
#include <renderer/Renderer2D.h>
#include <renderer/gpu/RenderCommand.h>
#include <scene/component/Canvas.h>
#include <scene/component/ID.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string_view>
#include <tuple>
#include <utility>

namespace owl::nest::panel {

namespace {

// Width of the palette column, in screen pixels.
constexpr float paletteWidth = 156.f;
// Distance in screen pixels within which a handle of the gizmo is grabbed.
constexpr float grabDistance = 7.f;
// Size of a handle of the gizmo, in screen pixels.
constexpr float handleSize = 6.f;

struct PaletteEntry {
	const char* kind;
	const char* label;
	const char* glyph;
	const char* description;
};

constexpr std::array<PaletteEntry, 9> palette{
		{{"canvas", "Canvas", gui::icon::panelsTopLeft, "A root of widgets, with its own reference resolution."},
		 {"panel", "Panel", gui::icon::square, "A coloured rectangle holding other widgets."},
		 {"label", "Label", gui::icon::type, "A text."},
		 {"image", "Image", gui::icon::image, "A texture, tinted."},
		 {"button", "Button", gui::icon::mousePointerClick, "A clickable rectangle with a label."},
		 {"slider", "Slider", gui::icon::slidersHorizontal, "A value dragged along a track."},
		 {"progress_bar", "Progress Bar", gui::icon::gauge, "A bar filled to a fraction."},
		 {"window", "Window", gui::icon::appWindow, "A draggable window with a title bar."},
		 {"layout", "Layout", gui::icon::layoutGrid, "A panel placing its children in a row, a column or a grid."}}};

auto isTexture(const std::filesystem::path& iPath) -> bool {
	auto extension = iPath.extension().string();
	std::ranges::transform(extension, extension.begin(),
						   [](const char iChar) -> char { return static_cast<char>(std::tolower(iChar)); });
	return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
		   extension == ".tga";
}

auto color(const math::vec4& iColor, const float iAlpha = 1.f) -> ImU32 {
	return ImGui::GetColorU32(ImVec4{iColor.x(), iColor.y(), iColor.z(), iColor.w() * iAlpha});
}

void dashedRect(ImDrawList& ioDraw, const ImVec2& iMin, const ImVec2& iMax, const ImU32 iColor) {
	constexpr float dash = 5.f;
	const auto line = [&](const ImVec2& iFrom, const ImVec2& iTo) -> void {
		const float length = std::hypot(iTo.x - iFrom.x, iTo.y - iFrom.y);
		if (length <= 0.f)
			return;
		const ImVec2 step{(iTo.x - iFrom.x) / length, (iTo.y - iFrom.y) / length};
		const auto count = static_cast<int>(length / (2.f * dash)) + 1;
		for (int i = 0; i < count; ++i) {
			const float at = static_cast<float>(i) * 2.f * dash;
			const float end = std::min(at + dash, length);
			ioDraw.AddLine({iFrom.x + step.x * at, iFrom.y + step.y * at},
						   {iFrom.x + step.x * end, iFrom.y + step.y * end}, iColor);
		}
	};
	line(iMin, {iMax.x, iMin.y});
	line({iMax.x, iMin.y}, iMax);
	line(iMax, {iMin.x, iMax.y});
	line({iMin.x, iMax.y}, iMin);
}

void diamond(ImDrawList& ioDraw, const ImVec2& iCenter, const float iRadius, const ImU32 iFill, const ImU32 iLine) {
	const std::array<ImVec2, 4> points{{{iCenter.x, iCenter.y - iRadius},
										{iCenter.x + iRadius, iCenter.y},
										{iCenter.x, iCenter.y + iRadius},
										{iCenter.x - iRadius, iCenter.y}}};
	ioDraw.AddConvexPolyFilled(points.data(), 4, iFill);
	ioDraw.AddPolyline(points.data(), 4, iLine, ImDrawFlags_Closed, 1.f);
}

}// namespace

HudEditor::HudEditor() = default;

HudEditor::~HudEditor() = default;

void HudEditor::detach() {
	m_framebuffer.reset();
	m_backdropTexture.reset();
	m_backdropLoaded.clear();
}

void HudEditor::ensureFramebuffer() {
	const auto& size = m_document.getResolution();
	if (!m_framebuffer) {
		const renderer::gpu::FramebufferSpecification specs{
				.size = size,
				.attachments =
						{
								{.format = renderer::gpu::AttachmentSpecification::Format::Surface,
								 .tiling = renderer::gpu::AttachmentSpecification::Tiling::Optimal},
								{.format = renderer::gpu::AttachmentSpecification::Format::RedInteger,
								 .tiling = renderer::gpu::AttachmentSpecification::Tiling::Optimal},
								{.format = renderer::gpu::AttachmentSpecification::Format::Depth24Stencil8,
								 .tiling = renderer::gpu::AttachmentSpecification::Tiling::Optimal},
						},
				.samples = 1,
				.swapChainTarget = false,
				.debugName = "hud"};
		m_framebuffer = renderer::gpu::Framebuffer::create(specs);
		return;
	}
	if (m_framebuffer->getSpecification().size != size)
		m_framebuffer->resize(size);
}

void HudEditor::onUpdate(scene::Scene& ioScene) {
	OWL_PROFILE_FUNCTION()

	ensureFramebuffer();
	if (!m_framebuffer)
		return;
	const auto& image = m_document.getBackdropImage();
	if (m_document.getBackdropKind() != HudBackdropKind::Image || image.empty()) {
		m_backdropTexture.reset();
		m_backdropLoaded.clear();
	} else if (image != m_backdropLoaded) {
		m_backdropTexture = renderer::gpu::Texture2D::createFromSerializedForDeserialize(image);
		m_backdropLoaded = image;
	}
	const auto& resolution = m_document.getResolution();
	m_framebuffer->bind();
	renderer::gpu::RenderCommand::setClearColor(m_document.getBackdropColor());
	renderer::gpu::RenderCommand::clear();
	m_framebuffer->clearAttachment(1, -1);
	if (m_backdropTexture) {
		renderer::Renderer2D::beginScene(renderer::CameraOrtho{0.f, static_cast<float>(resolution.x()), 0.f,
															   static_cast<float>(resolution.y())});
		math::Transform transform;
		transform.translation() = {static_cast<float>(resolution.x()) * 0.5f, static_cast<float>(resolution.y()) * 0.5f,
								   0.f};
		transform.scale() = {static_cast<float>(resolution.x()), static_cast<float>(resolution.y()), 1.f};
		renderer::Renderer2D::drawQuad({.transform = transform, .texture = m_backdropTexture, .entityId = -1});
		renderer::Renderer2D::endScene();
	}
	ioScene.renderHud(resolution, true);
	m_framebuffer->unbind();
}

auto HudEditor::getTarget(SceneDocument& ioDocument) const -> commands::CommandTarget {
	return {.registry = mp_parent != nullptr ? &mp_parent->getRegistry() : nullptr,
			.scene = ioDocument.getEditorScene(),
			.undo = &ioDocument.undoManager(),
			.source = commands::CommandSource::Gesture};
}

auto HudEditor::toPreview(const ImVec2& iScreen) const -> math::vec2 {
	return {(iScreen.x - m_origin.x) / m_zoom, (iScreen.y - m_origin.y) / m_zoom};
}

auto HudEditor::toScreen(const math::vec2& iPoint) const -> ImVec2 {
	return {m_origin.x + iPoint.x() * m_zoom, m_origin.y + iPoint.y() * m_zoom};
}

void HudEditor::syncSelection(SceneDocument& ioDocument) {
	if (mp_parent == nullptr)
		return;
	const auto& scene = ioDocument.getEditorScene();
	const auto editorSelection = mp_parent->getSelectedEntity();
	const bool valid = editorSelection && scene->registry.valid(static_cast<entt::entity>(editorSelection));
	const core::UUID editorId = valid ? editorSelection.getUUID() : core::UUID{0};
	if (editorId != m_syncedPrimary) {
		m_syncedPrimary = editorId;
		if (editorId == core::UUID{0} || !m_document.isSelected(editorId))
			m_document.select(editorId);
		else
			m_document.makePrimary(editorId);
		return;
	}
	if (const auto primary = m_document.getPrimary(); primary != m_syncedPrimary) {
		m_syncedPrimary = primary;
		mp_parent->setSelectedEntity(primary == core::UUID{0} ? scene::Entity{} : scene->findEntityByUUID(primary));
	}
}

void HudEditor::onRender(SceneDocument& ioDocument) {
	m_document.pruneSelection(*ioDocument.getEditorScene());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{6.f, 4.f});
	renderToolbar(ioDocument);
	ImGui::Separator();
	ImGui::BeginChild("##hudPalette", {paletteWidth, 0.f}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
	renderPalette(ioDocument);
	ImGui::EndChild();
	ImGui::SameLine(0.f, 0.f);
	ImGui::BeginChild("##hudPreviewArea", {0.f, 0.f}, ImGuiChildFlags_None,
					  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	renderPreview(ioDocument);
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

void HudEditor::renderToolbar(SceneDocument& ioDocument) {
	using gui::widgets::iconButton;
	const auto& theme = gui::UiLayer::getTheme();
	ImGui::SetCursorPos({ImGui::GetCursorPosX() + 6.f, ImGui::GetCursorPosY() + 4.f});

	const auto& resolution = m_document.getResolution();
	const auto preset = m_document.findPreset();
	const std::string preview = preset ? std::string{HudDocument::presets[*preset].name}
									   : std::format("Custom {} x {}", resolution.x(), resolution.y());
	ImGui::SetNextItemWidth(170.f);
	if (gui::widgets::beginCombo("##hudResolution", preview.c_str())) {
		for (size_t i = 0; i < HudDocument::presets.size(); ++i) {
			if (ImGui::Selectable(std::string{HudDocument::presets[i].name}.c_str(), preset == i))
				m_document.setResolution(HudDocument::presets[i].size);
		}
		ImGui::EndCombo();
	}
	gui::widgets::itemTooltip("Preview resolution", nullptr,
							  "The size in pixels the HUD is drawn at; the canvases scale from their reference "
							  "resolution.");
	auto width = static_cast<int>(resolution.x());
	auto height = static_cast<int>(resolution.y());
	ImGui::SameLine();
	ImGui::SetNextItemWidth(64.f);
	const bool widthChanged = ImGui::DragInt("##hudWidth", &width, 1.f, static_cast<int>(HudDocument::minResolution),
											 static_cast<int>(HudDocument::maxResolution), "%d");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(64.f);
	const bool heightChanged = ImGui::DragInt("##hudHeight", &height, 1.f, static_cast<int>(HudDocument::minResolution),
											  static_cast<int>(HudDocument::maxResolution), "%d");
	if (widthChanged || heightChanged)
		m_document.setResolution({static_cast<uint32_t>(width), static_cast<uint32_t>(height)});

	ImGui::SameLine();
	if (iconButton("hudFit", {.glyph = gui::icon::maximize2,
							  .tooltip = "Fit",
							  .shortcut = "F",
							  .description = "Zoom so the whole preview is visible.",
							  .selected = m_fit})) {
		m_fit = true;
		m_pan = {0.f, 0.f};
	}
	ImGui::SameLine();
	if (iconButton("hudActual", {.glyph = gui::icon::scan,
								 .tooltip = "Actual pixels",
								 .description = "One pixel of the HUD is one pixel of the screen."})) {
		m_fit = false;
		m_zoom = 1.f;
		m_pan = {0.f, 0.f};
	}
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextColored(gui::vec(theme.textMuted), "%.0f%%", static_cast<double>(m_zoom * 100.f));

	ImGui::SameLine();
	if (iconButton("hudOutlines", {.glyph = gui::icon::squareDashed,
								   .tooltip = "Widget outlines",
								   .description = "Draw the outline of every widget.",
								   .selected = m_showOutlines}))
		m_showOutlines = !m_showOutlines;
	ImGui::SameLine();
	if (iconButton("hudReference", {.glyph = gui::icon::focus,
									.tooltip = "Reference resolution",
									.description = "Frame the reference resolution of every canvas.",
									.selected = m_showReference}))
		m_showReference = !m_showReference;

	ImGui::SameLine();
	bool image = m_document.getBackdropKind() == HudBackdropKind::Image;
	if (iconButton("hudBackdropKind", {.glyph = gui::icon::image,
									   .tooltip = "Backdrop image",
									   .description = "Draw a picture (a screenshot of the game) behind the HUD "
													  "instead of a plain colour.",
									   .selected = image})) {
		image = !image;
		m_document.setBackdropKind(image ? HudBackdropKind::Image : HudBackdropKind::Color);
	}
	ImGui::SameLine();
	if (image) {
		ImGui::SetNextItemWidth(150.f);
		std::array<char, 256> text{};
		m_document.getBackdropImage().copy(text.data(), text.size() - 1);
		if (ImGui::InputTextWithHint("##hudBackdropImage", "drop a texture here", text.data(), text.size()))
			m_document.setBackdropImage(text.data());
		if (ImGui::BeginDragDropTarget()) {
			if (const auto* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"); payload != nullptr)
				m_document.setBackdropImage(static_cast<const char*>(payload->Data));
			ImGui::EndDragDropTarget();
		}
	} else {
		const auto& current = m_document.getBackdropColor();
		std::array<float, 4> backdrop{current.x(), current.y(), current.z(), current.w()};
		if (ImGui::ColorEdit4("##hudBackdropColor", backdrop.data(),
							  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
									  ImGuiColorEditFlags_AlphaPreview))
			m_document.setBackdropColor({backdrop[0], backdrop[1], backdrop[2], backdrop[3]});
		gui::widgets::itemTooltip("Backdrop colour");
	}

	auto& snap = m_document.getSnap();
	ImGui::SameLine();
	if (iconButton("hudGrid", {.glyph = gui::icon::grid3x3,
							   .tooltip = "Snap to pixels",
							   .description = "Round positions and edges to whole reference pixels (the grid step).",
							   .selected = snap.pixelGrid}))
		snap.pixelGrid = !snap.pixelGrid;
	ImGui::SameLine();
	ImGui::SetNextItemWidth(46.f);
	auto step = static_cast<int>(snap.gridStep);
	if (ImGui::DragInt("##hudGridStep", &step, 0.2f, 1, 64, "%d"))
		snap.gridStep = static_cast<float>(std::max(1, step));
	gui::widgets::itemTooltip("Grid step", nullptr, "In reference pixels.");
	ImGui::SameLine();
	if (iconButton("hudEdges", {.glyph = gui::icon::magnet,
								.tooltip = "Snap to edges",
								.description = "Stick to the edges and centres of the siblings, the parent and the "
											   "screen. Hold Shift to snap nothing while dragging.",
								.selected = snap.edges}))
		snap.edges = !snap.edges;

	const bool hasSelection = !m_document.getSelection().empty();
	ImGui::SameLine();
	if (iconButton("hudRaise", {.glyph = gui::icon::chevronUp,
								.tooltip = "Draw later",
								.description = "Move the widget one place down its siblings: drawn over them.",
								.disabled = !hasSelection}))
		std::ignore = m_document.reorderPrimary(getTarget(ioDocument), 1);
	ImGui::SameLine();
	if (iconButton("hudLower", {.glyph = gui::icon::chevronDown,
								.tooltip = "Draw earlier",
								.description = "Move the widget one place up its siblings: drawn under them.",
								.disabled = !hasSelection}))
		std::ignore = m_document.reorderPrimary(getTarget(ioDocument), -1);
	ImGui::SameLine();
	if (iconButton("hudDelete", {.glyph = gui::icon::trash2,
								 .tooltip = "Delete",
								 .shortcut = "Del",
								 .description = "Delete the selected widgets and their children.",
								 .disabled = !hasSelection}))
		std::ignore = m_document.deleteSelection(getTarget(ioDocument));

	if (ioDocument.getEditorScene()->getDocumentKind() != scene::DocumentKind::Hud) {
		ImGui::SameLine();
		if (gui::widgets::button(
					"hudLeave",
					{.glyph = gui::icon::clapperboard, .label = "Scene View", .tooltip = "Back to the scene viewport"}))
			ioDocument.setHudMode(false);
	}
	ImGui::Dummy({0.f, 2.f});
}

void HudEditor::renderPalette(SceneDocument& ioDocument) {
	const auto& theme = gui::UiLayer::getTheme();
	ImGui::SetCursorPos({ImGui::GetCursorPosX() + 6.f, ImGui::GetCursorPosY() + 4.f});
	ImGui::TextColored(gui::vec(theme.textMuted), "Widgets");
	const auto& resolution = m_document.getResolution();
	for (const auto& entry: palette) {
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.f);
		if (gui::widgets::button(entry.kind, {.glyph = entry.glyph,
											  .label = entry.label,
											  .tooltip = entry.label,
											  .description = entry.description,
											  .width = paletteWidth - 18.f})) {
			if (const auto added = m_document.addWidget(
						*ioDocument.getEditorScene(), getTarget(ioDocument), entry.kind,
						{static_cast<float>(resolution.x()) * 0.5f, static_cast<float>(resolution.y()) * 0.5f});
				added)
				syncSelection(ioDocument);
		}
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("HUD_WIDGET", entry.kind, std::string_view{entry.kind}.size() + 1);
			ImGui::Text("%s %s", entry.glyph, entry.label);
			ImGui::EndDragDropSource();
		}
	}
	ImGui::Dummy({0.f, 6.f});
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.f);
	ImGui::PushTextWrapPos(paletteWidth - 8.f);
	ImGui::TextColored(gui::vec(theme.textDisabled),
					   "Click or drag a widget into the preview. Drop a texture to make an image. Alt grabs the "
					   "pivot, Ctrl+Alt the anchors, Shift snaps nothing.");
	ImGui::PopTextWrapPos();
}

void HudEditor::renderOverlay(const scene::Scene& iScene) const {
	auto& draw = *ImGui::GetWindowDrawList();
	const auto& theme = gui::UiLayer::getTheme();
	const auto& resolution = m_document.getResolution();
	const ImVec2 imageMax{m_origin.x + static_cast<float>(resolution.x()) * m_zoom,
						  m_origin.y + static_cast<float>(resolution.y()) * m_zoom};
	draw.AddRect(m_origin, imageMax, color(theme.line));
	if (m_zoom >= 8.f && m_document.getSnap().pixelGrid) {
		const ImVec2 clipMin = draw.GetClipRectMin();
		const ImVec2 clipMax = draw.GetClipRectMax();
		const auto first = [this](const float iClip, const float iOrigin) -> int {
			return std::max(0, static_cast<int>(std::floor((iClip - iOrigin) / m_zoom)));
		};
		for (int x = first(clipMin.x, m_origin.x);
			 m_origin.x + static_cast<float>(x) * m_zoom < clipMax.x && x <= static_cast<int>(resolution.x()); ++x)
			draw.AddLine({m_origin.x + static_cast<float>(x) * m_zoom, std::max(m_origin.y, clipMin.y)},
						 {m_origin.x + static_cast<float>(x) * m_zoom, std::min(imageMax.y, clipMax.y)},
						 color(theme.text, 0.07f));
		for (int y = first(clipMin.y, m_origin.y);
			 m_origin.y + static_cast<float>(y) * m_zoom < clipMax.y && y <= static_cast<int>(resolution.y()); ++y)
			draw.AddLine({std::max(m_origin.x, clipMin.x), m_origin.y + static_cast<float>(y) * m_zoom},
						 {std::min(imageMax.x, clipMax.x), m_origin.y + static_cast<float>(y) * m_zoom},
						 color(theme.text, 0.07f));
	}
	const math::vec2 screen{static_cast<float>(resolution.x()), static_cast<float>(resolution.y())};
	if (m_showReference) {
		for (const auto view = iScene.registry.view<scene::component::Canvas>(); const auto canvas: view) {
			const auto& data = view.get<scene::component::Canvas>(canvas);
			const float scale = data.getScale(screen);
			dashedRect(draw, m_origin,
					   {m_origin.x + data.referenceResolution.x() * scale * m_zoom,
						m_origin.y + data.referenceResolution.y() * scale * m_zoom},
					   color(theme.info, 0.6f));
		}
	}
	const auto& nodes = iScene.getHudLayout().getNodes();
	const auto hovered =
			m_document.isDragging() ? core::UUID{0} : HudDocument::pickWidget(iScene, toPreview(ImGui::GetMousePos()));
	for (const auto& node: nodes) {
		const auto uuid = iScene.registry.get<scene::component::ID>(node.entity).id;
		const bool selected = m_document.isSelected(uuid);
		const bool isHovered = uuid == hovered;
		if (!selected && !isHovered && !m_showOutlines)
			continue;
		const ImVec2 lower = toScreen(node.bounds.position);
		const ImVec2 upper = toScreen(node.bounds.position + node.bounds.size);
		if (selected)
			draw.AddRect(lower, upper, color(theme.accent), 0.f, 0, 2.f);
		else
			draw.AddRect(lower, upper, color(theme.text, isHovered ? 0.7f : 0.18f));
	}
	if (const auto gizmo = m_document.getGizmo(iScene); gizmo && m_document.getSelection().size() == 1) {
		const ImVec2 lower = toScreen(gizmo->bounds.position);
		const ImVec2 upper = toScreen(gizmo->bounds.position + gizmo->bounds.size);
		const ImU32 accent = color(theme.accent);
		const ImU32 handleFill = color(theme.text);
		const ImVec2 middle{(lower.x + upper.x) * 0.5f, (lower.y + upper.y) * 0.5f};
		for (const ImVec2& corner:
			 {lower, ImVec2{upper.x, lower.y}, upper, ImVec2{lower.x, upper.y}, ImVec2{middle.x, lower.y},
			  ImVec2{middle.x, upper.y}, ImVec2{lower.x, middle.y}, ImVec2{upper.x, middle.y}}) {
			draw.AddRectFilled({corner.x - handleSize * 0.5f, corner.y - handleSize * 0.5f},
							   {corner.x + handleSize * 0.5f, corner.y + handleSize * 0.5f}, handleFill);
			draw.AddRect({corner.x - handleSize * 0.5f, corner.y - handleSize * 0.5f},
						 {corner.x + handleSize * 0.5f, corner.y + handleSize * 0.5f}, accent);
		}
		const ImVec2 parentLower = toScreen(gizmo->parent.position);
		const ImVec2 parentUpper = toScreen(gizmo->parent.position + gizmo->parent.size);
		dashedRect(draw, parentLower, parentUpper, color(theme.success, 0.5f));
		const ImVec2 pivot = toScreen(gizmo->pivot);
		draw.AddCircle(pivot, 6.f, color(theme.play), 16, 2.f);
		draw.AddCircleFilled(pivot, 2.f, color(theme.play));
		const ImVec2 anchorMin = toScreen(gizmo->anchorMin);
		const ImVec2 anchorMax = toScreen(gizmo->anchorMax);
		diamond(draw, anchorMin, 6.f, color(theme.success, 0.85f), color(theme.surface0));
		if (gizmo->stretched) {
			diamond(draw, anchorMax, 6.f, color(theme.success, 0.85f), color(theme.surface0));
			draw.AddLine(anchorMin, lower, color(theme.success, 0.5f));
			draw.AddLine(anchorMax, upper, color(theme.success, 0.5f));
		} else {
			draw.AddLine(anchorMin, pivot, color(theme.success, 0.5f));
		}
	}
	for (const float x: m_document.getGuidesX())
		draw.AddLine(toScreen({x, 0.f}), toScreen({x, screen.y()}), color(theme.error, 0.9f));
	for (const float y: m_document.getGuidesY())
		draw.AddLine(toScreen({0.f, y}), toScreen({screen.x(), y}), color(theme.error, 0.9f));
	if (m_boxStart) {
		const ImVec2 first = toScreen(*m_boxStart);
		const ImVec2 second = toScreen(m_boxEnd);
		draw.AddRectFilled(first, second, color(theme.accent, 0.15f));
		draw.AddRect(first, second, color(theme.accent, 0.9f));
	}
}

void HudEditor::handleMouse(SceneDocument& ioDocument, const math::vec2& iMouse, const bool iHovered) {
	const auto& scene = ioDocument.getEditorScene();
	const auto& io = ImGui::GetIO();
	if (m_document.isDragging()) {
		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			m_document.cancelDrag(*scene);
		else if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
			m_document.updateDrag(*scene, iMouse, !io.KeyShift);
		else
			std::ignore = m_document.endDrag(*scene, getTarget(ioDocument));
		return;
	}
	if (m_boxStart) {
		m_boxEnd = iMouse;
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			const math::vec2 low{std::min(m_boxStart->x(), iMouse.x()), std::min(m_boxStart->y(), iMouse.y())};
			const math::vec2 high{std::max(m_boxStart->x(), iMouse.x()), std::max(m_boxStart->y(), iMouse.y())};
			m_document.selectInBox(*scene, {.position = low, .size = high - low}, io.KeyCtrl);
			m_boxStart.reset();
		}
		return;
	}
	if (!iHovered)
		return;
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		m_contextPoint = iMouse;
		if (const auto pick = HudDocument::pickWidget(*scene, iMouse);
			pick != core::UUID{0} && !m_document.isSelected(pick))
			m_document.select(pick);
		return;
	}
	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		return;
	auto grab = HudGrabMode::Normal;
	if (io.KeyAlt)
		grab = io.KeyCtrl ? HudGrabMode::Anchors : HudGrabMode::Pivot;
	const auto handle = m_document.hitHandle(*scene, iMouse, grabDistance / m_zoom, grab);
	if (handle != HudHandle::None && handle != HudHandle::Body) {
		std::ignore = m_document.beginDrag(*scene, handle, iMouse);
		return;
	}
	const auto pick = HudDocument::pickWidget(*scene, iMouse);
	if (io.KeyCtrl) {
		if (pick != core::UUID{0})
			m_document.select(pick, HudSelectMode::Toggle);
		else
			m_boxStart = iMouse;
		return;
	}
	if (pick == core::UUID{0}) {
		m_document.select(core::UUID{0});
		m_boxStart = iMouse;
		m_boxEnd = iMouse;
		return;
	}
	if (!m_document.isSelected(pick))
		m_document.select(pick);
	std::ignore = m_document.beginDrag(*scene, HudHandle::Body, iMouse);
}

void HudEditor::handleKeys(SceneDocument& ioDocument) {
	const auto& io = ImGui::GetIO();
	if (io.WantTextInput || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || m_document.isDragging())
		return;
	const auto target = getTarget(ioDocument);
	if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))
		std::ignore = m_document.deleteSelection(target);
	const float stepSize = io.KeyShift ? 10.f : 1.f;
	math::vec2 nudge{0.f, 0.f};
	if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
		nudge.x() -= stepSize;
	if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
		nudge.x() += stepSize;
	if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
		nudge.y() -= stepSize;
	if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
		nudge.y() += stepSize;
	if (nudge != math::vec2{0.f, 0.f})
		std::ignore = m_document.moveSelection(target, nudge);
	if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
		m_fit = true;
		m_pan = {0.f, 0.f};
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		m_document.select(core::UUID{0});
}

void HudEditor::handleDrop(SceneDocument& ioDocument, const math::vec2& iMouse) {
	if (!ImGui::BeginDragDropTarget())
		return;
	auto& scene = *ioDocument.getEditorScene();
	if (const auto* payload = ImGui::AcceptDragDropPayload("HUD_WIDGET"); payload != nullptr) {
		const std::string kind{static_cast<const char*>(payload->Data)};
		std::ignore = m_document.addWidget(scene, getTarget(ioDocument), kind, iMouse);
	} else if (const auto* item = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"); item != nullptr) {
		const std::string path{static_cast<const char*>(item->Data)};
		if (isTexture(path))
			std::ignore = m_document.addWidget(scene, getTarget(ioDocument), "image", iMouse, path);
	}
	ImGui::EndDragDropTarget();
}

void HudEditor::renderContextMenu(SceneDocument& ioDocument) {
	if (!ImGui::BeginPopupContextItem("##hudContext", ImGuiPopupFlags_MouseButtonRight))
		return;
	const auto target = getTarget(ioDocument);
	const bool hasSelection = !m_document.getSelection().empty();
	if (ImGui::BeginMenu("Add Widget")) {
		for (const auto& entry: palette) {
			if (gui::widgets::menuItem({.glyph = entry.glyph, .label = entry.label}))
				std::ignore = m_document.addWidget(*ioDocument.getEditorScene(), target, entry.kind, m_contextPoint);
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (gui::widgets::menuItem({.glyph = gui::icon::chevronUp, .label = "Bring to Front", .enabled = hasSelection}))
		std::ignore = m_document.reorderPrimary(target, 100000);
	if (gui::widgets::menuItem({.glyph = gui::icon::chevronDown, .label = "Send to Back", .enabled = hasSelection}))
		std::ignore = m_document.reorderPrimary(target, -100000);
	ImGui::Separator();
	if (gui::widgets::menuItem(
				{.glyph = gui::icon::trash2, .label = "Delete", .shortcut = "Del", .enabled = hasSelection}))
		std::ignore = m_document.deleteSelection(target);
	ImGui::EndPopup();
}

void HudEditor::renderPreview(SceneDocument& ioDocument) {
	const auto& scene = ioDocument.getEditorScene();
	syncSelection(ioDocument);
	const ImVec2 area = ImGui::GetContentRegionAvail();
	if (area.x < 16.f || area.y < 16.f)
		return;
	const ImVec2 lower = ImGui::GetCursorScreenPos();
	const ImVec2 upper{lower.x + area.x, lower.y + area.y};
	const auto& resolution = m_document.getResolution();
	const auto& io = ImGui::GetIO();
	if (m_fit)
		m_zoom = std::clamp(std::min(area.x * 0.96f / static_cast<float>(resolution.x()),
									 area.y * 0.96f / static_cast<float>(resolution.y())),
							0.05f, 16.f);
	ImGui::InvisibleButton("##hudPreview", area,
						   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
								   ImGuiButtonFlags_MouseButtonMiddle);
	const bool hovered = ImGui::IsItemHovered();
	const auto place = [&]() -> void {
		m_origin = {(lower.x + upper.x) * 0.5f + m_pan.x - static_cast<float>(resolution.x()) * m_zoom * 0.5f,
					(lower.y + upper.y) * 0.5f + m_pan.y - static_cast<float>(resolution.y()) * m_zoom * 0.5f};
	};
	place();
	if (hovered && io.MouseWheel != 0.f && !m_document.isDragging()) {
		const auto anchor = toPreview(ImGui::GetMousePos());
		m_zoom = std::clamp(m_zoom * std::pow(1.1f, io.MouseWheel), 0.05f, 32.f);
		m_fit = false;
		place();
		const ImVec2 moved = toScreen(anchor);
		m_pan = {m_pan.x + ImGui::GetMousePos().x - moved.x, m_pan.y + ImGui::GetMousePos().y - moved.y};
		place();
	}
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.f)) {
		m_pan = {m_pan.x + io.MouseDelta.x, m_pan.y + io.MouseDelta.y};
		m_fit = false;
		place();
	}
	auto& draw = *ImGui::GetWindowDrawList();
	draw.PushClipRect(lower, upper, true);
	draw.AddRectFilled(lower, upper, color(gui::UiLayer::getTheme().well));
	if (const auto texture = gui::imTexture(m_framebuffer, 0); texture.has_value()) {
		draw.AddImage(texture.value(), m_origin,
					  {m_origin.x + static_cast<float>(resolution.x()) * m_zoom,
					   m_origin.y + static_cast<float>(resolution.y()) * m_zoom},
					  gui::vec(m_framebuffer->getLowerData()), gui::vec(m_framebuffer->getUpperData()));
	}
	renderOverlay(*scene);
	draw.PopClipRect();
	const auto mouse = toPreview(ImGui::GetMousePos());
	handleMouse(ioDocument, mouse, hovered);
	handleDrop(ioDocument, mouse);
	renderContextMenu(ioDocument);
	handleKeys(ioDocument);
	syncSelection(ioDocument);
}

}// namespace owl::nest::panel
