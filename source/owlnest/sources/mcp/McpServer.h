/**
 * @file McpServer.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../session/EditorSession.h"
#include "McpDispatcher.h"
#include "McpHttpServer.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace owl::nest::mcp {

/**
 * @brief
 *  Settings of an MCP server.
 */
struct McpServerOptions {
	/// Default port of the editor setting.
	static constexpr uint16_t defaultPort = 47800;
	/// Settings of the dispatcher (help bundle).
	McpOptions dispatcher{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Settings of the HTTP transport (the port is replaced by `applySettings`).
	HttpOptions http{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// File the token is written to; empty: `getDefaultTokenFile()`.
	std::filesystem::path tokenFile{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Messages run per frame at most.
	size_t maxMessagesPerFrame{8};
};

/**
 * @brief
 *  The MCP server of an editor session: the dispatcher behind the HTTP transport, its bearer token and its file.
 *
 * Off until `start` (or `applySettings` with the setting on). The token is drawn at the first start of the process
 * and kept until `regenerateToken`; its file exists while the server listens. `onUpdate`, called by the main loop,
 * runs the queued messages unless the editor is busy (a gesture or a modal), in which case they wait (and time out).
 */
class McpServer final {
public:
	McpServer(const McpServer&) = delete;

	McpServer(McpServer&&) = delete;

	auto operator=(const McpServer&) -> McpServer& = delete;

	auto operator=(McpServer&&) -> McpServer& = delete;

	/**
	 * @brief
	 *  Serve a session; opens nothing.
	 * @param[in,out] ioSession The session the tools act on (outlives the server).
	 * @param[in] iOptions The settings.
	 */
	explicit McpServer(session::EditorSession& ioSession, const McpServerOptions& iOptions = {});

	/**
	 * @brief
	 *  Destructor: stops the server and removes the token file.
	 */
	~McpServer();

	/**
	 * @brief
	 *  Write the token file and listen on the configured port.
	 * @return True when the server listens.
	 */
	auto start() -> bool;

	/**
	 * @brief
	 *  Stop listening (pending requests get 503) and remove the token file.
	 */
	void stop();

	/**
	 * @brief
	 *  Follow the editor settings: start, stop or move the server; a port that failed is not retried until the
	 *  settings change.
	 * @param[in] iEnabled Whether the server should listen.
	 * @param[in] iPort The port (0: any free port).
	 */
	void applySettings(bool iEnabled, uint16_t iPort);

	/**
	 * @brief
	 *  Draw a new token; when listening, rewrite the file and refuse the old token from now on.
	 * @return True on success.
	 */
	auto regenerateToken() -> bool;

	/**
	 * @brief
	 *  Run the queued messages on the calling (main) thread.
	 * @param[in] iBusy True while a gesture or a modal is in progress: the messages wait.
	 * @return Number of messages run.
	 */
	auto onUpdate(bool iBusy) -> size_t;

	/**
	 * @brief
	 *  Check whether the server listens.
	 * @return True when listening.
	 */
	[[nodiscard]] auto isRunning() const -> bool { return m_http.isRunning(); }

	/**
	 * @brief
	 *  Get the activity of the server.
	 * @return Off, listening or client active.
	 */
	[[nodiscard]] auto getStatus() const -> HttpStatus { return m_http.getStatus(); }

	/**
	 * @brief
	 *  Get the endpoint URL.
	 * @return The URL, empty when off.
	 */
	[[nodiscard]] auto getEndpoint() const -> std::string { return m_http.getEndpoint(); }

	/**
	 * @brief
	 *  Get the port listened on.
	 * @return The port, 0 when off.
	 */
	[[nodiscard]] auto getPort() const -> uint16_t { return m_http.getPort(); }

	/**
	 * @brief
	 *  Get the token file.
	 * @return The path (the file exists while listening).
	 */
	[[nodiscard]] auto getTokenFile() const -> const std::filesystem::path& { return m_tokenFile; }

	/**
	 * @brief
	 *  Get the bearer token.
	 * @return The token, empty before the first start.
	 */
	[[nodiscard]] auto getToken() const -> const std::string& { return m_token; }

	/**
	 * @brief
	 *  Get the port of the last failed start.
	 * @return The port, nothing when the last start succeeded.
	 */
	[[nodiscard]] auto getFailedPort() const -> std::optional<uint16_t> { return m_failedPort; }

	/**
	 * @brief
	 *  Access the dispatcher.
	 * @return The dispatcher.
	 */
	[[nodiscard]] auto getDispatcher() -> McpDispatcher& { return m_dispatcher; }

	/**
	 * @brief
	 *  Access the transport.
	 * @return The HTTP server.
	 */
	[[nodiscard]] auto getHttp() -> McpHttpServer& { return m_http; }

private:
	/// Settings.
	McpServerOptions m_options;
	/// File of the token.
	std::filesystem::path m_tokenFile;
	/// The JSON-RPC core.
	McpDispatcher m_dispatcher;
	/// The transport.
	McpHttpServer m_http;
	/// Bearer token (never logged).
	std::string m_token;
	/// Port the settings asked for, while running.
	std::optional<uint16_t> m_configuredPort;
	/// Port of the last failed start.
	std::optional<uint16_t> m_failedPort;
};

}// namespace owl::nest::mcp
