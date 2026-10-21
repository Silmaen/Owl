/**
 * @file RenderGraph_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/RenderGraph.h>
#include <renderer/gpu/RenderCommand.h>

#include <string>
#include <vector>

using namespace owl::renderer;
using Graph = RenderGraph;

namespace {
class RenderGraphTest : public testing::Test {
protected:
	void SetUp() override {
		owl::core::Log::init(owl::core::Log::Level::Off);
		gpu::RenderCommand::create(gpu::RenderAPI::Type::Null);
	}

	void TearDown() override {
		gpu::RenderCommand::invalidate();
		owl::core::Log::invalidate();
	}
};

auto writes(const Graph::Resource iTarget) -> Graph::Setup {
	return [iTarget](Graph::PassBuilder& ioBuilder) -> void { ioBuilder.write(iTarget); };
}

auto copies(const Graph::Resource iFrom, const Graph::Resource iTo) -> Graph::Setup {
	return [iFrom, iTo](Graph::PassBuilder& ioBuilder) -> void {
		ioBuilder.read(iFrom);
		ioBuilder.write(iTo);
	};
}

}// namespace

TEST_F(RenderGraphTest, keepsTheDeclarationOrderOfDependentPasses) {
	Graph graph;
	const auto target = graph.importTarget("target");
	const auto scene = graph.createTarget("scene", Graph::ColorSpace::Linear);
	graph.addPass("world", writes(scene), {});
	graph.addPass("sprites", writes(scene), {});
	graph.addPass("encode", copies(scene, target), {});
	graph.addPass("hud", writes(target), {});
	ASSERT_TRUE(graph.compile());
	EXPECT_EQ(graph.getCompiledPasses(), (std::vector<std::string>{"world", "sprites", "encode", "hud"}));
}

TEST_F(RenderGraphTest, dropsThePassesNothingReads) {
	Graph graph;
	const auto target = graph.importTarget("target");
	const auto scene = graph.createTarget("scene", Graph::ColorSpace::Linear);
	const auto unused = graph.createTarget("unused", Graph::ColorSpace::Display);
	graph.addPass("world", writes(scene), {});
	graph.addPass("orphan", copies(scene, unused), {});
	graph.addPass("encode", copies(scene, target), {});
	ASSERT_TRUE(graph.compile());
	EXPECT_EQ(graph.getCompiledPasses(), (std::vector<std::string>{"world", "encode"}));
}

TEST_F(RenderGraphTest, refusesAReadOfAnUnwrittenTarget) {
	Graph graph;
	const auto target = graph.importTarget("target");
	const auto scene = graph.createTarget("scene", Graph::ColorSpace::Linear);
	graph.addPass("encode", copies(scene, target), {});
	EXPECT_FALSE(graph.compile());
}

TEST_F(RenderGraphTest, refusesAPassWithoutTarget) {
	Graph graph;
	graph.addPass("nothing", {}, {});
	EXPECT_FALSE(graph.compile());
}

TEST_F(RenderGraphTest, refusesAPassReadingItsOwnTarget) {
	Graph graph;
	const auto target = graph.importTarget("target");
	graph.addPass("clear", writes(target), {});
	graph.addPass("feedback", copies(target, target), {});
	EXPECT_FALSE(graph.compile());
}

TEST_F(RenderGraphTest, runsThePassesWithTheirTargetsAndPoolsTheTransients) {
	Graph graph;
	std::vector<std::string> runs;
	gpu::Framebuffer* sceneFramebuffer = nullptr;
	for (int frame = 0; frame < 3; ++frame) {
		graph.reset();
		const auto target = graph.importTarget("target");
		const auto scene = graph.createTarget("scene", Graph::ColorSpace::Linear);
		graph.addPass("world", writes(scene), [&](const Graph::PassContext& iContext) -> void {
			runs.emplace_back("world");
			sceneFramebuffer = iContext.getFramebuffer(scene);
			EXPECT_EQ(gpu::Framebuffer::getBound(), sceneFramebuffer);
			EXPECT_TRUE(gpu::Framebuffer::isLinearTargetBound());
		});
		graph.addPass("encode", copies(scene, target), [&](const Graph::PassContext& iContext) -> void {
			runs.emplace_back("encode");
			EXPECT_EQ(iContext.getFramebuffer(target), nullptr);
			EXPECT_EQ(gpu::Framebuffer::getBound(), nullptr);
			EXPECT_EQ(iContext.getSize(), (owl::math::vec2ui{32, 16}));
		});
		ASSERT_TRUE(graph.compile());
		graph.execute({32, 16});
	}
	ASSERT_NE(sceneFramebuffer, nullptr);
	EXPECT_TRUE(sceneFramebuffer->isLinearColor());
	EXPECT_EQ(sceneFramebuffer->getSpecification().size, (owl::math::vec2ui{32, 16}));
	EXPECT_EQ(runs.size(), 6);
	EXPECT_EQ(graph.getPooledTargetCount(), 1);
	graph.releasePool();
	EXPECT_EQ(graph.getPooledTargetCount(), 0);
}

TEST_F(RenderGraphTest, givesBackTheBoundFramebuffer) {
	const auto target =
			gpu::Framebuffer::create({.size = {8, 8},
									  .attachments = {{.format = gpu::AttachmentSpecification::Format::Surface,
													   .tiling = gpu::AttachmentSpecification::Tiling::Optimal}},
									  .samples = 1,
									  .swapChainTarget = false,
									  .debugName = "target"});
	target->bind();
	Graph graph;
	const auto imported = graph.importTarget("target");
	const auto scene = graph.createTarget("scene", Graph::ColorSpace::Linear);
	graph.addPass("world", writes(scene), {});
	graph.addPass("encode", copies(scene, imported), [&](const Graph::PassContext& iContext) -> void {
		EXPECT_EQ(iContext.getFramebuffer(imported), target.get());
		EXPECT_EQ(gpu::Framebuffer::getBound(), target.get());
	});
	ASSERT_TRUE(graph.compile());
	graph.execute({8, 8});
	EXPECT_EQ(gpu::Framebuffer::getBound(), target.get());
	EXPECT_FALSE(gpu::Framebuffer::isLinearTargetBound());
	target->unbind();
}

TEST_F(RenderGraphTest, reusesAReleasedTransientInTheSameFrame) {
	Graph graph;
	const auto target = graph.importTarget("target");
	const auto first = graph.createTarget("first", Graph::ColorSpace::Linear);
	const auto second = graph.createTarget("second", Graph::ColorSpace::Linear);
	graph.addPass("a", writes(first), {});
	graph.addPass("b", copies(first, target), {});
	graph.addPass("c", writes(second), {});
	graph.addPass("d", copies(second, target), {});
	ASSERT_TRUE(graph.compile());
	graph.execute({4, 4});
	EXPECT_EQ(graph.getPooledTargetCount(), 1);
}
