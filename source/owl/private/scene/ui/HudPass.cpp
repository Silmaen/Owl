/**
 * @file HudPass.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "HudPass.h"

#include "app/Application.h"
#include "data/fonts/TextLayout.h"
#include "renderer/CameraOrtho.h"
#include "renderer/Renderer2D.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace owl::scene::ui {

namespace {

// A world-space string takes its em height from the Y scale: the X scale no longer squeezes the glyphs.
auto uniformTextTransform(math::Transform iTransform) -> math::Transform {
	iTransform.scale().x() = std::copysign(iTransform.scale().y(), iTransform.scale().x());
	return iTransform;
}

// A bar filled from its left edge up to a fraction of its width.
auto leftFill(const math::Transform& iTransform, const float iFraction) -> math::Transform {
	math::Transform fill = iTransform;
	fill.scale().x() *= iFraction;
	fill.translation().x() -= iTransform.scale().x() * (1.f - iFraction) * 0.5f;
	return fill;
}

// A rectangle of the Y-down pixel layout as a Y-up transform of the pixel camera.
auto pixelTransform(const component::UiRect::Bounds& iBounds, const float iHeight) -> math::Transform {
	const auto center = iBounds.getCenter();
	math::Transform transform;
	transform.translation() = {center.x(), iHeight - center.y(), 0.f};
	transform.scale() = {iBounds.size.x(), iBounds.size.y(), 1.f};
	return transform;
}

auto fontOrDefault(const shared<data::fonts::Font>& iFont) -> shared<data::fonts::Font> {
	if (!iFont && app::Application::instanced())
		return app::Application::get().getFontLibrary().getDefaultFont();
	return iFont;
}

// Layer of a primitive inside its window: text over circles over the other shapes, as a widget label over panels.
enum struct HudLayer : uint8_t { Shape, Circle, Text };

// Sort key of a HUD primitive: the rank of its window (a window covers those under it), then the layer.
auto hudDepth(const float iRank, const HudLayer iLayer) -> float { return iRank * 4.f + static_cast<float>(iLayer); }

// Smallest em height, in screen pixels, a scale mode may shrink a HUD text to: below it the glyphs turn to mush.
constexpr float g_minTextPixels = 9.f;

// Scale of a text: the canvas scale, raised so that a shrinking scale mode keeps the text legible, never past 1.
auto legibleTextScale(const float iScale, const float iFontSize) -> float {
	if (iFontSize <= 0.f || iScale >= 1.f)
		return iScale;
	return std::max(iScale, std::min(1.f, g_minTextPixels / iFontSize));
}

// A text laid out in a rectangle of the pixel layout, its font size given in reference pixels.
void drawBoxedText(const component::UiRect::Bounds& iBox, const float iScale, const float iHeight,
				   const std::string& iText, const math::vec4& iColor, const float iFontSize,
				   const data::fonts::HorizontalAlignment iHorizontal, const int iEntityId, const float iRank) {
	math::Transform transform = pixelTransform(iBox, iHeight);
	const float scale = legibleTextScale(iScale, iFontSize);
	transform.scale() = {scale, scale, 1.f};
	renderer::Renderer2D::drawString({.transform = transform,
									  .text = iText,
									  .font = fontOrDefault(nullptr),
									  .color = iColor,
									  .layout = {.fontSize = iFontSize,
												 .boxSize = iBox.size / scale,
												 .horizontal = iHorizontal,
												 .vertical = data::fonts::VerticalAlignment::Middle},
									  .entityId = iEntityId,
									  .depth = hudDepth(iRank, HudLayer::Text)});
}

void drawWindow(const component::UiWindow& iWindow, const HudNode& iNode, const float iHeight, const int iEntityId,
				const float iRank) {
	static constexpr math::vec4 closeColor{0.65f, 0.18f, 0.18f, 1.f};
	renderer::Renderer2D::drawQuad({.transform = pixelTransform(iNode.bounds, iHeight),
									.color = iWindow.bodyColor,
									.entityId = iEntityId,
									.depth = hudDepth(iRank, HudLayer::Shape)});
	const component::UiRect::Bounds bar{
			.position = iNode.bounds.position,
			.size = {iNode.bounds.size.x(), std::min(iWindow.titleBarHeight * iNode.scale, iNode.bounds.size.y())}};
	renderer::Renderer2D::drawQuad({.transform = pixelTransform(bar, iHeight),
									.color = iWindow.titleColor,
									.entityId = iEntityId,
									.depth = hudDepth(iRank, HudLayer::Shape)});
	const float margin = 6.f * iNode.scale;
	const float buttonSize = iWindow.closable ? bar.size.y() : 0.f;
	const component::UiRect::Bounds titleBox{
			.position = {bar.position.x() + margin, bar.position.y()},
			.size = {std::max(0.f, bar.size.x() - buttonSize - 2.f * margin), bar.size.y()}};
	drawBoxedText(titleBox, iNode.scale, iHeight, iWindow.title, iWindow.titleTextColor, iWindow.titleFontSize,
				  data::fonts::HorizontalAlignment::Left, iEntityId, iRank);
	if (!iWindow.closable)
		return;
	const component::UiRect::Bounds button{.position = {bar.position.x() + bar.size.x() - buttonSize, bar.position.y()},
										   .size = {buttonSize, buttonSize}};
	renderer::Renderer2D::drawQuad({.transform = pixelTransform(button, iHeight),
									.color = closeColor,
									.entityId = iEntityId,
									.depth = hudDepth(iRank, HudLayer::Shape)});
	// U+00D7 MULTIPLICATION SIGN, in the Latin-1 range of the default charset.
	drawBoxedText(button, iNode.scale, iHeight, "\xC3\x97", iWindow.titleTextColor, iWindow.titleFontSize * 1.3f,
				  data::fonts::HorizontalAlignment::Center, iEntityId, iRank);
}

void drawText(const Entity& iEntity, const HudNode& iNode, const math::Transform& iTransform, const int iEntityId,
			  const float iRank) {
	const auto& uiText = iEntity.getComponent<component::UiText>();
	shared<data::fonts::Font> font = uiText.font;
	if (!font && app::Application::instanced())
		font = app::Application::get().getFontLibrary().getDefaultFont();
	math::Transform textTransform;
	textTransform.translation() = iTransform.translation();
	const float scale = legibleTextScale(iNode.scale, uiText.fontSize);
	textTransform.scale() = {scale, scale, 1.f};

	renderer::Renderer2D::drawString({.transform = textTransform,
									  .text = uiText.text,
									  .font = font,
									  .color = uiText.color,
									  .layout = uiText.getLayout(iNode.bounds.size / scale),
									  .entityId = iEntityId,
									  .depth = hudDepth(iRank, HudLayer::Text)});
}

void drawSlider(const component::UiSlider& iSlider, const math::Transform& iTransform, const int iEntityId,
				const float iRank) {
	renderer::Renderer2D::drawQuad({.transform = iTransform,
									.color = iSlider.trackColor,
									.entityId = iEntityId,
									.depth = hudDepth(iRank, HudLayer::Shape)});
	const float norm = iSlider.getNormalized();
	if (norm > 0.f)
		renderer::Renderer2D::drawQuad({.transform = leftFill(iTransform, norm),
										.color = iSlider.fillColor,
										.entityId = iEntityId,
										.depth = hudDepth(iRank, HudLayer::Shape)});
	const float width = iTransform.scale().x();
	const float height = iTransform.scale().y();
	math::Transform handle = iTransform;
	handle.scale() = {height, height, 1.f};
	handle.translation().x() = iTransform.translation().x() - width * 0.5f + norm * width;

	renderer::Renderer2D::drawQuad({.transform = handle,
									.color = iSlider.handleColor,
									.entityId = iEntityId,
									.depth = hudDepth(iRank, HudLayer::Shape)});
}

// One bar: its track and, when the content overflows, its thumb.
void drawBar(const component::UiRect::Bounds& iTrack, const component::UiRect::Bounds& iThumb, const HudScroll& iScroll,
			 const float iHeight, const int iEntityId, const float iRank) {
	if (iTrack.size.x() <= 0.f || iTrack.size.y() <= 0.f)
		return;
	renderer::Renderer2D::drawQuad({.transform = pixelTransform(iTrack, iHeight),
									.color = iScroll.trackColor,
									.entityId = iEntityId,
									.depth = hudDepth(iRank, HudLayer::Shape)});
	renderer::Renderer2D::drawQuad({.transform = pixelTransform(iThumb, iHeight),
									.color = iScroll.thumbColor,
									.entityId = iEntityId,
									.depth = hudDepth(iRank, HudLayer::Shape)});
}

void drawScrollBars(const HudScroll& iScroll, const float iHeight, const int iEntityId, const float iRank) {
	const auto& geometry = iScroll.geometry;
	if (geometry.verticalBar)
		drawBar(geometry.verticalTrack, iScroll.verticalThumb(), iScroll, iHeight, iEntityId, iRank);
	if (geometry.horizontalBar)
		drawBar(geometry.horizontalTrack, iScroll.horizontalThumb(), iScroll, iHeight, iEntityId, iRank);
}

// The background and the texts of the rows in view; the others are never touched.
void drawListRow(const component::UiList& iList, const size_t iRow, const component::UiRect::Bounds& iRowBox,
				 const float iScale, const float iHeight, const int iEntityId, const float iRank) {
	const auto index = static_cast<int32_t>(iRow);
	math::vec4 background = iRow % 2 == 0 ? iList.rowColor : iList.rowAltColor;
	if (index == iList.selectedIndex)
		background = iList.selectedColor;
	else if (index == iList.hoverIndex)
		background = iList.hoverColor;
	renderer::Renderer2D::drawQuad({.transform = pixelTransform(iRowBox, iHeight),
									.color = background,
									.entityId = iEntityId,
									.depth = hudDepth(iRank, HudLayer::Shape)});
	const auto* row = iList.findRow(iRow);
	const float margin = 6.f * iScale;
	float left = iRowBox.position.x() + margin;
	if (row != nullptr && row->icon) {
		const float side = std::max(0.f, iRowBox.size.y() - 4.f * iScale);
		const component::UiRect::Bounds iconBox{.position = {left, iRowBox.position.y() + 2.f * iScale},
												.size = {side, side}};
		renderer::Renderer2D::drawQuad({.transform = pixelTransform(iconBox, iHeight),
										.color = {1.f, 1.f, 1.f, 1.f},
										.texture = row->icon,
										.entityId = iEntityId,
										.depth = hudDepth(iRank, HudLayer::Shape)});
		left += side + margin;
	}
	const float right = iRowBox.position.x() + iRowBox.size.x() - margin;
	const component::UiRect::Bounds textBox{.position = {left, iRowBox.position.y()},
											.size = {std::max(0.f, right - left), iRowBox.size.y()}};
	const std::string text = row != nullptr ? row->text : std::format("Item {}", iRow + 1);
	drawBoxedText(textBox, iScale, iHeight, text, iList.textColor, iList.fontSize,
				  data::fonts::HorizontalAlignment::Left, iEntityId, iRank);
	if (row != nullptr && !row->detail.empty())
		drawBoxedText(textBox, iScale, iHeight, row->detail, iList.textColor, iList.fontSize,
					  data::fonts::HorizontalAlignment::Right, iEntityId, iRank);
}

void drawList(const component::UiList& iList, const HudNode& iNode, const HudScroll& iScroll, const float iHeight,
			  const int iEntityId, const float iRank) {
	const auto& viewport = iScroll.geometry.viewport;
	const float pitch = iScroll.rowHeight + iScroll.rowSpacing;
	const auto [first, end] =
			visibleRows(iScroll.offset.y(), viewport.size.y(), iScroll.rowHeight, iScroll.rowSpacing, iScroll.rowCount);
	for (size_t row = first; row < end; ++row) {
		const component::UiRect::Bounds box{
				.position = {viewport.position.x(),
							 viewport.position.y() + static_cast<float>(row) * pitch - iScroll.offset.y()},
				.size = {viewport.size.x(), iScroll.rowHeight}};
		drawListRow(iList, row, box, iNode.scale, iHeight, iEntityId, iRank);
	}
}

void drawWidget(const Entity& iEntity, const HudNode& iNode, const math::Transform& iTransform, const float iHeight,
				const HudScroll* iScroll, const float iRank) {
	const int entityId = static_cast<int>(static_cast<entt::entity>(iEntity));
	if (iEntity.hasComponent<component::UiWindow>())
		drawWindow(iEntity.getComponent<component::UiWindow>(), iNode, iHeight, entityId, iRank);
	if (iEntity.hasComponent<component::SpriteRenderer>()) {
		const auto& sprite = iEntity.getComponent<component::SpriteRenderer>();
		renderer::Renderer2D::drawQuad({.transform = iTransform,
										.color = sprite.color,
										.texture = sprite.texture,
										.tilingFactor = sprite.tilingFactor,
										.entityId = entityId,
										.depth = hudDepth(iRank, HudLayer::Shape)});
	}
	if (iEntity.hasComponent<component::Text>()) {
		const auto& [text, font, color, kerning, lineSpacing] = iEntity.getComponent<component::Text>();
		renderer::Renderer2D::drawString({.transform = uniformTextTransform(iTransform),
										  .text = text,
										  .font = font,
										  .color = color,
										  .layout = {.kerning = kerning, .lineSpacing = lineSpacing},
										  .entityId = entityId,
										  .depth = hudDepth(iRank, HudLayer::Text)});
	}
	if (iEntity.hasComponent<component::CircleRenderer>()) {
		const auto& [color, thickness, fade] = iEntity.getComponent<component::CircleRenderer>();
		renderer::Renderer2D::drawCircle({.transform = iTransform,
										  .color = color,
										  .thickness = thickness,
										  .fade = fade,
										  .entityId = entityId,
										  .depth = hudDepth(iRank, HudLayer::Circle)});
	}
	if (iEntity.hasComponent<component::UiPanel>())
		renderer::Renderer2D::drawQuad({.transform = iTransform,
										.color = iEntity.getComponent<component::UiPanel>().backgroundColor,
										.entityId = entityId,
										.depth = hudDepth(iRank, HudLayer::Shape)});
	if (iEntity.hasComponent<component::UiImage>()) {
		const auto& [texture, tint] = iEntity.getComponent<component::UiImage>();
		renderer::Renderer2D::drawQuad({.transform = iTransform,
										.color = tint,
										.texture = texture,
										.entityId = entityId,
										.depth = hudDepth(iRank, HudLayer::Shape)});
	}
	if (iEntity.hasComponent<component::UiText>())
		drawText(iEntity, iNode, iTransform, entityId, iRank);
	if (iEntity.hasComponent<component::UiButton>())
		renderer::Renderer2D::drawQuad({.transform = iTransform,
										.color = iEntity.getComponent<component::UiButton>().getCurrentColor(),
										.entityId = entityId,
										.depth = hudDepth(iRank, HudLayer::Shape)});
	if (iEntity.hasComponent<component::UiProgressBar>()) {
		const auto& [value, backgroundColor, fillColor] = iEntity.getComponent<component::UiProgressBar>();
		renderer::Renderer2D::drawQuad({.transform = iTransform,
										.color = backgroundColor,
										.entityId = entityId,
										.depth = hudDepth(iRank, HudLayer::Shape)});
		if (const float fraction = std::clamp(value, 0.f, 1.f); fraction > 0.f)
			renderer::Renderer2D::drawQuad({.transform = leftFill(iTransform, fraction),
											.color = fillColor,
											.entityId = entityId,
											.depth = hudDepth(iRank, HudLayer::Shape)});
	}
	if (iEntity.hasComponent<component::UiSlider>())
		drawSlider(iEntity.getComponent<component::UiSlider>(), iTransform, entityId, iRank);
	if (iScroll != nullptr && iScroll->isList) {
		drawList(iEntity.getComponent<component::UiList>(), iNode, *iScroll, iHeight, entityId, iRank);
		drawScrollBars(*iScroll, iHeight, entityId, iRank);
	}
}

// Pixel rectangle of a clip, rounded outwards and kept inside the viewport.
auto scissorOf(const component::UiRect::Bounds& iClip, const math::vec2ui& iViewport) -> renderer::Renderer2D::Scissor {
	const auto edge = [](const float iValue, const uint32_t iLimit) -> uint32_t {
		return static_cast<uint32_t>(std::clamp(iValue, 0.f, static_cast<float>(iLimit)));
	};
	const uint32_t left = edge(std::floor(iClip.position.x()), iViewport.x());
	const uint32_t top = edge(std::floor(iClip.position.y()), iViewport.y());
	const uint32_t right = edge(std::ceil(iClip.position.x() + iClip.size.x()), iViewport.x());
	const uint32_t bottom = edge(std::ceil(iClip.position.y() + iClip.size.y()), iViewport.y());
	return {.position = {left, top}, .size = {right - left, bottom - top}, .targetHeight = iViewport.y()};
}

auto pixelCamera(const math::vec2ui& iViewport) -> renderer::CameraOrtho {
	return {0.f, static_cast<float>(iViewport.x()), 0.f, static_cast<float>(iViewport.y())};
}

void drawCanvases(Scene& ioScene, const math::vec2ui& iViewport, const bool iEditorMode) {
	auto& layout = ioScene.getHudLayout();
	layout.compute(ioScene, iViewport, iEditorMode);
	if (layout.getNodes().empty())
		return;
	const auto height = static_cast<float>(iViewport.y());
	// Renderer2D draws Y-up: the Y-down layout is flipped here, so the glyphs keep their orientation.
	renderer::Renderer2D::beginScene(pixelCamera(iViewport));
	// A window covers those under it through its rank in the sort key; a clip is a scissor.
	size_t window = HudNode::noParent;
	float rank = 0.f;
	std::optional<component::UiRect::Bounds> clip;
	const auto nodes = layout.getNodes();
	const auto scrolls = layout.getScrolls();
	const auto setClip = [&clip, &iViewport](const std::optional<component::UiRect::Bounds>& iWanted) -> bool {
		const bool changed = iWanted.has_value() != clip.has_value() ||
							 (iWanted && (iWanted->position != clip->position || iWanted->size != clip->size));
		if (!changed)
			return false;
		if (iWanted)
			renderer::Renderer2D::setScissor(scissorOf(*iWanted, iViewport));
		else
			renderer::Renderer2D::clearScissor();
		clip = iWanted;
		return true;
	};
	const auto ownClip = [](const HudNode& iNode) -> std::optional<component::UiRect::Bounds> {
		return iNode.clipped ? std::optional{iNode.clip} : std::nullopt;
	};
	// Scroll views draw their bars once their children are drawn, so the bars lie over the content.
	std::vector<size_t> pendingBars;
	const auto isBelow = [&nodes](size_t iIndex, const size_t iAncestor) -> bool {
		while (iIndex != HudNode::noParent && iIndex != iAncestor) iIndex = nodes[iIndex].parent;
		return iIndex == iAncestor;
	};
	const auto flushBars = [&](const size_t iNext) -> void {
		while (!pendingBars.empty() && (iNext == nodes.size() || !isBelow(iNext, pendingBars.back()))) {
			const auto& owner = nodes[pendingBars.back()];
			setClip(ownClip(owner));
			drawScrollBars(scrolls[owner.scroll], height, static_cast<int>(static_cast<entt::entity>(owner.entity)),
						   rank);
			pendingBars.pop_back();
		}
	};
	for (size_t index = 0; index < nodes.size(); ++index) {
		const auto& node = nodes[index];
		flushBars(index);
		const HudScroll* scroll = node.scroll != HudNode::noParent ? &scrolls[node.scroll] : nullptr;
		auto wanted = ownClip(node);
		if (scroll != nullptr && scroll->isList)
			wanted = node.clipped ? node.bounds.intersect(node.clip) : node.bounds;
		setClip(wanted);
		if (node.window != window) {
			rank += 1.f;
			window = node.window;
		}
		drawWidget(Entity{node.entity, &ioScene}, node, pixelTransform(node.bounds, height), height, scroll, rank);
		if (scroll != nullptr && !scroll->isList)
			pendingBars.push_back(index);
	}
	flushBars(nodes.size());
	renderer::Renderer2D::endScene();
}

// Scale of the canvas above an entity (1 when it hangs under none).
auto canvasScaleOf(const Scene& iScene, entt::entity iEntity, const math::vec2& iScreen) -> float {
	while (iEntity != entt::null) {
		if (const auto* canvas = iScene.registry.try_get<component::Canvas>(iEntity))
			return canvas->getScale(iScreen);
		const auto* hierarchy = iScene.registry.try_get<component::Hierarchy>(iEntity);
		if (hierarchy == nullptr || hierarchy->parentId == core::UUID{0})
			break;
		iEntity = static_cast<entt::entity>(iScene.findEntityByUUID(hierarchy->parentId));
	}
	return 1.f;
}

void drawTooltip(Scene& ioSurface, const Scene::UiInputState& iState, const math::vec2ui& iViewport) {
	if (!ioSurface.registry.valid(iState.tooltipWidget))
		return;
	const auto* tooltip = ioSurface.registry.try_get<component::UiTooltip>(iState.tooltipWidget);
	const auto* node = ioSurface.getHudLayout().findNode(iState.tooltipWidget);
	if (tooltip == nullptr || node == nullptr || tooltip->text.empty() || iState.hoverTime < tooltip->delay)
		return;
	const auto height = static_cast<float>(iViewport.y());
	// The whole tooltip grows with its text, so the box still fits it.
	const float scale = legibleTextScale(node->scale, tooltip->fontSize);
	const auto font = fontOrDefault(nullptr);
	const data::fonts::TextLayout layout{.fontSize = tooltip->fontSize};
	// Without a font (headless) a glyph is taken half an em wide.
	math::vec2 text{tooltip->fontSize * 0.5f * static_cast<float>(tooltip->text.size()), tooltip->fontSize};
	if (font) {
		const auto measured = measureTextLayout(*font, tooltip->text, layout).diagonal();
		text = {measured.x(), measured.y()};
	}
	const math::vec2 size = (text + math::vec2{2.f, 2.f} * tooltip->padding) * scale;
	const math::vec2 screen{static_cast<float>(iViewport.x()), static_cast<float>(iViewport.y())};
	const math::vec2 wanted = iState.mousePosition + tooltip->offset * scale;
	const component::UiRect::Bounds box{.position = {std::clamp(wanted.x(), 0.f, std::max(0.f, screen.x() - size.x())),
													 std::clamp(wanted.y(), 0.f, std::max(0.f, screen.y() - size.y()))},
										.size = size};
	renderer::Renderer2D::drawQuad(
			{.transform = pixelTransform(box, height), .color = tooltip->backgroundColor, .entityId = -1});
	const component::UiRect::Bounds textBox{.position = box.position +
														math::vec2{tooltip->padding, tooltip->padding} * scale,
											.size = text * scale};
	drawBoxedText(textBox, scale, height, tooltip->text, tooltip->textColor, tooltip->fontSize,
				  data::fonts::HorizontalAlignment::Left, -1, 0.f);
}

// A frame just outside the focused widget, in the colour and thickness of its `UiFocus`.
void drawFocusFrame(Scene& ioSurface, const Scene::UiInputState& iState, const math::vec2ui& iViewport) {
	if (!ioSurface.registry.valid(iState.focus))
		return;
	const auto* node = ioSurface.getHudLayout().findNode(iState.focus);
	if (node == nullptr)
		return;
	const component::UiFocus defaults;
	const auto* authored = ioSurface.registry.try_get<component::UiFocus>(iState.focus);
	const auto& focus = authored != nullptr ? *authored : defaults;
	const float thickness = std::max(1.f, focus.highlightThickness * node->scale);
	const auto height = static_cast<float>(iViewport.y());
	const auto& [position, size] = node->bounds;
	const std::array<component::UiRect::Bounds, 4> bars{{
			{.position = {position.x() - thickness, position.y() - thickness},
			 .size = {size.x() + 2.f * thickness, thickness}},
			{.position = {position.x() - thickness, position.y() + size.y()},
			 .size = {size.x() + 2.f * thickness, thickness}},
			{.position = {position.x() - thickness, position.y()}, .size = {thickness, size.y()}},
			{.position = {position.x() + size.x(), position.y()}, .size = {thickness, size.y()}},
	}};
	for (const auto& bar: bars)
		renderer::Renderer2D::drawQuad(
				{.transform = pixelTransform(bar, height), .color = focus.highlightColor, .entityId = -1});
}

// The first active, visible tool cursor of a surface.
auto findToolCursor(Scene& ioSurface, const bool iEditorMode) -> entt::entity {
	for (const auto view = ioSurface.registry.view<component::UiToolCursor>(); const auto entity: view) {
		if (view.get<component::UiToolCursor>(entity).active &&
			ioSurface.isEffectivelyVisible(Entity{entity, &ioSurface}, iEditorMode))
			return entity;
	}
	return entt::null;
}

void drawToolCursor(const Scene& iSurface, const entt::entity iEntity, const Scene::UiInputState& iState,
					const math::vec2ui& iViewport) {
	const auto& cursor = iSurface.registry.get<component::UiToolCursor>(iEntity);
	const math::vec2 screen{static_cast<float>(iViewport.x()), static_cast<float>(iViewport.y())};
	const float scale = canvasScaleOf(iSurface, iEntity, screen);
	const component::UiRect::Bounds box{
			.position = iState.mousePosition -
						math::vec2{cursor.hotspot.x() * cursor.size.x(), cursor.hotspot.y() * cursor.size.y()} * scale,
			.size = cursor.size * scale};
	renderer::Renderer2D::drawQuad({.transform = pixelTransform(box, screen.y()),
									.color = cursor.tint,
									.texture = cursor.texture,
									.entityId = -1});
}

// The tooltip then the tool cursor, over every window of the scene and of its HUD.
void drawOverlays(Scene& ioScene, Scene* iHud, const math::vec2ui& iViewport, const bool iEditorMode) {
	const auto& state = ioScene.getUiInputState();
	if (ioScene.status != Scene::Status::Playing || !state.hasMouse)
		return;
	const Scene* cursorSurface = nullptr;
	entt::entity cursor = entt::null;
	for (Scene* surface: {iHud, &ioScene}) {
		if (surface == nullptr || cursorSurface != nullptr)
			continue;
		if (cursor = findToolCursor(*surface, iEditorMode); cursor != entt::null)
			cursorSurface = surface;
	}
	const bool tooltip = state.tooltipWidget != entt::null;
	const bool focusFrame = state.focusVisible && state.focus != entt::null;
	if (!tooltip && !focusFrame && cursorSurface == nullptr)
		return;
	renderer::Renderer2D::beginScene(pixelCamera(iViewport));
	if (focusFrame)
		drawFocusFrame(state.focusInHud && iHud != nullptr ? *iHud : ioScene, state, iViewport);
	if (tooltip)
		drawTooltip(state.tooltipInHud && iHud != nullptr ? *iHud : ioScene, state, iViewport);
	if (cursorSurface != nullptr)
		drawToolCursor(*cursorSurface, cursor, state, iViewport);
	renderer::Renderer2D::endScene();
}

}// namespace

void HudPass::render(Scene& ioScene, const math::vec2ui& iViewport, const bool iEditorMode) {
	OWL_PROFILE_FUNCTION()

	drawCanvases(ioScene, iViewport, iEditorMode);
	const auto hud = ioScene.getHud();
	Scene* const hudScene = hud && hud.get() != &ioScene ? hud.get() : nullptr;
	if (hudScene != nullptr)
		drawCanvases(*hudScene, iViewport, iEditorMode);
	drawOverlays(ioScene, hudScene, iViewport, iEditorMode);
}

}// namespace owl::scene::ui
