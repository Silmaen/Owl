/**
 * @file TextureAsync_test.cpp
 * @author Silmaen
 * @date 23/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "testHelper.h"

#include "core/task/Scheduler.h"
#include "renderer/RenderCommand.h"
#include "renderer/Texture.h"

using namespace owl::renderer;

namespace {

auto getFixturePathStr() -> std::string {
	return (owl::test::getRootPath() / "engine_assets" / "textures" / "mario.png").string();
}

class TextureAsyncFixture : public ::testing::Test {
protected:
	void SetUp() override {
		owl::core::Log::init(owl::core::Log::Level::Off);
		RenderCommand::create(RenderAPI::Type::Null);
	}

	void TearDown() override {
		RenderCommand::invalidate();
		owl::core::Log::invalidate();
	}

	owl::core::task::Scheduler m_scheduler;
};

}// namespace

TEST_F(TextureAsyncFixture, PlaceholderIsReturnedImmediately) {
	const auto tex = Texture2D::createFromSerializedAsync("pat:" + getFixturePathStr(), m_scheduler);
	ASSERT_NE(tex, nullptr);
	EXPECT_EQ(tex->getLoadState(), LoadState::Pending);
	// Placeholder is sized to the real image.
	EXPECT_GT(tex->getSize().x(), 0u);
	EXPECT_GT(tex->getSize().y(), 0u);
	// Format is always Rgba8 in the async path (placeholder sizing assumes 4 channels).
	EXPECT_EQ(tex->getSpecification().format, ImageFormat::Rgba8);
	m_scheduler.waitEmptyQueue();
}

TEST_F(TextureAsyncFixture, SuccessfulLoadFlipsToReady) {
	const auto tex = Texture2D::createFromSerializedAsync("pat:" + getFixturePathStr(), m_scheduler);
	ASSERT_NE(tex, nullptr);
	EXPECT_EQ(tex->getLoadState(), LoadState::Pending);
	m_scheduler.waitEmptyQueue();
	EXPECT_EQ(tex->getLoadState(), LoadState::Ready);
}

TEST_F(TextureAsyncFixture, EmptyPrefixFallsBackToSync) {
	const auto tex = Texture2D::createFromSerializedAsync("emp:", m_scheduler);
	ASSERT_NE(tex, nullptr);
	EXPECT_FALSE(tex->isLoaded());
	EXPECT_EQ(tex->getLoadState(), LoadState::Ready);
}

TEST_F(TextureAsyncFixture, SpecPrefixFallsBackToSync) {
	const auto tex = Texture2D::createFromSerializedAsync("siz:32:32:Rgba8:true", m_scheduler);
	ASSERT_NE(tex, nullptr);
	EXPECT_EQ(tex->getSpecification().size.x(), 32u);
	EXPECT_EQ(tex->getSpecification().size.y(), 32u);
	EXPECT_EQ(tex->getLoadState(), LoadState::Ready);
}

TEST_F(TextureAsyncFixture, UnresolvableNameReturnsNull) {
	const auto tex = Texture2D::createFromSerializedAsync("nam:does_not_exist_xyz.png", m_scheduler);
	EXPECT_EQ(tex, nullptr);
}

TEST_F(TextureAsyncFixture, UnknownPrefixReturnsNull) {
	const auto tex = Texture2D::createFromSerializedAsync("xyz:whatever", m_scheduler);
	EXPECT_EQ(tex, nullptr);
}

TEST_F(TextureAsyncFixture, CreateFromSerializedForDeserializeStaysSyncWithoutApp) {
	// No Application instance → must fall back to the synchronous path and hand back a ready texture.
	const auto tex = Texture2D::createFromSerializedForDeserialize("pat:" + getFixturePathStr());
	ASSERT_NE(tex, nullptr);
	EXPECT_EQ(tex->getLoadState(), LoadState::Ready);
}

TEST_F(TextureAsyncFixture, SyncCreateFromSerializedStaysSynchronous) {
	// Contract preserved for packaging code: `createFromSerialized` never leaves the texture Pending.
	const auto tex = Texture2D::createFromSerialized("pat:" + getFixturePathStr());
	ASSERT_NE(tex, nullptr);
	EXPECT_EQ(tex->getLoadState(), LoadState::Ready);
}
