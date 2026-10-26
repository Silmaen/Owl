/**
 * @file AssetCommands_test.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "testHelper.h"

#include <app/Application.h>
#include <data/assets/AssetReference.h>
#include <data/assets/AssetRegistry.h>
#include <data/assets/cook/CookCache.h>
#include <platform/FileUtils.h>
#include <renderer/TextureDecoder.h>
#include <scene/Scene.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class AssetCommandsTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.name = "asset commands",
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .isDummy = true});
		m_root = std::filesystem::temp_directory_path() / "owl_asset_commands";
		std::filesystem::remove_all(m_root);
		const std::vector<uint8_t> pixels(8 * 8 * 4, 0x40);
		writeBytes(m_root / "textures" / "a.png", renderer::encodeImagePng({8, 8}, pixels));
		writeBytes(m_root / "textures" / "b.png", renderer::encodeImagePng({8, 8}, pixels));
		writeText(m_root / "scenes" / "level.owl",
				  "Scene: level\nEntities:\n  - Entity: 1\n    SpriteRenderer:\n      texture: nam:textures/a.png\n");
		m_app->addAssetDirectory({.title = "project", .assetsPath = m_root}, true);
	}

	void TearDown() override {
		m_app->removeAssetDirectory(m_root);
		std::filesystem::remove_all(m_root);
		app::Application::invalidate();
		m_app.reset();
		core::Log::invalidate();
	}

	static void writeBytes(const std::filesystem::path& iFile, const std::vector<uint8_t>& iBytes) {
		std::filesystem::create_directories(iFile.parent_path());
		std::ofstream file(iFile, std::ios::binary);
		file.write(reinterpret_cast<const char*>(iBytes.data()), static_cast<std::streamsize>(iBytes.size()));
	}

	static void writeText(const std::filesystem::path& iFile, const std::string& iText) {
		writeBytes(iFile, std::vector<uint8_t>(iText.begin(), iText.end()));
	}

	auto run(const std::string& iName, const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		return m_registry.execute(iName, iArgs, m_scene, m_undo);
	}

	[[nodiscard]] auto metaOf(const std::string& iPath) const -> data::assets::AssetMeta {
		const auto& registry = m_app->getAssetRegistry();
		return registry.find(*registry.findGuid(iPath))->meta;
	}

	/// The application owning the asset registry.
	shared<app::Application> m_app;
	/// Temporary project.
	std::filesystem::path m_root;
	/// Registry under test.
	CommandRegistry m_registry;
	/// Scene whose history records the changes.
	shared<scene::Scene> m_scene = mkShared<scene::Scene>();
	/// Undo history of the scene.
	SceneUndoManager m_undo;
};
OWL_DIAG_POP

}// namespace

TEST_F(AssetCommandsTest, SetImportIsUndoable) {
	ASSERT_TRUE(run("asset.set_import", {{"path", std::string{"textures/a.png"}}, {"nearest", true}}));
	EXPECT_TRUE(metaOf("textures/a.png").texture.nearest);
	EXPECT_TRUE(data::assets::readMeta(m_root / "textures" / "a.png.owlmeta")->texture.nearest);
	m_undo.undo(*m_scene);
	EXPECT_FALSE(metaOf("textures/a.png").texture.nearest);
	m_undo.redo(*m_scene);
	EXPECT_TRUE(metaOf("textures/a.png").texture.nearest);
	EXPECT_EQ(run("asset.set_import", {{"path", std::string{"textures/a.png"}}, {"nearest", true}}).error().error,
			  CommandError::NotApplicable);
	EXPECT_EQ(run("asset.set_import", {{"path", std::string{"missing.png"}}}).error().error,
			  CommandError::InvalidArgument);
}

TEST_F(AssetCommandsTest, SetImportOnAFolderChangesEveryTexture) {
	const auto result = run("asset.set_import", {{"path", (m_root / "textures").string()},
												 {"compression", std::string{"Uastc"}},
												 {"mips", false}});
	ASSERT_TRUE(result);
	EXPECT_EQ(std::get<int64_t>(result->data.at("assets")), 2);
	for (const auto* path: {"textures/a.png", "textures/b.png"}) {
		EXPECT_EQ(metaOf(path).texture.compression, data::assets::TextureCompression::Uastc) << path;
		EXPECT_FALSE(metaOf(path).texture.mips) << path;
	}
}

TEST_F(AssetCommandsTest, RenameMoveAndDeleteKeepSidecarsWithTheirFile) {
	const auto guid = *m_app->getAssetRegistry().findGuid("textures/a.png");
	const auto reference = data::assets::formatReference(guid, "textures/a.png");
	ASSERT_TRUE(run("asset.rename", {{"path", std::string{"textures/a.png"}}, {"name", std::string{"hero.png"}}}));
	EXPECT_TRUE(std::filesystem::exists(m_root / "textures" / "hero.png.owlmeta"));
	EXPECT_EQ(data::assets::toAssetPath(reference), "textures/hero.png");
	EXPECT_EQ(run("asset.rename", {{"path", std::string{"textures/hero.png"}}, {"name", std::string{"b.png"}}})
					  .error()
					  .error,
			  CommandError::NotApplicable);

	std::filesystem::create_directories(m_root / "sprites");
	ASSERT_TRUE(run("asset.move", {{"path", std::string{"textures/hero.png"}}, {"folder", std::string{"sprites"}}}));
	EXPECT_EQ(data::assets::toAssetPath(reference), "sprites/hero.png");
	EXPECT_FALSE(run("asset.move", {{"path", std::string{"sprites"}}, {"folder", std::string{"sprites"}}}));

	ASSERT_TRUE(run("asset.delete", {{"path", std::string{"sprites/hero.png"}}}));
	EXPECT_FALSE(std::filesystem::exists(m_root / "sprites" / "hero.png"));
	EXPECT_FALSE(std::filesystem::exists(m_root / "sprites" / "hero.png.owlmeta"));
	EXPECT_FALSE(m_app->getAssetRegistry().find(guid));
}

TEST_F(AssetCommandsTest, CookFillsTheCacheAndClearEmptiesIt) {
	const auto cook = run("asset.cook", {{"path", std::string{"textures"}}});
	ASSERT_TRUE(cook);
	// The two textures hold the same bytes: content addressing cooks the first and serves the second from the cache.
	EXPECT_EQ(std::get<int64_t>(cook->data.at("cooked")), 1);
	EXPECT_EQ(std::get<int64_t>(cook->data.at("cached")), 1);
	data::assets::cook::CookCache cache(m_root / ".owl" / "cache" / "cooked");
	EXPECT_TRUE(cache.isCached(m_root / "textures" / "a.png", metaOf("textures/a.png")));
	ASSERT_TRUE(run("asset.clear_cache", {}));
	EXPECT_FALSE(cache.isCached(m_root / "textures" / "a.png", metaOf("textures/a.png")));
}

TEST_F(AssetCommandsTest, UpgradeReferencesRewritesTheDocuments) {
	const auto result = run("asset.upgrade_references", {});
	ASSERT_TRUE(result);
	EXPECT_EQ(std::get<int64_t>(result->data.at("files")), 1);
	const auto text = platform::fileToString(m_root / "scenes" / "level.owl");
	const auto guid = *m_app->getAssetRegistry().findGuid("textures/a.png");
	EXPECT_NE(text.find("texture: " + data::assets::formatReference(guid, "textures/a.png")), std::string::npos);
	EXPECT_EQ(std::get<int64_t>(run("asset.upgrade_references", {})->data.at("files")), 0);
}
