/**
 * @file HudLibrary.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace owl::app {
class EngineContext;
}// namespace owl::app

namespace owl::scene {
class Scene;

/**
 * @brief
 *  The `.owlhud` files loaded so far, each one into its own scene, cached per path.
 *
 * Several scenes naming the same file share one HUD scene, so the values of its widgets (a score, a health bar) stay
 * across a scene transition. A file is read from the open pack, then from the asset folders, then as a plain path; one
 * that cannot be read or parsed is reported once and not retried until `invalidate` or `clear`. Owned by
 * `app::EngineContext`.
 */
class OWL_API HudLibrary final {
public:
	/**
	 * @brief
	 *  Create an empty library.
	 * @param[in] iContext The engine context the HUD scenes use (may be null: tests without application).
	 */
	explicit HudLibrary(app::EngineContext* iContext);

	HudLibrary(const HudLibrary&) = delete;

	HudLibrary(HudLibrary&&) = delete;

	auto operator=(const HudLibrary&) -> HudLibrary& = delete;

	auto operator=(HudLibrary&&) -> HudLibrary& = delete;

	/**
	 * @brief
	 *  Destructor: releases every HUD scene.
	 */
	~HudLibrary();

	/**
	 * @brief
	 *  Get the HUD scene of a file, loading it on first use.
	 * @param[in] iPath The asset path (`huds/gameplay.owlhud`), as written in a scene's `Hud:` key.
	 * @return The shared HUD scene, or nullptr when the file cannot be loaded.
	 */
	[[nodiscard]] auto get(const std::string& iPath) -> shared<Scene>;

	/**
	 * @brief
	 *  Check whether a file is already loaded.
	 * @param[in] iPath The asset path.
	 * @return True when `get` would not read the file.
	 */
	[[nodiscard]] auto isLoaded(const std::string& iPath) const -> bool;

	/**
	 * @brief
	 *  Forget a file: the next `get` reads it again (scenes that already hold the old HUD keep it until they ask again).
	 * @param[in] iPath The asset path.
	 */
	void invalidate(const std::string& iPath);

	/**
	 * @brief
	 *  Forget every file (the editor calls it when a HUD is saved and when Play starts or stops).
	 */
	void clear();

	/**
	 * @brief
	 *  Get the number of files read since the creation (a spy for the cache contract).
	 * @return The number of loads attempted.
	 */
	[[nodiscard]] auto getLoadCount() const -> uint64_t { return m_loadCount; }

	/**
	 * @brief
	 *  Get the number of files held (the ones that failed included).
	 * @return The number of cache entries.
	 */
	[[nodiscard]] auto size() const -> size_t { return m_huds.size(); }

private:
	/**
	 * @brief
	 *  Read and parse a file into a new scene.
	 * @param[in] iPath The asset path.
	 * @return The scene, or nullptr when the file is missing or invalid.
	 */
	[[nodiscard]] auto load(const std::string& iPath) -> shared<Scene>;

	/// Engine context given to the HUD scenes.
	app::EngineContext* mp_context = nullptr;
	/// HUD scenes by asset path (null for a file that failed to load).
	std::unordered_map<std::string, shared<Scene>> m_huds;
	/// Number of files read.
	uint64_t m_loadCount = 0;
};

}// namespace owl::scene
