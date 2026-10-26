/**
 * @file HudBench.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <renderer/Renderer2D.h>
#include <scene/Entity.h>
#include <scene/component/components.h>
#include <scene/ui/HudLayout.h>
#include <scene/ui/HudPass.h>

#include <cstdint>
#include <format>
#include <string>

namespace owl::bench {

namespace {

constexpr uint32_t g_windowCount = 100;
// Each window is a panel holding a title, a button, a progress bar and an icon.
constexpr uint32_t g_widgetsPerWindow = 5;
constexpr math::vec2ui g_viewport{1920, 1080};

auto addWidget(scene::Scene& ioScene, const scene::Entity& iParent, const scene::component::UiRect::Anchor iAnchor,
			   const math::vec2& iSize, const math::vec2& iOffset) -> scene::Entity {
	auto widget = ioScene.createEntity("Widget");
	auto& rect = widget.addComponent<scene::component::UiRect>();
	rect.setAnchor(iAnchor);
	rect.size = iSize;
	rect.anchorOffset = iOffset;
	ioScene.setParent(widget, iParent);
	return widget;
}

auto makeHudScene() -> shared<scene::Scene> {
	using Anchor = scene::component::UiRect::Anchor;
	auto sc = mkShared<scene::Scene>();
	auto canvas = sc->createEntity("Hud");
	canvas.addComponent<scene::component::Canvas>();
	for (uint32_t i = 0; i < g_windowCount; ++i) {
		const math::vec2 corner{static_cast<float>(i % 10) * 125.f + 70.f, static_cast<float>(i / 10) * 70.f + 40.f};
		auto window = addWidget(*sc, canvas, Anchor::TopLeft, {120.f, 64.f}, corner);
		window.addComponent<scene::component::UiPanel>();
		addWidget(*sc, window, Anchor::TopCenter, {110.f, 14.f}, {0.f, 10.f})
				.addComponent<scene::component::UiText>()
				.text = std::format("Window {}", i);
		addWidget(*sc, window, Anchor::Center, {60.f, 18.f}, {0.f, 4.f}).addComponent<scene::component::UiButton>();
		addWidget(*sc, window, Anchor::BottomCenter, {100.f, 6.f}, {0.f, -6.f})
				.addComponent<scene::component::UiProgressBar>()
				.value = 0.5f;
		addWidget(*sc, window, Anchor::TopLeft, {12.f, 12.f}, {8.f, 8.f}).addComponent<scene::component::UiImage>();
	}
	sc->onViewportResize(g_viewport);
	return sc;
}

// A list of a thousand rows at the size of a vehicle list: only the rows in view are laid out, drawn and hit-tested.
auto makeListScene(const uint32_t iRows) -> shared<scene::Scene> {
	auto sc = mkShared<scene::Scene>();
	auto canvas = sc->createEntity("Hud");
	canvas.addComponent<scene::component::Canvas>();
	auto list = addWidget(*sc, canvas, scene::component::UiRect::Anchor::TopLeft, {320.f, 600.f}, {40.f, 40.f});
	auto& data = list.addComponent<scene::component::UiList>();
	data.setCount(iRows);
	data.scrollOffset = static_cast<float>(iRows) * 15.f;
	sc->onViewportResize(g_viewport);
	return sc;
}

}// namespace

void runHudBenches(Runner& ioRunner) {
	if (!ioRunner.wants("hud"))
		return;
	const auto sc = makeHudScene();
	constexpr uint32_t widgets = g_windowCount * g_widgetsPerWindow;
	auto& layout = sc->getHudLayout();
	ioRunner.measure(std::format("hud/layout/{}_widgets", widgets), widgets,
					 [&]() -> void { layout.compute(*sc, g_viewport, false); });
	const auto frameName = std::format("hud/frame/{}_widgets", widgets);
	ioRunner.measure(frameName, widgets, [&]() -> void { scene::ui::HudPass::render(*sc, g_viewport, false); });
	if (ioRunner.wants(frameName)) {
		renderer::Renderer2D::resetStats();
		scene::ui::HudPass::render(*sc, g_viewport, false);
		ioRunner.metric(frameName + "_draw_calls", static_cast<double>(renderer::Renderer2D::getStats().drawCalls),
						"draw calls (one frame)");
		ioRunner.metric(std::format("hud/layout/{}_widgets_nodes", widgets),
						static_cast<double>(layout.getNodes().size()), "widgets laid out");
	}
	constexpr uint32_t rows = 1000;
	const auto listScene = makeListScene(rows);
	const auto listName = std::format("hud/frame/{}_row_list", rows);
	ioRunner.measure(listName, rows, [&]() -> void { scene::ui::HudPass::render(*listScene, g_viewport, false); });
	if (!ioRunner.wants(listName))
		return;
	renderer::Renderer2D::resetStats();
	scene::ui::HudPass::render(*listScene, g_viewport, false);
	ioRunner.metric(listName + "_quads", static_cast<double>(renderer::Renderer2D::getStats().quadCount),
					"quads (one frame)");
}

}// namespace owl::bench
