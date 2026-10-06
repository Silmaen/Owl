/**
 * @file Renderer2DBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <app/Application.h>
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
