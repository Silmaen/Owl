/**
 * @file McpDispatcher.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "McpDispatcher.h"

#include "McpPlay.h"
#include "McpTools.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <core/Core.h>
#include <core/Log.h>
#include <input/ActionMap.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

namespace owl::nest::mcp {

namespace {

using json = nlohmann::json;

constexpr std::string_view g_Instructions =
		"Owl Nest editor session. Tools are the editor commands: every change they make is undoable "
		"(edit.undo / edit.redo). Resources expose the scene tree (owl://scene), the components of an entity "
		"(owl://scene/entity/{id}), the assets, the prefabs, the statistics and the documentation pages, as data.";

auto rpcError(const RpcError iCode, const std::string_view iMessage, json iData = nullptr) -> json {
	json error{{"code", static_cast<int16_t>(iCode)}, {"message", iMessage}};
	if (!iData.is_null())
		error["data"] = std::move(iData);
	return error;
}

auto response(const json& iId, json iResult) -> json {
	return {{"jsonrpc", "2.0"}, {"id", iId}, {"result", std::move(iResult)}};
}

auto errorResponse(const json& iId, json iError) -> json {
	return {{"jsonrpc", "2.0"}, {"id", iId}, {"error", std::move(iError)}};
}

auto isValidId(const json& iId) -> bool { return iId.is_string() || iId.is_number_integer(); }

auto getString(const json& iObject, const std::string_view iKey) -> std::string {
	const auto found = iObject.find(iKey);
	return found != iObject.end() && found->is_string() ? found->get<std::string>() : std::string{};
}

auto invalidParams(const std::string_view iMessage) -> unexpected<json> {
	OWL_CORE_WARN("Mcp: Invalid parameters ({}).", iMessage)
	return unexpected{rpcError(RpcError::InvalidParams, iMessage)};
}

}// namespace

auto confinePath(const std::filesystem::path& iProject, const std::filesystem::path& iPath)
		-> expected<std::filesystem::path, std::string> {
	if (iProject.empty())
		return unexpected<std::string>{"no project is open, remote callers cannot name files"};
	std::error_code error;
	const auto root = std::filesystem::weakly_canonical(std::filesystem::absolute(iProject, error), error);
	if (error)
		return unexpected<std::string>{std::format("the project directory cannot be resolved ({})", error.message())};
	auto resolved = std::filesystem::weakly_canonical(iPath.is_absolute() ? iPath : root / iPath, error);
	if (error)
		return unexpected<std::string>{std::format("{} cannot be resolved ({})", iPath.string(), error.message())};
	const auto relative = resolved.lexically_relative(root);
	if (relative.empty() || relative == "." || *relative.begin() == "..")
		return unexpected<std::string>{std::format("{} is outside the project", iPath.string())};
	return resolved;
}

McpDispatcher::McpDispatcher(session::EditorSession& ioSession, const McpOptions& iOptions)
	: m_session{ioSession}, m_resources{ioSession, iOptions.helpRoot} {}

auto McpDispatcher::handle(const std::string_view iMessage) -> std::optional<std::string> {
	auto result = handleDeferrable(iMessage);
	if (result.poll)
		return result.poll(/*iCancel=*/true);
	return std::move(result.answer);
}

auto McpDispatcher::handleDeferrable(const std::string_view iMessage) -> DispatchResult {
	const auto message = json::parse(iMessage, nullptr, false);
	if (message.is_discarded()) {
		OWL_CORE_WARN("Mcp: Message is not JSON.")
		return {.answer = dumpJson(errorResponse(nullptr, rpcError(RpcError::ParseError, "Parse error")))};
	}
	if (message.is_array()) {
		OWL_CORE_WARN("Mcp: Batch refused.")
		return {.answer = dumpJson(
						errorResponse(nullptr, rpcError(RpcError::InvalidRequest,
														"Batch requests are not part of this protocol revision")))};
	}
	m_playDeferred = false;
	auto answer = handleMessage(message);
	if (m_playDeferred) {
		m_playDeferred = false;
		return {.poll = [this, id = m_currentId](const bool iCancel) -> std::optional<std::string> {
			if (iCancel)
				std::ignore = m_session.stopPlay();
			else if (m_session.isPlayRunning())
				return std::nullopt;
			return finishPlay(id);
		}};
	}
	if (answer)
		return {.answer = dumpJson(*answer)};
	return {};
}

auto McpDispatcher::handleMessage(const json& iMessage) -> std::optional<json> {
	if (!iMessage.is_object()) {
		OWL_CORE_WARN("Mcp: Message is not an object.")
		return errorResponse(nullptr, rpcError(RpcError::InvalidRequest, "Invalid Request"));
	}
	const bool hasId = iMessage.contains("id");
	const auto method = iMessage.find("method");
	if (method == iMessage.end()) {
		// A response to a server request: this server sends none, so it is dropped.
		if (hasId && (iMessage.contains("result") || iMessage.contains("error")))
			return std::nullopt;
		OWL_CORE_WARN("Mcp: Message without method.")
		return errorResponse(hasId && isValidId(iMessage["id"]) ? iMessage["id"] : json(nullptr),
							 rpcError(RpcError::InvalidRequest, "Invalid Request"));
	}
	const json id = hasId ? iMessage["id"] : json(nullptr);
	m_currentId = id;
	if (getString(iMessage, "jsonrpc") != "2.0" || !method->is_string() || (hasId && !isValidId(id))) {
		OWL_CORE_WARN("Mcp: Not a JSON-RPC 2.0 request.")
		if (!hasId)
			return std::nullopt;
		return errorResponse(isValidId(id) ? id : json(nullptr), rpcError(RpcError::InvalidRequest, "Invalid Request"));
	}
	const auto& name = method->get_ref<const std::string&>();
	if (!hasId) {
		if (name == "notifications/initialized")
			m_initialized = true;
		return std::nullopt;
	}
	const auto params = iMessage.find("params");
	if (params != iMessage.end() && !params->is_object()) {
		OWL_CORE_WARN("Mcp: Parameters of {} are not an object.", name)
		return errorResponse(id, rpcError(RpcError::InvalidParams, "params must be an object"));
	}
	try {
		auto result = dispatch(name, params != iMessage.end() ? *params : json::object());
		if (!result)
			return errorResponse(id, std::move(result.error()));
		return response(id, std::move(*result));
	} catch (const std::exception& ex) {
		OWL_CORE_ERROR("Mcp: {} failed ({}).", name, ex.what())
		return errorResponse(id, rpcError(RpcError::InternalError, "Internal error", ex.what()));
	}
}

auto McpDispatcher::dispatch(const std::string_view iMethod, const json& iParams) -> expected<json, json> {
	if (iMethod == "initialize")
		return initialize(iParams);
	if (iMethod == "ping")
		return json::object();
	if (iMethod == "tools/list")
		return listTools();
	if (iMethod == "tools/call")
		return callTool(iParams);
	if (iMethod == "resources/list")
		return json{{"resources", m_resources.list()}};
	if (iMethod == "resources/templates/list")
		return json{{"resourceTemplates", m_resources.listTemplates()}};
	if (iMethod == "resources/read")
		return readResource(iParams);
	OWL_CORE_WARN("Mcp: Unknown method {}.", iMethod)
	return unexpected{rpcError(RpcError::MethodNotFound, std::format("Method not found: {}", iMethod))};
}

auto McpDispatcher::initialize(const json& iParams) -> expected<json, json> {
	const auto requested = iParams.find("protocolVersion");
	if (requested == iParams.end() || !requested->is_string())
		return invalidParams("initialize: protocolVersion must be a string");
	const auto& version = requested->get_ref<const std::string&>();
	m_protocolVersion =
			std::ranges::contains(supportedProtocolVersions, version) ? version : std::string{defaultProtocolVersion};
	m_clientName.clear();
	if (const auto info = iParams.find("clientInfo"); info != iParams.end() && info->is_object())
		m_clientName = getString(*info, "name");
	m_initialized = false;
	return json{{"protocolVersion", m_protocolVersion},
				{"capabilities",
				 {{"tools", {{"listChanged", false}}}, {"resources", {{"subscribe", false}, {"listChanged", false}}}}},
				{"serverInfo", {{"name", serverName}, {"title", "Owl Nest"}, {"version", getVersionString()}}},
				{"instructions", g_Instructions}};
}

auto McpDispatcher::listTools() const -> json {
	auto tools = json::array();
	for (const auto& spec: m_session.getRegistry().getCommands()) tools.push_back(makeTool(spec));
	tools.push_back(makePlayTool());
	return {{"tools", std::move(tools)}};
}

auto McpDispatcher::callTool(const json& iParams) -> expected<json, json> {
	const auto name = iParams.find("name");
	if (name == iParams.end() || !name->is_string())
		return invalidParams("tools/call: name must be a string");
	const auto& toolName = name->get_ref<const std::string&>();
	if (toolName == playToolName) {
		const auto arguments = iParams.find("arguments");
		const auto result =
				callPlay(arguments != iParams.end() && arguments->is_object() ? *arguments : json::object());
		return result.is_null() ? json::object() : result;
	}
	const auto* spec = m_session.getRegistry().find(toolName);
	if (spec == nullptr) {
		OWL_CORE_WARN("Mcp: Unknown tool {}.", toolName)
		return unexpected{rpcError(RpcError::InvalidParams, std::format("Unknown tool: {}", toolName))};
	}
	const auto arguments = iParams.find("arguments");
	if (arguments != iParams.end() && !arguments->is_object() && !arguments->is_null())
		return invalidParams("tools/call: arguments must be an object");
	auto args =
			parseArguments(*spec, arguments != iParams.end() && arguments->is_object() ? *arguments : json::object());
	if (!args)
		return makeToolError(args.error());
	if (const auto confined = confinePaths(*spec, *args); !confined)
		return makeToolError(confined.error());
	const auto result = m_session.execute(toolName, *args);
	if (!result)
		return makeToolError(result.error());
	return makeToolResult(*result);
}

auto McpDispatcher::callPlay(const json& iArguments) -> json {
	if (m_playPending)
		return makePlayError("a play.run is still running");
	const auto map = app::Application::instanced() ? app::Application::get().getEngineContext().getActionMap()
												   : input::ActionMap::getDefault();
	const auto request = parsePlayRequest(iArguments, *map);
	if (!request)
		return makePlayError(request.error());
	if (const auto started = m_session.startPlay(*request); !started)
		return makePlayError(started.error().message);
	if (!m_session.isPlayRunning())
		return makePlayResult(m_session.takePlayReport().value_or(session::PlayReport{}));
	m_playDeferred = true;
	m_playPending = true;
	return nullptr;
}

auto McpDispatcher::finishPlay(const json& iId) -> std::string {
	m_playPending = false;
	auto report = m_session.takePlayReport();
	if (!report) {
		OWL_CORE_WARN("Mcp: The play ended without a report.")
		return dumpJson(response(iId, makePlayError("the play ended without a report")));
	}
	return dumpJson(response(iId, makePlayResult(*report)));
}

auto McpDispatcher::confinePaths(const commands::CommandSpec& iSpec, commands::CommandArgs& ioArgs) const
		-> expected<void, commands::CommandFailure> {
	const auto project = m_session.getSessionProjectDirectory();
	for (const auto& arg: iSpec.args) {
		if (!arg.projectPath || !ioArgs.has(arg.name))
			continue;
		const std::filesystem::path given{ioArgs.getText(arg.name)};
		auto confined = confinePath(project, given);
		if (!confined) {
			OWL_CORE_WARN("Mcp: Argument {} of {} refused ({}).", arg.name, iSpec.name, confined.error())
			return unexpected{commands::CommandFailure{.error = commands::CommandError::InvalidArgument,
													   .message = std::format("`{}`: {}", arg.name, confined.error())}};
		}
		ioArgs.set(arg.name, confined->string());
	}
	return {};
}

auto McpDispatcher::readResource(const json& iParams) const -> expected<json, json> {
	const auto uri = iParams.find("uri");
	if (uri == iParams.end() || !uri->is_string())
		return invalidParams("resources/read: uri must be a string");
	const auto& text = uri->get_ref<const std::string&>();
	auto contents = m_resources.read(text);
	if (!contents)
		return unexpected{rpcError(RpcError::ResourceNotFound, "Resource not found", json{{"uri", text}})};
	return json{{"contents", std::move(*contents)}};
}

}// namespace owl::nest::mcp
