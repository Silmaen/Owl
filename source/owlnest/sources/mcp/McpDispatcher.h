/**
 * @file McpDispatcher.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../session/EditorSession.h"
#include "McpResources.h"

#include <core/expected.h>

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace owl::nest::mcp {

/**
 * @brief
 *  JSON-RPC error codes the dispatcher answers with.
 */
enum struct RpcError : int16_t {
	ParseError = -32700,///< The message is not JSON.
	InvalidRequest = -32600,///< Not a JSON-RPC 2.0 request (a batch included: MCP 2025-11-25 has none).
	MethodNotFound = -32601,///< No such method.
	InvalidParams = -32602,///< Missing or ill-typed parameters, unknown tool.
	InternalError = -32603,///< The request failed inside the dispatcher.
	ResourceNotFound = -32002///< `resources/read` of an unknown URI.
};

/**
 * @brief
 *  Settings of a dispatcher.
 */
struct McpOptions {
	/// Directory of the help bundle (`index.yml`) served as `owl://help/` resources; empty: no documentation page.
	std::filesystem::path helpRoot{};// NOLINT(readability-redundant-member-init) callers may omit it
};

/**
 * @brief
 *  Resolve a file argument of a remote caller inside the project: relative to the project, symbolic links followed.
 * @param[in] iProject The project directory (empty: every file is refused).
 * @param[in] iPath The path given.
 * @return The canonical path, or why it is refused (outside the project, no project).
 */
[[nodiscard]] auto confinePath(const std::filesystem::path& iProject, const std::filesystem::path& iPath)
		-> expected<std::filesystem::path, std::string>;

/**
 * @brief
 *  A Model Context Protocol server without transport: one JSON-RPC message in, its response out.
 *
 * Tools are the commands of the session registry (hence undoable), resources the scene, its entities, the assets,
 * the prefabs, the documentation and the statistics. The core is stateless: a request is served before the
 * `initialize` handshake too, which only negotiates the protocol revision (2025-11-25, or 2026-07-28). Runs on the
 * thread that owns the session; the HTTP transport (MCP3) queues the messages for the main loop.
 */
class McpDispatcher final {
public:
	/// Revision answered when the client asks for an unsupported one.
	static constexpr std::string_view defaultProtocolVersion = "2025-11-25";
	/// Every revision accepted by `initialize`.
	static constexpr std::array<std::string_view, 2> supportedProtocolVersions{"2025-11-25", "2026-07-28"};
	/// Server name reported by `initialize`.
	static constexpr std::string_view serverName = "owl-nest";

	McpDispatcher(const McpDispatcher&) = delete;

	McpDispatcher(McpDispatcher&&) = delete;

	auto operator=(const McpDispatcher&) -> McpDispatcher& = delete;

	auto operator=(McpDispatcher&&) -> McpDispatcher& = delete;

	/**
	 * @brief
	 *  Serve a session.
	 * @param[in,out] ioSession The session the tools act on (outlives the dispatcher).
	 * @param[in] iOptions The settings.
	 */
	explicit McpDispatcher(session::EditorSession& ioSession, const McpOptions& iOptions = {});

	/**
	 * @brief
	 *  Destructor.
	 */
	~McpDispatcher() = default;

	/**
	 * @brief
	 *  Handle one JSON-RPC message.
	 * @param[in] iMessage The message text (one request, notification or response object).
	 * @return The response text, nothing for a notification or a response sent by the client.
	 */
	[[nodiscard]] auto handle(std::string_view iMessage) -> std::optional<std::string>;

	/**
	 * @brief
	 *  Get the protocol revision negotiated by the last `initialize`.
	 * @return The revision, empty before any handshake.
	 */
	[[nodiscard]] auto getProtocolVersion() const -> const std::string& { return m_protocolVersion; }

	/**
	 * @brief
	 *  Get the name the client gave in the last `initialize`.
	 * @return The client name, empty before any handshake.
	 */
	[[nodiscard]] auto getClientName() const -> const std::string& { return m_clientName; }

	/**
	 * @brief
	 *  Check whether the client confirmed the handshake (`notifications/initialized`).
	 * @return True once confirmed.
	 */
	[[nodiscard]] auto isInitialized() const -> bool { return m_initialized; }

private:
	/**
	 * @brief
	 *  Handle a parsed message.
	 * @param[in] iMessage The message.
	 * @return The response, nothing for a notification or a response.
	 */
	[[nodiscard]] auto handleMessage(const nlohmann::json& iMessage) -> std::optional<nlohmann::json>;

	/**
	 * @brief
	 *  Run a request method.
	 * @param[in] iMethod The method name.
	 * @param[in] iParams The parameters (an object, empty when absent).
	 * @return The result, or the JSON-RPC error object.
	 */
	[[nodiscard]] auto dispatch(std::string_view iMethod, const nlohmann::json& iParams)
			-> expected<nlohmann::json, nlohmann::json>;

	/**
	 * @brief
	 *  `initialize`: negotiate the revision, describe the server.
	 * @param[in] iParams The parameters.
	 * @return The result, or the error object.
	 */
	[[nodiscard]] auto initialize(const nlohmann::json& iParams) -> expected<nlohmann::json, nlohmann::json>;

	/**
	 * @brief
	 *  `tools/list`: one tool per registered command, in registration order.
	 * @return The result.
	 */
	[[nodiscard]] auto listTools() const -> nlohmann::json;

	/**
	 * @brief
	 *  `tools/call`: run a command; its failures are tool results with `isError`.
	 * @param[in] iParams The parameters (`name`, `arguments`).
	 * @return The result, or the error object (unknown tool, ill-typed parameters).
	 */
	[[nodiscard]] auto callTool(const nlohmann::json& iParams) -> expected<nlohmann::json, nlohmann::json>;

	/**
	 * @brief
	 *  Keep the file arguments (`ArgSpec::projectPath`) inside the project, rewritten as canonical paths.
	 * @param[in] iSpec The command.
	 * @param[in,out] ioArgs The parsed arguments.
	 * @return Nothing, or the failure naming the refused argument.
	 */
	[[nodiscard]] auto confinePaths(const commands::CommandSpec& iSpec, commands::CommandArgs& ioArgs) const
			-> expected<void, commands::CommandFailure>;

	/**
	 * @brief
	 *  `resources/read`.
	 * @param[in] iParams The parameters (`uri`).
	 * @return The result, or the error object (unknown URI).
	 */
	[[nodiscard]] auto readResource(const nlohmann::json& iParams) const -> expected<nlohmann::json, nlohmann::json>;

	/// The session served (non-owning).
	session::EditorSession& m_session;
	/// The resources of the session.
	McpResources m_resources;
	/// Revision negotiated by the last `initialize`.
	std::string m_protocolVersion;
	/// Client name of the last `initialize`.
	std::string m_clientName;
	/// True once the client sent `notifications/initialized`.
	bool m_initialized{false};
};

}// namespace owl::nest::mcp
