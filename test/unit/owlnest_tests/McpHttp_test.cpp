/**
 * @file McpHttp_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "mcp/McpHttpServer.h"
#include "mcp/McpServer.h"
#include "mcp/McpToken.h"
#include "nestTestHelper.h"
#include "session/HeadlessSession.h"

#include <app/Application.h>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::mcp;
using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {

/// What a client received.
struct HttpReply {
	/// HTTP status, -1 when the connection failed.
	int status{-1};
	/// Body.
	std::string body;
	/// Headers.
	httplib::Headers headers;

	[[nodiscard]] auto json() const -> nlohmann::json { return nlohmann::json::parse(body); }

	[[nodiscard]] auto header(const std::string& iName) const -> std::string {
		const auto found = headers.find(iName);
		return found == headers.end() ? std::string{} : found->second;
	}
};

using Request = std::function<httplib::Result(httplib::Client& ioClient)>;

auto readFile(const std::filesystem::path& iFile) -> std::string {
	const std::ifstream stream(iFile, std::ios::binary);
	std::stringstream buffer;
	buffer << stream.rdbuf();
	return buffer.str();
}

auto rpc(const std::string_view iMethod, const json& iParams = nullptr, const int iId = 1) -> std::string {
	json message{{"jsonrpc", "2.0"}, {"id", iId}, {"method", iMethod}};
	if (!iParams.is_null())
		message["params"] = iParams;
	return message.dump();
}

/// Send a request from a client thread while the calling (main) thread runs the loop step.
auto send(const uint16_t iPort, const Request& iRequest, const std::function<void()>& iLoopStep) -> HttpReply {
	auto reply = std::async(std::launch::async, [iPort, iRequest]() -> HttpReply {
		httplib::Client client{"127.0.0.1", iPort};
		client.set_read_timeout(20s);
		const auto result = iRequest(client);
		if (!result)
			return {};
		return {.status = result->status, .body = result->body, .headers = result->headers};
	});
	while (reply.wait_for(1ms) != std::future_status::ready) iLoopStep();
	return reply.get();
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class McpHttpTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "mcpHttpTest",
														  .assetsPattern = "",
														  .icon = "",
														  .width = 0,
														  .height = 0,
														  .argCount = 0,
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
		m_dir = std::filesystem::temp_directory_path() /
				std::format("owl_mcp_http_{}", generateRandomHex(4).value_or("x"));
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
	}

	void TearDown() override {
		std::filesystem::remove_all(m_dir);
		app::Application::invalidate();
		m_app.reset();
		core::Log::invalidate();
	}

	[[nodiscard]] auto options(const HttpOptions& iHttp = {}) const -> McpServerOptions {
		return {.dispatcher = {}, .http = iHttp, .tokenFile = m_dir / "run" / "mcp_token", .maxMessagesPerFrame = 8};
	}

	/// Authorization and JSON headers of a server.
	static auto headers(const McpServer& iServer) -> httplib::Headers {
		return {{"Authorization", "Bearer " + iServer.getToken()}};
	}

	/// POST a message to a running server, the test thread acting as the editor main loop.
	static auto post(McpServer& ioServer, const std::string& iBody, httplib::Headers iHeaders = {},
					 const std::string& iContentType = "application/json", const bool iBusy = false) -> HttpReply {
		if (iHeaders.empty())
			iHeaders = headers(ioServer);
		return send(
				ioServer.getPort(),
				[iBody, iHeaders, iContentType](httplib::Client& ioClient) -> httplib::Result {
					return ioClient.Post(std::string{McpHttpServer::endpointPath}, iHeaders, iBody, iContentType);
				},
				[&ioServer, iBusy]() -> void { std::ignore = ioServer.onUpdate(iBusy); });
	}

	/// Call a tool over HTTP and return its result.
	static auto callTool(McpServer& ioServer, const std::string_view iName, const json& iArguments) -> json {
		const auto reply = post(ioServer, rpc("tools/call", {{"name", iName}, {"arguments", iArguments}}));
		EXPECT_EQ(reply.status, 200) << reply.body;
		return reply.json()["result"];
	}

	/// Dummy application on the Null renderer.
	shared<app::Application> m_app;
	/// Scratch directory of the test.
	std::filesystem::path m_dir;
};
OWL_DIAG_POP

}// namespace

TEST(McpToken, DrawsDistinct256BitTokens) {
	const auto first = generateToken();
	const auto second = generateToken();
	ASSERT_TRUE(first.has_value());
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(first->size(), 64u);
	EXPECT_EQ(first->find_first_not_of("0123456789abcdef"), std::string::npos);
	EXPECT_NE(*first, *second);
	EXPECT_TRUE(isSameToken(*first, *first));
	EXPECT_FALSE(isSameToken(*first, *second));
	EXPECT_FALSE(isSameToken(*first, first->substr(1)));
	EXPECT_FALSE(isSameToken("", ""));
	EXPECT_EQ(getDefaultTokenFile().filename(), "mcp_token");
}

TEST(McpToken, WritesAFileOnlyItsOwnerReads) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_mcp_token_test";
	std::filesystem::remove_all(dir);
	const auto file = dir / "sub" / "mcp_token";
	ASSERT_TRUE(writeTokenFile(file, "abc"));
	EXPECT_EQ(readFile(file), "abc");
#ifndef OWL_PLATFORM_WINDOWS
	using std::filesystem::perms;
	EXPECT_EQ(status(file).permissions() & perms::all, perms::owner_read | perms::owner_write);
	EXPECT_EQ(status(file.parent_path()).permissions() & perms::all, perms::owner_all);
	// A file left with wider rights is narrowed when rewritten.
	permissions(file, perms::owner_read | perms::owner_write | perms::group_read | perms::others_read);
#endif
	ASSERT_TRUE(writeTokenFile(file, "defg"));
#ifndef OWL_PLATFORM_WINDOWS
	EXPECT_EQ(status(file).permissions() & perms::all, perms::owner_read | perms::owner_write);
#endif
	EXPECT_EQ(readFile(file), "defg");
	removeTokenFile(file);
	EXPECT_FALSE(exists(file));
	std::filesystem::remove_all(dir);
}

TEST(McpHttpServer, ChecksTheHeadersAgainstLoopback) {
	EXPECT_TRUE(McpHttpServer::isLoopbackHost("127.0.0.1:47800", 47800));
	EXPECT_TRUE(McpHttpServer::isLoopbackHost("LocalHost:47800", 47800));
	EXPECT_FALSE(McpHttpServer::isLoopbackHost("127.0.0.1:47801", 47800));
	EXPECT_FALSE(McpHttpServer::isLoopbackHost("127.0.0.1", 47800));
	EXPECT_FALSE(McpHttpServer::isLoopbackHost("evil.example:47800", 47800));
	EXPECT_FALSE(McpHttpServer::isLoopbackHost("localhost.evil.example:47800", 47800));
	EXPECT_FALSE(McpHttpServer::isLoopbackHost("", 47800));

	EXPECT_TRUE(McpHttpServer::isLoopbackOrigin("http://localhost"));
	EXPECT_TRUE(McpHttpServer::isLoopbackOrigin("http://127.0.0.1:5173"));
	EXPECT_TRUE(McpHttpServer::isLoopbackOrigin("https://localhost:8443"));
	EXPECT_FALSE(McpHttpServer::isLoopbackOrigin("http://evil.example"));
	EXPECT_FALSE(McpHttpServer::isLoopbackOrigin("http://localhost.evil.example"));
	EXPECT_FALSE(McpHttpServer::isLoopbackOrigin("http://localhost:80@evil.example"));
	EXPECT_FALSE(McpHttpServer::isLoopbackOrigin("null"));
	EXPECT_FALSE(McpHttpServer::isLoopbackOrigin("file://localhost"));

	EXPECT_TRUE(McpHttpServer::isJsonContentType("application/json"));
	EXPECT_TRUE(McpHttpServer::isJsonContentType("Application/JSON; charset=utf-8"));
	EXPECT_FALSE(McpHttpServer::isJsonContentType("text/plain"));
	EXPECT_FALSE(McpHttpServer::isJsonContentType("application/x-www-form-urlencoded"));
	EXPECT_FALSE(McpHttpServer::isJsonContentType(""));
}

TEST(McpPaths, KeepsFileArgumentsInsideTheProject) {
	const auto project = std::filesystem::temp_directory_path() / "owl_mcp_paths_test";
	std::filesystem::remove_all(project);
	std::filesystem::create_directories(project / "prefabs");
	const auto inside = confinePath(project, "prefabs/coin.owlprefab");
	ASSERT_TRUE(inside.has_value()) << inside.error();
	EXPECT_EQ(inside->filename(), "coin.owlprefab");
	EXPECT_TRUE(confinePath(project, project / "scenes" / "a.owl").has_value());
	EXPECT_FALSE(confinePath(project, "../outside.owl").has_value());
	EXPECT_FALSE(confinePath(project, "prefabs/../../outside.owl").has_value());
	EXPECT_FALSE(confinePath(project, "/etc/passwd").has_value());
	EXPECT_FALSE(confinePath(project, ".").has_value());
	EXPECT_FALSE(confinePath({}, "scene.owl").has_value());
#ifndef OWL_PLATFORM_WINDOWS
	std::filesystem::create_directory_symlink(std::filesystem::temp_directory_path(), project / "escape");
	EXPECT_FALSE(confinePath(project, "escape/outside.owl").has_value());
#endif
	std::filesystem::remove_all(project);
}

TEST_F(McpHttpTest, IsOffUntilStarted) {
	session::HeadlessSession session;
	McpServer server{session, options()};
	EXPECT_FALSE(server.isRunning());
	EXPECT_EQ(server.getStatus(), HttpStatus::Off);
	EXPECT_EQ(server.getPort(), 0u);
	EXPECT_TRUE(server.getEndpoint().empty());
	EXPECT_TRUE(server.getToken().empty());
	server.applySettings(false, 0);
	EXPECT_FALSE(server.isRunning());
	EXPECT_FALSE(exists(server.getTokenFile()));
	EXPECT_EQ(server.onUpdate(false), 0u);
}

TEST_F(McpHttpTest, ServesARoundTripOnLoopback) {
	session::HeadlessSession session;
	session.newScene();
	McpServer server{session, options()};
	server.applySettings(true, 0);
	ASSERT_TRUE(server.isRunning());
	EXPECT_NE(server.getPort(), 0u);
	EXPECT_EQ(server.getEndpoint(), std::format("http://127.0.0.1:{}/mcp", server.getPort()));
	EXPECT_EQ(server.getStatus(), HttpStatus::Listening);
	ASSERT_TRUE(exists(server.getTokenFile()));
	EXPECT_EQ(readFile(server.getTokenFile()), server.getToken());
	EXPECT_EQ(server.getToken().size(), 64u);

	auto reply = post(server, rpc("initialize", {{"protocolVersion", "2025-11-25"},
												 {"capabilities", json::object()},
												 {"clientInfo", {{"name", "http-test"}, {"version", "1"}}}}));
	ASSERT_EQ(reply.status, 200) << reply.body;
	EXPECT_EQ(reply.header("Content-Type"), "application/json");
	const auto sessionId = reply.header("Mcp-Session-Id");
	EXPECT_EQ(sessionId.size(), 32u);
	EXPECT_EQ(reply.json()["result"]["protocolVersion"], "2025-11-25");
	EXPECT_EQ(server.getDispatcher().getClientName(), "http-test");
	EXPECT_EQ(server.getStatus(), HttpStatus::ClientActive);

	auto withSession = headers(server);
	withSession.emplace("Mcp-Session-Id", sessionId);
	withSession.emplace("MCP-Protocol-Version", "2025-11-25");
	reply = post(server, R"({"jsonrpc":"2.0","method":"notifications/initialized"})", withSession);
	EXPECT_EQ(reply.status, 202);
	EXPECT_TRUE(reply.body.empty());
	EXPECT_TRUE(server.getDispatcher().isInitialized());

	reply = post(server, rpc("tools/list", nullptr, 2), withSession);
	ASSERT_EQ(reply.status, 200);
	EXPECT_EQ(reply.json()["id"], 2);
	EXPECT_EQ(reply.json()["result"]["tools"].size(), session.getRegistry().getCommands().size() + 1);
	EXPECT_EQ(server.getHttp().getPendingCount(), 0u);
}

TEST_F(McpHttpTest, CreatesThenUndoesThroughHttp) {
	session::HeadlessSession session;
	session.newScene();
	const auto before = nest::test::sceneState(*session.getSessionScene());
	McpServer server{session, options()};
	ASSERT_TRUE(server.start());

	const auto created = callTool(server, "entity.create", {{"name", "OverHttp"}});
	ASSERT_FALSE(created["isError"].get<bool>()) << created.dump();
	const auto uuid = created["structuredContent"]["entity"].get<uint64_t>();
	const auto entity = session.getSessionScene()->findEntityByUUID(core::UUID{uuid});
	ASSERT_TRUE(entity);
	EXPECT_EQ(entity.getName(), "OverHttp");
	EXPECT_NE(nest::test::sceneState(*session.getSessionScene()), before);

	const auto undone = callTool(server, "edit.undo", json::object());
	ASSERT_FALSE(undone["isError"].get<bool>()) << undone.dump();
	EXPECT_EQ(nest::test::sceneState(*session.getSessionScene()), before);
	EXPECT_EQ(session.getJournal().getEntries().back().source, commands::CommandSource::Mcp);
}

TEST_F(McpHttpTest, RefusesWhatD12Forbids) {
	session::HeadlessSession session;
	session.newScene();
	McpServer server{session, options()};
	ASSERT_TRUE(server.start());
	const auto ping = rpc("ping");
	const auto port = std::format("{}", server.getPort());

	auto reply = post(server, ping, {{"X-Nothing", "1"}});
	EXPECT_EQ(reply.status, 401);
	EXPECT_EQ(reply.header("WWW-Authenticate"), "Bearer");
	reply = post(server, ping, {{"Authorization", "Bearer " + std::string(64, '0')}});
	EXPECT_EQ(reply.status, 401);
	reply = post(server, ping, {{"Authorization", server.getToken()}});
	EXPECT_EQ(reply.status, 401);

	auto evilHost = headers(server);
	evilHost.emplace("Host", "evil.example:" + port);
	EXPECT_EQ(post(server, ping, evilHost).status, 403);
	auto evilOrigin = headers(server);
	evilOrigin.emplace("Origin", "http://evil.example");
	EXPECT_EQ(post(server, ping, evilOrigin).status, 403);
	auto localOrigin = headers(server);
	localOrigin.emplace("Origin", "http://localhost:3000");
	localOrigin.emplace("Host", "localhost:" + port);
	EXPECT_EQ(post(server, ping, localOrigin).status, 200);

	EXPECT_EQ(post(server, ping, headers(server), "text/plain").status, 415);
	EXPECT_EQ(post(server, ping, headers(server), "application/x-www-form-urlencoded").status, 415);

	const std::string huge = rpc("ping") + std::string((size_t{1} << 20u) + 1, ' ');
	EXPECT_EQ(post(server, huge).status, 413);

	auto otherSession = headers(server);
	otherSession.emplace("Mcp-Session-Id", "not-this-one");
	EXPECT_EQ(post(server, ping, otherSession).status, 404);
	auto oldProtocol = headers(server);
	oldProtocol.emplace("MCP-Protocol-Version", "2024-11-05");
	EXPECT_EQ(post(server, ping, oldProtocol).status, 400);

	const auto getReply = send(
			server.getPort(),
			[&server](httplib::Client& ioClient) -> httplib::Result {
				return ioClient.Get(std::string{McpHttpServer::endpointPath}, headers(server));
			},
			[&server]() -> void { std::ignore = server.onUpdate(false); });
	EXPECT_EQ(getReply.status, 405);
	const auto wrongPath = send(
			server.getPort(),
			[&server](httplib::Client& ioClient) -> httplib::Result {
				return ioClient.Post("/other", headers(server), "{}", "application/json");
			},
			[&server]() -> void { std::ignore = server.onUpdate(false); });
	EXPECT_EQ(wrongPath.status, 404);

	// Nothing refused reached the dispatcher.
	EXPECT_TRUE(server.getDispatcher().getClientName().empty());
	EXPECT_EQ(post(server, ping).status, 200);
}

TEST_F(McpHttpTest, AnswersBusyWhenTheQueueIsFull) {
	session::HeadlessSession session;
	session.newScene();
	McpServer server{session, options({.port = 0, .queueCapacity = 1})};
	ASSERT_TRUE(server.start());
	const auto port = server.getPort();
	const auto auth = headers(server);
	auto first = std::async(std::launch::async, [port, auth]() -> int {
		httplib::Client client{"127.0.0.1", port};
		client.set_read_timeout(20s);
		const auto result = client.Post("/mcp", auth, rpc("ping"), "application/json");
		return result ? result->status : -1;
	});
	while (server.getHttp().getPendingCount() == 0) std::this_thread::sleep_for(1ms);
	// The queue holds one message the main loop has not run: the next one is turned away at once.
	const auto second = send(
			port,
			[&auth](httplib::Client& ioClient) -> httplib::Result {
				return ioClient.Post("/mcp", auth, rpc("ping"), "application/json");
			},
			[]() -> void {});
	EXPECT_EQ(second.status, 503);
	EXPECT_EQ(second.header("Retry-After"), "1");
	while (first.wait_for(1ms) != std::future_status::ready) std::ignore = server.onUpdate(false);
	EXPECT_EQ(first.get(), 200);
}

TEST_F(McpHttpTest, WaitsWhileBusyThenTimesOut) {
	session::HeadlessSession session;
	session.newScene();
	const auto before = nest::test::sceneState(*session.getSessionScene());
	McpServer server{session, options({.port = 0, .requestTimeout = 150ms})};
	ASSERT_TRUE(server.start());

	const auto reply = post(server, rpc("tools/call", {{"name", "entity.create"}, {"arguments", json::object()}}, 9),
							{}, "application/json", /*iBusy=*/true);
	ASSERT_EQ(reply.status, 200);
	const auto answer = reply.json();
	EXPECT_EQ(answer["id"], 9);
	EXPECT_EQ(answer["error"]["code"], McpHttpServer::requestTimeoutCode);
	EXPECT_EQ(server.getHttp().getPendingCount(), 0u);
	EXPECT_EQ(server.onUpdate(false), 0u);
	EXPECT_EQ(nest::test::sceneState(*session.getSessionScene()), before);
}

TEST_F(McpHttpTest, RunsTheMessagesOnTheMainThread) {
	McpHttpServer server;
	ASSERT_TRUE(server.start({.port = 0}, "secret").has_value());
	const auto mainThread = std::this_thread::get_id();
	std::thread::id handledOn;
	size_t handled = 0;
	const McpHttpServer::Handler handler = [&](const std::string_view iMessage) -> std::optional<std::string> {
		handledOn = std::this_thread::get_id();
		++handled;
		return std::string{iMessage};
	};
	const auto reply = send(
			server.getPort(),
			[](httplib::Client& ioClient) -> httplib::Result {
				return ioClient.Post("/mcp", {{"Authorization", "Bearer secret"}}, R"({"echo":1})", "application/json");
			},
			[&server, &handler]() -> void { std::ignore = server.pump(handler, 4); });
	EXPECT_EQ(reply.status, 200);
	EXPECT_EQ(reply.body, R"({"echo":1})");
	EXPECT_EQ(handled, 1u);
	EXPECT_EQ(handledOn, mainThread);
	server.stop();
}

TEST_F(McpHttpTest, StopsAndRestartsCleanly) {
	session::HeadlessSession session;
	session.newScene();
	McpServer server{session, options()};
	ASSERT_TRUE(server.start());
	EXPECT_TRUE(server.start());
	const auto token = server.getToken();
	const auto firstSession = server.getHttp().getSessionId();
	const auto oldPort = server.getPort();
	EXPECT_EQ(post(server, rpc("ping")).status, 200);

	// A message still queued when the server stops gets 503.
	const auto auth = headers(server);
	auto pending = std::async(std::launch::async, [oldPort, auth]() -> int {
		httplib::Client client{"127.0.0.1", oldPort};
		client.set_read_timeout(20s);
		const auto result = client.Post("/mcp", auth, rpc("ping"), "application/json");
		return result ? result->status : -1;
	});
	while (server.getHttp().getPendingCount() == 0) std::this_thread::sleep_for(1ms);
	server.stop();
	EXPECT_EQ(pending.get(), 503);
	EXPECT_FALSE(server.isRunning());
	EXPECT_EQ(server.getStatus(), HttpStatus::Off);
	EXPECT_FALSE(exists(server.getTokenFile()));
	httplib::Client closed{"127.0.0.1", oldPort};
	closed.set_connection_timeout(1s);
	EXPECT_FALSE(closed.Post("/mcp", auth, rpc("ping"), "application/json"));

	ASSERT_TRUE(server.start());
	EXPECT_EQ(server.getToken(), token);
	EXPECT_NE(server.getHttp().getSessionId(), firstSession);
	EXPECT_EQ(readFile(server.getTokenFile()), token);
	EXPECT_EQ(post(server, rpc("ping")).status, 200);
	server.stop();
	server.stop();
	EXPECT_FALSE(server.isRunning());
}

TEST_F(McpHttpTest, RegeneratesTheToken) {
	session::HeadlessSession session;
	McpServer server{session, options()};
	ASSERT_TRUE(server.start());
	const auto oldHeaders = headers(server);
	const auto oldToken = server.getToken();
	ASSERT_TRUE(server.regenerateToken());
	EXPECT_NE(server.getToken(), oldToken);
	EXPECT_EQ(readFile(server.getTokenFile()), server.getToken());
	EXPECT_EQ(post(server, rpc("ping"), oldHeaders).status, 401);
	EXPECT_EQ(post(server, rpc("ping")).status, 200);
}

TEST_F(McpHttpTest, FollowsTheSettingsAndDoesNotRetryATakenPort) {
	session::HeadlessSession session;
	McpHttpServer squatter;
	ASSERT_TRUE(squatter.start({.port = 0}, "other").has_value());
	const auto taken = squatter.getPort();

	McpServer server{session, options()};
	server.applySettings(true, taken);
	EXPECT_FALSE(server.isRunning());
	EXPECT_EQ(server.getFailedPort(), taken);
	EXPECT_FALSE(exists(server.getTokenFile()));
	squatter.stop();
	server.applySettings(true, taken);
	EXPECT_FALSE(server.isRunning());

	server.applySettings(false, taken);
	EXPECT_FALSE(server.getFailedPort().has_value());
	server.applySettings(true, taken);
	EXPECT_TRUE(server.isRunning());
	EXPECT_EQ(server.getPort(), taken);
	server.applySettings(true, taken);
	EXPECT_TRUE(server.isRunning());
	server.applySettings(false, taken);
	EXPECT_FALSE(server.isRunning());
	EXPECT_FALSE(exists(server.getTokenFile()));
}

TEST_F(McpHttpTest, KeepsToolPathsInsideTheProject) {
	session::HeadlessSession session;
	ASSERT_TRUE(session.loadScene(owl::test::getRootPath() / "engine_demo" / "scenes" / "platformer_house.owl")
						.has_value());
	McpServer server{session, options()};
	ASSERT_TRUE(server.start());

	auto result = callTool(server, "prefab.instantiate", {{"path", "../CMakeLists.txt"}});
	EXPECT_TRUE(result["isError"].get<bool>());
	EXPECT_EQ(result["structuredContent"]["error"], "InvalidArgument");
	result = callTool(server, "scene.save", {{"path", "/tmp/outside.owl"}});
	EXPECT_TRUE(result["isError"].get<bool>());
	EXPECT_FALSE(exists(std::filesystem::path{"/tmp/outside.owl"}));
	result = callTool(server, "macro.run", {{"path", "../../outside.owlmacro"}});
	EXPECT_EQ(result["structuredContent"]["error"], "InvalidArgument");

	result = callTool(server, "prefab.instantiate", {{"path", "prefabs/coin_pair.owlprefab"}});
	EXPECT_FALSE(result["isError"].get<bool>()) << result.dump();
	EXPECT_FALSE(callTool(server, "edit.undo", json::object())["isError"].get<bool>());

	server.stop();
	session::HeadlessSession bare;
	bare.newScene();
	McpServer bareServer{bare, options()};
	ASSERT_TRUE(bareServer.start());
	result = callTool(bareServer, "scene.save", {{"path", "scene.owl"}});
	EXPECT_EQ(result["structuredContent"]["error"], "InvalidArgument");
}

TEST_F(McpHttpTest, DrivesACompleteSessionOverHttp) {
	session::HeadlessSession session;
	ASSERT_TRUE(session.loadScene(owl::test::getRootPath() / "engine_demo" / "scenes" / "platformer_house.owl")
						.has_value());
	const auto before = nest::test::sceneState(*session.getSessionScene());
	McpServer server{session, options()};
	ASSERT_TRUE(server.start());

	const auto list = post(server, rpc("tools/list"));
	ASSERT_EQ(list.status, 200);
	std::set<std::string> names;
	for (const auto& tool: list.json()["result"]["tools"]) names.insert(tool["name"].get<std::string>());
	for (const auto& spec: session.getRegistry().getCommands()) EXPECT_TRUE(names.contains(spec.name)) << spec.name;
	EXPECT_TRUE(names.contains("play.run"));

	const auto created = callTool(server, "entity.create", {{"name", "EndToEnd"}});
	ASSERT_FALSE(created["isError"].get<bool>()) << created.dump();
	EXPECT_NE(nest::test::sceneState(*session.getSessionScene()), before);
	ASSERT_FALSE(callTool(server, "edit.undo", json::object())["isError"].get<bool>());
	EXPECT_EQ(nest::test::sceneState(*session.getSessionScene()), before);

	const auto played = callTool(server, "play.run", {{"frames", 10}, {"actions", {{"platformer_move", 1}}}});
	ASSERT_FALSE(played["isError"].get<bool>()) << played.dump();
	EXPECT_EQ(played["structuredContent"]["frames"], 10);
	ASSERT_EQ(played["content"].size(), 2u);
	EXPECT_EQ(played["content"][1]["type"], "image");
	EXPECT_EQ(played["content"][1]["mimeType"], "image/png");
	EXPECT_FALSE(session.isScenePlaying());
	EXPECT_EQ(nest::test::sceneState(*session.getSessionScene()), before);

	const auto refused = callTool(server, "play.run", {{"frames", 1}, {"actions", {{"nope", true}}}});
	EXPECT_TRUE(refused["isError"].get<bool>());
	server.stop();
}

TEST_F(McpHttpTest, AnswersADeferredMessageWhenItsWorkEnds) {
	McpHttpServer server;
	ASSERT_TRUE(server.start({.port = 0}, "secret").has_value());
	int polls = 0;
	const McpHttpServer::DeferrableHandler handler = [&polls](const std::string_view) -> DispatchResult {
		return {.poll = [&polls](const bool iCancel) -> std::optional<std::string> {
			if (!iCancel && ++polls < 3)
				return std::nullopt;
			return R"({"done":true})";
		}};
	};
	const auto reply = send(
			server.getPort(),
			[](httplib::Client& ioClient) -> httplib::Result {
				return ioClient.Post("/mcp", {{"Authorization", "Bearer secret"}}, R"({"x":1})", "application/json");
			},
			[&server, &handler]() -> void {
				std::ignore = server.pump(handler, 4);
				std::ignore = server.pollDeferred();
			});
	EXPECT_EQ(reply.status, 200);
	EXPECT_EQ(reply.body, R"({"done":true})");
	EXPECT_EQ(polls, 3);
	EXPECT_EQ(server.getDeferredCount(), 0u);
	server.stop();
}

TEST_F(McpHttpTest, CancelsADeferredMessageAtStop) {
	McpHttpServer server;
	ASSERT_TRUE(server.start({.port = 0}, "secret").has_value());
	bool cancelled = false;
	const McpHttpServer::DeferrableHandler handler = [&cancelled](const std::string_view) -> DispatchResult {
		return {.poll = [&cancelled](const bool iCancel) -> std::optional<std::string> {
			cancelled = iCancel;
			return iCancel ? std::optional<std::string>{R"({"cancelled":true})"} : std::nullopt;
		}};
	};
	const auto port = server.getPort();
	auto client = std::async(std::launch::async, [port]() -> HttpReply {
		httplib::Client http{"127.0.0.1", port};
		http.set_read_timeout(20s);
		const auto result = http.Post("/mcp", {{"Authorization", "Bearer secret"}}, R"({"x":1})", "application/json");
		return result ? HttpReply{.status = result->status, .body = result->body, .headers = {}} : HttpReply{};
	});
	while (server.getDeferredCount() == 0) {
		std::ignore = server.pump(handler, 4);
		std::this_thread::sleep_for(1ms);
	}
	EXPECT_EQ(server.pollDeferred(), 0u);
	server.stop();
	const auto reply = client.get();
	EXPECT_EQ(reply.status, 200);
	EXPECT_EQ(reply.body, R"({"cancelled":true})");
	EXPECT_TRUE(cancelled);
}

TEST_F(McpHttpTest, CancelsADeferredMessageThatTakesTooLong) {
	McpHttpServer server;
	ASSERT_TRUE(server.start({.port = 0, .deferredTimeout = 30ms}, "secret").has_value());
	bool cancelled = false;
	const McpHttpServer::DeferrableHandler handler = [&cancelled](const std::string_view) -> DispatchResult {
		return {.poll = [&cancelled](const bool iCancel) -> std::optional<std::string> {
			cancelled = iCancel;
			return iCancel ? std::optional<std::string>{R"({"late":true})"} : std::nullopt;
		}};
	};
	const auto reply = send(
			server.getPort(),
			[](httplib::Client& ioClient) -> httplib::Result {
				return ioClient.Post("/mcp", {{"Authorization", "Bearer secret"}}, R"({"x":1})", "application/json");
			},
			[&server, &handler]() -> void {
				std::ignore = server.pump(handler, 4);
				std::ignore = server.pollDeferred();
			});
	EXPECT_EQ(reply.status, 200);
	EXPECT_EQ(reply.body, R"({"late":true})");
	EXPECT_TRUE(cancelled);
	server.stop();
}
