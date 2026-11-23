/**
 * @file RunnerStore.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "RunnerStore.h"

#include <core/Core.h>
#include <core/Log.h>

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest {

namespace {

using data::assets::pack::ExportTarget;

constexpr std::string_view g_manifestName = "runners.json";
// Written once an archive is unpacked: a folder without it is an interrupted install.
constexpr std::string_view g_installedMark = ".installed";
constexpr size_t g_tarBlock = 512;

auto sha256Of(const std::filesystem::path& iFile) -> std::string {
	std::ifstream in(iFile, std::ios::binary);
	if (!in)
		return {};
	auto* context = EVP_MD_CTX_new();
	EVP_DigestInit_ex(context, EVP_sha256(), nullptr);
	std::vector<char> buffer(size_t{1} << 16u);
	while (in.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || in.gcount() > 0)
		EVP_DigestUpdate(context, buffer.data(), static_cast<size_t>(in.gcount()));
	std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
	unsigned int length = 0;
	EVP_DigestFinal_ex(context, digest.data(), &length);
	EVP_MD_CTX_free(context);
	std::string hex;
	for (unsigned int i = 0; i < length; ++i) hex += std::format("{:02x}", digest[i]);
	return hex;
}

// Octal field of a tar header.
auto octal(const std::string_view iField) -> uint64_t {
	uint64_t value = 0;
	for (const char c: iField) {
		if (c < '0' || c > '7')
			break;
		value = value * 8 + static_cast<uint64_t>(c - '0');
	}
	return value;
}

// A `key=value` record of a PAX extended header (`<length> path=<name>\n`).
auto paxValue(const std::string& iRecords, const std::string_view iKey) -> std::string {
	size_t pos = 0;
	while (pos < iRecords.size()) {
		const auto space = iRecords.find(' ', pos);
		if (space == std::string::npos)
			break;
		const auto length = std::stoul(iRecords.substr(pos, space - pos));
		const auto record = iRecords.substr(space + 1, length - (space - pos) - 2);
		if (record.starts_with(iKey) && record.size() > iKey.size() && record[iKey.size()] == '=')
			return record.substr(iKey.size() + 1);
		pos += length;
	}
	return {};
}

// A path of the archive that stays inside the destination.
auto safePath(const std::string& iName) -> bool {
	const std::filesystem::path path{iName};
	if (path.is_absolute() || path.has_root_name())
		return false;
	return std::ranges::none_of(path, [](const auto& iPart) -> bool { return iPart == ".."; });
}

// A text field of a tar header, up to its first NUL.
auto field(const std::string_view iHeader, const size_t iOffset, const size_t iLength) -> std::string {
	const auto text = iHeader.substr(iOffset, iLength);
	return std::string{text.substr(0, text.find('\0'))};
}

auto readExactly(gzFile iFile, char* oData, const size_t iSize) -> bool {
	return gzread(iFile, oData, static_cast<unsigned>(iSize)) == static_cast<int>(iSize);
}

// Unpacks the regular files, folders and symbolic links of a tar.gz (ustar, PAX and GNU long names).
auto extractTarGz(const std::filesystem::path& iArchive, const std::filesystem::path& iDestination) -> bool {
	gzFile file = gzopen(iArchive.string().c_str(), "rb");
	if (file == nullptr)
		return false;
	std::array<char, g_tarBlock> header{};
	std::string longName;
	std::string longLink;
	bool ok = true;
	while (ok && readExactly(file, header.data(), g_tarBlock)) {
		if (header[0] == '\0')
			break;
		const std::string_view view{header.data(), g_tarBlock};
		const auto size = octal(view.substr(124, 12));
		const char type = header[156];
		std::string content(size, '\0');
		ok = size == 0 || readExactly(file, content.data(), size);
		const auto padding = (g_tarBlock - size % g_tarBlock) % g_tarBlock;
		std::array<char, g_tarBlock> skip{};
		if (ok && padding > 0)
			ok = readExactly(file, skip.data(), padding);
		if (!ok)
			break;
		if (type == 'x') {
			longName = paxValue(content, "path");
			longLink = paxValue(content, "linkpath");
			continue;
		}
		if (type == 'L' || type == 'K') {
			(type == 'L' ? longName : longLink) = content.substr(0, content.find('\0'));
			continue;
		}
		if (type == 'g')
			continue;
		std::string name = longName;
		if (name.empty()) {
			const auto prefix = field(view, 345, 155);
			const auto base = field(view, 0, 100);
			name = prefix.empty() ? base : std::format("{}/{}", prefix, base);
		}
		const auto link = longLink.empty() ? field(view, 157, 100) : longLink;
		longName.clear();
		longLink.clear();
		if (name.empty() || !safePath(name)) {
			OWL_CORE_WARN("RunnerStore: Unsafe path '{}' in '{}'.", name, iArchive.string())
			ok = false;
			break;
		}
		const auto target = iDestination / name;
		std::error_code ec;
		if (type == '5') {
			std::filesystem::create_directories(target, ec);
			continue;
		}
		std::filesystem::create_directories(target.parent_path(), ec);
		std::filesystem::remove(target, ec);
		if (type == '2') {
			if (!safePath(link)) {
				OWL_CORE_WARN("RunnerStore: Unsafe link '{}' in '{}'.", link, iArchive.string())
				ok = false;
				break;
			}
			std::filesystem::create_symlink(link, target, ec);
			if (ec) {
				// A Windows account without the symbolic link privilege: the link becomes a copy of its target.
				std::filesystem::copy_file(target.parent_path() / link, target, ec);
			}
			continue;
		}
		if (type == '0' || type == '\0') {
			std::ofstream out(target, std::ios::binary);
			out.write(content.data(), static_cast<std::streamsize>(content.size()));
			ok = out.good();
			if (const auto mode = octal(view.substr(100, 8)); ok && (mode & 0111u) != 0)
				std::filesystem::permissions(target,
											 std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
													 std::filesystem::perms::others_exec,
											 std::filesystem::perm_options::add, ec);
		}
	}
	gzclose(file);
	return ok;
}

}// namespace

RunnerStore::RunnerStore(std::filesystem::path iNestDirectory, std::filesystem::path iCacheRoot)
	: m_nestDirectory{std::move(iNestDirectory)}, m_cacheRoot{std::move(iCacheRoot)} {
	const auto manifestFile = m_nestDirectory / g_manifestName;
	std::ifstream in(manifestFile);
	if (!in)
		return;
	const auto manifest = nlohmann::json::parse(in, nullptr, false);
	if (manifest.is_discarded() || !manifest.contains("runners")) {
		OWL_CORE_WARN("RunnerStore: Cannot read '{}'.", manifestFile.string())
		return;
	}
	m_version = manifest.value("version", "");
	m_hash = manifest.value("hash", "");
	for (const auto& [target, entry]: manifest["runners"].items())
		m_archives[target] = {.file = entry.value("file", ""),
							  .size = entry.value("size", uint64_t{0}),
							  .sha256 = entry.value("sha256", "")};
}

auto RunnerStore::getTargetName(const ExportTarget iTarget) -> std::string_view {
	switch (data::assets::pack::resolveExportTarget(iTarget)) {
		case ExportTarget::Android:
			return "android-arm64";
		case ExportTarget::Web:
			return "web";
		case ExportTarget::LinuxArm64:
			return "linux-arm64";
		case ExportTarget::WindowsX64:
			return "windows-x64";
		case ExportTarget::Host:
		case ExportTarget::LinuxX64:
			return "linux-x64";
	}
	return "linux-x64";
}

auto RunnerStore::getErrorMessage(const RunnerError iError) -> std::string_view {
	switch (iError) {
		case RunnerError::NoManifest:
			return "This Owl Nest lists no runner (no runners.json)";
		case RunnerError::UnknownRunner:
			return "Not a runner of this Owl Nest release";
		case RunnerError::Download:
			return "The runner server could not be reached";
		case RunnerError::Mismatch:
			return "The archive differs from the one of this release";
		case RunnerError::Extract:
			return "The archive could not be unpacked";
	}
	return "Unknown error";
}

auto RunnerStore::isBundled(const ExportTarget iTarget) -> bool {
	const auto target = data::assets::pack::resolveExportTarget(iTarget);
	return target == ExportTarget::Web || target == data::assets::pack::getHostExportTarget();
}

auto RunnerStore::getArchive(const ExportTarget iTarget) const -> std::optional<RunnerArchive> {
	if (const auto found = m_archives.find(getTargetName(iTarget)); found != m_archives.end())
		return found->second;
	return std::nullopt;
}

auto RunnerStore::getRunnerDirectory(const ExportTarget iTarget) const -> std::filesystem::path {
	if (data::assets::pack::resolveExportTarget(iTarget) == ExportTarget::Web)
		return m_nestDirectory / "web";
	if (isBundled(iTarget))
		return m_nestDirectory;
	return getCacheDirectory() / getTargetName(iTarget);
}

auto RunnerStore::isInstalled(const ExportTarget iTarget) const -> bool {
	if (isBundled(iTarget))
		return true;
	return hasManifest() && exists(getRunnerDirectory(iTarget) / g_installedMark);
}

auto RunnerStore::getCacheDirectory() const -> std::filesystem::path {
	return m_cacheRoot / std::format("{}-{}", m_version, m_hash);
}

auto RunnerStore::getDownloadUrl(const ExportTarget iTarget, const std::string_view iServer) const -> std::string {
	const auto archive = getArchive(iTarget);
	if (!archive)
		return {};
	std::string server{iServer};
	while (server.ends_with('/')) server.pop_back();
	return std::format("{}/runners/{}/{}/{}", server, m_version, m_hash, archive->file);
}

auto RunnerStore::download(const ExportTarget iTarget, const std::string_view iServer,
						   const std::function<bool(uint64_t, uint64_t)>& iProgress) -> expected<void, RunnerError> {
	if (!hasManifest()) {
		OWL_CORE_WARN("RunnerStore: No runners.json next to Owl Nest.")
		return unexpected{RunnerError::NoManifest};
	}
	const auto archive = getArchive(iTarget);
	if (!archive) {
		OWL_CORE_WARN("RunnerStore: runners.json lists no {} runner.", getTargetName(iTarget))
		return unexpected{RunnerError::UnknownRunner};
	}
	const auto url = getDownloadUrl(iTarget, iServer);
	const auto schemeEnd = url.find("://");
	const auto pathStart = url.find('/', schemeEnd == std::string::npos ? 0 : schemeEnd + 3);
	httplib::Client client{url.substr(0, pathStart)};
	client.set_follow_location(true);
	client.set_default_headers({{"User-Agent", "OwlNest/" + getVersionString()}});
	std::error_code ec;
	std::filesystem::create_directories(m_cacheRoot, ec);
	const auto partial = m_cacheRoot / (archive->file + ".part");
	bool written = true;
	{
		std::ofstream out(partial, std::ios::binary);
		const auto result = client.Get(
				url.substr(pathStart),
				[&out, &written](const char* iData, const size_t iLength) -> bool {
					out.write(iData, static_cast<std::streamsize>(iLength));
					written = out.good();
					return written;
				},
				[&iProgress](const size_t iReceived, const size_t iTotal) -> bool {
					return !iProgress || iProgress(iReceived, iTotal);
				});
		if (!result || result->status != 200 || !written) {
			OWL_CORE_WARN("RunnerStore: Cannot download '{}' ({}).", url,
						  result ? std::format("HTTP {}", result->status) : httplib::to_string(result.error()))
			out.close();
			std::filesystem::remove(partial, ec);
			return unexpected{RunnerError::Download};
		}
	}
	auto installed = install(iTarget, partial);
	std::filesystem::remove(partial, ec);
	if (installed)
		OWL_CORE_INFO("RunnerStore: Runner {} downloaded from '{}'.", getTargetName(iTarget), url)
	return installed;
}

auto RunnerStore::importArchive(const std::filesystem::path& iArchive) -> expected<ExportTarget, RunnerError> {
	if (!hasManifest()) {
		OWL_CORE_WARN("RunnerStore: No runners.json next to Owl Nest.")
		return unexpected{RunnerError::NoManifest};
	}
	for (const auto target: {ExportTarget::LinuxX64, ExportTarget::LinuxArm64, ExportTarget::WindowsX64,
							 ExportTarget::Android, ExportTarget::Web}) {
		const auto archive = getArchive(target);
		if (!archive || archive->file != iArchive.filename().string())
			continue;
		if (const auto installed = install(target, iArchive); !installed)
			return unexpected{installed.error()};
		OWL_CORE_INFO("RunnerStore: Runner {} imported from '{}'.", getTargetName(target), iArchive.string())
		return target;
	}
	OWL_CORE_WARN("RunnerStore: '{}' is no runner archive of this release.", iArchive.filename().string())
	return unexpected{RunnerError::UnknownRunner};
}

auto RunnerStore::removeOtherVersions() -> size_t {
	size_t removed = 0;
	std::error_code ec;
	const auto current = getCacheDirectory().filename();
	for (const auto& entry: std::filesystem::directory_iterator(m_cacheRoot, ec)) {
		if (!entry.is_directory() || entry.path().filename() == current)
			continue;
		if (std::filesystem::remove_all(entry.path(), ec) > 0 && !ec)
			++removed;
	}
	return removed;
}

auto RunnerStore::install(const ExportTarget iTarget, const std::filesystem::path& iArchive) const
		-> expected<void, RunnerError> {
	const auto archive = getArchive(iTarget);
	if (!archive)
		return unexpected{RunnerError::UnknownRunner};
	std::error_code ec;
	if (std::filesystem::file_size(iArchive, ec) != archive->size || ec || sha256Of(iArchive) != archive->sha256) {
		OWL_CORE_WARN("RunnerStore: '{}' differs from the {} runner of this release.", iArchive.string(),
					  getTargetName(iTarget))
		return unexpected{RunnerError::Mismatch};
	}
	const auto directory = getRunnerDirectory(iTarget);
	std::filesystem::remove_all(directory, ec);
	std::filesystem::create_directories(directory, ec);
	if (!extractTarGz(iArchive, directory)) {
		OWL_CORE_WARN("RunnerStore: Cannot unpack '{}' into '{}'.", iArchive.string(), directory.string())
		std::filesystem::remove_all(directory, ec);
		return unexpected{RunnerError::Extract};
	}
	std::ofstream{directory / g_installedMark} << archive->sha256 << "\n";
	return {};
}

}// namespace owl::nest
