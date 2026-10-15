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
#include "renderer/CameraOrtho.h"
#include "renderer/Renderer2D.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"

#include <algorithm>
#include <cmath>

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

void drawText(const Entity& iEntity, const HudNode& iNode, const math::Transform& iTransform, const int iEntityId) {
	const auto& uiText = iEntity.getComponent<component::UiText>();
	shared<data::fonts::Font> font = uiText.font;
	if (!font && app::Application::instanced())
		font = app::Application::get().getFontLibrary().getDefaultFont();
	math::Transform textTransform;
	textTransform.translation() = iTransform.translation();
	textTransform.scale() = {iNode.scale, iNode.scale, 1.f};

	renderer::Renderer2D::drawString({.transform = textTransform,
									  .text = uiText.text,
									  .font = font,
									  .color = uiText.color,
									  .layout = uiText.getLayout(iNode.bounds.size / iNode.scale),
									  .entityId = iEntityId});
}

void drawSlider(const component::UiSlider& iSlider, const math::Transform& iTransform, const int iEntityId) {
	renderer::Renderer2D::drawQuad({.transform = iTransform, .color = iSlider.trackColor, .entityId = iEntityId});
	const float norm = iSlider.getNormalized();
	if (norm > 0.f)
		renderer::Renderer2D::drawQuad(
				{.transform = leftFill(iTransform, norm), .color = iSlider.fillColor, .entityId = iEntityId});
	const float width = iTransform.scale().x();
	const float height = iTransform.scale().y();
	math::Transform handle = iTransform;
	handle.scale() = {height, height, 1.f};
	handle.translation().x() = iTransform.translation().x() - width * 0.5f + norm * width;

	renderer::Renderer2D::drawQuad({.transform = handle, .color = iSlider.handleColor, .entityId = iEntityId});
}

void drawWidget(const Entity& iEntity, const HudNode& iNode, const math::Transform& iTransform) {
	const int entityId = static_cast<int>(static_cast<entt::entity>(iEntity));
	if (iEntity.hasComponent<component::SpriteRenderer>()) {
		const auto& sprite = iEntity.getComponent<component::SpriteRenderer>();
		renderer::Renderer2D::drawQuad({.transform = iTransform,
										.color = sprite.color,
										.texture = sprite.texture,
										.tilingFactor = sprite.tilingFactor,
										.entityId = entityId});
	}
	if (iEntity.hasComponent<component::Text>()) {
		const auto& [text, font, color, kerning, lineSpacing] = iEntity.getComponent<component::Text>();
		renderer::Renderer2D::drawString({.transform = uniformTextTransform(iTransform),
										  .text = text,
										  .font = font,
										  .color = color,
										  .layout = {.kerning = kerning, .lineSpacing = lineSpacing},
										  .entityId = entityId});
	}
	if (iEntity.hasComponent<component::CircleRenderer>()) {
		const auto& [color, thickness, fade] = iEntity.getComponent<component::CircleRenderer>();
		renderer::Renderer2D::drawCircle(
				{.transform = iTransform, .color = color, .thickness = thickness, .fade = fade, .entityId = entityId});
	}
	if (iEntity.hasComponent<component::UiPanel>())
		renderer::Renderer2D::drawQuad({.transform = iTransform,
										.color = iEntity.getComponent<component::UiPanel>().backgroundColor,
										.entityId = entityId});
	if (iEntity.hasComponent<component::UiImage>()) {
		const auto& [texture, tint] = iEntity.getComponent<component::UiImage>();
		renderer::Renderer2D::drawQuad(
				{.transform = iTransform, .color = tint, .texture = texture, .entityId = entityId});
	}
	if (iEntity.hasComponent<component::UiText>())
		drawText(iEntity, iNode, iTransform, entityId);
	if (iEntity.hasComponent<component::UiButton>())
		renderer::Renderer2D::drawQuad({.transform = iTransform,
										.color = iEntity.getComponent<component::UiButton>().getCurrentColor(),
										.entityId = entityId});
	if (iEntity.hasComponent<component::UiProgressBar>()) {
		const auto& [value, backgroundColor, fillColor] = iEntity.getComponent<component::UiProgressBar>();
		renderer::Renderer2D::drawQuad({.transform = iTransform, .color = backgroundColor, .entityId = entityId});
		if (const float fraction = std::clamp(value, 0.f, 1.f); fraction > 0.f)
			renderer::Renderer2D::drawQuad(
					{.transform = leftFill(iTransform, fraction), .color = fillColor, .entityId = entityId});
	}
	if (iEntity.hasComponent<component::UiSlider>())
		drawSlider(iEntity.getComponent<component::UiSlider>(), iTransform, entityId);
}

void drawCanvases(Scene& ioScene, const math::vec2ui& iViewport, const bool iEditorMode) {
	auto& layout = ioScene.getHudLayout();
	layout.compute(ioScene, iViewport, iEditorMode);
	if (layout.getNodes().empty())
		return;
	const auto height = static_cast<float>(iViewport.y());
	// Renderer2D draws Y-up: the Y-down layout is flipped here, so the glyphs keep their orientation.
	const renderer::CameraOrtho pixels{0.f, static_cast<float>(iViewport.x()), 0.f, height};
	renderer::Renderer2D::beginScene(pixels);
	for (const auto& node: layout.getNodes()) {
		const auto center = node.bounds.getCenter();
		math::Transform transform;
		transform.translation() = {center.x(), height - center.y(), 0.f};
		transform.scale() = {node.bounds.size.x(), node.bounds.size.y(), 1.f};
		drawWidget(Entity{node.entity, &ioScene}, node, transform);
	}
	renderer::Renderer2D::endScene();
}

}// namespace

void HudPass::render(Scene& ioScene, const math::vec2ui& iViewport, const bool iEditorMode) {
	OWL_PROFILE_FUNCTION()

	drawCanvases(ioScene, iViewport, iEditorMode);
	if (const auto hud = ioScene.getHud(); hud && hud.get() != &ioScene)
		drawCanvases(*hud, iViewport, iEditorMode);
}

}// namespace owl::scene::ui
