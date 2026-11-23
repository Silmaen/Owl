/**
 * @file WebPreviewServer.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "WebPreviewServer.h"

#include <core/Log.h>
#include <debug/Profiler.h>

#include <httplib.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <thread>
#include <utility>

namespace owl::nest {

struct WebPreviewServer::Impl {
	// The server, null when not serving.
	uniq<httplib::Server> server;
	// The thread running `listen_after_bind`.
	std::thread thread;
	// The port served.
	uint16_t port = 0;
};

WebPreviewServer::WebPreviewServer() : mp_impl{mkUniq<Impl>()} {}

WebPreviewServer::~WebPreviewServer() { stop(); }

auto WebPreviewServer::start(const std::filesystem::path& iRoot) -> bool {
	stop();
	if (!is_regular_file(iRoot / "index.html")) {
		OWL_CORE_WARN("WebPreview: No index.html in '{}'.", iRoot.string())
		return false;
	}
	auto server = mkUniq<httplib::Server>();
	if (!server->set_mount_point("/", iRoot.string())) {
		OWL_CORE_WARN("WebPreview: Cannot serve '{}'.", iRoot.string())
		return false;
	}
	server->set_file_extension_and_mimetype_mapping("wasm", "application/wasm");
	server->set_file_extension_and_mimetype_mapping("data", "application/octet-stream");
	server->set_file_extension_and_mimetype_mapping("owlpack", "application/octet-stream");
	server->set_post_routing_handler([](const httplib::Request&, httplib::Response& oResponse) -> void {
		oResponse.set_header("Cross-Origin-Opener-Policy", "same-origin");
		oResponse.set_header("Cross-Origin-Embedder-Policy", "require-corp");
		oResponse.set_header("Cache-Control", "no-store");
	});
	server->Post(R"(/owl-(log|exit|shot))",
				 [](const httplib::Request&, httplib::Response& oResponse) -> void { oResponse.status = 204; });
	const int bound = server->bind_to_any_port("127.0.0.1");
	if (bound <= 0) {
		OWL_CORE_WARN("WebPreview: Cannot listen on 127.0.0.1.")
		return false;
	}
	mp_impl->server = std::move(server);
	mp_impl->port = static_cast<uint16_t>(bound);
	mp_impl->thread = std::thread([http = mp_impl->server.get()]() -> void {
		OWL_PROFILE_THREAD_NAME("Web preview")
		http->listen_after_bind();
	});
	mp_impl->server->wait_until_ready();
	OWL_CORE_INFO("WebPreview: Serving '{}' on {}.", iRoot.string(), getUrl())
	return true;
}

void WebPreviewServer::stop() {
	if (mp_impl->server == nullptr)
		return;
	mp_impl->server->stop();
	if (mp_impl->thread.joinable())
		mp_impl->thread.join();
	mp_impl->server.reset();
	mp_impl->port = 0;
}

auto WebPreviewServer::isRunning() const -> bool { return mp_impl->server != nullptr; }

auto WebPreviewServer::getPort() const -> uint16_t { return mp_impl->port; }

auto WebPreviewServer::getUrl() const -> std::string {
	return isRunning() ? std::format("http://127.0.0.1:{}/index.html", mp_impl->port) : std::string{};
}

}// namespace owl::nest
