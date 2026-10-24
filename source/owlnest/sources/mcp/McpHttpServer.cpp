/**
 * @file McpHttpServer.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "McpHttpServer.h"

#include "McpDispatcher.h"
#include "McpToken.h"

#include <core/Log.h>
#include <debug/Profiler.h>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <deque>
#include <exception>
#include <format>
#include <future>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest::mcp {

namespace {

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;
using HandlerResponse = httplib::Server::HandlerResponse;

// Answer to the messages still queued at stop, built without JSON so stopping (and destroying) cannot throw.
constexpr std::string_view g_stoppedBody =
		R"({"jsonrpc":"2.0","id":null,"error":{"code":-32603,"message":"The server stopped before running the request"}})";

// Answer of a queued message.
struct Reply {
	// HTTP status.
	int status{200};
	// JSON body; empty for 202.
	std::string body;
};

// A message waiting for the main loop.
struct Job {
	// The JSON-RPC message.
	std::string body;
	// Fulfilled by the main loop, or by `stop`.
	std::promise<Reply> promise;
	// Set by whoever takes the job first: the main loop runs it, the server thread abandons it on timeout.
	std::atomic<bool> claimed{false};
};

// A message whose answer waits for the main loop to end some work.
struct Deferred {
	// The message and its promise.
	shared<Job> job;
	// Gives the answer once the work ends.
	DeferredPoll poll;
	// When the answer was deferred.
	Clock::time_point since;
};

auto errorBody(const json& iId, const int iCode, const std::string_view iMessage) -> std::string {
	return json{{"jsonrpc", "2.0"}, {"id", iId}, {"error", {{"code", iCode}, {"message", iMessage}}}}.dump();
}

void refuse(httplib::Response& oResponse, const int iStatus, const std::string_view iMessage) {
	OWL_CORE_WARN("Mcp: Request refused with {} ({}).", iStatus, iMessage)
	oResponse.status = iStatus;
	oResponse.set_content(errorBody(nullptr, static_cast<int>(RpcError::InvalidRequest), iMessage), "application/json");
}

auto messageId(const std::string& iBody) -> json {
	const auto message = json::parse(iBody, nullptr, false);
	if (message.is_object()) {
		if (const auto id = message.find("id"); id != message.end() && (id->is_string() || id->is_number_integer()))
			return *id;
	}
	return nullptr;
}

auto toLower(const std::string_view iText) -> std::string {
	std::string lower{iText};
	std::ranges::transform(lower, lower.begin(), [](const char iChar) -> char {
		return static_cast<char>(std::tolower(static_cast<unsigned char>(iChar)));
	});
	return lower;
}

auto isLoopbackName(const std::string_view iName) -> bool { return iName == "127.0.0.1" || iName == "localhost"; }

}// namespace

struct McpHttpServer::Impl {
	// The server of the current run.
	uniq<httplib::Server> server;
	// Thread running `listen_after_bind`.
	std::thread thread;
	// Settings of the current run.
	HttpOptions options;
	// Guards `queue`, `accepting`, `token` and `sessionId`.
	mutable std::mutex mutex;
	// Messages waiting for the main loop.
	std::deque<shared<Job>> queue;
	// Answers waiting for their work to end (main thread only).
	std::vector<Deferred> deferred;
	// False while stopping: new messages get 503.
	bool accepting{false};
	// Bearer token.
	std::string token;
	// Value of `Mcp-Session-Id`.
	std::string sessionId;
	// Port listened on (0 when stopped).
	std::atomic<uint16_t> port{0};
	// Time of the last served request, in clock ticks.
	std::atomic<Clock::rep> lastServed{0};
	// True between start and stop.
	std::atomic<bool> running{false};

	[[nodiscard]] auto checkHeaders(const httplib::Request& iRequest, httplib::Response& oResponse) const
			-> HandlerResponse;

	[[nodiscard]] auto checkRequest(const httplib::Request& iRequest, httplib::Response& oResponse) const
			-> HandlerResponse;

	void onPost(const httplib::Request& iRequest, httplib::Response& oResponse);

	void configure();
};

auto McpHttpServer::Impl::checkHeaders(const httplib::Request& iRequest, httplib::Response& oResponse) const
		-> HandlerResponse {
	if (!isLoopbackHost(iRequest.get_header_value("Host"), port.load())) {
		refuse(oResponse, 403, "Host is not a loopback address of this server");
		return HandlerResponse::Handled;
	}
	if (iRequest.has_header("Origin") && !isLoopbackOrigin(iRequest.get_header_value("Origin"))) {
		refuse(oResponse, 403, "Origin is not a loopback origin");
		return HandlerResponse::Handled;
	}
	return HandlerResponse::Unhandled;
}

auto McpHttpServer::Impl::checkRequest(const httplib::Request& iRequest, httplib::Response& oResponse) const
		-> HandlerResponse {
	constexpr std::string_view bearer = "Bearer ";
	const auto authorization = iRequest.get_header_value("Authorization");
	std::string expected;
	std::string session;
	{
		const std::lock_guard<std::mutex> lock{mutex};
		expected = token;
		session = sessionId;
	}
	if (!authorization.starts_with(bearer) ||
		!isSameToken(expected, std::string_view{authorization}.substr(bearer.size()))) {
		oResponse.set_header("WWW-Authenticate", "Bearer");
		refuse(oResponse, 401, "Missing or wrong bearer token");
		return HandlerResponse::Handled;
	}
	if (iRequest.method == "POST" && !isJsonContentType(iRequest.get_header_value("Content-Type"))) {
		refuse(oResponse, 415, "Content-Type must be application/json");
		return HandlerResponse::Handled;
	}
	if (iRequest.has_header("Mcp-Session-Id") && iRequest.get_header_value("Mcp-Session-Id") != session) {
		refuse(oResponse, 404, "Unknown Mcp-Session-Id");
		return HandlerResponse::Handled;
	}
	if (iRequest.has_header("MCP-Protocol-Version") &&
		!std::ranges::contains(McpDispatcher::supportedProtocolVersions,
							   iRequest.get_header_value("MCP-Protocol-Version"))) {
		refuse(oResponse, 400, "Unsupported MCP-Protocol-Version");
		return HandlerResponse::Handled;
	}
	return HandlerResponse::Unhandled;
}

void McpHttpServer::Impl::onPost(const httplib::Request& iRequest, httplib::Response& oResponse) {
	auto job = mkShared<Job>();
	job->body = iRequest.body;
	auto future = job->promise.get_future();
	{
		const std::lock_guard<std::mutex> lock{mutex};
		if (!accepting) {
			refuse(oResponse, 503, "The server is stopping");
			return;
		}
		if (queue.size() >= options.queueCapacity) {
			oResponse.set_header("Retry-After", "1");
			refuse(oResponse, 503, "Too many pending requests");
			return;
		}
		queue.push_back(job);
	}
	if (options.onQueued)
		options.onQueued();
	if (future.wait_for(options.requestTimeout) != std::future_status::ready) {
		if (!job->claimed.exchange(true)) {
			{
				const std::lock_guard<std::mutex> lock{mutex};
				std::erase(queue, job);
			}
			OWL_CORE_WARN("Mcp: Request timed out after {} ms (the editor is busy).", options.requestTimeout.count())
			oResponse.status = 200;
			oResponse.set_content(errorBody(messageId(job->body), requestTimeoutCode,
											std::format("Request timed out after {} ms: the editor is busy (a gesture "
														"or a modal is in progress); nothing was run",
														options.requestTimeout.count())),
								  "application/json");
			return;
		}
		future.wait();
	}
	const auto reply = future.get();
	lastServed.store(Clock::now().time_since_epoch().count());
	oResponse.status = reply.status;
	if (!reply.body.empty())
		oResponse.set_content(reply.body, "application/json");
	if (reply.status < 300) {
		const std::lock_guard<std::mutex> lock{mutex};
		oResponse.set_header("Mcp-Session-Id", sessionId);
	}
}

void McpHttpServer::Impl::configure() {
	const size_t workers = std::max<size_t>(1, options.workerThreads);
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory) httplib takes ownership of the raw pointer
	server->new_task_queue = [workers]() -> httplib::TaskQueue* { return new httplib::ThreadPool(1, workers); };
	// httplib's default SO_REUSEPORT would let another local process listen on the same port and take requests.
	server->set_socket_options([](const socket_t iSocket) -> void {
#ifdef OWL_PLATFORM_WINDOWS
		std::ignore = httplib::set_socket_opt(iSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 1);
#else
		std::ignore = httplib::set_socket_opt(iSocket, SOL_SOCKET, SO_REUSEADDR, 1);
#endif
	});
	// Without it, Nagle and the client's delayed ACK hold every answer about 40 ms.
	server->set_tcp_nodelay(true);
	server->set_payload_max_length(options.maxBodyBytes);
	server->set_keep_alive_timeout(2);
	server->set_read_timeout(5);
	server->set_write_timeout(5);
	server->set_pre_routing_handler(
			[this](const httplib::Request& iRequest, httplib::Response& oResponse) -> HandlerResponse {
				return checkHeaders(iRequest, oResponse);
			});
	server->set_pre_request_handler(
			[this](const httplib::Request& iRequest, httplib::Response& oResponse) -> HandlerResponse {
				return checkRequest(iRequest, oResponse);
			});
	const std::string path{endpointPath};
	server->Post(path, [this](const httplib::Request& iRequest, httplib::Response& oResponse) -> void {
		onPost(iRequest, oResponse);
	});
	const auto notAllowed = [](const httplib::Request&, httplib::Response& oResponse) -> void {
		oResponse.set_header("Allow", "POST");
		refuse(oResponse, 405, "Only POST is served (no event stream, no session termination)");
	};
	server->Get(path, notAllowed);
	server->Delete(path, notAllowed);
}

McpHttpServer::McpHttpServer() : mp_impl{mkUniq<Impl>()} {}

McpHttpServer::~McpHttpServer() { stop(); }

auto McpHttpServer::start(const HttpOptions& iOptions, const std::string& iToken) -> expected<void, HttpError> {
	if (mp_impl->running.load()) {
		OWL_CORE_WARN("Mcp: The HTTP server already listens on port {}.", mp_impl->port.load())
		return unexpected{HttpError::AlreadyRunning};
	}
	if (iToken.empty()) {
		OWL_CORE_WARN("Mcp: No bearer token, the HTTP server does not start.")
		return unexpected{HttpError::NoToken};
	}
	auto session = generateRandomHex(16);
	if (!session) {
		OWL_CORE_WARN("Mcp: No session id, the HTTP server does not start.")
		return unexpected{HttpError::RandomFailed};
	}
	mp_impl->options = iOptions;
	mp_impl->server = mkUniq<httplib::Server>();
	mp_impl->configure();
	int bound = iOptions.port;
	if (iOptions.port == 0)
		bound = mp_impl->server->bind_to_any_port("127.0.0.1");
	else if (!mp_impl->server->bind_to_port("127.0.0.1", iOptions.port))
		bound = -1;
	if (bound <= 0) {
		OWL_CORE_ERROR("Mcp: Cannot listen on 127.0.0.1:{}.", iOptions.port)
		mp_impl->server.reset();
		return unexpected{HttpError::BindFailed};
	}
	{
		const std::lock_guard<std::mutex> lock{mp_impl->mutex};
		mp_impl->token = iToken;
		mp_impl->sessionId = std::move(*session);
		mp_impl->accepting = true;
	}
	mp_impl->port.store(static_cast<uint16_t>(bound));
	mp_impl->lastServed.store(0);
	mp_impl->thread = std::thread([impl = mp_impl.get()]() -> void {
		OWL_PROFILE_THREAD_NAME("Mcp HTTP")
		impl->server->listen_after_bind();
	});
	mp_impl->server->wait_until_ready();
	mp_impl->running.store(true);
	OWL_CORE_INFO("Mcp: Listening on http://127.0.0.1:{}{}.", bound, endpointPath)
	return {};
}

void McpHttpServer::stop() {
	if (!mp_impl->running.load())
		return;
	std::deque<shared<Job>> pending;
	{
		const std::lock_guard<std::mutex> lock{mp_impl->mutex};
		mp_impl->accepting = false;
		pending.swap(mp_impl->queue);
	}
	for (const auto& job: pending) {
		if (!job->claimed.exchange(true))
			job->promise.set_value({.status = 503, .body = std::string{g_stoppedBody}});
	}
	for (auto& waiting: mp_impl->deferred) {
		auto answer = waiting.poll(/*iCancel=*/true);
		waiting.job->promise.set_value(
				{.status = 200, .body = answer ? std::move(*answer) : std::string{g_stoppedBody}});
	}
	mp_impl->deferred.clear();
	mp_impl->server->stop();
	if (mp_impl->thread.joinable())
		mp_impl->thread.join();
	mp_impl->server.reset();
	mp_impl->port.store(0);
	mp_impl->running.store(false);
	OWL_CORE_INFO("Mcp: HTTP server stopped.")
}

auto McpHttpServer::isRunning() const -> bool { return mp_impl->running.load(); }

auto McpHttpServer::getPort() const -> uint16_t { return mp_impl->port.load(); }

auto McpHttpServer::getEndpoint() const -> std::string {
	const auto port = getPort();
	return port == 0 ? std::string{} : std::format("http://127.0.0.1:{}{}", port, endpointPath);
}

auto McpHttpServer::getSessionId() const -> std::string {
	const std::lock_guard<std::mutex> lock{mp_impl->mutex};
	return mp_impl->sessionId;
}

void McpHttpServer::setToken(const std::string& iToken) {
	const std::lock_guard<std::mutex> lock{mp_impl->mutex};
	mp_impl->token = iToken;
}

auto McpHttpServer::pump(const Handler& iHandler, const size_t iMaxMessages) -> size_t {
	return pump(
			[&iHandler](const std::string_view iMessage) -> DispatchResult { return {.answer = iHandler(iMessage)}; },
			iMaxMessages);
}

auto McpHttpServer::pump(const DeferrableHandler& iHandler, const size_t iMaxMessages) -> size_t {
	size_t ran = 0;
	while (ran < iMaxMessages) {
		shared<Job> job;
		{
			const std::lock_guard<std::mutex> lock{mp_impl->mutex};
			if (mp_impl->queue.empty())
				break;
			job = mp_impl->queue.front();
			mp_impl->queue.pop_front();
		}
		if (job->claimed.exchange(true))
			continue;
		Reply reply;
		try {
			auto result = iHandler(job->body);
			if (result.poll) {
				mp_impl->deferred.push_back({.job = job, .poll = std::move(result.poll), .since = Clock::now()});
				++ran;
				continue;
			}
			if (result.answer.has_value())
				reply = {.status = 200, .body = std::move(*result.answer)};
			else
				reply = {.status = 202, .body = {}};
		} catch (const std::exception& iEx) {
			OWL_CORE_ERROR("Mcp: Request failed ({}).", iEx.what())
			reply = {.status = 500,
					 .body = errorBody(messageId(job->body), static_cast<int>(RpcError::InternalError), iEx.what())};
		}
		job->promise.set_value(std::move(reply));
		++ran;
	}
	return ran;
}

auto McpHttpServer::pollDeferred() -> size_t {
	size_t sent = 0;
	const auto now = Clock::now();
	std::erase_if(mp_impl->deferred, [this, now, &sent](Deferred& ioWaiting) -> bool {
		const bool expired = now - ioWaiting.since > mp_impl->options.deferredTimeout;
		if (expired)
			OWL_CORE_WARN("Mcp: A deferred answer took more than {} ms, cancelled.",
						  mp_impl->options.deferredTimeout.count())
		auto answer = ioWaiting.poll(expired);
		if (!answer)
			return false;
		ioWaiting.job->promise.set_value({.status = 200, .body = std::move(*answer)});
		++sent;
		return true;
	});
	return sent;
}

auto McpHttpServer::getDeferredCount() const -> size_t { return mp_impl->deferred.size(); }

auto McpHttpServer::getPendingCount() const -> size_t {
	const std::lock_guard<std::mutex> lock{mp_impl->mutex};
	return mp_impl->queue.size();
}

auto McpHttpServer::getStatus() const -> HttpStatus {
	if (!isRunning())
		return HttpStatus::Off;
	const auto last = mp_impl->lastServed.load();
	if (last == 0)
		return HttpStatus::Listening;
	const auto elapsed = Clock::now() - Clock::time_point{Clock::duration{last}};
	return elapsed <= mp_impl->options.activeWindow ? HttpStatus::ClientActive : HttpStatus::Listening;
}

auto McpHttpServer::isLoopbackHost(const std::string_view iHost, const uint16_t iPort) -> bool {
	const auto colon = iHost.rfind(':');
	if (colon == std::string_view::npos || iPort == 0)
		return false;
	return isLoopbackName(toLower(iHost.substr(0, colon))) && iHost.substr(colon + 1) == std::format("{}", iPort);
}

auto McpHttpServer::isLoopbackOrigin(const std::string_view iOrigin) -> bool {
	const auto origin = toLower(iOrigin);
	std::string_view rest{origin};
	if (rest.starts_with("http://"))
		rest.remove_prefix(7);
	else if (rest.starts_with("https://"))
		rest.remove_prefix(8);
	else
		return false;
	if (const auto colon = rest.find(':'); colon != std::string_view::npos) {
		const auto port = rest.substr(colon + 1);
		if (port.empty() || !std::ranges::all_of(port, [](const char iChar) -> bool {
				return std::isdigit(static_cast<unsigned char>(iChar)) != 0;
			}))
			return false;
		rest = rest.substr(0, colon);
	}
	return isLoopbackName(rest);
}

auto McpHttpServer::isJsonContentType(const std::string_view iContentType) -> bool {
	const auto type = toLower(iContentType.substr(0, iContentType.find(';')));
	const auto first = type.find_first_not_of(" \t");
	const auto last = type.find_last_not_of(" \t");
	return first != std::string::npos && type.substr(first, last - first + 1) == "application/json";
}

}// namespace owl::nest::mcp
