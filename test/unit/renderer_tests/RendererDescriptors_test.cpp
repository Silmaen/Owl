/**
 * @file RendererDescriptors_test.cpp
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/gpu/RenderCommand.h>
#include <renderer/gpu/RendererDescriptors.h>

#include <array>
#include <limits>

using namespace owl::renderer::gpu;

TEST(RendererDescriptors, idIsStablePerName) {
	const auto first = RendererDescriptors::getId("RendererDescriptorsTest.first");
	const auto second = RendererDescriptors::getId("RendererDescriptorsTest.second");
	EXPECT_NE(first, second);
	EXPECT_EQ(RendererDescriptors::getId("RendererDescriptorsTest.first"), first);
	EXPECT_EQ(RendererDescriptors::getId("RendererDescriptorsTest.second"), second);
}

TEST(RendererDescriptors, idSurvivesDeclareAndRelease) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	const auto id = RendererDescriptors::getId("RendererDescriptorsTest.block");
	const std::array bindings{BindingDecl{.binding = 0, .type = BindingType::UniformBuffer}};
	RendererDescriptors::declare("RendererDescriptorsTest.block", bindings);
	EXPECT_EQ(RendererDescriptors::getId("RendererDescriptorsTest.block"), id);
	{
		const RendererDescriptors::ScopedActive scoped{id};
		const RendererDescriptors::ScopedActive unknown{std::numeric_limits<RendererId>::max()};
	}
	RendererDescriptors::release("RendererDescriptorsTest.block");
	EXPECT_EQ(RendererDescriptors::getId("RendererDescriptorsTest.block"), id);
	{ const RendererDescriptors::ScopedActive scoped{id}; }
	RendererDescriptors::releaseAll();
	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}
