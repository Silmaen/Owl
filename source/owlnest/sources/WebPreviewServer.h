/**
 * @file WebPreviewServer.h
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace owl::nest {

/**
 * @brief
 *  Local HTTP server of a Web export, so that a browser plays it: the folder on 127.0.0.1, cross-origin isolated
 *  (the threads of the page need `SharedArrayBuffer`), never cached.
 *
 * The page's reports (`/owl-log`, `/owl-exit`, `/owl-shot`) are accepted and dropped.
 */
class WebPreviewServer final {
public:
	WebPreviewServer(const WebPreviewServer&) = delete;

	WebPreviewServer(WebPreviewServer&&) = delete;

	auto operator=(const WebPreviewServer&) -> WebPreviewServer& = delete;

	auto operator=(WebPreviewServer&&) -> WebPreviewServer& = delete;

	/**
	 * @brief
	 *  Constructor, not serving.
	 */
	WebPreviewServer();

	/**
	 * @brief
	 *  Destructor, stops serving.
	 */
	~WebPreviewServer();

	/**
	 * @brief
	 *  Serve a folder on a free port of 127.0.0.1, in place of the one served before.
	 * @param[in] iRoot The folder of the exported game (its `index.html`).
	 * @return True when the server listens.
	 */
	auto start(const std::filesystem::path& iRoot) -> bool;

	/**
	 * @brief
	 *  Stop serving.
	 */
	void stop();

	/**
	 * @brief
	 *  Check whether a folder is served.
	 * @return True while serving.
	 */
	[[nodiscard]] auto isRunning() const -> bool;

	/**
	 * @brief
	 *  Get the port served.
	 * @return The port, 0 when not serving.
	 */
	[[nodiscard]] auto getPort() const -> uint16_t;

	/**
	 * @brief
	 *  Get the address of the game page.
	 * @return `http://127.0.0.1:<port>/index.html`, empty when not serving.
	 */
	[[nodiscard]] auto getUrl() const -> std::string;

private:
	/// The HTTP server and its thread.
	struct Impl;
	/// Implementation, keeping cpp-httplib out of the header.
	uniq<Impl> mp_impl;
};

}// namespace owl::nest
