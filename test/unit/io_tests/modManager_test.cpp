/**
 * @file modManager_test.cpp
 * @author Silmaen
 * @date 16/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <data/assets/pack/ExtensionRegistry.h>
#include <data/assets/pack/ModManager.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::data::assets::pack;

namespace {

void writeText(const std::filesystem::path& iFile, const std::string& iText) {
	std::filesystem::create_directories(iFile.parent_path());
	std::ofstream file(iFile, std::ios::binary);
	file << iText;
}

auto idsOf(const ModManager& iManager) -> std::vector<std::string> {
	std::vector<std::string> ids;
	for (const auto& mod: iManager.getMods()) ids.push_back(mod.manifest.id);
	return ids;
}

auto mountedIds(const ExtensionRegistry& iRegistry) -> std::vector<std::string> {
	std::vector<std::string> ids;
	for (const auto* pack: iRegistry.getPacks()) ids.push_back(pack->getManifest().id);
	return ids;
}

class ModManagerFixture : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_mod_manager_test";
		std::filesystem::remove_all(m_dir);
		makeMod("alpha", "  loadOrder: 1\n");
		makeMod("beta", "");
		makeMod("gamma", "  dependencies: [beta]\n");
	}

	void TearDown() override {
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	void makeMod(const std::string& iId, const std::string& iExtra) const {
		writeText(m_dir / "mods" / iId / "extension.yml", "OwlExtension:\n  id: " + iId + "\n" + iExtra);
		writeText(m_dir / "mods" / iId / "textures" / "shared.png", iId);
		writeText(m_dir / "mods" / iId / "textures" / (iId + ".png"), iId);
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(ModManagerFixture, registryReportsConflictsAndTakesALoadOrder) {
	ExtensionRegistry registry;
	EXPECT_EQ(registry.mountFolder(m_dir / "mods"), 3u);
	EXPECT_EQ(mountedIds(registry), (std::vector<std::string>{"beta", "gamma", "alpha"}));
	const auto conflicts = registry.getConflicts();
	ASSERT_EQ(conflicts.size(), 1u);
	EXPECT_EQ(conflicts[0].path, "textures/shared.png");
	EXPECT_EQ(conflicts[0].providers, (std::vector<std::string>{"beta", "gamma", "alpha"}));
	registry.clear();
	EXPECT_EQ(registry.mount(ExtensionRegistry::listSources(m_dir / "mods"), {{"alpha", -1}, {"beta", 0}}), 3u);
	EXPECT_EQ(mountedIds(registry), (std::vector<std::string>{"alpha", "beta", "gamma"}));
	const auto shared = registry.findEntries(".png");
	ASSERT_EQ(shared.size(), 4u);
	EXPECT_EQ(shared[0].path, "textures/alpha.png");
	EXPECT_EQ(shared[3].path, "textures/shared.png");
	EXPECT_EQ(shared[3].id, "gamma");
	EXPECT_TRUE(registry.findEntries(".owlprefab").empty());
}

TEST_F(ModManagerFixture, choicesAreKeptSavedAndApplied) {
	const auto settings = m_dir / "user" / "mods.yml";
	ModManager manager;
	EXPECT_FALSE(manager.isConfigured());
	EXPECT_FALSE(manager.save());
	manager.configure(m_dir / "mods", settings);
	ASSERT_TRUE(manager.isConfigured());
	EXPECT_EQ(idsOf(manager), (std::vector<std::string>{"beta", "gamma", "alpha"}));
	EXPECT_TRUE(manager.isDirty());
	EXPECT_EQ(manager.find("none"), nullptr);
	EXPECT_FALSE(manager.setEnabled("none", false));
	EXPECT_FALSE(manager.move("none", 1));
	EXPECT_FALSE(manager.move("beta", -1));

	EXPECT_TRUE(manager.move("gamma", -5));
	EXPECT_TRUE(manager.setEnabled("alpha", false));
	EXPECT_EQ(idsOf(manager), (std::vector<std::string>{"gamma", "beta", "alpha"}));
	EXPECT_EQ(manager.getEnabledSources().size(), 2u);
	const auto order = manager.getLoadOrder();
	EXPECT_EQ(order.at("gamma"), 0);
	EXPECT_EQ(order.at("alpha"), 2);

	ExtensionRegistry registry;
	EXPECT_EQ(registry.mount(manager.getEnabledSources(), manager.getLoadOrder()), 2u);
	// gamma moved to the top, but it needs beta: the dependency still loads first.
	EXPECT_EQ(mountedIds(registry), (std::vector<std::string>{"beta", "gamma"}));
	manager.update(registry);
	EXPECT_FALSE(manager.isDirty());
	EXPECT_TRUE(manager.find("gamma")->mounted);
	EXPECT_FALSE(manager.find("alpha")->mounted);
	EXPECT_TRUE(manager.find("alpha")->issue.empty());
	ASSERT_TRUE(manager.save());

	ModManager reloaded;
	reloaded.configure(m_dir / "mods", settings);
	EXPECT_EQ(idsOf(reloaded), (std::vector<std::string>{"gamma", "beta", "alpha"}));
	EXPECT_FALSE(reloaded.find("alpha")->enabled);

	makeMod("delta", "  loadOrder: -3\n");
	std::filesystem::remove_all(m_dir / "mods" / "beta");
	reloaded.rescan();
	EXPECT_EQ(idsOf(reloaded), (std::vector<std::string>{"gamma", "alpha", "delta"}));
	ExtensionRegistry broken;
	std::ignore = broken.mount(reloaded.getEnabledSources(), reloaded.getLoadOrder());
	reloaded.update(broken);
	EXPECT_FALSE(reloaded.find("gamma")->mounted);
	EXPECT_NE(reloaded.find("gamma")->issue.find("beta"), std::string::npos);

	reloaded.reset();
	EXPECT_FALSE(reloaded.isConfigured());
	EXPECT_TRUE(reloaded.getMods().empty());
}

TEST_F(ModManagerFixture, unreadableChoicesAreIgnored) {
	writeText(m_dir / "user" / "mods.yml", "OwlMods: [not, a, map");
	ModManager manager;
	manager.configure(m_dir / "mods", m_dir / "user" / "mods.yml");
	EXPECT_EQ(manager.getMods().size(), 3u);
	writeText(m_dir / "user" / "mods.yml", "FormatVersion: 99\nOwlMods: {}\n");
	manager.configure(m_dir / "mods", m_dir / "user" / "mods.yml");
	EXPECT_EQ(manager.getEnabledSources().size(), 3u);
}

TEST_F(ModManagerFixture, applicationMountsTheChoices) {
	const auto app = mkShared<app::Application>(app::AppParams{.renderer = renderer::gpu::RenderAPI::Type::Null,
															   .hasGui = false,
															   .useDebugging = false,
															   .isDummy = true});
	EXPECT_EQ(app->applyMods(), 0u);
	app->getModManager().configure(m_dir / "mods", m_dir / "user" / "mods.yml");
	ASSERT_TRUE(app->getModManager().setEnabled("beta", false));
	EXPECT_EQ(app->applyMods(), 1u);
	EXPECT_EQ(mountedIds(app->getExtensions()), (std::vector<std::string>{"alpha"}));
	EXPECT_EQ(std::as_const(*app).getModManager().find("gamma")->mounted, false);
	EXPECT_TRUE(std::filesystem::exists(m_dir / "user" / "mods.yml"));
	std::ignore = app->setExtensionSources({});
	app->getModManager().reset();
	app::Application::invalidate();
}
