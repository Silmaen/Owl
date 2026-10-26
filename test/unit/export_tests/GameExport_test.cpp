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
#include <data/assets/AssetMeta.h>
#include <data/assets/AssetRegistry.h>
#include <data/assets/pack/GameExporter.h>
#include <data/assets/pack/PackReader.h>
#include <renderer/TextureDecoder.h>
#include <renderer/gpu/Texture.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <regex>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
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
	const std::ifstream in(iPath, std::ios::binary);
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
	// NOLINTNEXTLINE(bugprone-command-processor) Launching the game is the test.
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
		writeFile(m_project / "input_actions.yml", "actions:\n  - {name: fire, bindings: [{key: F}]}\n");
		writeFile(m_project / "input" / "gamecontrollerdb.txt", "# mappings\n");
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
				.defaultHud = {},
				.gamepadMappings = "input/gamecontrollerdb.txt",
				.projectDirectory = m_project,
				.outputDirectory = m_root / "out",
				.runnerDirectory = m_runner,
				.windowSize = {800, 600},
				.fullscreen = false,
				.resizable = true,
				.rendererStackYaml = {},
				.imGui = false,
				.packFlags = PackFlags::Default,
				.cookCacheDirectory = {}};
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
	settings.gamepadMappings = "input/missing.txt";
	EXPECT_EQ(GameExporter::validate(settings, assets).size(), 3u);
	settings.firstScene = "scenes/missing";
	EXPECT_GE(GameExporter::validate(settings, assets).size(), 4u);
	EXPECT_TRUE(assets.empty());
}

TEST_F(GameExporterFixture, ExportCooksTexturesAndPacksTheIndex) {
	const std::vector<uint8_t> pixels(16 * 16 * 4, 0x80);
	const auto png = renderer::encodeImagePng({16, 16}, pixels);
	{
		std::ofstream file(m_project / "textures" / "a.png", std::ios::binary);
		file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
	}
	auto& app = app::Application::get();
	app.removeAssetDirectory(m_project);
	app.addAssetDirectory({"Project: test", m_project}, true);
	auto& registry = app.getAssetRegistry();
	const auto guid = registry.findGuid("textures/a.png");
	ASSERT_TRUE(guid);
	auto meta = registry.find(*guid)->meta;
	meta.texture.compression = data::assets::TextureCompression::Uastc;
	ASSERT_TRUE(registry.setMeta(meta));

	auto settings = makeSettings();
	settings.cookCacheDirectory = m_root / "cook";
	const auto first = GameExporter::exportGame(settings, {});
	ASSERT_TRUE(first.has_value());
	EXPECT_GE(first->cookedAssets, 1u);
	const auto cooked = readPackEntry(first->packFile, "textures/a.png");
	EXPECT_TRUE(renderer::isKtx2(std::span{reinterpret_cast<const uint8_t*>(cooked.data()), cooked.size()}));
	EXPECT_EQ(readPackEntry(first->packFile, "textures/b.png"), "PNG-B");
	const auto index = readPackEntry(first->packFile, std::string{g_assetIndexEntry});
	EXPECT_NE(index.find(data::assets::formatGuid(*guid)), std::string::npos);
	EXPECT_FALSE(exists(m_project / ".owl"));

	const auto second = GameExporter::exportGame(settings, {});
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(second->cookedAssets, 0u);
	EXPECT_EQ(second->cacheHits, first->cookedAssets + first->cacheHits);
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
	EXPECT_EQ(config["GamepadMappings"].as<std::string>(), "input/gamecontrollerdb.txt");
	EXPECT_EQ(config["WindowWidth"].as<uint32_t>(), 800u);
	EXPECT_FLOAT_EQ(config["TickRate"].as<float>(), settings.tickRate);
	EXPECT_EQ(config["MaxTicksPerFrame"].as<uint32_t>(), settings.maxTicksPerFrame);
	const auto info = YAML::LoadFile((gameDir / "game_info.yml").string())["GameInfo"];
	ASSERT_TRUE(info);
	EXPECT_EQ(info["Description"].as<std::string>(), "A # tricky: description");

	PackReader reader;
	ASSERT_TRUE(reader.open(result->packFile));
	for (const auto* entry: {"scenes/level.owl", "textures/a.png", "textures/b.png", "tilesets/blocks.owltileset",
							 "game_settings.yml", "input_actions.yml", "input/gamecontrollerdb.txt"})
		EXPECT_TRUE(reader.contains(entry)) << entry;
	reader.close();
	const auto scene = readPackEntry(result->packFile, "scenes/level.owl");
	EXPECT_NE(scene.find("nam:textures/a.png"), std::string::npos);
	EXPECT_EQ(scene.find("pat:"), std::string::npos);
}

TEST_F(GameExporterFixture, ExportPacksOnlyTheEngineFilesOfTheGame) {
	const auto entriesOf = [](const std::filesystem::path& iPack) -> std::vector<std::string> {
		PackReader reader;
		if (!reader.open(iPack))
			return {};
		return reader.listEntries();
	};
	const auto hasPrefix = [](const std::vector<std::string>& iEntries, const std::string_view iPrefix) -> bool {
		return std::ranges::any_of(iEntries,
								   [&](const std::string& iEntry) -> bool { return iEntry.starts_with(iPrefix); });
	};
	auto settings = makeSettings();
	settings.icon.clear();
	const auto flat = GameExporter::exportGame(settings, {});
	ASSERT_TRUE(flat.has_value());
	const auto flatEntries = entriesOf(flat->packFile);
	if (!hasPrefix(flatEntries, "shaders/renderer2D/"))
		GTEST_SKIP() << "No engine shaders in the asset directories of the test.";
	EXPECT_TRUE(flat->renderers.empty());
	EXPECT_GT(flat->engineFilesSkipped, 0u);
	for (const auto* absent: {"shaders/renderer3D/", "shaders/raycast_dda/", "shaders/isometric/",
							  "shaders/frustum_culling/", "icons/", "logo/", "fonts/Inter-Regular.ttf"})
		EXPECT_FALSE(hasPrefix(flatEntries, absent)) << absent;
	EXPECT_TRUE(hasPrefix(flatEntries, "fonts/OpenSans-Regular.ttf"));
	EXPECT_TRUE(exists(flat->gameDirectory / "logo" / "logo_owl_low.png"));
	const auto flatConfig = YAML::LoadFile((flat->gameDirectory / "runner.yml").string())["RunnerConfig"];
	EXPECT_FALSE(flatConfig["ImGui"].as<bool>());
	EXPECT_EQ(flatConfig["Icon"].as<std::string>(), "logo/logo_owl_low.png");
	EXPECT_TRUE(flatConfig["Renderers"].IsSequence());
	EXPECT_EQ(flatConfig["Renderers"].size(), 0u);

	settings.rendererStackYaml = "- Type: Renderer2D\n  Name: world\n- Type: RendererMesh\n  Name: meshes\n";
	settings.imGui = true;
	const auto meshes = GameExporter::exportGame(settings, {});
	ASSERT_TRUE(meshes.has_value());
	EXPECT_EQ(meshes->renderers, std::vector<std::string>{"Renderer3D"});
	const auto meshEntries = entriesOf(meshes->packFile);
	for (const auto* present: {"shaders/renderer3D/", "shaders/mesh_culling/", "fonts/lucide.ttf"})
		EXPECT_TRUE(hasPrefix(meshEntries, present)) << present;
	EXPECT_FALSE(hasPrefix(meshEntries, "shaders/raycast_dda/"));
	const auto meshConfig = YAML::LoadFile((meshes->gameDirectory / "runner.yml").string())["RunnerConfig"];
	EXPECT_TRUE(meshConfig["ImGui"].as<bool>());
	EXPECT_EQ(meshConfig["Renderers"].as<std::vector<std::string>>(), std::vector<std::string>{"Renderer3D"});
}

TEST_F(GameExporterFixture, AnEnvironmentBringsRenderer3D) {
	writeFile(m_project / "scenes" / "level.owl",
			  "Scene: level\nEntities:\n  - Entity: 1\n    Environment:\n      showSky: true\n");
	const auto result = GameExporter::exportGame(makeSettings(), {});
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->renderers, std::vector<std::string>{"Renderer3D"});
}

TEST_F(GameExporterFixture, ExportPacksTheHudsAndTheirAssets) {
	writeFile(m_project / "huds" / "own.owlhud",
			  "Hud: own\nEntities:\n  - Entity: 1\n    UiImage:\n      texture: nam:textures/a.png\n");
	writeFile(m_project / "huds" / "default.owlhud",
			  "Hud: default\nEntities:\n  - Entity: 1\n    UiImage:\n      texture: nam:textures/b.png\n");
	writeFile(m_project / "scenes" / "level.owl", "Scene: level\nHud: huds/own.owlhud\nEntities: []\n");
	auto settings = makeSettings();
	settings.defaultHud = "huds/default.owlhud";
	std::vector<AssetReference> assets;
	EXPECT_TRUE(GameExporter::validate(settings, assets).empty());
	const auto result = GameExporter::exportGame(settings, assets);
	ASSERT_TRUE(result.has_value());
	PackReader reader;
	ASSERT_TRUE(reader.open(result->packFile));
	for (const auto* entry: {"huds/own.owlhud", "huds/default.owlhud", "textures/a.png", "textures/b.png"})
		EXPECT_TRUE(reader.contains(entry)) << entry;
	reader.close();
	const auto config = YAML::LoadFile((result->gameDirectory / "runner.yml").string())["RunnerConfig"];
	EXPECT_EQ(config["DefaultHud"].as<std::string>(), "huds/default.owlhud");
	settings.defaultHud = "huds/missing.owlhud";
	EXPECT_EQ(GameExporter::validate(settings, assets).size(), 1u);
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
namespace {

// The projects shipped with the editor: the Engine Demo, then every `samples/<game>/owl_project.yml`.
auto shippedProjects() -> std::vector<std::filesystem::path> {
	const auto root = test::getRootPath();
	std::vector<std::filesystem::path> projects{root / "engine_demo"};
	std::vector<std::filesystem::path> samples;
	std::error_code ec;
	for (const auto& entry: std::filesystem::directory_iterator(root / "samples", ec)) {
		if (exists(entry.path() / "owl_project.yml"))
			samples.push_back(entry.path());
	}
	std::ranges::sort(samples);
	projects.insert(projects.end(), samples.begin(), samples.end());
	return projects;
}

// The single game folder an export wrote into `iOut`, empty when there is not exactly one.
auto exportedGameFolder(const std::filesystem::path& iOut) -> std::filesystem::path {
	std::vector<std::filesystem::path> folders;
	std::error_code ec;
	for (const auto& entry: std::filesystem::directory_iterator(iOut, ec)) {
		if (entry.is_directory())
			folders.push_back(entry.path());
	}
	return folders.size() == 1 ? folders.front() : std::filesystem::path{};
}

// Every file of a project sub-folder (`scenes`, `scripts`...), as pack paths.
auto packPathsUnder(const std::filesystem::path& iProject, const std::string& iFolder) -> std::vector<std::string> {
	std::vector<std::string> paths;
	std::error_code ec;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(iProject / iFolder, ec)) {
		if (entry.is_regular_file() && entry.path().extension() != owl::data::assets::g_MetaExtension)
			paths.push_back(entry.path().lexically_relative(iProject).generic_string());
	}
	return paths;
}

void checkProjectPlaysFromItsExportFolder(const std::filesystem::path& iBinDir, const std::filesystem::path& iProject) {
	const auto root = makeTempDir("e2e_" + iProject.filename().string());
	const auto workDir = root / "cwd";
	std::filesystem::create_directories(workDir);

	ASSERT_EQ(runCommand(std::format("cd '{}' && '{}' --export '{}' '{}' --cook-cache '{}' > export.txt 2>&1",
									 workDir.string(), (iBinDir / "OwlNest").string(), iProject.string(),
									 (root / "out").string(), (root / "cook").string())),
			  0)
			<< readFile(workDir / "export.txt");
	const auto exported = exportedGameFolder(root / "out");
	ASSERT_FALSE(exported.empty()) << readFile(workDir / "export.txt");
	const auto pack = exported / (exported.filename().string() + ".owlpack");
	ASSERT_TRUE(exists(pack));

	PackReader reader;
	ASSERT_TRUE(reader.open(pack));
	size_t sceneCount = 0;
	for (const auto& entry: std::filesystem::directory_iterator(iProject / "scenes")) {
		if (entry.path().extension() != ".owl")
			continue;
		++sceneCount;
		const auto packPath = "scenes/" + entry.path().filename().string();
		EXPECT_TRUE(reader.contains(packPath)) << packPath;
		const auto data = reader.readEntry(packPath);
		ASSERT_TRUE(data.has_value());
		EXPECT_EQ(std::string(data->begin(), data->end()).find("pat:/"), std::string::npos) << packPath;
	}
	for (const auto& script: packPathsUnder(iProject, "scripts")) EXPECT_TRUE(reader.contains(script)) << script;
	for (const auto* entry: {"game_settings.yml", "input_actions.yml"}) {
		if (exists(iProject / entry)) {
			EXPECT_TRUE(reader.contains(entry)) << entry;
		}
	}
	if (iProject.filename() == "engine_demo") {
		for (const auto* entry: {"tilesets/voxel_blocks.owltileset", "textures/atlases/voxel_blocks.png",
								 "tilesets/raycast_w3d_walls.owltileset", "tilemaps/world_map.owltilemap",
								 "sounds/hub/birds_4.wav", "sounds/hub/wind_3.wav", "sounds/hub/step_2.wav"})
			EXPECT_TRUE(reader.contains(entry)) << entry;
	}
	reader.close();

	// Move the game away from its export folder to prove it carries everything it needs.
	const auto moved = root / "moved" / exported.filename();
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
	std::error_code ec;
	std::filesystem::remove_all(root, ec);
}

}// namespace

namespace {
// One case per shipped project: ctest runs them in parallel shards, each in its own temporary folder.
class GameExportEndToEnd : public testing::TestWithParam<std::filesystem::path> {};
}// namespace

TEST_P(GameExportEndToEnd, PlaysFromItsExportFolder) {
	std::error_code ec;
	const auto binDir = std::filesystem::canonical("/proc/self/exe", ec).parent_path();
	if (ec || !exists(binDir / "OwlNest") || !exists(binDir / "OwlRunner"))
		GTEST_SKIP() << "OwlNest / OwlRunner not built next to the test.";
	checkProjectPlaysFromItsExportFolder(binDir, GetParam());
}

INSTANTIATE_TEST_SUITE_P(Shipped, GameExportEndToEnd, testing::ValuesIn(shippedProjects()),
						 [](const testing::TestParamInfo<std::filesystem::path>& iInfo) -> std::string {
							 return iInfo.param.filename().string();
						 });

TEST(GameExportCoverage, EverySampleGameHasAScenario) {
	const auto root = test::getRootPath();
	for (const auto& project: shippedProjects()) {
		if (project.filename() == "engine_demo")
			continue;
		// A scenario of the game plays one of its scenes (`scene: ../../../samples/<game>/scenes/...`).
		const auto scenes = std::format("samples/{}/scenes/", project.filename().string());
		bool found = false;
		for (const auto& entry: std::filesystem::directory_iterator(root / "test" / "unit" / "scenarios")) {
			if (entry.path().extension() == ".owltest" && readFile(entry.path()).find(scenes) != std::string::npos)
				found = true;
		}
		EXPECT_TRUE(found) << "no test/unit/scenarios/*.owltest plays a scene of " << project;
	}
}
#endif
