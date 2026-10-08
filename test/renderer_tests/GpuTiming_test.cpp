/**
 * @file GpuTiming_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/gpu/RenderCommand.h>

TEST(GpuTiming, NullBackendIsNoOp) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	using owl::renderer::gpu::RenderCommand;
	RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	EXPECT_FALSE(RenderCommand::hasGpuTimestamps());
	RenderCommand::setGpuTimestampsEnabled(true);
	RenderCommand::beginFrame();
	EXPECT_EQ(RenderCommand::getGpuFrameId(), 0u);
	RenderCommand::endFrame();
	EXPECT_TRUE(RenderCommand::popGpuFrameTimings().empty());
	RenderCommand::setVSync(false);
	EXPECT_EQ(RenderCommand::getPresentMode(), "none");
	EXPECT_EQ(RenderCommand::getDeviceName(), "none");
	const auto counters = RenderCommand::getRenderCounters();
	EXPECT_EQ(counters.submits, 0u);
	EXPECT_EQ(counters.queueWaitIdles, 0u);
	EXPECT_EQ(counters.deviceWaitIdles, 0u);
	EXPECT_EQ(counters.fenceWaits, 0u);
	RenderCommand::setGpuTimestampsEnabled(false);
	RenderCommand::invalidate();
	EXPECT_FALSE(RenderCommand::hasGpuTimestamps());
	EXPECT_TRUE(RenderCommand::popGpuFrameTimings().empty());
	EXPECT_EQ(RenderCommand::getPresentMode(), "none");
	owl::core::Log::invalidate();
}
