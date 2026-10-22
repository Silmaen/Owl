/**
 * @file McpPlay_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "mcp/McpDispatcher.h"
#include "mcp/McpPlay.h"
#include "nestTestHelper.h"
#include "session/HeadlessSession.h"
#include "session/HeldInput.h"

#include <app/Application.h>
#include <input/ActionMap.h>
#include <input/KeyCodes.h>
#include <scene/Entity.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::mcp;
using json = nlohmann::json;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
/// A session whose play spans several polls, like the editor's.
class AsyncSession final : public session::EditorSession {
public:
	[[nodiscard]] auto getSessionScene() const -> shared<scene::Scene> override { return nullptr; }

	[[nodiscard]] auto getSessionScenePath() const -> std::filesystem::path override { return {}; }

	[[nodiscard]] auto getSessionProjectDirectory() const -> std::filesystem::path override { return {}; }

	[[nodiscard]] auto getSessionUndo() -> SceneUndoManager* override { return nullptr; }

	[[nodiscard]] auto getRegistry() const -> const commands::CommandRegistry& override { return m_registry; }

	[[nodiscard]] auto makeCommandContext() -> commands::CommandContext override { return {}; }

	[[nodiscard]] auto isScenePlaying() const -> bool override { return m_running; }

	auto startPlay(const session::PlayRequest& iRequest) -> expected<void, session::SessionFailure> override {
		m_request = iRequest;
		m_running = true;
		return {};
	}

	[[nodiscard]] auto isPlayRunning() const -> bool override { return m_running; }

	auto takePlayReport() -> std::optional<session::PlayReport> override {
		return std::exchange(m_report, std::nullopt);
	}

	auto stopPlay() -> expected<void, session::SessionFailure> override {
		if (!m_running)
			return unexpected{
					session::SessionFailure{.error = session::SessionError::NotPlaying, .message = "not playing"}};
		finish(/*iInterrupted=*/true);
		return {};
	}

	auto captureFrame() -> expected<session::FrameCapture, session::SessionFailure> override {
		return unexpected{
				session::SessionFailure{.error = session::SessionError::CaptureUnavailable, .message = "no capture"}};
	}

	[[nodiscard]] auto getStatistics() const -> session::SessionStatistics override { return {}; }

	/// The main loop played the frames: the play ends.
	void finish(const bool iInterrupted) {
		m_running = false;
		session::PlayReport report;
		report.frames = m_request.frames;
		report.interrupted = iInterrupted;
		m_report = report;
	}

	/// Last request received.
	session::PlayRequest m_request;

private:
	/// Registry without commands.
	commands::CommandRegistry m_registry;
	/// True while the play runs.
	bool m_running{false};
	/// Report waiting to be taken.
	std::optional<session::PlayReport> m_report;
};

class McpPlayTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "mcpPlayTest",
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

	void TearDown() override {
		app::Application::invalidate();
		m_app.reset();
		core::Log::invalidate();
	}

	/// Send a `tools/call` message text.
	static auto callMessage(const json& iArguments, const int iId = 3) -> std::string {
		return json{{"jsonrpc", "2.0"},
					{"id", iId},
					{"method", "tools/call"},
					{"params", {{"name", "play.run"}, {"arguments", iArguments}}}}
				.dump();
	}

	/// Call `play.run` and return its result.
	static auto play(McpDispatcher& ioDispatcher, const json& iArguments) -> json {
		const auto answer = ioDispatcher.handle(callMessage(iArguments));
		return answer ? json::parse(*answer)["result"] : json{};
	}

	static auto samplePath() -> std::filesystem::path {
		return owl::test::getRootPath() / "engine_demo" / "scenes" / "platformer_house.owl";
	}

	static auto playerX(const session::EditorSession& iSession) -> float {
		const auto scene = iSession.getSessionScene();
		return scene->getWorldTransform(scene->getPrimaryPlayer()).translation().x();
	}

	/// Dummy application on the Null renderer.
	shared<app::Application> m_app;
};
OWL_DIAG_POP

}// namespace

TEST(McpBase64, EncodesTheRfc4648Vectors) {
	const auto encode = [](const std::string_view iText) -> std::string {
		const std::vector<uint8_t> bytes{iText.begin(), iText.end()};
		return encodeBase64(bytes);
	};
	EXPECT_EQ(encode(""), "");
	EXPECT_EQ(encode("f"), "Zg==");
	EXPECT_EQ(encode({"foo", 2}), "Zm8=");
	EXPECT_EQ(encode("foo"), "Zm9v");
	EXPECT_EQ(encode("foob"), "Zm9vYg==");
	EXPECT_EQ(encode("fooba"), "Zm9vYmE=");
	EXPECT_EQ(encode("foobar"), "Zm9vYmFy");
	constexpr std::array<uint8_t, 3> high{0xFB, 0xFF, 0xBF};
	EXPECT_EQ(encodeBase64(high), "+/+/");
}

TEST(McpPlayArguments, ParsesTheRequest) {
	const auto map = input::ActionMap::getDefault();
	const auto request = parsePlayRequest({{"frames", 12},
										   {"ticks", 3},
										   {"timestep_ms", 8.5},
										   {"keys", {"Space"}},
										   {"capture", false},
										   {"stop_at_end", false},
										   {"actions", {{"jump", true}}}},
										  *map);
	ASSERT_TRUE(request.has_value()) << request.error();
	EXPECT_EQ(request->frames, 12u);
	EXPECT_EQ(request->ticks, 3u);
	EXPECT_DOUBLE_EQ(request->timestepMs, 8.5);
	EXPECT_FALSE(request->capture);
	EXPECT_FALSE(request->stopAtEnd);
	EXPECT_NE(std::ranges::find(request->keys, input::key::Space), request->keys.end());

	const auto defaults = parsePlayRequest({{"frames", 1}}, *map);
	ASSERT_TRUE(defaults.has_value());
	EXPECT_TRUE(defaults->capture);
	EXPECT_TRUE(defaults->stopAtEnd);
	EXPECT_TRUE(defaults->keys.empty());
}

TEST(McpPlayArguments, RefusesWhatDoesNotFit) {
	const auto map = input::ActionMap::getDefault();
	const auto refused = [&map](const json& iArguments) -> std::string {
		const auto request = parsePlayRequest(iArguments, *map);
		return request ? std::string{} : request.error();
	};
	EXPECT_NE(refused({{"frames", -1}}), "");
	EXPECT_NE(refused({{"frames", "ten"}}), "");
	EXPECT_NE(refused({{"frames", 1.5}}), "");
	EXPECT_NE(refused({{"ticks", session::PlayRequest::maxCount + 1}}), "");
	EXPECT_NE(refused({{"timestep_ms", "fast"}}), "");
	EXPECT_NE(refused({{"capture", 1}}), "");
	EXPECT_NE(refused({{"keys", {"NotAKey"}}}), "");
	EXPECT_NE(refused({{"keys", "Space"}}), "");
	EXPECT_NE(refused({{"actions", {{"nope", true}}}}), "");
	EXPECT_NE(refused({{"actions", {{"jump", "yes"}}}}), "");
	EXPECT_NE(refused({{"actions", {{"move", {1, 2, 3}}}}}), "");
	EXPECT_NE(refused({{"actions", json::array()}}), "");
	EXPECT_NE(refused({{"unexpected", 1}}), "");
}

TEST(HeldInput, TurnsActionsIntoBindingInputs) {
	const auto map = input::ActionMap::getDefault();
	session::HeldInput held;
	ASSERT_TRUE(session::resolveActionHolds(*map, {{.name = "jump", .value = {1.f, 0.f}}}, held).empty());
	EXPECT_FALSE(held.keys.empty());
	EXPECT_FALSE(held.gamepad.has_value());

	session::HeldInput moving;
	EXPECT_TRUE(session::resolveActionHolds(*map, {{.name = "move", .value = {1.f, -1.f}}}, moving).empty());
	EXPECT_GE(moving.keys.size(), 2u);

	session::HeldInput idle;
	EXPECT_TRUE(session::resolveActionHolds(*map, {{.name = "jump", .value = {0.f, 0.f}}}, idle).empty());
	EXPECT_TRUE(idle.keys.empty());

	session::HeldInput unknown;
	EXPECT_NE(session::resolveActionHolds(*map, {{.name = "fly", .value = {1.f, 0.f}}}, unknown), "");
}

TEST_F(McpPlayTest, ListsThePlayTool) {
	session::HeadlessSession session;
	McpDispatcher dispatcher{session};
	const auto answer = json::parse(
			dispatcher.handle(json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}}.dump()).value());
	const auto& tools = answer["result"]["tools"];
	ASSERT_EQ(tools.size(), session.getRegistry().getCommands().size() + 1);
	EXPECT_EQ(tools.back()["name"], "play.run");
	EXPECT_FALSE(tools.back()["inputSchema"]["additionalProperties"].get<bool>());
	EXPECT_TRUE(tools.back()["inputSchema"]["properties"].contains("actions"));
}

TEST_F(McpPlayTest, PlaysWithAHeldActionAndCapturesTheViewport) {
	session::HeadlessSession session;
	ASSERT_TRUE(session.loadScene(samplePath()).has_value());
	McpDispatcher dispatcher{session};
	const float idleX = playerX(session);

	const auto still = play(dispatcher, {{"frames", 30}, {"stop_at_end", false}});
	ASSERT_FALSE(still["isError"].get<bool>()) << still.dump();
	const float stillX = playerX(session);
	ASSERT_TRUE(session.stopPlay().has_value());

	const auto moved =
			play(dispatcher, {{"frames", 30}, {"actions", {{"platformer_move", 1}}}, {"stop_at_end", false}});
	ASSERT_FALSE(moved["isError"].get<bool>()) << moved.dump();
	EXPECT_GT(playerX(session), stillX + 0.1f);
	ASSERT_TRUE(session.stopPlay().has_value());

	const auto& report = moved["structuredContent"];
	EXPECT_EQ(report["frames"], 30);
	EXPECT_EQ(report["ticks"], 30);
	EXPECT_FALSE(report["interrupted"].get<bool>());
	EXPECT_GT(report["entity_count"].get<size_t>(), 0u);
	EXPECT_TRUE(report["phases"].contains("render_ms"));
	EXPECT_GT(report["capture"]["width"].get<uint32_t>(), 0u);

	const auto& content = moved["content"];
	ASSERT_EQ(content.size(), 2u);
	EXPECT_EQ(content[0]["type"], "text");
	EXPECT_EQ(content[1]["type"], "image");
	EXPECT_EQ(content[1]["mimeType"], "image/png");
	// A PNG file starts with the signature 89 50 4E 47 0D 0A 1A 0A.
	EXPECT_EQ(content[1]["data"].get<std::string>().substr(0, 11), "iVBORw0KGgo");

	EXPECT_FALSE(session.isScenePlaying());
	EXPECT_FLOAT_EQ(playerX(session), idleX);
}

TEST_F(McpPlayTest, PlaysAndCapturesAnEmptySceneWithoutCamera) {
	session::HeadlessSession session;
	session.newScene();
	McpDispatcher dispatcher{session};
	const auto result = play(dispatcher, {{"ticks", 3}});
	ASSERT_FALSE(result["isError"].get<bool>()) << result.dump();
	EXPECT_EQ(result["structuredContent"]["ticks"], 3);
}

TEST_F(McpPlayTest, SkipsTheCaptureWhenAsked) {
	session::HeadlessSession session;
	session.newScene();
	McpDispatcher dispatcher{session};
	const auto result = play(dispatcher, {{"ticks", 2}, {"capture", false}});
	ASSERT_FALSE(result["isError"].get<bool>()) << result.dump();
	EXPECT_EQ(result["content"].size(), 1u);
	EXPECT_FALSE(result["structuredContent"].contains("capture"));
	EXPECT_EQ(result["structuredContent"]["ticks"], 2);
}

TEST_F(McpPlayTest, ReportsFailuresAsToolErrors) {
	session::HeadlessSession session;
	McpDispatcher dispatcher{session};
	auto result = play(dispatcher, {{"frames", 1}});
	EXPECT_TRUE(result["isError"].get<bool>());
	EXPECT_EQ(result["structuredContent"]["error"], "PlayFailed");
	session.newScene();
	result = play(dispatcher, json::object());
	EXPECT_TRUE(result["isError"].get<bool>());
	result = play(dispatcher, {{"frames", 1}, {"actions", {{"nope", true}}}});
	EXPECT_TRUE(result["isError"].get<bool>());
	result = play(dispatcher, {{"frames", 1}, {"timestep_ms", -3}});
	EXPECT_TRUE(result["isError"].get<bool>());
	EXPECT_FALSE(session.isScenePlaying());
}

TEST_F(McpPlayTest, DefersTheAnswerOfAPlaySpanningFrames) {
	AsyncSession session;
	McpDispatcher dispatcher{session};
	auto result = dispatcher.handleDeferrable(callMessage({{"frames", 5}, {"capture", false}}, 41));
	EXPECT_FALSE(result.answer.has_value());
	ASSERT_TRUE(static_cast<bool>(result.poll));
	EXPECT_EQ(session.m_request.frames, 5u);
	EXPECT_FALSE(session.m_request.capture);

	EXPECT_FALSE(result.poll(false).has_value());
	const auto busy = play(dispatcher, {{"frames", 1}});
	EXPECT_TRUE(busy["isError"].get<bool>());
	session.finish(/*iInterrupted=*/false);
	const auto answer = result.poll(false);
	ASSERT_TRUE(answer.has_value());
	const auto parsed = json::parse(*answer);
	EXPECT_EQ(parsed["id"], 41);
	EXPECT_EQ(parsed["result"]["structuredContent"]["frames"], 5);
	EXPECT_FALSE(parsed["result"]["structuredContent"]["interrupted"].get<bool>());

	// The next play can start.
	EXPECT_TRUE(dispatcher.handleDeferrable(callMessage({{"frames", 1}})).poll);
}

TEST_F(McpPlayTest, CancelStopsTheDeferredPlayAndAnswers) {
	AsyncSession session;
	McpDispatcher dispatcher{session};
	const auto result = dispatcher.handleDeferrable(callMessage({{"frames", 50}}, 8));
	ASSERT_TRUE(static_cast<bool>(result.poll));
	const auto answer = result.poll(/*iCancel=*/true);
	ASSERT_TRUE(answer.has_value());
	const auto parsed = json::parse(*answer);
	EXPECT_EQ(parsed["id"], 8);
	EXPECT_TRUE(parsed["result"]["structuredContent"]["interrupted"].get<bool>());
	EXPECT_FALSE(session.isPlayRunning());
}

TEST_F(McpPlayTest, SynchronousHandleCancelsADeferredPlay) {
	AsyncSession session;
	McpDispatcher dispatcher{session};
	const auto answer = dispatcher.handle(callMessage({{"frames", 50}}, 5));
	ASSERT_TRUE(answer.has_value());
	EXPECT_TRUE(json::parse(*answer)["result"]["structuredContent"]["interrupted"].get<bool>());
}
