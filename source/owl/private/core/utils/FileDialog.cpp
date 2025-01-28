/**
 * @file FileDialog.cpp
 * @author Silmaen
 * @date 27/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/Application.h"
#include "core/utils/FileDialog.h"

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
		const std::string sstr{str};
		if (sstr.empty())
			continue;
		auto items = split(sstr, '|');
		if (items.size() != 2)
			continue;
		if (items[0].empty() || items[1].empty())
			continue;
		OWL_DIAG_PUSH
		OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
		auto* const s0 = new nfdu8char_t[items[0].size() + 1];
		memcpy(s0, items[0].data(), items[0].size());
		s0[items[0].size()] = '\0';
		auto* const s1 = new nfdu8char_t[items[1].size() + 1];
		memcpy(s1, items[1].data(), items[1].size());
		s1[items[1].size()] = '\0';
		OWL_DIAG_POP
		filters.push_back(nfdu8filteritem_t{s0, s1});
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

	if (const auto result = NFD_OpenDialogU8(&outPath, ff.data(), static_cast<nfdfiltersize_t>(ff.size()), tmp.c_str());
		result == NFD_CANCEL) {
		resultPath = std::filesystem::path{};
	} else if (result == NFD_OKAY) {
		resultPath = std::filesystem::path{outPath};
		NFD_FreePath(outPath);
	} else {
		OWL_CORE_ERROR("while opening file: {}", NFD::GetError())
		OWL_ASSERT(false, "Error Opening file")
	}
	for (const auto& [name, spec]: ff) {
		delete name;
		delete spec;
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
				NFD_SaveDialogU8(&outPath, ff.data(), static_cast<nfdfiltersize_t>(ff.size()),
								 Application::get().getAssetDirectories().front().assetsPath.string().c_str(), nullptr);
		result == NFD_CANCEL) {
		resultPath = std::filesystem::path{};
	} else if (result == NFD_OKAY) {
		resultPath = std::filesystem::path{outPath};
	} else {
		OWL_CORE_ERROR("while opening file: {}", NFD::GetError())
		OWL_ASSERT(false, "Error Opening file")
	}
	for (const auto& [name, spec]: ff) {
		delete name;
		delete spec;
	}
	NFD::Quit();
	return resultPath;
}

}// namespace owl::core::utils
