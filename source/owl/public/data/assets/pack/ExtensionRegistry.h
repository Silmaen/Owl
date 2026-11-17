/**
 * @file ExtensionRegistry.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "ExtensionManifest.h"
#include "ExtensionPack.h"

#include "core/Core.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace owl::data::assets::pack {

/**
 * @brief
 *  An extension pack left out, and why.
 */
struct OWL_API ExtensionIssue {
	/// Id of the pack (empty when its manifest cannot be read).
	std::string id;
	/// File or folder of the pack (empty for a manifest given alone).
	std::filesystem::path source;
	/// Why it is left out.
	ExtensionLoadError error{ExtensionLoadError::CannotOpen};
	/// The dependency concerned, when the error is about one.
	std::string detail;
};

/**
 * @brief
 *  An asset several mounted packs hold: the last one serves it.
 */
struct OWL_API ExtensionConflict {
	/// Asset path.
	std::string path;
	/// Ids of the packs holding it, in load order: the last one wins.
	std::vector<std::string> providers;
};

/**
 * @brief
 *  An asset a mounted pack serves.
 */
struct OWL_API ExtensionEntry {
	/// Id of the pack serving it (the last one mounted holding it).
	std::string id;
	/// Asset path.
	std::string path;
};

/**
 * @brief
 *  Result of a load order resolution.
 */
struct OWL_API ExtensionResolution {
	/// Indices of the manifests loaded, in load order.
	std::vector<size_t> order;
	/// Manifests left out.
	std::vector<ExtensionIssue> issues;
};

/**
 * @brief
 *  Order a set of extension packs: every dependency before the packs that need it, then by `loadOrder`, then by id.
 *
 * A pack is left out when its id is taken by an earlier manifest of the list, when a dependency is missing, older than
 * asked or left out itself, or when it lies on a dependency cycle.
 * @param[in] iManifests The manifests.
 * @return The load order and the packs left out.
 */
[[nodiscard]] OWL_API auto resolveLoadOrder(const std::vector<ExtensionManifest>& iManifests) -> ExtensionResolution;

/**
 * @brief
 *  Extension packs mounted over the assets of a game or a project: mods of a game, plugins of a project.
 *
 * The packs are kept in load order; a lookup asks the last one first, so a pack overrides the game and every pack
 * loaded before it. Mounting and clearing are not thread-safe with the lookups: they run before the assets load.
 */
class OWL_API ExtensionRegistry final {
public:
	/**
	 * @brief
	 *  Default constructor: no pack.
	 */
	ExtensionRegistry() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~ExtensionRegistry();

	ExtensionRegistry(const ExtensionRegistry&) = delete;

	/**
	 * @brief
	 *  Move constructor.
	 */
	ExtensionRegistry(ExtensionRegistry&&) noexcept = default;

	auto operator=(const ExtensionRegistry&) -> ExtensionRegistry& = delete;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(ExtensionRegistry&&) noexcept -> ExtensionRegistry& = default;

	/**
	 * @brief
	 *  List the extension packs of a folder: its `.owlmod` files and its sub-folders holding `extension.yml`.
	 * @param[in] iFolder The folder.
	 * @return The packs found, sorted by path; none when the folder does not exist.
	 */
	[[nodiscard]] static auto listSources(const std::filesystem::path& iFolder) -> std::vector<std::filesystem::path>;

	/**
	 * @brief
	 *  List the extension packs of a folder (`listSources`) but those whose id is excluded or whose manifest cannot
	 *  be read.
	 * @param[in] iFolder The folder.
	 * @param[in] iExcluded Ids left out (the packs disabled in a project).
	 * @return The packs kept, sorted by path.
	 */
	[[nodiscard]] static auto listSources(const std::filesystem::path& iFolder,
										  const std::vector<std::string>& iExcluded)
			-> std::vector<std::filesystem::path>;

	/**
	 * @brief
	 *  Open extension packs and mount them with the ones already mounted, all ordered again.
	 * @param[in] iSources `.owlmod` files or source folders.
	 * @param[in] iLoadOrder Rank of some packs by id, replacing the `loadOrder` of their manifest (kept by the next
	 *  mounts); the dependencies still come first.
	 * @return The number of packs mounted after the call.
	 */
	auto mount(const std::vector<std::filesystem::path>& iSources,
			   const std::unordered_map<std::string, int32_t>& iLoadOrder = {}) -> size_t;

	/**
	 * @brief
	 *  Mount the extension packs of a folder (`listSources`).
	 * @param[in] iFolder The folder.
	 * @return The number of packs mounted after the call.
	 */
	auto mountFolder(const std::filesystem::path& iFolder) -> size_t;

	/**
	 * @brief
	 *  Unmount every pack and forget the issues.
	 */
	void clear();

	/**
	 * @brief
	 *  Check whether no pack is mounted.
	 * @return True when none is.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return m_order.empty(); }

	/**
	 * @brief
	 *  Get the packs mounted, in load order.
	 * @return The packs (pointers into the registry, valid until the next mount or clear).
	 */
	[[nodiscard]] auto getPacks() const -> std::vector<const ExtensionPack*>;

	/**
	 * @brief
	 *  Get the packs left out by the last mount, and why.
	 * @return The issues.
	 */
	[[nodiscard]] auto getIssues() const -> const std::vector<ExtensionIssue>& { return m_issues; }

	/**
	 * @brief
	 *  Find a mounted pack by id.
	 * @param[in] iId The id.
	 * @return The pack, or nothing when no mounted pack has this id.
	 */
	[[nodiscard]] auto find(const std::string& iId) const -> const ExtensionPack*;

	/**
	 * @brief
	 *  Find the pack an asset is read from: the last mounted that holds it.
	 * @param[in] iPath Asset path, relative to the asset root.
	 * @return The pack, or nothing when no mounted pack holds the asset.
	 */
	[[nodiscard]] auto findProvider(const std::string& iPath) const -> const ExtensionPack*;

	/**
	 * @brief
	 *  Check whether a mounted pack holds an asset.
	 * @param[in] iPath Asset path, relative to the asset root.
	 * @return True when one does.
	 */
	[[nodiscard]] auto contains(const std::string& iPath) const -> bool { return findProvider(iPath) != nullptr; }

	/**
	 * @brief
	 *  Read an asset from the pack that provides it (thread-safe).
	 * @param[in] iPath Asset path, relative to the asset root.
	 * @return The bytes, or nothing when no mounted pack holds the asset.
	 */
	[[nodiscard]] auto readEntry(const std::string& iPath) const -> std::optional<std::vector<uint8_t>>;

	/**
	 * @brief
	 *  List the assets of every mounted pack, sorted, each once.
	 * @return The asset paths.
	 */
	[[nodiscard]] auto listEntries() const -> std::vector<std::string>;

	/**
	 * @brief
	 *  The assets held by more than one mounted pack.
	 * @return The conflicts, ordered by path.
	 */
	[[nodiscard]] auto getConflicts() const -> std::vector<ExtensionConflict>;

	/**
	 * @brief
	 *  The assets of one kind the mounted packs serve, each with the pack serving it: what the editor offers as
	 *  plugin nodes (`.lua`, `.owlvs`, `.owlmat`) and templates (`.owlprefab`).
	 * @param[in] iExtension File extension, dot included (`.owlprefab`).
	 * @return The entries, ordered by path.
	 */
	[[nodiscard]] auto findEntries(std::string_view iExtension) const -> std::vector<ExtensionEntry>;

private:
	/// Every pack opened, mounted or left out.
	std::vector<ExtensionPack> m_candidates;
	/// Indices in `m_candidates` of the packs mounted, in load order.
	std::vector<size_t> m_order;
	/// Packs left out by the last mount.
	std::vector<ExtensionIssue> m_issues;
	/// Packs that could not be opened (kept for the issues of the next mounts).
	std::vector<ExtensionIssue> m_openIssues;
	/// Ranks replacing the `loadOrder` of the manifests, by id.
	std::unordered_map<std::string, int32_t> m_loadOrder;
};

}// namespace owl::data::assets::pack
