/**
 * @file MaterialLibrary.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Material.h"
#include "core/Core.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::renderer {

/**
 * @brief
 *  The `.owlmat` materials of an engine context, loaded once per file and shared by every entity. A material instance
 *  (`parent`) is resolved over its parent chain; a file that changes (hot reload, an edit saved by Owl Nest) drops
 *  itself and every instance built on it, and bumps the generation so that the holders of a resolved material fetch
 *  it again.
 */
class OWL_API MaterialLibrary final {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	MaterialLibrary() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~MaterialLibrary() = default;

	/**
	 * @brief
	 *  Copy constructor (deleted).
	 */
	MaterialLibrary(const MaterialLibrary&) = delete;

	/**
	 * @brief
	 *  Move constructor (deleted).
	 */
	MaterialLibrary(MaterialLibrary&&) = delete;

	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return This library.
	 */
	auto operator=(const MaterialLibrary&) -> MaterialLibrary& = delete;

	/**
	 * @brief
	 *  Move assignment (deleted).
	 * @return This library.
	 */
	auto operator=(MaterialLibrary&&) -> MaterialLibrary& = delete;

	/**
	 * @brief
	 *  The resolved material of a `.owlmat` asset path (the open pack first, then the asset directories).
	 * @param[in] iAssetPath Asset-relative (or absolute) path of the file.
	 * @return The material, null when the file or its parent chain cannot be read (logged once).
	 */
	[[nodiscard]] auto get(const std::filesystem::path& iAssetPath) -> shared<const Material>;

	/**
	 * @brief
	 *  The file content of a material, as last read (for the editor).
	 * @param[in] iAssetPath Asset path of the file.
	 * @return The asset, nothing when it cannot be read.
	 */
	[[nodiscard]] auto getAsset(const std::filesystem::path& iAssetPath) -> std::optional<MaterialAsset>;

	/**
	 * @brief
	 *  Drop a changed file and every material instance built on it.
	 * @param[in] iFile The changed file (absolute) or asset path.
	 * @return Number of dropped materials.
	 */
	auto invalidate(const std::filesystem::path& iFile) -> uint32_t;

	/**
	 * @brief
	 *  Counter bumped by every invalidation that dropped something.
	 * @return The generation.
	 */
	[[nodiscard]] auto getGeneration() const -> uint64_t { return m_generation; }

	/**
	 * @brief
	 *  Drop every material.
	 */
	void clear();

	/**
	 * @brief
	 *  Number of looked-up asset paths.
	 * @return The count.
	 */
	[[nodiscard]] auto size() const -> size_t { return m_entries.size(); }

	/// Longest parent chain followed before a material is refused as cyclic.
	static constexpr uint32_t g_maxParentDepth = 16;

private:
	/// One looked-up asset path.
	struct Entry {
		/// The resolved material, null when it cannot be read.
		shared<const Material> material;
		/// The file content.
		std::optional<MaterialAsset> asset;
		/// File it was read from (empty for a pack entry).
		std::filesystem::path file;
		/// Keys of the materials of its parent chain.
		std::vector<std::string> ancestors;
	};

	/**
	 * @brief
	 *  Load and resolve an entry.
	 * @param[in] iAssetPath Asset path.
	 * @param[in] iDepth Depth in the parent chain being resolved.
	 * @return The entry.
	 */
	auto resolve(const std::filesystem::path& iAssetPath, uint32_t iDepth) -> const Entry&;

	/// Entries keyed by the generic form of their asset path.
	std::unordered_map<std::string, Entry> m_entries;
	/// Bumped by every invalidation that dropped something.
	uint64_t m_generation = 1;
};

}// namespace owl::renderer
