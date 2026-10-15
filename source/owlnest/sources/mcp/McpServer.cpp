/**
 * @file McpServer.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "McpServer.h"

#include "McpToken.h"

#include <core/Log.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace owl::nest::mcp {

McpServer::McpServer(session::EditorSession& ioSession, const McpServerOptions& iOptions)
	: m_options{iOptions}, m_tokenFile{iOptions.tokenFile.empty() ? getDefaultTokenFile() : iOptions.tokenFile},
	  m_dispatcher{ioSession, iOptions.dispatcher} {}

McpServer::~McpServer() { stop(); }

auto McpServer::start() -> bool {
	if (m_http.isRunning())
		return true;
	if (m_token.empty()) {
		auto token = generateToken();
		if (!token) {
			OWL_CORE_ERROR("Mcp: No token could be drawn, the server stays off.")
			return false;
		}
		m_token = std::move(*token);
	}
	if (!writeTokenFile(m_tokenFile, m_token)) {
		OWL_CORE_ERROR("Mcp: The token file cannot be written, the server stays off.")
		return false;
	}
	if (const auto started = m_http.start(m_options.http, m_token); !started) {
		removeTokenFile(m_tokenFile);
		OWL_CORE_ERROR("Mcp: The server cannot start on port {}.", m_options.http.port)
		return false;
	}
	OWL_CORE_INFO("Mcp: Token written to {}.", m_tokenFile.string())
	return true;
}

void McpServer::stop() {
	if (!m_http.isRunning())
		return;
	m_http.stop();
	removeTokenFile(m_tokenFile);
	m_configuredPort.reset();
}

void McpServer::applySettings(const bool iEnabled, const uint16_t iPort) {
	if (!iEnabled) {
		stop();
		m_failedPort.reset();
		return;
	}
	if (m_http.isRunning() && m_configuredPort == iPort)
		return;
	if (m_failedPort == iPort)
		return;
	stop();
	m_options.http.port = iPort;
	if (!start()) {
		m_failedPort = iPort;
		return;
	}
	m_failedPort.reset();
	m_configuredPort = iPort;
}

auto McpServer::regenerateToken() -> bool {
	auto token = generateToken();
	if (!token) {
		OWL_CORE_ERROR("Mcp: No token could be drawn, the old one stays.")
		return false;
	}
	if (m_http.isRunning() && !writeTokenFile(m_tokenFile, *token)) {
		OWL_CORE_ERROR("Mcp: The token file cannot be rewritten, the old token stays.")
		return false;
	}
	m_token = std::move(*token);
	m_http.setToken(m_token);
	OWL_CORE_INFO("Mcp: New token drawn.")
	return true;
}

auto McpServer::onUpdate(const bool iBusy) -> size_t {
	if (iBusy || !m_http.isRunning())
		return 0;
	return m_http.pump(
			[this](const std::string_view iMessage) -> std::optional<std::string> {
				return m_dispatcher.handle(iMessage);
			},
			m_options.maxMessagesPerFrame);
}

}// namespace owl::nest::mcp
