/**
 * @file Renderer2DBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <app/Application.h>
#include <data/fonts/TextLayout.h>
#include <renderer/CameraOrtho.h>
#include <renderer/Renderer2D.h>
#include <renderer/gpu/Texture.h>

#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace owl::bench {

namespace {

auto makeTransforms(const uint32_t iCount) -> std::vector<math::Transform> {
	std::vector<math::Transform> transforms;
	transforms.reserve(iCount);
	for (uint32_t i = 0; i < iCount; ++i) {
		math::Transform tr;
		tr.translation() = {static_cast<float>(i % 300), static_cast<float>(i / 300), 0.f};
		tr.scale() = {0.9f, 0.9f, 1.f};
		transforms.push_back(tr);
	}
	return transforms;
}

template<typename Fn>
void measureFrame(Runner& ioRunner, const std::string& iName, const uint32_t iCount,
				  const renderer::CameraOrtho& iCamera, Fn&& iDraws) {
	const auto frame = [&]() -> void {
		renderer::Renderer2D::beginScene(iCamera);
		iDraws();
		renderer::Renderer2D::endScene();
	};
	ioRunner.measure(iName, iCount, frame);
	if (!ioRunner.wants(iName))
		return;
	renderer::Renderer2D::resetStats();
	frame();
	ioRunner.metric(iName + "_draw_calls", renderer::Renderer2D::getStats().drawCalls, "draw calls (one frame)");
}

// Transient draws (no scene world index): quads and circles sharing the transient matrices, rects, polylines.
void runTransientMixes(Runner& ioRunner, const renderer::CameraOrtho& iCamera,
					   const std::vector<math::Transform>& iTransforms) {
	const auto count = static_cast<uint32_t>(iTransforms.size());
	measureFrame(ioRunner, std::format("renderer2d/transient_quads_circles/{}", count), count, iCamera, [&]() -> void {
		for (uint32_t i = 0; i < count; ++i) {
			if ((i & 1U) == 0)
				renderer::Renderer2D::drawQuad({.transform = iTransforms[i], .color = {1.f, 0.5f, 0.2f, 1.f}});
			else
				renderer::Renderer2D::drawCircle({.transform = iTransforms[i], .color = {1.f, 1.f, 1.f, 1.f}});
		}
	});
	measureFrame(ioRunner, std::format("renderer2d/rects/{}", count), count, iCamera, [&]() -> void {
		for (uint32_t i = 0; i < count; ++i)
			renderer::Renderer2D::drawRect({.transform = iTransforms[i], .color = {0.f, 1.f, 0.f, 1.f}});
	});
	renderer::PolyLineData polyLine{.transform = {},
									.points = {{0.f, 0.f, 0.f},
											   {1.f, 0.f, 0.f},
											   {1.f, 1.f, 0.f},
											   {0.5f, 1.5f, 0.f},
											   {0.f, 1.f, 0.f},
											   {-0.5f, 0.5f, 0.f},
											   {0.f, 0.2f, 0.f},
											   {0.2f, 0.f, 0.f}},
									.closed = true,
									.color = {0.f, 0.f, 1.f, 1.f}};
	const uint32_t polyCount = count / static_cast<uint32_t>(polyLine.points.size());
	measureFrame(ioRunner, std::format("renderer2d/polylines_8/{}", count), count, iCamera, [&]() -> void {
		for (uint32_t i = 0; i < polyCount; ++i) {
			polyLine.transform = iTransforms[i];
			renderer::Renderer2D::drawPolyLine(polyLine);
		}
	});
}

void runQuads(Runner& ioRunner, const renderer::CameraOrtho& iCamera) {
	std::vector<shared<renderer::gpu::Texture>> textures;
	for (uint32_t i = 0; i < 16; ++i)
		textures.push_back(renderer::gpu::Texture2D::create(
				renderer::gpu::Texture::Specification{.size = {64, 64}, .format = renderer::gpu::ImageFormat::Rgba8}));
	for (const uint32_t count: {1000U, 10000U, 100000U}) {
		if (!ioRunner.wants("renderer2d"))
			return;
		const auto transforms = makeTransforms(count);
		ioRunner.measure(std::format("renderer2d/transform_to_matrix/{}", count), count, [&]() -> void {
			for (const auto& tr: transforms) doNotOptimize(tr());
		});
		measureFrame(ioRunner, std::format("renderer2d/quads_color/{}", count), count, iCamera, [&]() -> void {
			for (uint32_t i = 0; i < count; ++i)
				renderer::Renderer2D::drawQuad({.transform = transforms[i], .color = {1.f, 0.5f, 0.2f, 1.f}});
		});
		measureFrame(ioRunner, std::format("renderer2d/quads_world_index/{}", count), count, iCamera, [&]() -> void {
			for (uint32_t i = 0; i < count; ++i)
				renderer::Renderer2D::drawQuad(
						{.transform = {}, .worldIndex = static_cast<int32_t>(i), .color = {1.f, 0.5f, 0.2f, 1.f}});
		});
		measureFrame(ioRunner, std::format("renderer2d/quads_16_textures/{}", count), count, iCamera, [&]() -> void {
			for (uint32_t i = 0; i < count; ++i)
				renderer::Renderer2D::drawQuad({.transform = transforms[i], .texture = textures[i % textures.size()]});
		});
		measureFrame(ioRunner, std::format("renderer2d/circles/{}", count), count, iCamera, [&]() -> void {
			for (uint32_t i = 0; i < count; ++i)
				renderer::Renderer2D::drawCircle({.transform = transforms[i], .color = {1.f, 1.f, 1.f, 1.f}});
		});
		if (count < 10000U)
			continue;
		runTransientMixes(ioRunner, iCamera, transforms);
	}
}

void runText(Runner& ioRunner, const renderer::CameraOrtho& iCamera) {
	if (!ioRunner.wants("renderer2d/text") || !app::Application::instanced())
		return;
	const auto& font = app::Application::get().getFontLibrary().getDefaultFont();
	if (font == nullptr)
		return;
	const std::string line = "The quick brown fox jumps over the lazy dog 0123456789";
	const auto transforms = makeTransforms(100);
	ioRunner.measure("renderer2d/text/100_strings_54_chars", 100 * line.size(), [&]() -> void {
		renderer::Renderer2D::beginScene(iCamera);
		for (const auto& tr: transforms)
			renderer::Renderer2D::drawString(
					{.transform = tr, .text = line, .font = font, .color = {1.f, 1.f, 1.f, 1.f}});
		renderer::Renderer2D::endScene();
	});
	// 50 code points, a third of them beyond U+00FF: 200 strings make 10 000 glyphs, one batch.
	const std::string utf8Line =
			"\u0152uvre \u00E0 20 \u20AC \u201Cl\u2019\u00E9t\u00E9\u201D \u2026 \u0161\u0165\u017E\u0151 "
			"\u0153\u0153\u0153 fa\u00E7ade \u2013 na\u00EFve \u0141\u00F3d";
	const auto utf8Transforms = makeTransforms(200);
	measureFrame(ioRunner, "renderer2d/text/10000_glyphs_utf8", 10000U, iCamera, [&]() -> void {
		for (const auto& tr: utf8Transforms)
			renderer::Renderer2D::drawString(
					{.transform = tr, .text = utf8Line, .font = font, .color = {1.f, 1.f, 1.f, 1.f}});
	});
	// Same glyphs wrapped and shrunk in a box: the layout must keep them in one batch.
	const data::fonts::TextLayout wrapped{.boxSize = {12.f, 3.f}, .wrap = true, .fit = data::fonts::TextFit::Shrink};
	measureFrame(ioRunner, "renderer2d/text/10000_glyphs_wrapped", 10000U, iCamera, [&]() -> void {
		for (const auto& tr: utf8Transforms)
			renderer::Renderer2D::drawString({.transform = tr,
											  .text = utf8Line,
											  .font = font,
											  .color = {1.f, 1.f, 1.f, 1.f},
											  .layout = wrapped});
	});
}

}// namespace

void runRenderer2DBenches(Runner& ioRunner) {
	if (!ioRunner.wants("renderer2d"))
		return;
	const renderer::CameraOrtho camera(0, 300, 0, 400);
	runQuads(ioRunner, camera);
	runText(ioRunner, camera);
}

}// namespace owl::bench
