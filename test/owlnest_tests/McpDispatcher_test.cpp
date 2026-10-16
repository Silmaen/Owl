/**
 * @file McpDispatcher_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "mcp/McpDispatcher.h"
#include "nestTestHelper.h"
#include "session/HeadlessSession.h"

#include <app/Application.h>
#include <scene/Entity.h>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::mcp;
using json = nlohmann::json;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class McpDispatcherTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "mcpDispatcherTest",
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

	/// Send a request and parse its response.
	static auto request(McpDispatcher& ioDispatcher, const std::string_view iMethod, const json& iParams = nullptr)
			-> json {
		json message{{"jsonrpc", "2.0"}, {"id", 7}, {"method", iMethod}};
		if (!iParams.is_null())
			message["params"] = iParams;
		const auto answer = ioDispatcher.handle(message.dump());
		if (!answer)
			return nullptr;
		return json::parse(*answer);
	}

	/// Call a tool and return its result.
	static auto callTool(McpDispatcher& ioDispatcher, const std::string_view iName, const json& iArguments) -> json {
		return request(ioDispatcher, "tools/call", {{"name", iName}, {"arguments", iArguments}})["result"];
	}

	/// Read a resource and return its first content.
	static auto readResource(McpDispatcher& ioDispatcher, const std::string_view iUri) -> json {
		return request(ioDispatcher, "resources/read", {{"uri", iUri}})["result"]["contents"][0];
	}

	static auto options() -> McpOptions { return {.helpRoot = std::filesystem::path{OWL_HELP_BUILD_DIR}}; }

	/// Dummy application on the Null renderer.
	shared<app::Application> m_app;
};
OWL_DIAG_POP

}// namespace

TEST_F(McpDispatcherTest, NegotiatesTheProtocolVersion) {
	session::HeadlessSession session;
	McpDispatcher dispatcher{session};
	EXPECT_TRUE(dispatcher.getProtocolVersion().empty());

	auto answer = request(dispatcher, "initialize",
						  {{"protocolVersion", "2025-11-25"},
						   {"capabilities", json::object()},
						   {"clientInfo", {{"name", "test-client"}, {"version", "1.0"}}}});
	EXPECT_EQ(answer["jsonrpc"], "2.0");
	EXPECT_EQ(answer["id"], 7);
	const auto& result = answer["result"];
	EXPECT_EQ(result["protocolVersion"], "2025-11-25");
	EXPECT_EQ(result["serverInfo"]["name"], "owl-nest");
	EXPECT_TRUE(result["capabilities"].contains("tools"));
	EXPECT_TRUE(result["capabilities"].contains("resources"));
	EXPECT_EQ(dispatcher.getClientName(), "test-client");
	EXPECT_FALSE(dispatcher.isInitialized());

	EXPECT_FALSE(dispatcher.handle(R"({"jsonrpc":"2.0","method":"notifications/initialized"})").has_value());
	EXPECT_TRUE(dispatcher.isInitialized());

	answer = request(dispatcher, "initialize", {{"protocolVersion", "2026-07-28"}});
	EXPECT_EQ(answer["result"]["protocolVersion"], "2026-07-28");
	answer = request(dispatcher, "initialize", {{"protocolVersion", "2024-11-05"}});
	EXPECT_EQ(answer["result"]["protocolVersion"], "2025-11-25");
	EXPECT_EQ(dispatcher.getProtocolVersion(), "2025-11-25");

	answer = request(dispatcher, "ping");
	EXPECT_TRUE(answer["result"].is_object());
	EXPECT_TRUE(answer["result"].empty());
}

TEST_F(McpDispatcherTest, ListsEveryCommandWithASchema) {
	session::HeadlessSession session;
	McpDispatcher dispatcher{session};
	const auto tools = request(dispatcher, "tools/list")["result"]["tools"];
	const auto& commands = session.getRegistry().getCommands();
	ASSERT_EQ(tools.size(), commands.size() + 1);
	EXPECT_EQ(tools.back()["name"], "play.run");
	for (size_t i = 0; i < commands.size(); ++i) {
		const auto& tool = tools[i];
		const auto& spec = commands[i];
		EXPECT_EQ(tool["name"], spec.name);
		EXPECT_EQ(tool["description"], spec.description);
		const auto& schema = tool["inputSchema"];
		ASSERT_EQ(schema["type"], "object") << spec.name;
		ASSERT_TRUE(schema["properties"].is_object()) << spec.name;
		EXPECT_EQ(schema["properties"].size(), spec.args.size()) << spec.name;
		for (const auto& arg: spec.args) {
			ASSERT_TRUE(schema["properties"].contains(arg.name)) << spec.name << " " << arg.name;
			const auto& property = schema["properties"][arg.name];
			EXPECT_TRUE(property.contains("type") || property.contains("anyOf")) << spec.name << " " << arg.name;
			EXPECT_EQ(property.contains("default"), arg.defaultValue.has_value()) << spec.name << " " << arg.name;
			if (!arg.choices.empty()) {
				EXPECT_EQ(property["enum"].size(), arg.choices.size());
			}
		}
		if (schema.contains("required")) {
			for (const auto& name: schema["required"]) EXPECT_TRUE(schema["properties"].contains(name)) << spec.name;
		}
	}
	const auto create = tools[0];
	EXPECT_EQ(create["name"], "entity.create");
	EXPECT_EQ(create["inputSchema"]["properties"]["name"]["default"], "Empty Entity");
	EXPECT_EQ(create["inputSchema"]["properties"]["parent"]["type"], json::array({"integer", "string"}));
	EXPECT_EQ(tools[5]["inputSchema"]["properties"]["translation"]["minItems"], 3);
}

TEST_F(McpDispatcherTest, CreatesThroughAToolThenUndoRestoresTheScene) {
	session::HeadlessSession session;
	session.newScene();
	McpDispatcher dispatcher{session};
	const auto before = nest::test::sceneState(*session.getSessionScene());

	const auto created = callTool(dispatcher, "entity.create", {{"name", "FromMcp"}});
	ASSERT_FALSE(created["isError"].get<bool>()) << created.dump();
	const auto uuid = created["structuredContent"]["entity"].get<uint64_t>();
	const auto entity = session.getSessionScene()->findEntityByUUID(core::UUID{uuid});
	ASSERT_TRUE(entity);
	EXPECT_EQ(entity.getName(), "FromMcp");
	EXPECT_EQ(json::parse(created["content"][0]["text"].get<std::string>()), created["structuredContent"]);

	const auto moved = callTool(dispatcher, "entity.set_transform", {{"entity", uuid}, {"translation", {1, 2, 3}}});
	ASSERT_FALSE(moved["isError"].get<bool>()) << moved.dump();
	EXPECT_FLOAT_EQ(nest::test::localX(entity), 1.f);

	EXPECT_FALSE(callTool(dispatcher, "edit.undo", json::object())["isError"].get<bool>());
	const auto undone = callTool(dispatcher, "edit.undo", json::object());
	ASSERT_FALSE(undone["isError"].get<bool>()) << undone.dump();
	EXPECT_EQ(undone["structuredContent"]["can_undo"], false);
	EXPECT_EQ(nest::test::sceneState(*session.getSessionScene()), before);
}

TEST_F(McpDispatcherTest, ReportsCommandFailuresAsToolErrors) {
	session::HeadlessSession session;
	session.newScene();
	McpDispatcher dispatcher{session};

	auto result = callTool(dispatcher, "entity.rename", {{"entity", "Nobody"}, {"name", "x"}});
	EXPECT_TRUE(result["isError"].get<bool>());
	EXPECT_EQ(result["structuredContent"]["error"], "UnknownEntity");
	result = callTool(dispatcher, "entity.create", {{"name", 3}});
	EXPECT_EQ(result["structuredContent"]["error"], "WrongArgumentType");
	result = callTool(dispatcher, "entity.create", {{"colour", "red"}});
	EXPECT_EQ(result["structuredContent"]["error"], "UnknownArgument");
	result = callTool(dispatcher, "entity.rename", {{"name", "x"}});
	EXPECT_EQ(result["structuredContent"]["error"], "MissingArgument");

	ASSERT_TRUE(session.startPlay({.frames = 1, .stopAtEnd = false}).has_value());
	result = callTool(dispatcher, "entity.create", json::object());
	EXPECT_TRUE(result["isError"].get<bool>());
	EXPECT_EQ(result["structuredContent"]["error"], "RefusedDuringPlay");
	EXPECT_FALSE(result["content"][0]["text"].get<std::string>().empty());
	ASSERT_TRUE(session.stopPlay().has_value());
}

TEST_F(McpDispatcherTest, AnswersProtocolErrorsWithTheirCodes) {
	session::HeadlessSession session;
	McpDispatcher dispatcher{session};
	const auto code = [&dispatcher](const std::string_view iMessage) -> json {
		const auto answer = dispatcher.handle(iMessage);
		return answer ? json::parse(*answer)["error"]["code"] : json(nullptr);
	};
	EXPECT_EQ(code("{not json"), -32700);
	EXPECT_EQ(json::parse(*dispatcher.handle("{not json"))["id"], nullptr);
	EXPECT_EQ(code(R"([{"jsonrpc":"2.0","id":1,"method":"ping"}])"), -32600);
	EXPECT_EQ(code(R"({"id":1,"method":"ping"})"), -32600);
	EXPECT_EQ(code(R"({"jsonrpc":"2.0","id":1,"method":42})"), -32600);
	EXPECT_EQ(code(R"({"jsonrpc":"2.0","id":{"a":1},"method":"ping"})"), -32600);
	EXPECT_EQ(code("42"), -32600);
	EXPECT_EQ(code(R"({"jsonrpc":"2.0","id":1,"method":"nothing/here"})"), -32601);
	EXPECT_EQ(code(R"({"jsonrpc":"2.0","id":1,"method":"ping","params":[1]})"), -32602);
	EXPECT_EQ(code(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})"), -32602);
	EXPECT_EQ(code(R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{}})"), -32602);
	EXPECT_EQ(code(R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"no.such_tool"}})"), -32602);
	EXPECT_EQ(code(R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"edit.undo","arguments":[]}})"),
			  -32602);
	EXPECT_EQ(code(R"({"jsonrpc":"2.0","id":1,"method":"resources/read","params":{}})"), -32602);
	EXPECT_EQ(code(R"({"jsonrpc":"2.0","id":1,"method":"resources/read","params":{"uri":"owl://nothing"}})"), -32002);

	EXPECT_FALSE(dispatcher.handle(R"({"jsonrpc":"2.0","method":"notifications/cancelled","params":{}})").has_value());
	EXPECT_FALSE(dispatcher.handle(R"({"jsonrpc":"2.0","method":"tools/list"})").has_value());
	EXPECT_FALSE(dispatcher.handle(R"({"jsonrpc":"2.0","id":3,"result":{}})").has_value());
	const auto answer = json::parse(*dispatcher.handle(R"({"jsonrpc":"2.0","id":"abc","method":"ping"})"));
	EXPECT_EQ(answer["id"], "abc");
}

TEST_F(McpDispatcherTest, ReadsTheResources) {
	session::HeadlessSession session;
	ASSERT_TRUE(session.loadScene(owl::test::getRootPath() / "engine_demo" / "scenes" / "platformer_house.owl")
						.has_value());
	McpDispatcher dispatcher{session, options()};

	const auto list = request(dispatcher, "resources/list")["result"]["resources"];
	ASSERT_GT(list.size(), 4u);
	EXPECT_EQ(list[0]["uri"], "owl://scene");
	bool hasReadme = false;
	for (const auto& resource: list) hasReadme = hasReadme || resource["uri"] == "owl://help/README";
	EXPECT_TRUE(hasReadme);
	EXPECT_EQ(request(dispatcher, "resources/templates/list")["result"]["resourceTemplates"].size(), 2u);

	const auto sceneContent = readResource(dispatcher, "owl://scene");
	EXPECT_EQ(sceneContent["mimeType"], "application/json");
	const auto tree = json::parse(sceneContent["text"].get<std::string>());
	EXPECT_TRUE(tree["open"].get<bool>());
	ASSERT_FALSE(tree["entities"].empty());
	const auto& first = tree["entities"][0];
	const auto uuid = first["id"].get<uint64_t>();

	const auto entity = readResource(dispatcher, std::format("owl://scene/entity/{}", uuid));
	EXPECT_EQ(entity["mimeType"], "application/yaml");
	EXPECT_NE(entity["text"].get<std::string>().find(first["name"].get<std::string>()), std::string::npos);
	EXPECT_EQ(request(dispatcher, "resources/read", {{"uri", "owl://scene/entity/12"}})["error"]["code"], -32002);
	EXPECT_EQ(request(dispatcher, "resources/read", {{"uri", "owl://scene/entity/abc"}})["error"]["code"], -32002);

	const auto prefabs = json::parse(readResource(dispatcher, "owl://prefabs")["text"].get<std::string>());
	bool hasCoinPair = false;
	for (const auto& file: prefabs["files"]) hasCoinPair = hasCoinPair || file["path"] == "prefabs/coin_pair.owlprefab";
	EXPECT_TRUE(hasCoinPair) << prefabs.dump();
	const auto assets = json::parse(readResource(dispatcher, "owl://assets")["text"].get<std::string>());
	EXPECT_GT(assets["files"].size(), prefabs["files"].size());

	const auto stats = json::parse(readResource(dispatcher, "owl://statistics")["text"].get<std::string>());
	EXPECT_TRUE(stats.contains("frame_count"));
	EXPECT_EQ(stats["entity_count"], session.getStatistics().entityCount);

	const auto readme = readResource(dispatcher, "owl://help/readme");
	EXPECT_EQ(readme["mimeType"], "text/markdown");
	EXPECT_FALSE(readme["text"].get<std::string>().empty());
	EXPECT_EQ(request(dispatcher, "resources/read", {{"uri", "owl://help/../index"}})["error"]["code"], -32002);
}

TEST_F(McpDispatcherTest, ServesWithoutSceneNorHelp) {
	session::HeadlessSession session;
	McpDispatcher dispatcher{session};
	EXPECT_EQ(request(dispatcher, "resources/list")["result"]["resources"].size(), 4u);
	const auto tree = json::parse(readResource(dispatcher, "owl://scene")["text"].get<std::string>());
	EXPECT_FALSE(tree["open"].get<bool>());
	const auto result = callTool(dispatcher, "entity.create", json::object());
	EXPECT_TRUE(result["isError"].get<bool>());
}
