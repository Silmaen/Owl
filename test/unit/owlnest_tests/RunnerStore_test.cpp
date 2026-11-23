/**
 * @file RunnerStore_test.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "RunnerStore.h"

#include <httplib.h>
#include <openssl/evp.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::nest;
using data::assets::pack::ExportTarget;

namespace {

constexpr auto g_version = "0.8.0";
constexpr auto g_hash = "abcdef1";

// A member of a test archive: name, content, type ('0' file, '2' link to the content) and mode.
struct Member {
	std::string name;
	std::string content;
	char type = '0';
	unsigned mode = 0644;
};

void putField(std::array<char, 512>& ioHeader, const size_t iOffset, const std::string& iText) {
	std::ranges::copy(iText, ioHeader.begin() + static_cast<std::ptrdiff_t>(iOffset));
}

// A ustar tar.gz, written as the CI writes the runner archives.
void writeTarGz(const std::filesystem::path& iFile, const std::vector<Member>& iMembers) {
	std::string tar;
	for (const auto& member: iMembers) {
		std::array<char, 512> header{};
		const bool link = member.type == '2';
		const size_t size = link ? 0 : member.content.size();
		putField(header, 0, member.name);
		putField(header, 100, std::format("{:07o}", member.mode));
		putField(header, 108, "0000000");
		putField(header, 116, "0000000");
		putField(header, 124, std::format("{:011o}", size));
		putField(header, 136, "00000000000");
		header[156] = member.type;
		if (link)
			putField(header, 157, member.content);
		putField(header, 257, "ustar");
		putField(header, 263, "00");
		std::ranges::fill(header.begin() + 148, header.begin() + 156, ' ');
		unsigned sum = 0;
		for (const char c: header) sum += static_cast<unsigned char>(c);
		putField(header, 148, std::format("{:06o}", sum));
		header[154] = '\0';
		tar.append(header.data(), header.size());
		if (size > 0) {
			tar += member.content;
			tar.append((512 - size % 512) % 512, '\0');
		}
	}
	tar.append(1024, '\0');
	gzFile out = gzopen(iFile.string().c_str(), "wb");
	gzwrite(out, tar.data(), static_cast<unsigned>(tar.size()));
	gzclose(out);
}

auto sha256Of(const std::filesystem::path& iFile) -> std::string {
	const std::ifstream in(iFile, std::ios::binary);
	std::stringstream content;
	content << in.rdbuf();
	const auto data = content.str();
	std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
	unsigned int length = 0;
	EVP_Digest(data.data(), data.size(), digest.data(), &length, EVP_sha256(), nullptr);
	std::string hex;
	for (unsigned int i = 0; i < length; ++i) hex += std::format("{:02x}", digest[i]);
	return hex;
}

auto archiveName(const std::string_view iTarget) -> std::string {
	return std::format("OwlGameRunner-{}-{}-{}.tar.gz", g_version, g_hash, iTarget);
}

// The other desktop target, the one downloaded on this platform.
auto otherDesktop() -> ExportTarget {
	return data::assets::pack::getHostExportTarget() == ExportTarget::WindowsX64 ? ExportTarget::LinuxX64
																				 : ExportTarget::WindowsX64;
}

class RunnerStoreFixture : public ::testing::Test {
protected:
	void SetUp() override {
		m_root = std::filesystem::temp_directory_path() / "owl_runner_store";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root / "nest");
		std::filesystem::create_directories(m_root / "archives");
		m_target = otherDesktop();
		m_archive = m_root / "archives" / archiveName(RunnerStore::getTargetName(m_target));
		writeTarGz(m_archive, {{.name = "OwlGameRunner.exe", .content = "RUNNER", .type = '0', .mode = 0755},
							   {.name = "libOwlEngine.so.0.8", .content = "ENGINE"},
							   {.name = "libOwlEngine.so", .content = "libOwlEngine.so.0.8", .type = '2'}});
		writeManifest(m_archive, RunnerStore::getTargetName(m_target));
	}

	void TearDown() override { std::filesystem::remove_all(m_root); }

	void writeManifest(const std::filesystem::path& iArchive, const std::string_view iTarget) const {
		std::ofstream{m_root / "nest" / "runners.json"} << std::format(
				R"({{"version": "{}", "hash": "{}", "runners": {{"{}": {{"file": "{}", "size": {}, )"
				R"("sha256": "{}"}}}}}})",
				g_version, g_hash, iTarget, iArchive.filename().string(), file_size(iArchive), sha256Of(iArchive));
	}

	[[nodiscard]] auto makeStore() const -> uniq<RunnerStore> {
		return mkUniq<RunnerStore>(m_root / "nest", m_root / "cache");
	}

	std::filesystem::path m_root;
	std::filesystem::path m_archive;
	ExportTarget m_target{};
};

}// namespace

TEST_F(RunnerStoreFixture, TheHostAndWebRunnersShipWithOwlNest) {
	const auto store = makeStore();
	ASSERT_TRUE(store->hasManifest());
	EXPECT_TRUE(RunnerStore::isBundled(ExportTarget::Host));
	EXPECT_TRUE(RunnerStore::isBundled(ExportTarget::Web));
	EXPECT_FALSE(RunnerStore::isBundled(m_target));
	EXPECT_FALSE(RunnerStore::isBundled(ExportTarget::Android));
	EXPECT_EQ(store->getRunnerDirectory(ExportTarget::Host), m_root / "nest");
	EXPECT_EQ(store->getRunnerDirectory(ExportTarget::Web), m_root / "nest" / "web");
	EXPECT_EQ(store->getRunnerDirectory(m_target),
			  m_root / "cache" / "0.8.0-abcdef1" / std::string{RunnerStore::getTargetName(m_target)});
	EXPECT_TRUE(store->isInstalled(ExportTarget::Host));
	EXPECT_FALSE(store->isInstalled(m_target));
}

TEST_F(RunnerStoreFixture, TheDownloadAddressNamesTheVersionAndTheCommit) {
	const auto store = makeStore();
	EXPECT_EQ(store->getDownloadUrl(m_target, "https://owl.example.org/"),
			  std::format("https://owl.example.org/runners/0.8.0/abcdef1/{}", m_archive.filename().string()));
	EXPECT_TRUE(store->getDownloadUrl(ExportTarget::Android, "https://owl.example.org").empty());
	EXPECT_EQ(RunnerStore::getTargetName(ExportTarget::LinuxArm64), "linux-arm64");
	EXPECT_EQ(RunnerStore::getTargetName(ExportTarget::Android), "android-arm64");
}

TEST_F(RunnerStoreFixture, AnImportedArchiveIsUnpackedInTheCache) {
	const auto store = makeStore();
	const auto imported = store->importArchive(m_archive);
	ASSERT_TRUE(imported.has_value());
	EXPECT_EQ(*imported, m_target);
	EXPECT_TRUE(store->isInstalled(m_target));
	const auto directory = store->getRunnerDirectory(m_target);
	std::ifstream runner(directory / "OwlGameRunner.exe");
	std::string content;
	runner >> content;
	EXPECT_EQ(content, "RUNNER");
	EXPECT_TRUE(exists(directory / "libOwlEngine.so"));
#ifndef OWL_PLATFORM_WINDOWS
	EXPECT_TRUE(is_symlink(directory / "libOwlEngine.so"));
	EXPECT_NE(status(directory / "OwlGameRunner.exe").permissions() & std::filesystem::perms::owner_exec,
			  std::filesystem::perms::none);
#endif
}

TEST_F(RunnerStoreFixture, AnArchiveOfAnotherBuildIsRefused) {
	const auto store = makeStore();
	writeTarGz(m_archive, {{.name = "OwlGameRunner.exe", .content = "TAMPERED"}});
	const auto imported = store->importArchive(m_archive);
	ASSERT_FALSE(imported.has_value());
	EXPECT_EQ(imported.error(), RunnerError::Mismatch);
	EXPECT_FALSE(store->isInstalled(m_target));

	const auto unknown = m_root / "archives" / "OwlGameRunner-0.7.0-1234567-web.tar.gz";
	writeTarGz(unknown, {{.name = "OwlGameRunner.html", .content = "PAGE"}});
	const auto refused = store->importArchive(unknown);
	ASSERT_FALSE(refused.has_value());
	EXPECT_EQ(refused.error(), RunnerError::UnknownRunner);
	EXPECT_FALSE(RunnerStore::getErrorMessage(RunnerError::Mismatch).empty());
}

TEST_F(RunnerStoreFixture, AnArchiveCannotWriteOutsideItsFolder) {
	writeTarGz(m_archive, {{.name = "../escaped.txt", .content = "OUT"}});
	writeManifest(m_archive, RunnerStore::getTargetName(m_target));
	const auto store = makeStore();
	const auto imported = store->importArchive(m_archive);
	ASSERT_FALSE(imported.has_value());
	EXPECT_EQ(imported.error(), RunnerError::Extract);
	EXPECT_FALSE(exists(m_root / "cache" / "0.8.0-abcdef1" / "escaped.txt"));
	EXPECT_FALSE(store->isInstalled(m_target));
}

TEST_F(RunnerStoreFixture, WithoutManifestNothingIsDownloaded) {
	std::filesystem::remove(m_root / "nest" / "runners.json");
	const auto store = makeStore();
	EXPECT_FALSE(store->hasManifest());
	EXPECT_FALSE(store->isInstalled(m_target));
	const auto downloaded = store->download(m_target, "http://127.0.0.1:1", {});
	ASSERT_FALSE(downloaded.has_value());
	EXPECT_EQ(downloaded.error(), RunnerError::NoManifest);
}

TEST_F(RunnerStoreFixture, ADownloadAsksTheServerAsOwlNest) {
	httplib::Server server;
	std::string userAgent;
	std::string path;
	const std::ifstream in(m_archive, std::ios::binary);
	std::stringstream content;
	content << in.rdbuf();
	const auto body = content.str();
	server.Get(R"(/runners/.*)", [&](const httplib::Request& iRequest, httplib::Response& oResponse) -> void {
		userAgent = iRequest.get_header_value("User-Agent");
		path = iRequest.path;
		oResponse.set_content(body, "application/gzip");
	});
	const int port = server.bind_to_any_port("127.0.0.1");
	ASSERT_GT(port, 0);
	std::thread thread([&server]() -> void { server.listen_after_bind(); });
	server.wait_until_ready();

	const auto store = makeStore();
	uint64_t received = 0;
	const auto downloaded = store->download(m_target, std::format("http://127.0.0.1:{}", port),
											[&received](const uint64_t iReceived, uint64_t) -> bool {
												received = iReceived;
												return true;
											});
	server.stop();
	thread.join();
	ASSERT_TRUE(downloaded.has_value());
	EXPECT_EQ(userAgent, "OwlNest/" + getVersionString());
	EXPECT_EQ(path, std::format("/runners/0.8.0/abcdef1/{}", m_archive.filename().string()));
	EXPECT_EQ(received, body.size());
	EXPECT_TRUE(store->isInstalled(m_target));
}

TEST_F(RunnerStoreFixture, AnUnreachableServerLeavesNothingBehind) {
	const auto store = makeStore();
	const auto downloaded = store->download(m_target, "http://127.0.0.1:1", {});
	ASSERT_FALSE(downloaded.has_value());
	EXPECT_EQ(downloaded.error(), RunnerError::Download);
	EXPECT_FALSE(store->isInstalled(m_target));
	EXPECT_FALSE(exists(m_root / "cache" / (m_archive.filename().string() + ".part")));
}

TEST_F(RunnerStoreFixture, TheRunnersOfOtherReleasesAreRemoved) {
	const auto store = makeStore();
	ASSERT_TRUE(store->importArchive(m_archive).has_value());
	std::filesystem::create_directories(m_root / "cache" / "0.7.0-1234567" / "linux-x64");
	EXPECT_EQ(store->removeOtherVersions(), 1u);
	EXPECT_FALSE(exists(m_root / "cache" / "0.7.0-1234567"));
	EXPECT_TRUE(store->isInstalled(m_target));
}
