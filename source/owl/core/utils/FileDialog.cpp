/**
 * @file FileDialog.cpp
 * @author Silmaen
 * @date 27/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "FileDialog.h"
#include "core/Application.h"

#include <nfd.hpp>

namespace owl::core::utils {
namespace {
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
auto split(const std::string_view iString, const char iDelimiter = '\n') -> std::vector<std::string_view> {
	std::vector<std::string_view> result;
	int indexCommaToRightOfColumn = -1;
	for (uint32_t i = 0; i < iString.size(); i++) {
		if (iString[i] == iDelimiter) {
			const int indexCommaToLeftOfColumn = indexCommaToRightOfColumn;
			indexCommaToRightOfColumn = static_cast<int>(i);
			const int32_t index = indexCommaToLeftOfColumn + 1;
			const auto length = static_cast<uint32_t>(indexCommaToRightOfColumn - index);
			const std::string_view column(iString.data() + index, length);
			result.push_back(column);
		}
	}
	const std::string_view finalColumn(iString.data() + indexCommaToRightOfColumn + 1,
									   iString.size() - static_cast<size_t>(indexCommaToRightOfColumn - 1));
	result.push_back(finalColumn);
	return result;
}
OWL_DIAG_POP

auto parseFilter(const std::string& iFilter) -> std::vector<nfdu8filteritem_t> {
	std::vector<nfdu8filteritem_t> filters;
	for (const auto filterStr = split(iFilter); auto str: filterStr) {
		if (str.empty())
			continue;
		auto items = split(str, '|');
		if (items.size() != 2)
			continue;
		if (items[0].empty() || items[1].empty())
			continue;
		filters.push_back({std::string{items[0]}.c_str(), std::string{items[1]}.c_str()});
	}
	return filters;
}
}// namespace

auto FileDialog::openFile(const std::string& iFilter) -> std::filesystem::path {
	NFD::Init();
	nfdu8char_t* outPath = nullptr;
	std::filesystem::path resultPath;
	const std::string& filters{iFilter};
	const auto ff = parseFilter(filters);
	const std::string tmp = Application::get().getAssetDirectories().front().assetsPath.string();

	if (const auto result = NFD::OpenDialog(outPath, ff.data(), static_cast<uint32_t>(ff.size()), tmp.c_str());
		result == NFD_CANCEL) {
		resultPath = std::filesystem::path{};
	} else if (result == NFD_OKAY) {
		resultPath = std::filesystem::path{outPath};
		NFD_FreePath(outPath);
	} else {
		OWL_CORE_ERROR("while opening file: {}", NFD::GetError())
		OWL_ASSERT(false, "Error Opening file")
	}
	NFD::Quit();
	return resultPath;
}

auto FileDialog::saveFile([[maybe_unused]] const std::string& iFilter) -> std::filesystem::path {
	NFD::Init();
	nfdu8char_t* outPath = nullptr;
	std::filesystem::path resultPath;
	const std::string& filters{iFilter};
	const auto ff = parseFilter(filters);
	if (const auto result =
				NFD::SaveDialog(outPath, ff.data(), static_cast<uint32_t>(ff.size()),
								Application::get().getAssetDirectories().front().assetsPath.string().c_str(), nullptr);
		result == NFD_CANCEL) {
		resultPath = std::filesystem::path{};
	} else if (result == NFD_OKAY) {
		resultPath = std::filesystem::path{outPath};
	} else {
		OWL_CORE_ERROR("while opening file: {}", NFD::GetError())
		OWL_ASSERT(false, "Error Opening file")
	}
	NFD::Quit();
	return resultPath;
}

}// namespace owl::core::utils
