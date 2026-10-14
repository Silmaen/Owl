/**
 * @file McpResources.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "McpResources.h"

#include <app/Application.h>
#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/SceneSerializer.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <exception>
#include <format>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace owl::nest::mcp {

namespace {

using json = nlohmann::json;

constexpr std::string_view g_SceneUri = "owl://scene";
constexpr std::string_view g_EntityPrefix = "owl://scene/entity/";
constexpr std::string_view g_AssetsUri = "owl://assets";
constexpr std::string_view g_PrefabsUri = "owl://prefabs";
constexpr std::string_view g_StatisticsUri = "owl://statistics";
constexpr std::string_view g_HelpPrefix = "owl://help/";
constexpr std::string_view g_JsonMime = "application/json";
constexpr std::string_view g_YamlMime = "application/yaml";
constexpr std::string_view g_MarkdownMime = "text/markdown";

auto resource(const std::string_view iUri, const std::string_view iName, const std::string_view iTitle,
			  const std::string_view iDescription, const std::string_view iMime) -> json {
	return {{"uri", iUri}, {"name", iName}, {"title", iTitle}, {"description", iDescription}, {"mimeType", iMime}};
}

auto contents(const std::string_view iUri, const std::string_view iMime, std::string iText) -> json {
	return json::array({{{"uri", iUri}, {"mimeType", iMime}, {"text", std::move(iText)}}});
}

auto equalsIgnoringCase(const std::string_view iLeft, const std::string_view iRight) -> bool {
	return std::ranges::equal(iLeft, iRight, [](const unsigned char iA, const unsigned char iB) -> bool {
		return std::tolower(iA) == std::tolower(iB);
	});
}

auto readText(const std::filesystem::path& iPath) -> std::optional<std::string> {
	const std::ifstream stream(iPath, std::ios::binary);
	if (!stream) {
		OWL_CORE_WARN("Mcp: Cannot read {}.", iPath.string())
		return std::nullopt;
	}
	std::stringstream buffer;
	buffer << stream.rdbuf();
	return buffer.str();
}

auto isInside(const std::filesystem::path& iPath, const std::filesystem::path& iRoot) -> bool {
	const auto relative =
			std::filesystem::weakly_canonical(iPath).lexically_relative(std::filesystem::weakly_canonical(iRoot));
	return !relative.empty() && *relative.begin() != "..";
}

// NOLINTNEXTLINE(misc-no-recursion) the depth is the depth of the scene hierarchy
auto entityNode(const scene::Scene& iScene, const scene::Entity& iEntity) -> json {
	auto children = json::array();
	for (const auto& child: iScene.getChildren(iEntity)) children.push_back(entityNode(iScene, child));
	return {{"id", static_cast<uint64_t>(iEntity.getUUID())}, {"name", iEntity.getName()}, {"children", children}};
}

auto isHidden(const std::filesystem::path& iRelative) -> bool {
	return std::ranges::any_of(
			iRelative, [](const std::filesystem::path& iPart) -> bool { return iPart.string().starts_with('.'); });
}

}// namespace

McpResources::McpResources(const session::EditorSession& iSession, std::filesystem::path iHelpRoot)
	: m_session{iSession}, m_helpRoot{std::move(iHelpRoot)} {
	loadHelpIndex();
}

void McpResources::loadHelpIndex() {
	if (m_helpRoot.empty())
		return;
	const auto indexFile = m_helpRoot / "index.yml";
	if (!exists(indexFile)) {
		OWL_CORE_WARN("Mcp: No help index in {}.", m_helpRoot.string())
		return;
	}
	try {
		const auto pages = YAML::LoadFile(indexFile.string())["Help"]["Pages"];
		if (!pages || !pages.IsSequence())
			return;
		for (const auto& entry: pages) {
			HelpPage page{.id = entry["id"].as<std::string>(""),
						  .title = entry["title"].as<std::string>(""),
						  .path = entry["path"].as<std::string>("")};
			if (!page.id.empty() && !page.path.empty())
				m_pages.push_back(std::move(page));
		}
	} catch (const std::exception& ex) {
		OWL_CORE_WARN("Mcp: Cannot parse the help index {} ({}).", indexFile.string(), ex.what())
		m_pages.clear();
	}
}

auto McpResources::list() const -> json {
	auto resources = json::array(
			{resource(g_SceneUri, "scene", "Scene tree", "Entities of the scene, with their UUID and children.",
					  g_JsonMime),
			 resource(g_AssetsUri, "assets", "Assets", "Files of the asset directories.", g_JsonMime),
			 resource(g_PrefabsUri, "prefabs", "Prefabs", "Prefab files of the asset directories.", g_JsonMime),
			 resource(g_StatisticsUri, "statistics", "Statistics", "Frame measurements of the session.", g_JsonMime)});
	for (const auto& page: m_pages)
		resources.push_back(resource(std::format("{}{}", g_HelpPrefix, page.id), page.id,
									 page.title.empty() ? page.id : page.title, "Documentation page.", g_MarkdownMime));
	return resources;
}

auto McpResources::listTemplates() const -> json {
	return json::array({{{"uriTemplate", std::format("{}{{id}}", g_EntityPrefix)},
						 {"name", "entity"},
						 {"title", "Entity components"},
						 {"description", "Components of an entity (UUID from owl://scene), as in the scene file."},
						 {"mimeType", g_YamlMime}},
						{{"uriTemplate", std::format("{}{{page}}", g_HelpPrefix)},
						 {"name", "help"},
						 {"title", "Documentation page"},
						 {"description", "A page of the Owl documentation, by identifier."},
						 {"mimeType", g_MarkdownMime}}});
}

auto McpResources::read(const std::string_view iUri) const -> std::optional<json> {
	if (iUri == g_SceneUri)
		return contents(iUri, g_JsonMime, readSceneTree().dump(2));
	if (iUri == g_AssetsUri)
		return contents(iUri, g_JsonMime, readAssets({}).dump(2));
	if (iUri == g_PrefabsUri)
		return contents(iUri, g_JsonMime, readAssets(".owlprefab").dump(2));
	if (iUri == g_StatisticsUri)
		return contents(iUri, g_JsonMime, readStatistics().dump(2));
	if (iUri.starts_with(g_EntityPrefix)) {
		if (auto yaml = readEntity(iUri.substr(g_EntityPrefix.size())); yaml)
			return contents(iUri, g_YamlMime, std::move(*yaml));
	} else if (iUri.starts_with(g_HelpPrefix)) {
		if (auto page = readPage(iUri.substr(g_HelpPrefix.size())); page)
			return contents(iUri, g_MarkdownMime, std::move(*page));
	}
	OWL_CORE_WARN("Mcp: No resource {}.", iUri)
	return std::nullopt;
}

auto McpResources::readSceneTree() const -> json {
	const auto scene = m_session.getSessionScene();
	if (!scene)
		return {{"open", false}, {"entities", json::array()}};
	auto roots = json::array();
	for (const auto& root: scene->getRootEntities()) roots.push_back(entityNode(*scene, root));
	return {{"open", true},
			{"path", m_session.getSessionScenePath().generic_string()},
			{"playing", m_session.isScenePlaying()},
			{"entity_count", scene->getEntityCount()},
			{"entities", std::move(roots)}};
}

auto McpResources::readEntity(const std::string_view iId) const -> std::optional<std::string> {
	const auto scene = m_session.getSessionScene();
	uint64_t uuid = 0;
	if (const auto [ptr, ec] = std::from_chars(iId.data(), iId.data() + iId.size(), uuid);
		!scene || ec != std::errc{} || ptr != iId.data() + iId.size()) {
		OWL_CORE_WARN("Mcp: Entity resource `{}` names no UUID of an open scene.", iId)
		return std::nullopt;
	}
	const auto entity = scene->findEntityByUUID(core::UUID{uuid});
	if (!entity) {
		OWL_CORE_WARN("Mcp: No entity {}.", uuid)
		return std::nullopt;
	}
	return scene::SceneSerializer::serializeEntityToString(entity);
}

auto McpResources::readAssets(const std::string_view iExtension) -> json {
	auto files = json::array();
	bool truncated = false;
	if (app::Application::instanced()) {
		for (const auto& directory: app::Application::get().getAssetDirectories()) {
			std::error_code error;
			for (auto it = std::filesystem::recursive_directory_iterator(
						 directory.assetsPath, std::filesystem::directory_options::skip_permission_denied, error);
				 !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
				if (!it->is_regular_file(error))
					continue;
				const auto relative = it->path().lexically_relative(directory.assetsPath);
				if (isHidden(relative) || (!iExtension.empty() && it->path().extension() != iExtension))
					continue;
				if (files.size() >= maxListedFiles) {
					truncated = true;
					break;
				}
				files.push_back({{"path", relative.generic_string()},
								 {"directory", directory.title},
								 {"file", it->path().generic_string()}});
			}
		}
	}
	return {{"files", std::move(files)}, {"truncated", truncated}};
}

auto McpResources::readStatistics() const -> json {
	const auto stats = m_session.getStatistics();
	return {{"frame_count", stats.frameCount},
			{"last_frame_ms", stats.lastFrameMs},
			{"mean_frame_ms", stats.meanFrameMs},
			{"last_draw_calls", stats.lastDrawCalls},
			{"entity_count", stats.entityCount},
			{"tick_count", stats.tickCount},
			{"playing", m_session.isScenePlaying()},
			{"last_phases",
			 {{"scripts_ms", stats.lastPhases.scriptsMs},
			  {"physics_ms", stats.lastPhases.physicsMs},
			  {"render_ms", stats.lastPhases.renderMs},
			  {"total_ms", stats.lastPhases.totalMs}}}};
}

auto McpResources::readPage(const std::string_view iId) const -> std::optional<std::string> {
	const auto page = std::ranges::find_if(
			m_pages, [iId](const HelpPage& iPage) -> bool { return equalsIgnoringCase(iPage.id, iId); });
	if (page == m_pages.end()) {
		OWL_CORE_WARN("Mcp: No documentation page {}.", iId)
		return std::nullopt;
	}
	const auto file = m_helpRoot / page->path;
	if (!isInside(file, m_helpRoot)) {
		OWL_CORE_WARN("Mcp: Documentation page {} lies outside the help bundle.", iId)
		return std::nullopt;
	}
	return readText(file);
}

}// namespace owl::nest::mcp
