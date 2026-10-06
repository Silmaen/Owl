/**
 * @file AtomicFile.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "platform/AtomicFile.h"
#include "platform/AtomicFileFault.h"

#include "core/UUID.h"

#ifdef OWL_PLATFORM_WINDOWS
// clang-format off
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
// clang-format on
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <format>
#include <system_error>

namespace owl::platform {

namespace {

constexpr int g_NoFault = -1;
std::atomic<int> g_fault{g_NoFault};

auto isFaulty(const WriteError iStep) -> bool { return g_fault.load() == static_cast<int>(iStep); }

auto lastErrorText() -> std::string {
	const int err = errno;
	return std::error_code{err, std::generic_category()}.message();
}

auto makeTempPath(const std::filesystem::path& iTarget) -> std::filesystem::path {
	const auto dir = iTarget.has_parent_path() ? iTarget.parent_path() : std::filesystem::path{"."};
	return dir / std::format(".{}.{:016x}.tmp", iTarget.filename().string(), static_cast<uint64_t>(core::UUID{}));
}

#ifdef OWL_PLATFORM_WINDOWS
auto openTemp(const std::filesystem::path& iTemp, const std::filesystem::path& /*iTarget*/) -> int {
	return _wopen(iTemp.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
}

auto writeChunk(const int iFd, const char* iData, const size_t iSize) -> int64_t {
	return _write(iFd, iData, static_cast<unsigned int>(std::min<size_t>(iSize, 1U << 30U)));
}

auto syncFile(const int iFd) -> bool { return _commit(iFd) == 0; }

auto closeFile(const int iFd) -> bool { return _close(iFd) == 0; }

auto replaceTarget(const std::filesystem::path& iTemp, const std::filesystem::path& iTarget) -> bool {
	if (MoveFileExW(iTemp.c_str(), iTarget.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
		return true;
	errno = EACCES;
	return false;
}

void syncDirectory(const std::filesystem::path& /*iDir*/) {}
#else
auto openTemp(const std::filesystem::path& iTemp, const std::filesystem::path& iTarget) -> int {
	const int fd = ::open(iTemp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);// NOLINT(hicpp-signed-bitwise)
	if (struct stat previous{}; fd >= 0 && ::stat(iTarget.c_str(), &previous) == 0)
		std::ignore = ::fchmod(fd, previous.st_mode & 07777U);
	return fd;
}

auto writeChunk(const int iFd, const char* iData, const size_t iSize) -> int64_t { return ::write(iFd, iData, iSize); }

auto syncFile(const int iFd) -> bool { return ::fsync(iFd) == 0; }

auto closeFile(const int iFd) -> bool { return ::close(iFd) == 0; }

auto replaceTarget(const std::filesystem::path& iTemp, const std::filesystem::path& iTarget) -> bool {
	return ::rename(iTemp.c_str(), iTarget.c_str()) == 0;
}

void syncDirectory(const std::filesystem::path& iDir) {
	const int fd = ::open(iDir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);// NOLINT(hicpp-signed-bitwise)
	if (fd < 0)
		return;
	std::ignore = ::fsync(fd);
	std::ignore = ::close(fd);
}
#endif

auto writeAll(const int iFd, const std::string_view iContent) -> bool {
	const bool interrupted = isFaulty(WriteError::WriteFailed);
	const size_t total = interrupted ? iContent.size() / 2 : iContent.size();
	size_t written = 0;
	while (written < total) {
		const auto count = writeChunk(iFd, iContent.data() + written, total - written);
		if (count < 0 && errno == EINTR)
			continue;
		if (count <= 0)
			return false;
		written += static_cast<size_t>(count);
	}
	if (interrupted)
		errno = EIO;
	return !interrupted;
}

auto fail(const int iFd, const std::filesystem::path& iTemp, const std::filesystem::path& iTarget,
		  const WriteError iError) -> unexpected<WriteError> {
	const auto reason = lastErrorText();
	if (iFd >= 0)
		std::ignore = closeFile(iFd);
	std::error_code ec;
	std::filesystem::remove(iTemp, ec);
	OWL_CORE_ERROR("AtomicFile: Cannot write '{}': {} ({}). The previous file is left untouched.", iTarget.string(),
				   describe(iError), reason)
	return unexpected{iError};
}

}// namespace

auto describe(const WriteError iError) -> std::string_view {
	switch (iError) {
		case WriteError::CreateFailed:
			return "the temporary file cannot be created";
		case WriteError::WriteFailed:
			return "the content cannot be written";
		case WriteError::FlushFailed:
			return "the content cannot be flushed to disk";
		case WriteError::RenameFailed:
			return "the temporary file cannot replace the target";
	}
	return "unknown error";
}

void setAtomicWriteFault(const std::optional<WriteError> iFault) {
	g_fault.store(iFault ? static_cast<int>(*iFault) : g_NoFault);
}

auto writeFileAtomic(const std::filesystem::path& iPath, const std::string_view iContent)
		-> expected<void, WriteError> {
	const auto temp = makeTempPath(iPath);
	const int fd = isFaulty(WriteError::CreateFailed) ? -1 : openTemp(temp, iPath);
	if (fd < 0) {
		if (isFaulty(WriteError::CreateFailed))
			errno = EACCES;
		return fail(fd, temp, iPath, WriteError::CreateFailed);
	}
	if (!writeAll(fd, iContent))
		return fail(fd, temp, iPath, WriteError::WriteFailed);
	if (isFaulty(WriteError::FlushFailed) || !syncFile(fd)) {
		if (isFaulty(WriteError::FlushFailed))
			errno = EIO;
		return fail(fd, temp, iPath, WriteError::FlushFailed);
	}
	if (!closeFile(fd))
		return fail(-1, temp, iPath, WriteError::FlushFailed);
	if (isFaulty(WriteError::RenameFailed) || !replaceTarget(temp, iPath)) {
		if (isFaulty(WriteError::RenameFailed))
			errno = EXDEV;
		return fail(-1, temp, iPath, WriteError::RenameFailed);
	}
	syncDirectory(temp.parent_path());
	return {};
}

}// namespace owl::platform
