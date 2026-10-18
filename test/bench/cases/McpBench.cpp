/**
 * @file McpBench.cpp
 * @author Silmaen
 * @date 18/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include "mcp/McpHttpServer.h"
#include "mcp/McpServer.h"
#include "session/HeadlessSession.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>

namespace owl::bench {

namespace {

using json = nlohmann::json;
using nest::mcp::McpHttpServer;
using nest::mcp::McpServer;
using namespace std::chrono_literals;

// What the main loop hands to the client thread and what it gets back; tickets order the two sides.
struct Exchange {
	std::atomic<uint32_t> requested{0};
	std::atomic<uint32_t> answered{0};
	std::atomic<bool> stopping{false};
	std::string body;
	int status{-1};
	std::string reply;
};

auto rpc(const std::string_view iMethod, const json& iParams) -> std::string {
	json message{{"jsonrpc", "2.0"}, {"id", 1}, {"method", iMethod}};
	if (!iParams.is_null())
		message["params"] = iParams;
	return message.dump();
}

// One kept-alive connection, so a round trip pays no TCP handshake nor thread start.
void serveRequests(Exchange& ioExchange, const uint16_t iPort, const std::string& iToken) {
	httplib::Client client{"127.0.0.1", iPort};
	client.set_keep_alive(true);
	client.set_tcp_nodelay(true);
	client.set_read_timeout(20s);
	const httplib::Headers headers{{"Authorization", "Bearer " + iToken}};
	uint32_t seen = 0;
	while (true) {
		ioExchange.requested.wait(seen, std::memory_order_acquire);
		seen = ioExchange.requested.load(std::memory_order_acquire);
		if (ioExchange.stopping.load(std::memory_order_acquire))
			return;
		const auto result =
				client.Post(std::string{McpHttpServer::endpointPath}, headers, ioExchange.body, "application/json");
		ioExchange.status = result ? result->status : -1;
		ioExchange.reply = result ? result->body : std::string{};
		ioExchange.answered.store(seen, std::memory_order_release);
	}
}

// The calling thread is the editor main loop: it runs the server's queue until the reply is back.
auto roundTrip(McpServer& ioServer, Exchange& ioExchange, const std::string& iBody) -> int {
	ioExchange.body = iBody;
	const uint32_t ticket = ioExchange.requested.fetch_add(1, std::memory_order_release) + 1;
	ioExchange.requested.notify_one();
	while (ioExchange.answered.load(std::memory_order_acquire) != ticket) {
		std::ignore = ioServer.onUpdate(false);
		std::this_thread::yield();
	}
	return ioExchange.status;
}

void runRoundTrips(Runner& ioRunner, McpServer& ioServer, Exchange& ioExchange, const uint64_t iTarget) {
	const auto ping = rpc("ping", nullptr);
	uint32_t failures = 0;
	ioRunner.measure("mcp/ping", 1,
					 [&]() -> void { failures += roundTrip(ioServer, ioExchange, ping) == 200 ? 0U : 1U; });
	const std::array renames{
			rpc("tools/call", {{"name", "entity.rename"}, {"arguments", {{"entity", iTarget}, {"name", "Ping"}}}}),
			rpc("tools/call", {{"name", "entity.rename"}, {"arguments", {{"entity", iTarget}, {"name", "Pong"}}}})};
	size_t next = 0;
	ioRunner.measure("mcp/tools_call/entity_rename", 1, [&]() -> void {
		failures += roundTrip(ioServer, ioExchange, renames[next]) == 200 ? 0U : 1U;
		next = 1 - next;
	});
	if (!ioRunner.wants("mcp/tools_call/entity_rename"))
		return;
	const bool renamed = roundTrip(ioServer, ioExchange, renames[0]) == 200 &&
						 !json::parse(ioExchange.reply)["result"]["isError"].get<bool>();
	ioRunner.metric("mcp/tools_call/entity_rename_ok", renamed && failures == 0 ? 1.0 : 0.0,
					"1 when every call answered 200 and the tool succeeded");
}

}// namespace

void runMcpBenches(Runner& ioRunner) {
	if (!ioRunner.wants("mcp"))
		return;
	nest::session::HeadlessSession session;
	session.newScene();
	const uint64_t target = session.getSessionScene()->createEntity("Target").getUUID();
	const auto tokenDir = std::filesystem::temp_directory_path() / "owl_bench_mcp";
	{
		McpServer server{session,
						 {.dispatcher = {}, .http = {}, .tokenFile = tokenDir / "mcp_token", .maxMessagesPerFrame = 8}};
		if (!server.start()) {
			ioRunner.metric("mcp/server_started", 0.0, "the MCP server could not listen on 127.0.0.1");
			return;
		}
		Exchange exchange;
		const std::jthread client{[&exchange, port = server.getPort(), token = server.getToken()]() -> void {
			serveRequests(exchange, port, token);
		}};
		runRoundTrips(ioRunner, server, exchange, target);
		exchange.stopping.store(true, std::memory_order_release);
		exchange.requested.fetch_add(1, std::memory_order_release);
		exchange.requested.notify_one();
	}
	std::error_code error;
	std::filesystem::remove_all(tokenDir, error);
}

}// namespace owl::bench
