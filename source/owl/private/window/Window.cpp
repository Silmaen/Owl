/**
 * @file Window.cpp
 * @author Silmaen
 * @date 04/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/Log.h"
#include "glfw/Window.h"
#include "null/Window.h"
#include "window/Window.h"

#include <cctype>

namespace owl::window {

namespace {
[[nodiscard]] auto toLower(const std::string_view iText) -> std::string {
	std::string result;
	result.reserve(iText.size());
	for (const char chr: iText) result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(chr))));
	return result;
}
}// namespace

auto parsePlatform(const std::string_view iName) -> std::optional<Platform> {
	const auto name = toLower(iName);
	if (name == "auto" || name == "any")
		return Platform::Auto;
	if (name == "wayland")
		return Platform::Wayland;
	if (name == "x11" || name == "xwayland")
		return Platform::X11;
	if (name == "win32" || name == "windows")
		return Platform::Win32;
	if (name == "none" || name == "null")
		return Platform::None;
	return std::nullopt;
}

auto platformName(const Platform iPlatform) -> std::string_view {
	switch (iPlatform) {
		case Platform::Auto:
			return "auto";
		case Platform::Wayland:
			return "wayland";
		case Platform::X11:
			return "x11";
		case Platform::Win32:
			return "win32";
		case Platform::None:
			return "none";
	}
	return "auto";
}

auto resolvePlatform(const Platform iConfigured, const std::optional<std::string_view>& iEnvValue) -> Platform {
	if (!iEnvValue.has_value() || iEnvValue->empty())
		return iConfigured;
	if (const auto parsed = parsePlatform(*iEnvValue); parsed.has_value())
		return *parsed;
	OWL_CORE_WARN("Window: Ignoring unknown {} value '{}' (expected auto, wayland or x11).", g_PlatformEnvVar,
				  *iEnvValue)
	return iConfigured;
}

auto makeAppId(const std::string_view iName) -> std::string {
	std::string result;
	result.reserve(iName.size());
	for (const char chr: iName) {
		const auto uChr = static_cast<unsigned char>(chr);
		if (std::isalnum(uChr) != 0)
			result.push_back(static_cast<char>(std::tolower(uChr)));
		else if (chr == '.' || chr == '-' || chr == '_')
			result.push_back(chr);
		else if (chr == ' ' && !result.empty() && result.back() != '-')
			result.push_back('-');
	}
	while (!result.empty() && (result.back() == '-' || result.back() == '.')) result.pop_back();
	if (result.empty())
		return "owl-engine";
	return result;
}

auto Window::create(const Properties& iProps) -> uniq<Window> {
	switch (iProps.winType) {
		case Type::Glfw:
			return mkUniq<glfw::Window>(iProps);
		case Type::Null:
			return mkUniq<null::Window>(iProps);
	}
	return nullptr;
}

Window::~Window() = default;

}// namespace owl::window
