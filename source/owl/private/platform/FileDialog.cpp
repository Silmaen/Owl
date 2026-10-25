/**
 * @file FileDialog.cpp
 * @author Silmaen
 * @date 27/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "app/Application.h"
#include "platform/FileDialog.h"
#include "window/Window.h"

#include <nfd.hpp>
#if OWL_WITH_RENDER && defined(OWL_PLATFORM_LINUX)
// The portal needs the parent window as an X11 id or an exported Wayland surface: nfd_glfw3.h gets both from GLFW.
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <nfd_glfw3.h>
#define OWL_FILE_DIALOG_GLFW_PARENT 1
#else
#define OWL_FILE_DIALOG_GLFW_PARENT 0
#endif

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::platform {

namespace {

// Filters of a dialogue: the strings own the text, the items point into them.
struct Filters {
	std::vector<std::pair<std::string, std::string>> texts;
	std::vector<nfdu8filteritem_t> items;
};

// Parses "Name|ext1,ext2\nName2|ext3": one filter per line, its name and its extensions split by '|'.
auto parseFilter(const std::string_view iFilter) -> Filters {
	Filters filters;
	std::string_view rest = iFilter;
	while (!rest.empty()) {
		const auto eol = rest.find('\n');
		const std::string_view line = rest.substr(0, eol);
		rest = eol == std::string_view::npos ? std::string_view{} : rest.substr(eol + 1);
		const auto bar = line.find('|');
		if (bar == std::string_view::npos || bar == 0 || bar + 1 == line.size() ||
			line.find('|', bar + 1) != std::string_view::npos)
			continue;
		filters.texts.emplace_back(line.substr(0, bar), line.substr(bar + 1));
	}
	filters.items.reserve(filters.texts.size());
	for (const auto& [name, spec]: filters.texts)
		filters.items.push_back(nfdu8filteritem_t{name.c_str(), spec.c_str()});
	return filters;
}

// One NFD session per dialogue: the portal opens its D-Bus connection in NFD::Init and closes it in NFD::Quit.
auto beginDialog() -> bool {
	if (NFD::Init() != NFD_OKAY) {
		OWL_CORE_ERROR("FileDialog: Cannot reach the native dialogues ({}).", NFD::GetError())
		NFD::ClearError();
		return false;
	}
	return true;
}

// The parent window, so the dialogue is modal over Owl Nest; a value-initialized handle means no parent.
auto parentWindow() -> nfdwindowhandle_t {
	nfdwindowhandle_t handle{};
#if OWL_FILE_DIALOG_GLFW_PARENT
	auto& window = app::Application::get().getWindow();
	if (window.getType() != window::Type::Glfw)
		return handle;
	auto* const glfwWindow = static_cast<GLFWwindow*>(window.getNativeWindow());
	if (glfwWindow == nullptr)
		return handle;
	// Under Wayland the portal needs the application's display to export the parent surface.
	if (!NFD_SetDisplayPropertiesFromGLFW())
		OWL_CORE_WARN("FileDialog: Cannot hand the Wayland display to the dialogues.")
	if (!NFD_GetNativeWindowFromGLFWWindow(glfwWindow, &handle))
		handle = nfdwindowhandle_t{};
#endif
	return handle;
}

// Absolute, native separators, UTF-8: Windows refuses to open a dialogue on a default folder it cannot parse.
auto defaultDirectory() -> std::string {
	std::error_code error;
	auto directory = std::filesystem::absolute(app::Application::get().getAssetDirectories().front().assetsPath, error);
	if (error || !std::filesystem::is_directory(directory, error))
		return {};
	directory.make_preferred();
	const auto utf8 = directory.u8string();
	return {utf8.begin(), utf8.end()};
}

auto finishDialog(const nfdresult_t iResult, nfdu8char_t* iOutPath, const std::string_view iWhat)
		-> std::filesystem::path {
	std::filesystem::path resultPath;
	if (iResult == NFD_OKAY) {
		resultPath = std::filesystem::path{std::u8string_view{reinterpret_cast<const char8_t*>(iOutPath)}};
		NFD_FreePathU8(iOutPath);
	} else if (iResult == NFD_ERROR) {
		OWL_CORE_ERROR("FileDialog: Error while {} ({}).", iWhat, NFD::GetError())
		NFD::ClearError();
	}
	NFD::Quit();
	return resultPath;
}

}// namespace

auto FileDialog::openFile(const std::string& iFilter) -> std::filesystem::path {
	if (!beginDialog())
		return {};
	const auto filters = parseFilter(iFilter);
	const std::string defaultDir = defaultDirectory();
	nfdopendialogu8args_t args{};
	args.filterList = filters.items.data();
	args.filterCount = static_cast<nfdfiltersize_t>(filters.items.size());
	args.defaultPath = defaultDir.empty() ? nullptr : defaultDir.c_str();
	args.parentWindow = parentWindow();
	nfdu8char_t* outPath = nullptr;
	const auto result = NFD_OpenDialogU8_With(&outPath, &args);
	return finishDialog(result, outPath, "opening a file");
}

auto FileDialog::saveFile(const std::string& iFilter, const std::string& iDefaultName) -> std::filesystem::path {
	if (!beginDialog())
		return {};
	const auto filters = parseFilter(iFilter);
	const std::string defaultDir = defaultDirectory();
	nfdsavedialogu8args_t args{};
	args.filterList = filters.items.data();
	args.filterCount = static_cast<nfdfiltersize_t>(filters.items.size());
	args.defaultPath = defaultDir.empty() ? nullptr : defaultDir.c_str();
	args.defaultName = iDefaultName.empty() ? nullptr : iDefaultName.c_str();
	args.parentWindow = parentWindow();
	nfdu8char_t* outPath = nullptr;
	const auto result = NFD_SaveDialogU8_With(&outPath, &args);
	return finishDialog(result, outPath, "saving a file");
}

auto FileDialog::pickFolder() -> std::filesystem::path {
	if (!beginDialog())
		return {};
	const std::string defaultDir = defaultDirectory();
	nfdpickfolderu8args_t args{};
	args.defaultPath = defaultDir.empty() ? nullptr : defaultDir.c_str();
	args.parentWindow = parentWindow();
	nfdu8char_t* outPath = nullptr;
	const auto result = NFD_PickFolderU8_With(&outPath, &args);
	return finishDialog(result, outPath, "picking a folder");
}

}// namespace owl::platform
