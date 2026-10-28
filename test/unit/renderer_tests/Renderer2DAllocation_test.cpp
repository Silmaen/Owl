/**
 * @file Renderer2DAllocation_test.cpp
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <renderer/Renderer.h>
#include <renderer/Renderer2D.h>
#include <renderer/gpu/Texture.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <vector>

// Counting replaces the global allocator: Linux only (a Windows DLL keeps its own), never under a sanitizer's.
#if defined(OWL_PLATFORM_LINUX) && !defined(OWL_SANITIZER_CUSTOM_ALLOCATOR)
#define OWL_COUNT_ALLOCATIONS 1
// NOLINTBEGIN(*-no-malloc,cppcoreguidelines-owning-memory,misc-new-delete-overloads)
namespace {
// True while the allocations of the test thread are counted.
std::atomic<bool> g_counting{false};
// Allocations seen while counting.
std::atomic<size_t> g_allocations{0};

auto countedAlloc(const size_t iSize) -> void* {
	if (g_counting.load(std::memory_order_relaxed))
		g_allocations.fetch_add(1, std::memory_order_relaxed);
	if (void* const mem = std::malloc(iSize == 0 ? 1 : iSize); mem != nullptr)
		return mem;
	throw std::bad_alloc{};
}
}// namespace

// Replaced for this test binary only: an allocation inside a counted frame fails the test below.
auto operator new(const size_t iSize) -> void* { return countedAlloc(iSize); }

auto operator new[](const size_t iSize) -> void* { return countedAlloc(iSize); }

void operator delete(void* iMemory) noexcept { std::free(iMemory); }

void operator delete[](void* iMemory) noexcept { std::free(iMemory); }

void operator delete(void* iMemory, [[maybe_unused]] size_t iSize) noexcept { std::free(iMemory); }

void operator delete[](void* iMemory, [[maybe_unused]] size_t iSize) noexcept { std::free(iMemory); }
// NOLINTEND(*-no-malloc,cppcoreguidelines-owning-memory,misc-new-delete-overloads)
#endif

using namespace owl;
using namespace owl::renderer;

#ifdef OWL_COUNT_ALLOCATIONS
namespace {

void drawFrame(const CameraOrtho& iCamera, const std::vector<math::Transform>& iTransforms,
			   const shared<gpu::Texture>& iTexture, const StringData& iText, const PolyLineData& iPolyLine) {
	Renderer2D::beginScene(iCamera);
	for (const auto& tr: iTransforms) {
		Renderer2D::drawQuad({.transform = tr, .color = {1.f, 0.5f, 0.2f, 1.f}});
		Renderer2D::drawQuad({.transform = tr, .texture = iTexture});
		Renderer2D::drawCircle({.transform = tr, .color = {1.f, 1.f, 1.f, 1.f}});
	}
	for (size_t i = 0; i < 100; ++i) {
		Renderer2D::drawRect({.transform = iTransforms[i]});
		Renderer2D::drawPolyLine(iPolyLine);
	}
	if (iText.font != nullptr)
		Renderer2D::drawString(iText);
	Renderer2D::endScene();
}

}// namespace
#endif

TEST(Renderer2D, transientFrameDoesNotAllocate) {
#ifndef OWL_COUNT_ALLOCATIONS
	GTEST_SKIP() << "Allocation counting needs the global operator new of this binary.";
#else
	core::Log::init(core::Log::Level::Off);
	auto app = mkShared<app::Application>(app::AppParams{.renderer = gpu::RenderAPI::Type::Null,
														 .hasGui = false,
														 .useDebugging = false,
														 .isDummy = true});
	const auto font = app->getFontLibrary().getDefaultFont();
	gpu::RenderCommand::create(gpu::RenderAPI::Type::Null);
	Renderer::init();
	const CameraOrtho camera(0, 300, 0, 400);
	std::vector<math::Transform> transforms(12000);
	for (size_t i = 0; i < transforms.size(); ++i)
		transforms[i].translation() = {static_cast<float>(i % 300), static_cast<float>(i) / 300.f, 0.f};
	const shared<gpu::Texture> texture =
			gpu::Texture2D::create(gpu::Texture::Specification{.size = {4, 4}, .format = gpu::ImageFormat::Rgba8});
	const PolyLineData polyLine{.transform = {},
								.points = {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 1.f, 0.f}, {0.f, 1.f, 0.f}},
								.closed = true};
	const StringData text{.transform = {}, .text = "Zero allocation \xc3\xa9t\xc3\xa9\n2D", .font = font};

	drawFrame(camera, transforms, texture, text, polyLine);
	Renderer2D::resetStats();
	g_allocations = 0;
	g_counting = true;
	drawFrame(camera, transforms, texture, text, polyLine);
	g_counting = false;

	EXPECT_EQ(g_allocations.load(), 0u);
	const auto stats = Renderer2D::getStats();
	// 24 000 quads and 12 000 circles, each with a transient matrix: two batches of 20 000 transients.
	EXPECT_GE(stats.drawCalls, 4u);
	EXPECT_EQ(stats.lineCount, 800u);

	Renderer::shutdown();
	gpu::RenderCommand::invalidate();
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
#endif
}
