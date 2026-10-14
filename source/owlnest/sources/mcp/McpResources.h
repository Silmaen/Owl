/**
 * @file McpResources.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../session/EditorSession.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::mcp {

/**
 * @brief
 *  The read-only resources of an editor session, addressed by `owl://` URIs.
 *
 * - `owl://scene`: the entity tree of the scene (JSON);
 * - `owl://scene/entity/{id}`: the components of an entity, as in the scene file (YAML);
 * - `owl://assets`, `owl://prefabs`: the files of the asset directories (JSON);
 * - `owl://statistics`: the frame measurements of the session (JSON);
 * - `owl://help/{page}`: a documentation page of the help bundle (Markdown).
 *
 * Everything is returned as plain data.
 */
class McpResources final {
public:
	/// Largest number of files listed by `owl://assets` and `owl://prefabs`.
	static constexpr size_t maxListedFiles = 2000;

	McpResources(const McpResources&) = delete;

	McpResources(McpResources&&) = delete;

	auto operator=(const McpResources&) -> McpResources& = delete;

	auto operator=(McpResources&&) -> McpResources& = delete;

	/**
	 * @brief
	 *  Bind the resources to a session and read the help index.
	 * @param[in] iSession The session read (outlives the resources).
	 * @param[in] iHelpRoot Directory of the help bundle (`index.yml`); empty: no documentation page.
	 */
	McpResources(const session::EditorSession& iSession, std::filesystem::path iHelpRoot);

	/**
	 * @brief
	 *  Destructor.
	 */
	~McpResources() = default;

	/**
	 * @brief
	 *  Every concrete resource: the fixed ones, then one per documentation page.
	 * @return The `resources` array of `resources/list`.
	 */
	[[nodiscard]] auto list() const -> nlohmann::json;

	/**
	 * @brief
	 *  The parameterised resources (entity components, documentation page).
	 * @return The `resourceTemplates` array of `resources/templates/list`.
	 */
	[[nodiscard]] auto listTemplates() const -> nlohmann::json;

	/**
	 * @brief
	 *  Read a resource.
	 * @param[in] iUri The resource URI.
	 * @return The `contents` array of `resources/read`, nothing when no resource has this URI.
	 */
	[[nodiscard]] auto read(std::string_view iUri) const -> std::optional<nlohmann::json>;

	/**
	 * @brief
	 *  Get the number of documentation pages.
	 * @return The pages of the help index.
	 */
	[[nodiscard]] auto getPageCount() const -> size_t { return m_pages.size(); }

private:
	/**
	 * @brief
	 *  A page of the help index.
	 */
	struct HelpPage {
		/// Page identifier (`editor`).
		std::string id;
		/// Page title.
		std::string title;
		/// File, relative to the help root.
		std::string path;
	};

	/**
	 * @brief
	 *  Read the help index.
	 */
	void loadHelpIndex();

	/**
	 * @brief
	 *  The entity tree of the scene.
	 * @return The JSON document.
	 */
	[[nodiscard]] auto readSceneTree() const -> nlohmann::json;

	/**
	 * @brief
	 *  The components of an entity, as YAML.
	 * @param[in] iId The entity UUID, as text.
	 * @return The YAML text, nothing when no entity has this UUID.
	 */
	[[nodiscard]] auto readEntity(std::string_view iId) const -> std::optional<std::string>;

	/**
	 * @brief
	 *  The files of the asset directories.
	 * @param[in] iExtension Only the files with this extension (empty: every file).
	 * @return The JSON document.
	 */
	[[nodiscard]] static auto readAssets(std::string_view iExtension) -> nlohmann::json;

	/**
	 * @brief
	 *  The frame measurements.
	 * @return The JSON document.
	 */
	[[nodiscard]] auto readStatistics() const -> nlohmann::json;

	/**
	 * @brief
	 *  A documentation page.
	 * @param[in] iId The page identifier (case-insensitive).
	 * @return The Markdown text, nothing when no page has this identifier.
	 */
	[[nodiscard]] auto readPage(std::string_view iId) const -> std::optional<std::string>;

	/// The session read (non-owning).
	const session::EditorSession& m_session;
	/// Directory of the help bundle.
	std::filesystem::path m_helpRoot;
	/// Pages of the help index, in index order.
	std::vector<HelpPage> m_pages;
};

}// namespace owl::nest::mcp
