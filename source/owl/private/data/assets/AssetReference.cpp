/**
 * @file AssetReference.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/AssetReference.h"

#include "app/Application.h"

#include <charconv>
#include <cstdint>
#include <format>
#include <system_error>

namespace owl::data::assets {

namespace {
constexpr size_t g_guidDigits = 16;
}// namespace

auto formatGuid(const AssetGuid iGuid) -> std::string { return std::format("{:016x}", static_cast<uint64_t>(iGuid)); }

auto parseGuid(const std::string_view iText) -> std::optional<AssetGuid> {
	if (iText.size() != g_guidDigits)
		return std::nullopt;
	uint64_t value = 0;
	const auto [end, error] = std::from_chars(iText.data(), iText.data() + iText.size(), value, 16);
	if (error != std::errc{} || end != iText.data() + iText.size())
		return std::nullopt;
	return AssetGuid{value};
}

auto parseReference(const std::string_view iReference) -> ParsedReference {
	if (!iReference.starts_with(g_GuidReferencePrefix))
		return {.guid = std::nullopt, .path = std::string{iReference}};
	const auto body = iReference.substr(g_GuidReferencePrefix.size());
	const auto separator = body.find(':');
	const auto guid = parseGuid(body.substr(0, separator));
	if (!guid) {
		OWL_CORE_WARN("Assets: Malformed GUID reference '{}', read as a path.", iReference)
		return {.guid = std::nullopt, .path = std::string{iReference}};
	}
	return {.guid = guid, .path = separator == std::string_view::npos ? std::string{} : std::string{body.substr(separator + 1)}};
}

auto formatReference(const AssetGuid iGuid, const std::string_view iPathHint) -> std::string {
	return std::format("{}{}:{}", g_GuidReferencePrefix, formatGuid(iGuid), iPathHint);
}

auto toAssetPath(const std::string_view iReference) -> std::string {
	if (app::Application::instanced())
		return app::Application::get().getAssetRegistry().toAssetPath(iReference);
	return parseReference(iReference).path;
}

auto toAssetReference(const std::string_view iPath) -> std::string {
	if (app::Application::instanced())
		return app::Application::get().getAssetRegistry().toAssetReference(iPath);
	return std::string{iPath};
}

auto toTextureReference(const std::string_view iSerialized) -> std::string {
	if (!iSerialized.starts_with(g_TextureNamePrefix))
		return std::string{iSerialized};
	auto reference = toAssetReference(iSerialized.substr(g_TextureNamePrefix.size()));
	return reference.starts_with(g_GuidReferencePrefix) ? reference : std::string{iSerialized};
}

auto fromTextureReference(const std::string_view iSerialized) -> std::string {
	if (!iSerialized.starts_with(g_GuidReferencePrefix))
		return std::string{iSerialized};
	return std::format("{}{}", g_TextureNamePrefix, toAssetPath(iSerialized));
}

}// namespace owl::data::assets
