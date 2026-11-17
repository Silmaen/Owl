/**
 * @file extensionPack_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <data/assets/AssetSearchPaths.h>
#include <data/assets/pack/ExtensionManifest.h>
#include <data/assets/pack/ExtensionPack.h>
#include <data/assets/pack/ExtensionRegistry.h>
#include <data/assets/pack/PackWriter.h>
#include <renderer/Renderer.h>
#include <renderer/TextureDecoder.h>
#include <renderer/gpu/Texture.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::data::assets::pack;

namespace {

auto manifestOf(const std::string& iId, const std::vector<ExtensionDependency>& iDependencies = {},
				const int32_t iLoadOrder = 0) -> ExtensionManifest {
	return ExtensionManifest{.id = iId,
							 .name = {},
							 .version = {},
							 .kind = ExtensionKind::Data,
							 .loadOrder = iLoadOrder,
							 .author = {},
							 .description = {},
							 .dependencies = iDependencies,
							 .script = {}};
}

auto idsOf(const ExtensionResolution& iResolution, const std::vector<ExtensionManifest>& iManifests)
		-> std::vector<std::string> {
	std::vector<std::string> ids;
	for (const size_t index: iResolution.order) ids.push_back(iManifests[index].id);
	return ids;
}

void writeText(const std::filesystem::path& iFile, const std::string& iText) {
	std::filesystem::create_directories(iFile.parent_path());
	std::ofstream file(iFile, std::ios::binary);
	file << iText;
}

auto textOf(const std::optional<std::vector<uint8_t>>& iBytes) -> std::string {
	return iBytes ? std::string{iBytes->begin(), iBytes->end()} : std::string{"<none>"};
}

}// namespace

TEST(ExtensionManifest, versionParsesAndCompares) {
	EXPECT_EQ(ExtensionVersion::parse("1.4.2").value_or(ExtensionVersion{}).toString(), "1.4.2");
	EXPECT_EQ(ExtensionVersion::parse("2").value_or(ExtensionVersion{}).toString(), "2.0.0");
	EXPECT_EQ(ExtensionVersion::parse("1.4").value_or(ExtensionVersion{}).toString(), "1.4.0");
	EXPECT_FALSE(ExtensionVersion::parse("").has_value());
	EXPECT_FALSE(ExtensionVersion::parse("1..2").has_value());
	EXPECT_FALSE(ExtensionVersion::parse("1.2.3.4").has_value());
	EXPECT_FALSE(ExtensionVersion::parse("v1").has_value());
	EXPECT_FALSE(ExtensionVersion::parse("1.2.").has_value());
	EXPECT_LT(*ExtensionVersion::parse("1.9.9"), *ExtensionVersion::parse("1.10"));
	EXPECT_EQ(*ExtensionVersion::parse("1"), *ExtensionVersion::parse("1.0.0"));
}

TEST(ExtensionManifest, idRules) {
	EXPECT_TRUE(isValidExtensionId("demo.recolour"));
	EXPECT_TRUE(isValidExtensionId("a_b-2"));
	EXPECT_FALSE(isValidExtensionId(""));
	EXPECT_FALSE(isValidExtensionId(".hidden"));
	EXPECT_FALSE(isValidExtensionId("Upper"));
	EXPECT_FALSE(isValidExtensionId("with space"));
	EXPECT_FALSE(isValidExtensionId("../escape"));
	EXPECT_FALSE(isValidExtensionId(std::string(65, 'a')));
	EXPECT_EQ(parseKind("Script"), ExtensionKind::Script);
	EXPECT_FALSE(parseKind("Native").has_value());
	EXPECT_EQ(getKindName(ExtensionKind::Data), "Data");
}

TEST(ExtensionManifest, serializeRoundTrips) {
	core::Log::init(core::Log::Level::Off);
	const ExtensionManifest manifest{
			.id = "demo.recolour",
			.name = "Recoloured items",
			.version = *ExtensionVersion::parse("1.2.3"),
			.kind = ExtensionKind::Script,
			.loadOrder = -4,
			.author = "Silmaen",
			.description = "Items in other colours.",
			.dependencies = {{.id = "base.items", .minVersion = *ExtensionVersion::parse("0.5")}},
			.script = "scripts/items/main.lua"};
	const auto text = manifest.serialize();
	EXPECT_NE(text.find("OwlExtension:"), std::string::npos);
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);
	const auto read = ExtensionManifest::parse(text, "test");
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, manifest);
	EXPECT_EQ(read->getDisplayName(), "Recoloured items");
	EXPECT_EQ(manifestOf("x").getDisplayName(), "x");
	core::Log::invalidate();
}

TEST(ExtensionManifest, parseDefaultsAndErrors) {
	core::Log::init(core::Log::Level::Off);
	const auto minimal = ExtensionManifest::parse("OwlExtension:\n  id: tiny\n  dependencies: [other]\n", "test");
	ASSERT_TRUE(minimal.has_value());
	EXPECT_EQ(minimal->version.toString(), "1.0.0");
	EXPECT_EQ(minimal->kind, ExtensionKind::Data);
	ASSERT_EQ(minimal->dependencies.size(), 1u);
	EXPECT_EQ(minimal->dependencies.front().minVersion.toString(), "0.0.0");
	EXPECT_EQ(ExtensionManifest::parse("[unclosed", "test").error(), ManifestError::Unreadable);
	EXPECT_EQ(ExtensionManifest::parse("- a list", "test").error(), ManifestError::Unreadable);
	EXPECT_EQ(ExtensionManifest::parse("Other: {}", "test").error(), ManifestError::Unreadable);
	EXPECT_EQ(ExtensionManifest::parse("FormatVersion: 99\nOwlExtension:\n  id: a\n", "test").error(),
			  ManifestError::NewerVersion);
	EXPECT_EQ(ExtensionManifest::parse("OwlExtension:\n  name: no id\n", "test").error(), ManifestError::InvalidId);
	EXPECT_EQ(ExtensionManifest::parse("OwlExtension:\n  id: a\n  version: x\n", "test").error(),
			  ManifestError::InvalidVersion);
	EXPECT_EQ(ExtensionManifest::parse("OwlExtension:\n  id: a\n  kind: Native\n", "test").error(),
			  ManifestError::UnknownKind);
	EXPECT_EQ(
			ExtensionManifest::parse("OwlExtension:\n  id: a\n  dependencies: [{id: b, version: z}]\n", "test").error(),
			ManifestError::InvalidDependency);
	EXPECT_EQ(ExtensionManifest::parse("OwlExtension:\n  id: a\n  loadOrder: [1]\n", "test").error(),
			  ManifestError::Unreadable);
	for (const auto error:
		 {ManifestError::Unreadable, ManifestError::NewerVersion, ManifestError::InvalidId,
		  ManifestError::InvalidVersion, ManifestError::UnknownKind, ManifestError::InvalidDependency})
		EXPECT_FALSE(describe(error).empty());
	core::Log::invalidate();
}

TEST(ExtensionResolver, dependenciesFirstThenLoadOrderThenId) {
	const std::vector manifests{manifestOf("zeta"), manifestOf("child", {{.id = "base", .minVersion = {}}}, -10),
								manifestOf("base", {}, 5), manifestOf("alpha"), manifestOf("early", {}, -1)};
	const auto resolution = resolveLoadOrder(manifests);
	EXPECT_TRUE(resolution.issues.empty());
	EXPECT_EQ(idsOf(resolution, manifests), (std::vector<std::string>{"early", "alpha", "zeta", "base", "child"}));
}

TEST(ExtensionResolver, leavesOutBrokenPacks) {
	const std::vector manifests{
			manifestOf("dup"),
			manifestOf("dup"),
			manifestOf("needs.missing", {{.id = "missing", .minVersion = {}}}),
			manifestOf("needs.new", {{.id = "dup", .minVersion = *ExtensionVersion::parse("2.0")}}),
			manifestOf("needs.rejected", {{.id = "needs.missing", .minVersion = {}}}),
			manifestOf("cycle.a", {{.id = "cycle.b", .minVersion = {}}}),
			manifestOf("cycle.b", {{.id = "cycle.a", .minVersion = {}}}),
			manifestOf("self", {{.id = "self", .minVersion = {}}}),
	};
	const auto resolution = resolveLoadOrder(manifests);
	EXPECT_EQ(idsOf(resolution, manifests), std::vector<std::string>{"dup"});
	const auto errorOf = [&resolution](const std::string& iId) -> std::optional<ExtensionLoadError> {
		for (const auto& issue: resolution.issues)
			if (issue.id == iId)
				return issue.error;
		return std::nullopt;
	};
	EXPECT_EQ(errorOf("needs.missing"), ExtensionLoadError::MissingDependency);
	EXPECT_EQ(errorOf("needs.new"), ExtensionLoadError::DependencyTooOld);
	EXPECT_EQ(errorOf("needs.rejected"), ExtensionLoadError::DependencyRejected);
	EXPECT_EQ(errorOf("cycle.a"), ExtensionLoadError::DependencyCycle);
	EXPECT_EQ(errorOf("cycle.b"), ExtensionLoadError::DependencyCycle);
	EXPECT_EQ(errorOf("self"), ExtensionLoadError::DependencyCycle);
	EXPECT_TRUE(std::ranges::any_of(resolution.issues, [](const ExtensionIssue& iIssue) -> bool {
		return iIssue.id == "dup" && iIssue.error == ExtensionLoadError::DuplicateId;
	}));
	for (const auto error:
		 {ExtensionLoadError::CannotOpen, ExtensionLoadError::NoManifest, ExtensionLoadError::InvalidManifest,
		  ExtensionLoadError::DuplicateId, ExtensionLoadError::MissingDependency, ExtensionLoadError::DependencyTooOld,
		  ExtensionLoadError::DependencyRejected, ExtensionLoadError::DependencyCycle, ExtensionLoadError::WriteFailed})
		EXPECT_FALSE(describe(error).empty());
}

TEST(ExtensionPack, packAssetTypes) {
	EXPECT_EQ(getPackAssetType("scenes/a.owl"), AssetType::Scene);
	EXPECT_EQ(getPackAssetType("a.png"), AssetType::Texture);
	EXPECT_EQ(getPackAssetType("a.glb"), AssetType::Mesh);
	EXPECT_EQ(getPackAssetType("a.ogg"), AssetType::Sound);
	EXPECT_EQ(getPackAssetType("a.lua"), AssetType::Script);
	EXPECT_EQ(getPackAssetType("a.ttf"), AssetType::Font);
	EXPECT_EQ(getPackAssetType("a.owlmat"), AssetType::Other);
	EXPECT_EQ(getPackAssetType("a.txt"), AssetType::Other);
}

namespace {

class ExtensionPackFixture : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_extension_pack_test";
		std::filesystem::remove_all(m_dir);
	}

	void TearDown() override {
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	[[nodiscard]] auto makeFolder(const std::string& iId, const std::string& iExtra = {}) const
			-> std::filesystem::path {
		const auto folder = m_dir / "src" / iId;
		writeText(folder / "extension.yml", "OwlExtension:\n  id: " + iId + "\n" + iExtra);
		writeText(folder / "textures" / "hero.png", "hero of " + iId);
		writeText(folder / "textures" / "hero.png.owlmeta", "sidecar");
		writeText(folder / "scripts" / "rule.lua", "-- rule of " + iId);
		writeText(folder / ".git" / "config", "hidden");
		return folder;
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(ExtensionPackFixture, folderPackListsItsEntries) {
	const auto folder = makeFolder("data.pack");
	auto pack = ExtensionPack::open(folder);
	ASSERT_TRUE(pack.has_value());
	EXPECT_FALSE(pack->isPacked());
	EXPECT_EQ(pack->getManifest().id, "data.pack");
	EXPECT_EQ(pack->getSource(), folder);
	EXPECT_EQ(pack->listEntries(), std::vector<std::string>{"textures/hero.png"});
	EXPECT_TRUE(pack->contains("textures/hero.png"));
	EXPECT_FALSE(pack->contains("extension.yml"));
	EXPECT_FALSE(pack->contains("scripts/rule.lua"));
	EXPECT_EQ(textOf(pack->readEntry("textures/hero.png")), "hero of data.pack");
	EXPECT_FALSE(pack->readEntry("scripts/rule.lua").has_value());
	const ExtensionPack moved = std::move(*pack);
	EXPECT_EQ(moved.listEntries().size(), 1u);
	const auto script = ExtensionPack::open(makeFolder("script.pack", "  kind: Script\n"));
	ASSERT_TRUE(script.has_value());
	EXPECT_EQ(script->listEntries(), (std::vector<std::string>{"scripts/rule.lua", "textures/hero.png"}));
}

TEST_F(ExtensionPackFixture, openRefusesWhatIsNoPack) {
	EXPECT_EQ(ExtensionPack::open(m_dir / "missing.owlmod").error(), ExtensionLoadError::CannotOpen);
	std::filesystem::create_directories(m_dir / "empty");
	EXPECT_EQ(ExtensionPack::open(m_dir / "empty").error(), ExtensionLoadError::NoManifest);
	writeText(m_dir / "bad" / "extension.yml", "OwlExtension:\n  id: Bad Id\n");
	EXPECT_EQ(ExtensionPack::open(m_dir / "bad").error(), ExtensionLoadError::InvalidManifest);
	PackWriter writer;
	writer.addData({1, 2, 3}, "textures/a.png", AssetType::Texture);
	ASSERT_TRUE(writer.write(m_dir / "plain.owlmod"));
	EXPECT_EQ(ExtensionPack::readManifest(m_dir / "plain.owlmod").error(), ExtensionLoadError::NoManifest);
	EXPECT_EQ(writeExtensionPack(m_dir / "none", m_dir / "out.owlmod").error(), ExtensionLoadError::CannotOpen);
	EXPECT_EQ(writeExtensionPack(m_dir / "empty", m_dir / "out.owlmod").error(), ExtensionLoadError::NoManifest);
}

TEST_F(ExtensionPackFixture, writtenPackReadsLikeItsFolder) {
	const auto folder = makeFolder("script.pack", "  kind: Script\n  version: 2.1.0\n");
	const auto file = m_dir / "out" / "script.pack.owlmod";
	const auto written = writeExtensionPack(folder, file);
	ASSERT_TRUE(written.has_value());
	EXPECT_EQ(*written, 2u);
	auto pack = ExtensionPack::open(file);
	ASSERT_TRUE(pack.has_value());
	EXPECT_TRUE(pack->isPacked());
	EXPECT_EQ(pack->getManifest().version.toString(), "2.1.0");
	EXPECT_EQ(pack->listEntries(), (std::vector<std::string>{"scripts/rule.lua", "textures/hero.png"}));
	EXPECT_EQ(textOf(pack->readEntry("scripts/rule.lua")), "-- rule of script.pack");
	EXPECT_EQ(writeExtensionPack(folder, m_dir / "out").error(), ExtensionLoadError::WriteFailed);
}

TEST_F(ExtensionPackFixture, registryOverridesInLoadOrder) {
	std::ignore = makeFolder("base");
	std::ignore = makeFolder("late", "  loadOrder: 5\n");
	std::ignore = makeFolder("needs.base", "  dependencies: [{id: base, version: 1.0.0}]\n  loadOrder: -5\n");
	std::ignore = makeFolder("broken", "  dependencies: [absent]\n");
	writeText(m_dir / "src" / "base" / "textures" / "only_base.png", "base only");
	writeText(m_dir / "src" / "not_a_pack.txt", "ignored");
	ASSERT_TRUE(writeExtensionPack(makeFolder("packed"), m_dir / "src" / "packed.owlmod").has_value());
	std::filesystem::remove_all(m_dir / "src" / "packed");
	writeText(m_dir / "src" / "junk.owlmod", "not a pack");

	EXPECT_EQ(ExtensionRegistry::listSources(m_dir / "src").size(), 6u);
	EXPECT_TRUE(ExtensionRegistry::listSources(m_dir / "absent").empty());
	ExtensionRegistry registry;
	EXPECT_TRUE(registry.isEmpty());
	EXPECT_FALSE(registry.contains("textures/hero.png"));
	EXPECT_EQ(registry.mountFolder(m_dir / "src"), 4u);
	std::vector<std::string> ids;
	for (const auto* pack: registry.getPacks()) ids.push_back(pack->getManifest().id);
	EXPECT_EQ(ids, (std::vector<std::string>{"base", "needs.base", "packed", "late"}));
	EXPECT_EQ(registry.getIssues().size(), 2u);
	EXPECT_EQ(registry.findProvider("textures/hero.png")->getManifest().id, "late");
	EXPECT_EQ(registry.findProvider("textures/../textures/only_base.png")->getManifest().id, "base");
	EXPECT_EQ(textOf(registry.readEntry("textures/hero.png")), "hero of late");
	EXPECT_EQ(registry.findProvider(""), nullptr);
	EXPECT_FALSE(registry.readEntry("textures/none.png").has_value());
	EXPECT_EQ(registry.listEntries(), (std::vector<std::string>{"textures/hero.png", "textures/only_base.png"}));
	ASSERT_NE(registry.find("packed"), nullptr);
	EXPECT_TRUE(registry.find("packed")->isPacked());
	EXPECT_EQ(registry.find("broken"), nullptr);
	EXPECT_EQ(registry.mount({m_dir / "src" / "base"}), 4u);
	std::ignore = makeFolder("absent");
	EXPECT_EQ(registry.mount({m_dir / "src" / "absent"}), 6u);
	ExtensionRegistry moved = std::move(registry);
	EXPECT_EQ(moved.getPacks().size(), 6u);
	moved.clear();
	EXPECT_TRUE(moved.isEmpty());
	EXPECT_TRUE(moved.getIssues().empty());
}

namespace {

class ExtensionAssetSource : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
		m_dir = std::filesystem::temp_directory_path() / "owl_extension_asset_source_test";
		std::filesystem::remove_all(m_dir);
		writeText(m_dir / "mod" / "extension.yml", "OwlExtension:\n  id: mod\n");
		writeText(m_dir / "mod" / "textures" / "ui" / "hero.png", "modded hero");
		writeText(m_dir / "mod" / "textures" / "new.png", "new texture");
		writeText(m_dir / "game" / "textures" / "ui" / "hero.png", "game hero");
		m_app->addAssetDirectory({.title = "game", .assetsPath = m_dir / "game"});
	}

	void TearDown() override {
		m_app->getExtensions().clear();
		m_app->closePack();
		std::filesystem::remove_all(m_dir);
		app::Application::invalidate();
		m_app.reset();
		core::Log::invalidate();
	}

	shared<app::Application> m_app;
	std::filesystem::path m_dir;
};

}// namespace

TEST_F(ExtensionAssetSource, modsOverrideTheDiskAndThePack) {
	using data::assets::readAssetText;
	EXPECT_FALSE(m_app->hasPackedAssets());
	EXPECT_EQ(readAssetText("textures/ui/hero.png").value_or(""), "game hero");
	ASSERT_EQ(m_app->getExtensions().mount({m_dir / "mod"}), 1u);
	EXPECT_TRUE(m_app->hasPackedAssets());
	EXPECT_FALSE(m_app->hasOpenPack());
	EXPECT_EQ(readAssetText("textures/ui/hero.png").value_or(""), "modded hero");
	EXPECT_EQ(readAssetText("textures/new.png").value_or(""), "new texture");
	EXPECT_TRUE(data::assets::isPackedAsset("textures/new.png"));
	EXPECT_EQ(data::assets::findPackedAsset("new", {".png"}).value_or(""), "textures/new.png");

	PackWriter writer;
	const std::string packed = "packed hero";
	writer.addData(std::vector<uint8_t>(packed.begin(), packed.end()), "textures/ui/hero.png", AssetType::Texture);
	writer.addData(std::vector<uint8_t>(packed.begin(), packed.end()), "textures/packed.png", AssetType::Texture);
	ASSERT_TRUE(writer.write(m_dir / "game.owlpack"));
	ASSERT_TRUE(m_app->openPack(m_dir / "game.owlpack"));
	EXPECT_EQ(textOf(m_app->loadFromPack("textures/ui/hero.png")), "modded hero");
	EXPECT_EQ(textOf(m_app->loadFromPack("textures/packed.png")), "packed hero");
	EXPECT_TRUE(m_app->packContains("textures/new.png"));
	EXPECT_TRUE(m_app->packContains("textures/packed.png"));
	EXPECT_FALSE(m_app->packContains("textures/none.png"));
	EXPECT_EQ(data::assets::findPackedAsset("packed", {".png"}).value_or(""), "textures/packed.png");
	m_app->getExtensions().clear();
	EXPECT_EQ(textOf(m_app->loadFromPack("textures/ui/hero.png")), "packed hero");
	const auto& constApp = *m_app;
	EXPECT_TRUE(constApp.getExtensions().isEmpty());
}

TEST_F(ExtensionAssetSource, setExtensionSourcesReloadsTheOverriddenTextures) {
	const auto writePng = [](const std::filesystem::path& iFile, const uint32_t iSide) -> void {
		std::filesystem::create_directories(iFile.parent_path());
		const std::vector<uint8_t> pixels(static_cast<size_t>(iSide) * iSide * 4, 0x80);
		ASSERT_TRUE(renderer::writeImagePng(iFile, {iSide, iSide}, pixels));
	};
	writePng(m_dir / "game" / "textures" / "kit.png", 2);
	writePng(m_dir / "mod" / "textures" / "kit.png", 4);
	auto texture = renderer::gpu::Texture2D::create(std::filesystem::path{"textures/kit.png"});
	ASSERT_NE(texture, nullptr);
	EXPECT_EQ(texture->getSize(), (math::vec2ui{2, 2}));
	renderer::Renderer::getTextureLibrary().add("nam:textures/kit.png", texture);
	EXPECT_EQ(m_app->setExtensionSources({m_dir / "mod"}), 1u);
	EXPECT_EQ(texture->getSize(), (math::vec2ui{4, 4}));
	EXPECT_EQ(m_app->getHotReload().getLastReport().reloaded, 1u);
	EXPECT_EQ(m_app->setExtensionSources({}), 0u);
	EXPECT_TRUE(m_app->getExtensions().isEmpty());
	EXPECT_EQ(texture->getSize(), (math::vec2ui{2, 2}));
	std::filesystem::remove(m_dir / "game" / "textures" / "kit.png");
	EXPECT_FALSE(texture->reloadFromAssets());
	const auto unnamed = renderer::gpu::Texture2D::create(renderer::gpu::Texture::Specification{.size = {1, 1}});
	ASSERT_NE(unnamed, nullptr);
	EXPECT_FALSE(unnamed->reloadFromAssets());
	std::ignore = renderer::Renderer::getTextureLibrary().remove("nam:textures/kit.png");
	EXPECT_EQ(m_app->getHotReload().reloadAssets({}).reloaded, 0u);
}
