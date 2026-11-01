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
#include "app/EngineContext.h"
#include "data/fonts/TextLayout.h"
#include "renderer/CameraOrtho.h"
#include "renderer/Renderer2D.h"
#include "renderer/gpu/Framebuffer.h"
#include "scene/Entity.h"
#include "scene/HudTheme.h"
#include "scene/Scene.h"
#include "scene/component/components.h"
#include "scene/ui/HudMarkerPlacement.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numbers>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::scene::ui {

namespace {

// A world-space string takes its em height from the Y scale: the X scale does not squeeze the glyphs.
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

// Painter's order of the shapes of a window across the quad and box buffers (design: hud-system.md#hud-themes).
class ShapeOrder {
public:
	explicit ShapeOrder(const bool iBoxed) : m_boxed{iBoxed} {}

	// A flat shape of the canvas: a square box in a boxed canvas (an untiled quad), else a quad.
	void drawShape(renderer::Quad2DData&& ioQuad, const float iRank) {
		if (m_boxed && isUntiled(ioQuad)) {
			const auto& low = ioQuad.textureCoords[0];
			const auto& high = ioQuad.textureCoords[2];
			renderer::Renderer2D::drawBox(
					{.transform = ioQuad.transform,
					 .size = {std::abs(ioQuad.transform.scale().x()), std::abs(ioQuad.transform.scale().y())},
					 .topColor = ioQuad.color,
					 .bottomColor = ioQuad.color,
					 .texture = std::move(ioQuad.texture),
					 .uvRect = {low.x(), low.y(), high.x(), high.y()},
					 .entityId = ioQuad.entityId,
					 .depth = depth(iRank, true)});
			return;
		}
		ioQuad.depth = depth(iRank, false);
		renderer::Renderer2D::drawQuad(ioQuad);
	}

	auto depth(const float iRank, const bool iBox) -> float {
		if (iRank < m_rank || m_rank < iRank) {
			m_rank = iRank;
			m_step = 0;
			m_box = iBox;
			m_depth = hudDepth(iRank, HudLayer::Shape);
		} else if (iBox != m_box && m_step < g_maxSteps) {
			++m_step;
			m_box = iBox;
			m_depth =
					hudDepth(iRank, HudLayer::Shape) + static_cast<float>(m_step) / static_cast<float>(g_maxSteps + 1);
		}
		return m_depth;
	}

private:
	static constexpr uint32_t g_maxSteps = 1023;
	static auto isUntiled(const renderer::Quad2DData& iQuad) -> bool {
		return iQuad.normalMap == nullptr && std::abs(iQuad.tilingFactor.x() - 1.f) < 1e-6f &&
			   std::abs(iQuad.tilingFactor.y() - 1.f) < 1e-6f;
	}

	bool m_boxed = false;
	float m_rank = -1.f;
	float m_depth = 0.f;
	uint32_t m_step = 0;
	bool m_box = false;
};

// The box of a widget at `iTransform` (Y-up pixels): a flat quad for a plain box, else the rounded-box shader.
void drawStyledBox(const math::Transform& iTransform, const math::vec4& iColor, const component::UiBox& iBox,
				   const BoxPart iPart, const float iScale, const int iEntityId, const float iRank,
				   ShapeOrder& ioOrder) {
	if (iBox.isPlain()) {
		ioOrder.drawShape({.transform = iTransform, .color = iColor, .entityId = iEntityId}, iRank);
		return;
	}
	auto data = makeWidgetBox(iTransform, iColor, iBox, iPart, iScale);
	if (data.plain) {
		ioOrder.drawShape({.transform = data.box.transform, .color = iColor, .entityId = iEntityId}, iRank);
		return;
	}
	data.box.entityId = iEntityId;
	data.box.depth = ioOrder.depth(iRank, true);
	renderer::Renderer2D::drawBox(data.box);
}

// Smallest em height, in screen pixels, a scale mode may shrink a HUD text to: below it the glyphs turn to mush.
constexpr float g_minTextPixels = 12.f;

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
				const float iRank, ShapeOrder& ioOrder) {
	static constexpr math::vec4 closeColor{0.65f, 0.18f, 0.18f, 1.f};
	drawStyledBox(pixelTransform(iNode.bounds, iHeight), iWindow.bodyColor, iWindow.box, BoxPart::Whole, iNode.scale,
				  iEntityId, iRank, ioOrder);
	const component::UiRect::Bounds bar{
			.position = iNode.bounds.position,
			.size = {iNode.bounds.size.x(), std::min(iWindow.titleBarHeight * iNode.scale, iNode.bounds.size.y())}};
	drawStyledBox(pixelTransform(bar, iHeight), iWindow.titleColor, iWindow.box, BoxPart::Inner, iNode.scale, iEntityId,
				  iRank, ioOrder);
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
	drawStyledBox(pixelTransform(button, iHeight), closeColor, iWindow.box, BoxPart::Inner, iNode.scale, iEntityId,
				  iRank, ioOrder);
	// U+00D7 MULTIPLICATION SIGN, in the Latin-1 range of the default charset.
	drawBoxedText(button, iNode.scale, iHeight, "\xC3\x97", iWindow.titleTextColor, iWindow.titleFontSize * 1.3f,
				  data::fonts::HorizontalAlignment::Center, iEntityId, iRank);
}

void drawText(const Entity& iEntity, const HudNode& iNode, const math::Transform& iTransform, const int iEntityId,
			  const float iRank) {
	const auto& uiText = iEntity.getComponent<component::UiText>();
	shared<data::fonts::Font> font = uiText.font;
	std::string_view shown = uiText.text;
	if (const auto* context = iEntity.getScene() != nullptr ? iEntity.getScene()->getEngineContext() : nullptr;
		context != nullptr && !uiText.textKey.empty()) {
		shown = context->translate(uiText.textKey);
		if (const auto& languageFont = context->getLanguageFont();
			!languageFont.empty() && app::Application::instanced())
			font = app::Application::get().getFontLibrary().getFont(languageFont);
	}
	if (!font && app::Application::instanced())
		font = app::Application::get().getFontLibrary().getDefaultFont();
	math::Transform textTransform;
	textTransform.translation() = iTransform.translation();
	const float scale = legibleTextScale(iNode.scale, uiText.fontSize);
	textTransform.scale() = {scale, scale, 1.f};

	renderer::Renderer2D::drawString({.transform = textTransform,
									  .text = std::string{shown},
									  .font = font,
									  .color = uiText.color,
									  .layout = uiText.getLayout(iNode.bounds.size / scale),
									  .entityId = iEntityId,
									  .depth = hudDepth(iRank, HudLayer::Text),
									  .outlineColor = uiText.outlineColor,
									  .outlineWidth = uiText.outlineWidth * iNode.scale / scale,
									  .backgroundColor = uiText.backgroundColor,
									  .backgroundPadding = uiText.backgroundPadding * iNode.scale / scale});
}

void drawSlider(const component::UiSlider& iSlider, const math::Transform& iTransform, const float iScale,
				const int iEntityId, const float iRank, ShapeOrder& ioOrder) {
	drawStyledBox(iTransform, iSlider.trackColor, iSlider.box, BoxPart::Whole, iScale, iEntityId, iRank, ioOrder);
	const float norm = iSlider.getNormalized();
	if (norm > 0.f)
		drawStyledBox(leftFill(iTransform, norm), iSlider.fillColor, iSlider.box, BoxPart::Inner, iScale, iEntityId,
					  iRank, ioOrder);
	const float width = iTransform.scale().x();
	const float height = iTransform.scale().y();
	math::Transform handle = iTransform;
	handle.scale() = {height, height, 1.f};
	handle.translation().x() = iTransform.translation().x() - width * 0.5f + norm * width;

	drawStyledBox(handle, iSlider.handleColor, iSlider.box, BoxPart::NoShadow, iScale, iEntityId, iRank, ioOrder);
}

// One bar: its track and, when the content overflows, its thumb.
void drawBar(const component::UiRect::Bounds& iTrack, const component::UiRect::Bounds& iThumb, const HudScroll& iScroll,
			 const float iHeight, const int iEntityId, const float iRank, ShapeOrder& ioOrder) {
	if (iTrack.size.x() <= 0.f || iTrack.size.y() <= 0.f)
		return;
	ioOrder.drawShape(
			{.transform = pixelTransform(iTrack, iHeight), .color = iScroll.trackColor, .entityId = iEntityId}, iRank);
	ioOrder.drawShape(
			{.transform = pixelTransform(iThumb, iHeight), .color = iScroll.thumbColor, .entityId = iEntityId}, iRank);
}

void drawScrollBars(const HudScroll& iScroll, const float iHeight, const int iEntityId, const float iRank,
					ShapeOrder& ioOrder) {
	const auto& geometry = iScroll.geometry;
	if (geometry.verticalBar)
		drawBar(geometry.verticalTrack, iScroll.verticalThumb(), iScroll, iHeight, iEntityId, iRank, ioOrder);
	if (geometry.horizontalBar)
		drawBar(geometry.horizontalTrack, iScroll.horizontalThumb(), iScroll, iHeight, iEntityId, iRank, ioOrder);
}

// The background and the texts of the rows in view; the others are never touched.
void drawListRow(const component::UiList& iList, const size_t iRow, const component::UiRect::Bounds& iRowBox,
				 const float iScale, const float iHeight, const int iEntityId, const float iRank, ShapeOrder& ioOrder) {
	const auto index = static_cast<int32_t>(iRow);
	math::vec4 background = iRow % 2 == 0 ? iList.rowColor : iList.rowAltColor;
	if (index == iList.selectedIndex)
		background = iList.selectedColor;
	else if (index == iList.hoverIndex)
		background = iList.hoverColor;
	drawStyledBox(pixelTransform(iRowBox, iHeight), background, iList.box, BoxPart::NoShadow, iScale, iEntityId, iRank,
				  ioOrder);
	const auto* row = iList.findRow(iRow);
	const float margin = 6.f * iScale;
	float left = iRowBox.position.x() + margin;
	if (row != nullptr && row->icon) {
		const float side = std::max(0.f, iRowBox.size.y() - 4.f * iScale);
		const component::UiRect::Bounds iconBox{.position = {left, iRowBox.position.y() + 2.f * iScale},
												.size = {side, side}};
		ioOrder.drawShape({.transform = pixelTransform(iconBox, iHeight),
						   .color = {1.f, 1.f, 1.f, 1.f},
						   .texture = row->icon,
						   .entityId = iEntityId},
						  iRank);
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
			  const int iEntityId, const float iRank, ShapeOrder& ioOrder) {
	const auto& viewport = iScroll.geometry.viewport;
	const float pitch = iScroll.rowHeight + iScroll.rowSpacing;
	const auto [first, end] =
			visibleRows(iScroll.offset.y(), viewport.size.y(), iScroll.rowHeight, iScroll.rowSpacing, iScroll.rowCount);
	for (size_t row = first; row < end; ++row) {
		const component::UiRect::Bounds box{
				.position = {viewport.position.x(),
							 viewport.position.y() + static_cast<float>(row) * pitch - iScroll.offset.y()},
				.size = {viewport.size.x(), iScroll.rowHeight}};
		drawListRow(iList, row, box, iNode.scale, iHeight, iEntityId, iRank, ioOrder);
	}
}

// The image of a render target of the world, its rows the right way up on either backend.
void setRenderTargetImage(renderer::Quad2DData& ioQuad, const Scene& iWorld, const std::string& iName) {
	const auto framebuffer = iWorld.getRenderTarget(iName);
	if (!framebuffer)
		return;
	ioQuad.texture = framebuffer->getAttachmentTexture(0);
	const auto low = framebuffer->getLowerData();
	const auto high = framebuffer->getUpperData();
	ioQuad.textureCoords = {math::vec2{low.x(), high.y()}, math::vec2{high.x(), high.y()},
							math::vec2{high.x(), low.y()}, math::vec2{low.x(), low.y()}};
}

void drawWidget(const Scene& iWorld, const Entity& iEntity, const HudNode& iNode, const math::Transform& iTransform,
				const float iHeight, const HudScroll* iScroll, const float iRank, ShapeOrder& ioOrder) {
	const int entityId = static_cast<int>(static_cast<entt::entity>(iEntity));
	if (iEntity.hasComponent<component::UiWindow>())
		drawWindow(iEntity.getComponent<component::UiWindow>(), iNode, iHeight, entityId, iRank, ioOrder);
	if (iEntity.hasComponent<component::SpriteRenderer>()) {
		const auto& sprite = iEntity.getComponent<component::SpriteRenderer>();
		ioOrder.drawShape({.transform = iTransform,
						   .color = sprite.color,
						   .texture = sprite.texture,
						   .tilingFactor = sprite.tilingFactor,
						   .entityId = entityId},
						  iRank);
	}
	if (iEntity.hasComponent<component::Text>()) {
		const auto& text = iEntity.getComponent<component::Text>();
		renderer::Renderer2D::drawString({.transform = uniformTextTransform(iTransform),
										  .text = text.text,
										  .font = text.font,
										  .color = text.color,
										  .layout = {.kerning = text.kerning, .lineSpacing = text.lineSpacing},
										  .entityId = entityId,
										  .depth = hudDepth(iRank, HudLayer::Text),
										  .outlineColor = text.outlineColor,
										  .outlineWidth = text.outlineWidth,
										  .backgroundColor = text.backgroundColor,
										  .backgroundPadding = text.backgroundPadding});
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
	if (iEntity.hasComponent<component::UiPanel>()) {
		const auto& panel = iEntity.getComponent<component::UiPanel>();
		drawStyledBox(iTransform, panel.backgroundColor, panel.box, BoxPart::Whole, iNode.scale, entityId, iRank,
					  ioOrder);
	}
	if (iEntity.hasComponent<component::UiImage>()) {
		const auto& image = iEntity.getComponent<component::UiImage>();
		renderer::Quad2DData quad{.transform = iTransform,
								  .color = image.tint,
								  .texture = image.texture,
								  .textureCoords = image.getFrameCoords(),
								  .entityId = entityId};
		if (!image.renderTarget.empty())
			setRenderTargetImage(quad, iWorld, image.renderTarget);
		ioOrder.drawShape(std::move(quad), iRank);
	}
	if (iEntity.hasComponent<component::UiText>())
		drawText(iEntity, iNode, iTransform, entityId, iRank);
	if (iEntity.hasComponent<component::UiButton>()) {
		const auto& button = iEntity.getComponent<component::UiButton>();
		drawStyledBox(iTransform, button.getCurrentColor(), button.box, BoxPart::Whole, iNode.scale, entityId, iRank,
					  ioOrder);
	}
	if (iEntity.hasComponent<component::UiProgressBar>()) {
		const auto& bar = iEntity.getComponent<component::UiProgressBar>();
		drawStyledBox(iTransform, bar.backgroundColor, bar.box, BoxPart::Whole, iNode.scale, entityId, iRank, ioOrder);
		if (const float fraction = std::clamp(bar.value, 0.f, 1.f); fraction > 0.f)
			drawStyledBox(leftFill(iTransform, fraction), bar.fillColor, bar.box, BoxPart::Inner, iNode.scale, entityId,
						  iRank, ioOrder);
	}
	if (iEntity.hasComponent<component::UiSlider>())
		drawSlider(iEntity.getComponent<component::UiSlider>(), iTransform, iNode.scale, entityId, iRank, ioOrder);
	if (iScroll != nullptr && iScroll->isList) {
		drawList(iEntity.getComponent<component::UiList>(), iNode, *iScroll, iHeight, entityId, iRank, ioOrder);
		drawScrollBars(*iScroll, iHeight, entityId, iRank, ioOrder);
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

// Whether a widget of the scene draws a rounded box (corners, gradient, border, shadow or nine-slice).
auto hasRoundedBox(const Scene& iScene) -> bool {
	const auto any = [&iScene]<typename Widget>() -> bool {
		for (const auto view = iScene.registry.view<const Widget>(); const auto entity: view)
			if (!view.template get<const Widget>(entity).box.isPlain())
				return true;
		return false;
	};
	return any.template operator()<component::UiPanel>() || any.template operator()<component::UiButton>() ||
		   any.template operator()<component::UiSlider>() || any.template operator()<component::UiProgressBar>() ||
		   any.template operator()<component::UiList>() || any.template operator()<component::UiWindow>();
}

void drawCanvases(Scene& ioScene, const Scene& iWorld, const math::vec2ui& iViewport, const bool iEditorMode) {
	if (const auto theme = ioScene.getHudTheme(); theme)
		applyHudTheme(ioScene, *theme);
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
	ShapeOrder order{hasRoundedBox(ioScene)};
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
						   rank, order);
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
		drawWidget(iWorld, Entity{node.entity, &ioScene}, node, pixelTransform(node.bounds, height), height, scroll,
				   rank, order);
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
			return canvas->getScale(iScreen) * HudLayout::getUserScale(iScene);
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
	// A rounded widget gets a rounded frame, following its corners at the frame's distance.
	float radius = 0.f;
	if (const auto* button = ioSurface.registry.try_get<component::UiButton>(iState.focus); button != nullptr)
		radius = button->box.cornerRadius;
	else if (const auto* slider = ioSurface.registry.try_get<component::UiSlider>(iState.focus); slider != nullptr)
		radius = slider->box.cornerRadius;
	if (radius > 0.f) {
		const component::UiRect::Bounds frame{.position = position - math::vec2{thickness, thickness},
											  .size = size + math::vec2{2.f * thickness, 2.f * thickness}};
		renderer::Renderer2D::drawBox({.transform = pixelTransform(frame, height),
									   .size = frame.size,
									   .topColor = {0.f, 0.f, 0.f, 0.f},
									   .bottomColor = {0.f, 0.f, 0.f, 0.f},
									   .cornerRadius = radius * node->scale + thickness,
									   .borderWidth = thickness,
									   .borderColor = focus.highlightColor,
									   .entityId = -1});
		return;
	}
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

// A quad of `iSize` pixels centred on a point of the Y-down layout, turned by `iAngle` (radians, Y down).
auto turnedQuad(const math::vec2& iCentre, const math::vec2& iSize, const float iAngle, const float iHeight)
		-> math::Transform {
	math::Transform transform;
	transform.translation() = {iCentre.x(), iHeight - iCentre.y(), 0.f};
	transform.rotation() = {0.f, 0.f, -iAngle};
	transform.scale() = {iSize.x(), iSize.y(), 1.f};
	return transform;
}

// The arrow of an edge marker, between the icon and the border, pointing at the entity.
void drawMarkerArrow(const component::HudMarker& iMarker, const HudMarkerPlacement& iPlacement, const float iIconSize,
					 const float iHeight, const int iEntityId) {
	const float length = iIconSize * 0.6f;
	const math::vec2 direction{std::cos(iPlacement.angle), std::sin(iPlacement.angle)};
	const math::vec2 centre = iPlacement.position + direction * (iIconSize * 0.5f + length * 0.6f);
	if (iMarker.arrow) {
		renderer::Renderer2D::drawQuad({.transform = turnedQuad(centre, {length, length}, iPlacement.angle, iHeight),
										.color = iMarker.color,
										.texture = iMarker.arrow,
										.entityId = iEntityId});
		return;
	}
	// A chevron: two bars meeting at the tip, each turned 135 degrees back from the direction.
	const math::vec2 tip = centre + direction * (length * 0.5f);
	const float arm = length * 0.7f;
	for (const float side: {-1.f, 1.f}) {
		const float angle = iPlacement.angle + side * std::numbers::pi_v<float> * 0.75f;
		const math::vec2 back{std::cos(angle), std::sin(angle)};
		renderer::Renderer2D::drawQuad(
				{.transform = turnedQuad(tip + back * (arm * 0.5f), {arm, length * 0.22f}, angle, iHeight),
				 .color = iMarker.color,
				 .entityId = iEntityId});
	}
}

// One marker: its icon (a diamond without one), its edge arrow when off-screen, its label under the icon.
void drawMarker(const component::HudMarker& iMarker, const HudMarkerPlacement& iPlacement, const std::string& iLabel,
				const float iScale, const float iHeight, const int iEntityId) {
	const math::vec2 size = iMarker.size * iScale;
	if (iMarker.icon)
		renderer::Renderer2D::drawQuad({.transform = turnedQuad(iPlacement.position, size, 0.f, iHeight),
										.color = iMarker.color,
										.texture = iMarker.icon,
										.entityId = iEntityId});
	else
		renderer::Renderer2D::drawQuad(
				{.transform = turnedQuad(iPlacement.position, size / std::numbers::sqrt2_v<float>,
										 std::numbers::pi_v<float> * 0.25f, iHeight),
				 .color = iMarker.color,
				 .entityId = iEntityId});
	if (!iPlacement.onScreen)
		drawMarkerArrow(iMarker, iPlacement, std::max(size.x(), size.y()), iHeight, iEntityId);
	if (iLabel.empty())
		return;
	const float lineHeight = iMarker.fontSize * 1.4f * legibleTextScale(iScale, iMarker.fontSize);
	const float width = std::max(200.f * iScale, size.x());
	// Under the icon, or over it when the bottom border would cut the text.
	float top = iPlacement.position.y() + size.y() * 0.5f;
	if (top + lineHeight > iHeight)
		top = iPlacement.position.y() - size.y() * 0.5f - lineHeight;
	drawBoxedText({.position = {iPlacement.position.x() - width * 0.5f, top}, .size = {width, lineHeight}}, iScale,
				  iHeight, iLabel, iMarker.labelColor, iMarker.fontSize, data::fonts::HorizontalAlignment::Center,
				  iEntityId, 0.f);
}

// The markers of the world entities, under every canvas: projected through the layer of their entity.
void drawMarkers(Scene& ioScene, const math::vec2ui& iViewport, const bool iEditorMode) {
	const auto view = ioScene.registry.view<component::HudMarker>();
	if (view.empty())
		return;
	const math::vec2 screen{static_cast<float>(iViewport.x()), static_cast<float>(iViewport.y())};
	const float scale = component::Canvas{}.getScale(screen) * HudLayout::getUserScale(ioScene);
	const auto viewer = ioScene.getViewerPosition();
	bool open = false;
	for (const auto entity: view) {
		const auto& marker = view.get<component::HudMarker>(entity);
		const Entity owner{entity, &ioScene};
		if (!marker.enabled || !ioScene.isEffectivelyVisible(owner, iEditorMode))
			continue;
		const math::vec3 anchor = ioScene.getWorldTransform(owner).translation() + marker.offset;
		const float distance = viewer ? (anchor - *viewer).norm() : 0.f;
		if (marker.maxDistance > 0.f && distance > marker.maxDistance)
			continue;
		const auto point = ioScene.projectToViewport(owner, anchor, iViewport);
		if (!point)
			break;
		const auto placement = placeHudMarker(*point, screen, marker.edgeMargin * scale);
		if (!placement.onScreen && !marker.offscreenIndicator)
			continue;
		if (!open) {
			renderer::Renderer2D::beginScene(pixelCamera(iViewport));
			open = true;
		}
		std::string label = marker.label;
		if (marker.showDistance && viewer)
			label = std::format("{}{}{}m", label, label.empty() ? "" : " ", std::lround(distance));
		drawMarker(marker, placement, label, scale, screen.y(), static_cast<int>(static_cast<uint32_t>(entity)));
	}
	if (open)
		renderer::Renderer2D::endScene();
}

// Em height of a subtitle in reference pixels, before the HUD and subtitle scales.
constexpr float g_subtitleFontSize = 28.f;

// The subtitles in Play, centred at the bottom of the screen over every canvas, the newest line lowest.
void drawSubtitles(const Scene& iScene, const math::vec2ui& iViewport) {
	const auto lines = iScene.getSubtitles().getLines();
	if (lines.empty() || iScene.status == Scene::Status::Editing)
		return;
	const auto settings = iScene.getAccessibility();
	if (!settings.subtitles)
		return;
	const math::vec2 screen{static_cast<float>(iViewport.x()), static_cast<float>(iViewport.y())};
	const float scale = component::Canvas{}.getScale(screen) * HudLayout::getUserScale(iScene) * settings.subtitleScale;
	const float padding = g_subtitleFontSize * 0.3f;
	const auto font = fontOrDefault(nullptr);
	const data::fonts::TextLayout layout{.fontSize = g_subtitleFontSize,
										 .boxSize = {screen.x() * 0.8f / scale, 0.f},
										 .wrap = true};
	float bottom = screen.y() * 0.06f;
	renderer::Renderer2D::beginScene(pixelCamera(iViewport));
	for (const auto& line: std::views::reverse(lines)) {
		const float height = font ? measureTextLayout(*font, line.text, layout).diagonal().y() : g_subtitleFontSize;
		const float boxHeight = (height + 2.f * padding) * scale;
		math::Transform transform;
		transform.translation() = {screen.x() * 0.5f, bottom + boxHeight * 0.5f, 0.f};
		transform.scale() = {scale, scale, 1.f};
		renderer::Renderer2D::drawString({.transform = transform,
										  .text = line.text,
										  .font = font,
										  .color = math::vec4{1.f, 1.f, 1.f, 1.f},
										  .layout = layout,
										  .entityId = -1,
										  .outlineColor = math::vec4{0.f, 0.f, 0.f, 1.f},
										  .outlineWidth = g_subtitleFontSize * 0.06f,
										  .backgroundColor = math::vec4{0.f, 0.f, 0.f, settings.subtitleBackground},
										  .backgroundPadding = padding});
		bottom += boxHeight + padding * scale;
	}
	renderer::Renderer2D::endScene();
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

auto makeWidgetBox(const math::Transform& iTransform, const math::vec4& iColor, const component::UiBox& iBox,
				   const BoxPart iPart, const float iScale) -> WidgetBox {
	component::UiBox box = iBox;
	math::Transform transform = iTransform;
	if (iPart != BoxPart::Whole)
		box.shadowColor.a() = 0.f;
	if (iPart == BoxPart::Inner) {
		const float inset = box.borderWidth * iScale;
		transform.scale().x() = std::max(0.f, std::abs(transform.scale().x()) - 2.f * inset);
		transform.scale().y() = std::max(0.f, std::abs(transform.scale().y()) - 2.f * inset);
		box.cornerRadius = std::max(0.f, box.cornerRadius - box.borderWidth);
		box.texture.reset();
		box.borderWidth = 0.f;
	}
	WidgetBox result{.box = {.transform = transform}, .plain = box.isPlain()};
	if (result.plain)
		return result;
	auto& data = result.box;
	data.size = {std::abs(transform.scale().x()), std::abs(transform.scale().y())};
	data.topColor = iColor;
	data.bottomColor = {iColor.x() * box.bottomTint.x(), iColor.y() * box.bottomTint.y(),
						iColor.z() * box.bottomTint.z(), iColor.w() * box.bottomTint.w()};
	data.cornerRadius = box.cornerRadius * iScale;
	data.borderWidth = box.borderWidth * iScale;
	data.borderColor = box.borderColor;
	data.shadowColor = box.shadowColor;
	data.shadowOffset = math::vec2{box.shadowOffset.x(), -box.shadowOffset.y()} * iScale;
	data.shadowBlur = box.shadowBlur * iScale;
	if (box.texture && box.texture->isLoaded()) {
		const auto textureSize = box.texture->getSize();
		const float width = static_cast<float>(textureSize.x());
		const float height = static_cast<float>(textureSize.y());
		data.texture = box.texture;
		data.sliceInsets = box.sliceInsets * iScale;
		data.sliceUv = {box.sliceInsets.x() / width, box.sliceInsets.y() / height, box.sliceInsets.z() / width,
						box.sliceInsets.w() / height};
	}
	return result;
}

void HudPass::render(Scene& ioScene, const math::vec2ui& iViewport, const bool iEditorMode) {
	OWL_PROFILE_FUNCTION()

	drawMarkers(ioScene, iViewport, iEditorMode);
	drawCanvases(ioScene, ioScene, iViewport, iEditorMode);
	const auto hud = ioScene.getHud();
	Scene* const hudScene = hud && hud.get() != &ioScene ? hud.get() : nullptr;
	if (hudScene != nullptr)
		drawCanvases(*hudScene, ioScene, iViewport, iEditorMode);
	if (!iEditorMode)
		drawSubtitles(ioScene, iViewport);
	drawOverlays(ioScene, hudScene, iViewport, iEditorMode);
}

}// namespace owl::scene::ui
