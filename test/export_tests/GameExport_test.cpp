/**
 * @file GameExport_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/external/yaml.h>
#include <data/assets/pack/GameExporter.h>
#include <data/assets/pack/PackReader.h>
#include <renderer/gpu/Texture.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifdef OWL_PLATFORM_LINUX
#include <sys/wait.h>
#endif

using namespace owl;
using namespace owl::data::assets::pack;

namespace {

// Runner file name the exporter looks for on this platform.
#ifdef OWL_PLATFORM_WINDOWS
constexpr auto g_runnerFile = "OwlRunner.exe";
#else
constexpr auto g_runnerFile = "OwlRunner";
#endif

void writeFile(const std::filesystem::path& iPath, const std::string& iContent) {
	std::filesystem::create_directories(iPath.parent_path());
	std::ofstream out(iPath, std::ios::binary);
	out << iContent;
}

auto readFile(const std::filesystem::path& iPath) -> std::string {
	std::ifstream in(iPath, std::ios::binary);
	std::stringstream buffer;
	buffer << in.rdbuf();
	return buffer.str();
}

auto makeTempDir(const std::string& iName) -> std::filesystem::path {
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	auto dir = std::filesystem::temp_directory_path() / std::format("owl_export_{}_{}", iName, stamp);
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	return dir;
}

auto readPackEntry(const std::filesystem::path& iPack, const std::string& iEntry) -> std::string {
	PackReader reader;
	if (!reader.open(iPack))
		return {};
	const auto data = reader.readEntry(iEntry);
	return data ? std::string(data->begin(), data->end()) : std::string{};
}

#ifdef OWL_PLATFORM_LINUX
auto runCommand(const std::string& iCommand) -> int {
	// NOLINTNEXTLINE(concurrency-mt-unsafe,cert-env33-c,bugprone-command-processor) Launching the game is the test.
	const int status = std::system(iCommand.c_str());
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
#endif

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class GameExporterFixture : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "gameExporter",
														  .assetsPattern = "",
														  .icon = "",
														  .width = 0,
														  .height = 0,
														  .argCount = 0,
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
	}

	static void TearDownTestSuite() {
		app::Application::invalidate();
		s_app.reset();
		core::Log::invalidate();
	}

	void SetUp() override {
		m_root = makeTempDir(testing::UnitTest::GetInstance()->current_test_info()->name());
		m_project = m_root / "project";
		m_runner = m_root / "runner";
		writeFile(m_project / "textures" / "a.png", "PNG-A");
		writeFile(m_project / "textures" / "b.png", "PNG-B");
		writeFile(m_project / "logo" / "icon.png", "ICON");
		writeFile(m_project / "game_settings.yml", "GameSettings: []\n");
		writeFile(m_project / "tilesets" / "blocks.owltileset", "Tileset: blocks\ntexture: nam:textures/b.png\n");
		writeFile(m_project / "scenes" / "level.owl", std::format("Scene: level\nEntities:\n"
																  "  - Entity: 1\n"
																  "    SpriteRenderer:\n"
																  "      texture: pat:{}\n"
																  "  - Entity: 2\n"
																  "    VoxelWorld:\n"
																  "      Tileset: tilesets/blocks.owltileset\n",
																  (m_project / "textures" / "a.png").string()));
		writeFile(m_runner / g_runnerFile, "RUNNER");
		writeFile(m_runner / "libfake.so", "NEW-LIB");
		app::Application::get().addAssetDirectory({"Project: test", m_project});
	}

	void TearDown() override {
		app::Application::get().removeAssetDirectory(m_project);
		std::filesystem::remove_all(m_root);
	}

	[[nodiscard]] auto makeSettings() const -> ExportSettings {
		return {.gameName = "My Game: Deluxe",
				.firstScene = "scenes/level",
				.version = "1.2",
				.author = "Tester",
				.description = "A # tricky: description",
				.icon = "logo/icon.png",
				.projectDirectory = m_project,
				.outputDirectory = m_root / "out",
				.runnerDirectory = m_runner,
				.windowSize = {800, 600},
				.fullscreen = false,
				.resizable = true,
				.rendererStack = {},
				.packFlags = PackFlags::Default};
	}

	std::filesystem::path m_root;
	std::filesystem::path m_project;
	std::filesystem::path m_runner;
	inline static shared<app::Application> s_app;
};
OWL_DIAG_POP

}// namespace

TEST(GameExporter, SanitizeFilename) {
	EXPECT_EQ(GameExporter::sanitizeFilename("My Game: Deluxe"), "My_Game__Deluxe");
	EXPECT_EQ(GameExporter::sanitizeFilename("a/b\\c*d?e\"f<g>h|i"), "a_b_c_d_e_f_g_h_i");
	EXPECT_EQ(GameExporter::sanitizeFilename("Plain"), "Plain");
}

TEST(GameExporter, ErrorMessages) {
	for (const auto error: {ExportError::NoAssets, ExportError::OutputDirectory, ExportError::PackWrite,
							ExportError::RunnerNotFound, ExportError::RunnerCopy, ExportError::Cancelled})
		EXPECT_FALSE(GameExporter::getErrorMessage(error).empty());
}

TEST_F(GameExporterFixture, RelocateAbsolutePaths) {
	const std::vector<AssetReference> assets{{.packPath = "textures/a.png",
											  .diskPath = m_project / "textures" / "a.png",
											  .assetType = AssetType::Texture}};
	const auto text =
			std::format("a: pat:{}\nb: \"pat:{}\"\nc: pat:/no/such/file.png\nd: nam:keep.png\n",
						(m_project / "textures" / "a.png").string(), (m_project / "textures" / "a.png").string());
	EXPECT_EQ(GameExporter::relocateAbsolutePaths(text, assets),
			  "a: nam:textures/a.png\nb: \"nam:textures/a.png\"\nc: pat:/no/such/file.png\nd: nam:keep.png\n");
}

TEST_F(GameExporterFixture, TextureInsideAssetDirectorySerializesByName) {
	const auto texture = renderer::gpu::Texture2D::create(m_project / "textures" / "a.png");
	ASSERT_NE(texture, nullptr);
	EXPECT_EQ(texture->getSerializeString(), "nam:textures/a.png");
	const auto outside = m_root / "outside.png";
	writeFile(outside, "PNG");
	const auto other = renderer::gpu::Texture2D::create(outside);
	ASSERT_NE(other, nullptr);
	EXPECT_EQ(other->getSerializeString(), "pat:" + outside.string());
}

TEST_F(GameExporterFixture, ValidateReportsMissingPieces) {
	auto settings = makeSettings();
	std::vector<AssetReference> assets;
	EXPECT_TRUE(GameExporter::validate(settings, assets).empty());
	EXPECT_FALSE(assets.empty());
	settings.icon = "logo/missing.png";
	settings.runnerDirectory = m_root / "nowhere";
	EXPECT_EQ(GameExporter::validate(settings, assets).size(), 2u);
	settings.firstScene = "scenes/missing";
	EXPECT_GE(GameExporter::validate(settings, assets).size(), 3u);
	EXPECT_TRUE(assets.empty());
}

TEST_F(GameExporterFixture, ExportWritesAPlayableFolder) {
	const auto settings = makeSettings();
	const auto gameDir = settings.outputDirectory / "My_Game__Deluxe";
	writeFile(gameDir / "libfake.so", "STALE-LIB");
	const auto result = GameExporter::exportGame(settings, {});
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->gameDirectory, gameDir);
	EXPECT_EQ(result->packFile, gameDir / "My_Game__Deluxe.owlpack");
	EXPECT_GT(result->packBytes, 0u);
	EXPECT_EQ(readFile(gameDir / "libfake.so"), "NEW-LIB");
	EXPECT_EQ(readFile(gameDir / "logo" / "icon.png"), "ICON");
	EXPECT_EQ(readFile(result->executable), "RUNNER");
#ifdef OWL_PLATFORM_LINUX
	EXPECT_TRUE(exists(gameDir / "launch.sh"));
	EXPECT_NE(std::filesystem::status(result->executable).permissions() & std::filesystem::perms::owner_exec,
			  std::filesystem::perms::none);
#endif

	const auto config = YAML::LoadFile((gameDir / "runner.yml").string())["RunnerConfig"];
	ASSERT_TRUE(config);
	EXPECT_EQ(config["GameName"].as<std::string>(), "My Game: Deluxe");
	EXPECT_EQ(config["FirstScene"].as<std::string>(), "scenes/level.owl");
	EXPECT_EQ(config["PackFile"].as<std::string>(), "My_Game__Deluxe.owlpack");
	EXPECT_EQ(config["Icon"].as<std::string>(), "logo/icon.png");
	EXPECT_EQ(config["WindowWidth"].as<uint32_t>(), 800u);
	const auto info = YAML::LoadFile((gameDir / "game_info.yml").string())["GameInfo"];
	ASSERT_TRUE(info);
	EXPECT_EQ(info["Description"].as<std::string>(), "A # tricky: description");

	PackReader reader;
	ASSERT_TRUE(reader.open(result->packFile));
	for (const auto* entry:
		 {"scenes/level.owl", "textures/a.png", "textures/b.png", "tilesets/blocks.owltileset", "game_settings.yml"})
		EXPECT_TRUE(reader.contains(entry)) << entry;
	reader.close();
	const auto scene = readPackEntry(result->packFile, "scenes/level.owl");
	EXPECT_NE(scene.find("nam:textures/a.png"), std::string::npos);
	EXPECT_EQ(scene.find("pat:"), std::string::npos);
}

TEST_F(GameExporterFixture, ExportFailures) {
	auto settings = makeSettings();
	settings.runnerDirectory = m_root / "nowhere";
	EXPECT_EQ(GameExporter::exportGame(settings, {}).error(), ExportError::RunnerNotFound);
	settings = makeSettings();
	settings.firstScene = "scenes/missing";
	EXPECT_EQ(GameExporter::exportGame(settings, {}).error(), ExportError::NoAssets);
	settings = makeSettings();
	writeFile(m_root / "blocker", "file");
	settings.outputDirectory = m_root / "blocker";
	EXPECT_EQ(GameExporter::exportGame(settings, {}).error(), ExportError::OutputDirectory);
	settings = makeSettings();
	EXPECT_EQ(GameExporter::exportGame(settings, {}, {}, []() -> bool { return true; }).error(),
			  ExportError::Cancelled);
}

#ifdef OWL_PLATFORM_LINUX
TEST(GameExportEndToEnd, SampleProjectPlaysFromItsExportFolder) {
	std::error_code ec;
	const auto binDir = std::filesystem::canonical("/proc/self/exe", ec).parent_path();
	if (ec || !exists(binDir / "OwlNest") || !exists(binDir / "OwlRunner"))
		GTEST_SKIP() << "OwlNest / OwlRunner not built next to the test.";
	const auto sample = test::getRootPath() / "sample_project";
	const auto root = makeTempDir("e2e");
	const auto workDir = root / "cwd";
	std::filesystem::create_directories(workDir);

	ASSERT_EQ(runCommand(std::format("cd '{}' && '{}' --export '{}' '{}' > export.txt 2>&1", workDir.string(),
									 (binDir / "OwlNest").string(), sample.string(), (root / "out").string())),
			  0)
			<< readFile(workDir / "export.txt");
	const auto exported = root / "out" / "OwlFeatureDemo";
	ASSERT_TRUE(exists(exported / "OwlFeatureDemo.owlpack"));

	PackReader reader;
	ASSERT_TRUE(reader.open(exported / "OwlFeatureDemo.owlpack"));
	size_t sceneCount = 0;
	for (const auto& entry: std::filesystem::directory_iterator(sample / "scenes")) {
		if (entry.path().extension() != ".owl")
			continue;
		++sceneCount;
		const auto packPath = "scenes/" + entry.path().filename().string();
		EXPECT_TRUE(reader.contains(packPath)) << packPath;
		const auto data = reader.readEntry(packPath);
		ASSERT_TRUE(data.has_value());
		EXPECT_EQ(std::string(data->begin(), data->end()).find("pat:/"), std::string::npos) << packPath;
	}
	for (const auto* entry:
		 {"tilesets/voxel_blocks.owltileset", "textures/voxel_blocks.png", "tilesets/raycast_w3d_walls.owltileset",
		  "tilemaps/world_map.owltilemap", "scripts/main_menu.lua", "game_settings.yml"})
		EXPECT_TRUE(reader.contains(entry)) << entry;
	reader.close();

	// Move the game away from its export folder to prove it carries everything it needs.
	const auto moved = root / "moved" / "OwlFeatureDemo";
	std::filesystem::create_directories(moved.parent_path());
	std::filesystem::rename(exported, moved);
	const int exitCode = runCommand(std::format("cd '{}' && '{}' --headless --smoke-test 60 > runner.txt 2>&1",
												workDir.string(), (moved / "launch.sh").string()));
	const auto log = readFile(workDir / "Owl.log");
	std::string problems;
	static const std::regex g_problem(R"(\[(warning|error|critical)\][^\n]*)");
	for (auto it = std::sregex_iterator(log.begin(), log.end(), g_problem); it != std::sregex_iterator(); ++it)
		problems += it->str() + "\n";
	EXPECT_EQ(exitCode, 0) << readFile(workDir / "runner.txt");
	EXPECT_TRUE(problems.empty()) << problems;
	EXPECT_NE(log.find(std::format("Smoke test passed on {} scene(s)", sceneCount)), std::string::npos) << log;
	std::filesystem::remove_all(root, ec);
}
#endif
