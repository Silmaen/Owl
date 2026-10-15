/**
 * @file McpHttpServer.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "McpDispatcher.h"

#include <core/Core.h>
#include <core/expected.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace owl::nest::mcp {

/**
 * @brief
 *  Why the HTTP server did not start.
 */
enum struct HttpError : uint8_t {
	AlreadyRunning,///< The server listens already.
	NoToken,///< No bearer token was given.
	BindFailed,///< The port cannot be bound on 127.0.0.1 (taken, or reserved).
	RandomFailed///< The system random generator failed (session id).
};

/**
 * @brief
 *  Settings of the HTTP transport.
 */
struct HttpOptions {
	/// Port listened on `127.0.0.1`; 0 picks a free one (tests).
	uint16_t port{0};
	/// Messages waiting for the main loop beyond which a request gets 503.
	size_t queueCapacity{4};
	/// Largest request body, in bytes (413 beyond).
	size_t maxBodyBytes{size_t{1} << 20u};
	/// Time a request waits for the main loop before its timeout answer.
	std::chrono::milliseconds requestTimeout{std::chrono::seconds{30}};
	/// Time a deferred answer (a play spanning frames) may take before the work is cancelled and answered.
	std::chrono::milliseconds deferredTimeout{std::chrono::minutes{10}};
	/// Threads serving the connections.
	size_t workerThreads{8};
	/// Time after a served request during which a client counts as active.
	std::chrono::milliseconds activeWindow{std::chrono::seconds{10}};
};

/**
 * @brief
 *  Activity of the HTTP server.
 */
enum struct HttpStatus : uint8_t {
	Off,///< No socket open.
	Listening,///< Listening, no request lately.
	ClientActive///< A request was served within the active window.
};

/**
 * @brief
 *  MCP Streamable HTTP transport, JSON responses only, bound to `127.0.0.1`.
 *
 * The server threads only check and queue the messages: the main loop runs them by calling `pump`, so the session
 * is only ever touched by the thread that owns it. Every request is checked before its body is read: `Host` and
 * `Origin` must be loopback (403), the bearer token must match (401), the body must be `application/json` (415),
 * a `Mcp-Session-Id` other than the server's is unknown (404); then the body must fit `maxBodyBytes` (413) and the
 * queue must have room (503). A message not run within `requestTimeout` gets a JSON-RPC error and is dropped.
 */
class McpHttpServer final {
public:
	/// Message handler: one JSON-RPC message in, its response out (nothing for a notification).
	using Handler = std::function<std::optional<std::string>(std::string_view iMessage)>;
	/// Message handler that may answer later: the response, or the poll that gives it.
	using DeferrableHandler = std::function<DispatchResult(std::string_view iMessage)>;
	/// Path of the MCP endpoint.
	static constexpr std::string_view endpointPath = "/mcp";
	/// JSON-RPC error code of a message the main loop did not run in time.
	static constexpr int requestTimeoutCode = -32001;

	McpHttpServer(const McpHttpServer&) = delete;

	McpHttpServer(McpHttpServer&&) = delete;

	auto operator=(const McpHttpServer&) -> McpHttpServer& = delete;

	auto operator=(McpHttpServer&&) -> McpHttpServer& = delete;

	/**
	 * @brief
	 *  Constructor; opens nothing.
	 */
	McpHttpServer();

	/**
	 * @brief
	 *  Destructor: stops the server.
	 */
	~McpHttpServer();

	/**
	 * @brief
	 *  Bind `127.0.0.1` and listen on a thread of its own.
	 * @param[in] iOptions The settings.
	 * @param[in] iToken The bearer token the clients must send.
	 * @return Nothing, or why it did not start.
	 */
	auto start(const HttpOptions& iOptions, const std::string& iToken) -> expected<void, HttpError>;

	/**
	 * @brief
	 *  Answer every queued message with 503, close the socket and join the threads. Main thread.
	 */
	void stop();

	/**
	 * @brief
	 *  Check whether the server listens.
	 * @return True between a successful `start` and `stop`.
	 */
	[[nodiscard]] auto isRunning() const -> bool;

	/**
	 * @brief
	 *  Get the port listened on.
	 * @return The bound port, 0 when stopped.
	 */
	[[nodiscard]] auto getPort() const -> uint16_t;

	/**
	 * @brief
	 *  Get the endpoint URL.
	 * @return `http://127.0.0.1:<port>/mcp`, empty when stopped.
	 */
	[[nodiscard]] auto getEndpoint() const -> std::string;

	/**
	 * @brief
	 *  Get the session id answered in `Mcp-Session-Id`.
	 * @return The id, new at every start.
	 */
	[[nodiscard]] auto getSessionId() const -> std::string;

	/**
	 * @brief
	 *  Replace the bearer token; requests already accepted are not affected.
	 * @param[in] iToken The new token.
	 */
	void setToken(const std::string& iToken);

	/**
	 * @brief
	 *  Run the queued messages through a handler, on the calling thread (the main loop).
	 * @param[in] iHandler The message handler (the dispatcher).
	 * @param[in] iMaxMessages Largest number of messages run by this call.
	 * @return Number of messages run.
	 */
	auto pump(const Handler& iHandler, size_t iMaxMessages) -> size_t;

	/**
	 * @brief
	 *  Run the queued messages through a handler that may answer later, on the calling thread (the main loop).
	 *
	 * A message whose answer is deferred stays out of the queue until its poll answers (`pollDeferred`).
	 * @param[in] iHandler The message handler.
	 * @param[in] iMaxMessages Largest number of messages run by this call.
	 * @return Number of messages run.
	 */
	auto pump(const DeferrableHandler& iHandler, size_t iMaxMessages) -> size_t;

	/**
	 * @brief
	 *  Poll the deferred answers; the ones ready (or past `HttpOptions::deferredTimeout`, cancelled) are sent. Main thread.
	 * @return Number of answers sent.
	 */
	auto pollDeferred() -> size_t;

	/**
	 * @brief
	 *  Get the number of answers waiting for their work to end.
	 * @return The count.
	 */
	[[nodiscard]] auto getDeferredCount() const -> size_t;

	/**
	 * @brief
	 *  Get the number of messages waiting for the main loop.
	 * @return The queue length.
	 */
	[[nodiscard]] auto getPendingCount() const -> size_t;

	/**
	 * @brief
	 *  Get the activity of the server.
	 * @return Off, listening, or client active.
	 */
	[[nodiscard]] auto getStatus() const -> HttpStatus;

	/**
	 * @brief
	 *  Check a `Host` header against the loopback names of a port.
	 * @param[in] iHost The header value.
	 * @param[in] iPort The port listened on.
	 * @return True for `127.0.0.1:<port>` or `localhost:<port>`.
	 */
	[[nodiscard]] static auto isLoopbackHost(std::string_view iHost, uint16_t iPort) -> bool;

	/**
	 * @brief
	 *  Check an `Origin` header.
	 * @param[in] iOrigin The header value.
	 * @return True for an `http` or `https` origin on `127.0.0.1` or `localhost`, any port.
	 */
	[[nodiscard]] static auto isLoopbackOrigin(std::string_view iOrigin) -> bool;

	/**
	 * @brief
	 *  Check a `Content-Type` header.
	 * @param[in] iContentType The header value.
	 * @return True for `application/json`, parameters (`charset`) allowed.
	 */
	[[nodiscard]] static auto isJsonContentType(std::string_view iContentType) -> bool;

private:
	struct Impl;
	/// Server state, shared with its threads.
	uniq<Impl> mp_impl;
};

}// namespace owl::nest::mcp
