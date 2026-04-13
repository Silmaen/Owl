/**
 * @file GameState.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace owl::scene {

/**
 * @brief Global key-value store for game progression data.
 *
 * Stores int, float, string, and bool values by key. Accessible from Lua via
 * the `gamestate` table. Serialized with save files.
 */
class OWL_API GameState final {
public:
	/// Value type for game state entries.
	using Value = std::variant<int64_t, float, std::string, bool>;

	GameState() = default;
	~GameState() = default;
	GameState(const GameState&) = default;
	GameState(GameState&&) = default;
	auto operator=(const GameState&) -> GameState& = default;
	auto operator=(GameState&&) -> GameState& = default;

	/**
	 * @brief Set a value for a key.
	 * @param[in] iKey The key.
	 * @param[in] iValue The value.
	 */
	void set(const std::string& iKey, Value iValue);

	/**
	 * @brief Get a value by key.
	 * @param[in] iKey The key.
	 * @return The value, or std::nullopt if not found.
	 */
	[[nodiscard]] auto get(const std::string& iKey) const -> std::optional<Value>;

	/**
	 * @brief Get a value by key with a default fallback.
	 * @param[in] iKey The key.
	 * @param[in] iDefault Default value if key is missing.
	 * @return The value, or the default.
	 */
	[[nodiscard]] auto get(const std::string& iKey, const Value& iDefault) const -> Value;

	/**
	 * @brief Remove a key.
	 * @param[in] iKey The key to remove.
	 */
	void remove(const std::string& iKey);

	/// @brief Remove all entries.
	void clear();

	/// @brief Get all keys.
	[[nodiscard]] auto keys() const -> std::vector<std::string>;

	/// @brief Check if the store is empty.
	[[nodiscard]] auto empty() const -> bool;

	/// @brief Get the number of entries.
	[[nodiscard]] auto size() const -> size_t;

	/**
	 * @brief Serialize the game state to YAML.
	 * @param[in] iOut The serializer context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief Deserialize the game state from YAML.
	 * @param[in] iNode The serializer context.
	 */
	void deserialize(const core::Serializer& iNode);

private:
	/// Internal storage.
	std::unordered_map<std::string, Value> m_data;
};

}// namespace owl::scene
