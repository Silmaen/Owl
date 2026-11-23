/**
 * @file WebPreviewServer_test.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "WebPreviewServer.h"

#include <httplib.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace owl;
using namespace owl::nest;

namespace {

auto makeGame(const std::string& iName) -> std::filesystem::path {
	const auto folder = std::filesystem::temp_directory_path() / iName;
	std::filesystem::remove_all(folder);
	std::filesystem::create_directories(folder);
	std::ofstream{folder / "index.html"} << "<html></html>";
	std::ofstream{folder / "OwlGameRunner.wasm"} << "wasm";
	return folder;
}

}// namespace

TEST(WebPreviewServer, ServesTheGameCrossOriginIsolated) {
	const auto game = makeGame("web_preview_serves");
	WebPreviewServer server;
	ASSERT_TRUE(server.start(game));
	ASSERT_TRUE(server.isRunning());
	ASSERT_NE(server.getPort(), 0);
	EXPECT_EQ(server.getUrl(), "http://127.0.0.1:" + std::to_string(server.getPort()) + "/index.html");

	httplib::Client client{"127.0.0.1", server.getPort()};
	const auto page = client.Get("/index.html");
	ASSERT_TRUE(page);
	EXPECT_EQ(page->status, 200);
	EXPECT_EQ(page->body, "<html></html>");
	EXPECT_EQ(page->get_header_value("Cross-Origin-Opener-Policy"), "same-origin");
	EXPECT_EQ(page->get_header_value("Cross-Origin-Embedder-Policy"), "require-corp");
	EXPECT_EQ(page->get_header_value("Cache-Control"), "no-store");
	const auto wasm = client.Get("/OwlGameRunner.wasm");
	ASSERT_TRUE(wasm);
	EXPECT_EQ(wasm->get_header_value("Content-Type"), "application/wasm");
	const auto exitReport = client.Post("/owl-exit", "0", "text/plain");
	ASSERT_TRUE(exitReport);
	EXPECT_EQ(exitReport->status, 204);
	const auto missing = client.Get("/nothing.js");
	ASSERT_TRUE(missing);
	EXPECT_EQ(missing->status, 404);

	server.stop();
	EXPECT_FALSE(server.isRunning());
	EXPECT_EQ(server.getPort(), 0);
	EXPECT_TRUE(server.getUrl().empty());
	std::filesystem::remove_all(game);
}

TEST(WebPreviewServer, AFolderWithoutPageIsNotServed) {
	const auto folder = std::filesystem::temp_directory_path() / "web_preview_empty";
	std::filesystem::create_directories(folder);
	WebPreviewServer server;
	EXPECT_FALSE(server.start(folder));
	EXPECT_FALSE(server.isRunning());
	std::filesystem::remove_all(folder);
}

TEST(WebPreviewServer, ANewGameReplacesTheServedOne) {
	const auto first = makeGame("web_preview_first");
	const auto second = makeGame("web_preview_second");
	std::ofstream{second / "index.html"} << "second";
	WebPreviewServer server;
	ASSERT_TRUE(server.start(first));
	ASSERT_TRUE(server.start(second));
	httplib::Client client{"127.0.0.1", server.getPort()};
	const auto page = client.Get("/index.html");
	ASSERT_TRUE(page);
	EXPECT_EQ(page->body, "second");
	server.stop();
	std::filesystem::remove_all(first);
	std::filesystem::remove_all(second);
}
