/**
 * @file UUID.h
 * @author Silmaen
 * @date 14/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::core {
/**
 * @brief Uuid object.
 */
class OWL_API UUID {
public:
	UUID(const UUID&) = default;
	UUID(UUID&&) = default;
	auto operator=(const UUID&) -> UUID& = default;
	auto operator=(UUID&&) -> UUID& = default;

	/**
	 * @brief Default constructor.
	 */
	UUID();

	// NOLINTBEGIN(google-explicit-constructor,hicpp-explicit-conversions)
	/**
	 * @brief Default constructor.
	 * @param[in] iUuid The uuid to initialize.
	 */
	UUID(const uint64_t iUuid) : m_uuid{iUuid} {}

	/**
	 * @brief Destructor.
	 */
	~UUID() = default;

	/**
	 * @brief Implicit conversion to uint64_t.
	 * @return Access const to internal ID.
	 */
	operator uint64_t() const { return m_uuid; }
	// NOLINTEND(google-explicit-constructor,hicpp-explicit-conversions)

private:
	/// The uuid code.
	uint64_t m_uuid;
};
}// namespace owl::core

/// @cond

/**
 * @brief Hash function for the UUID object.
 */
template<>
struct std::hash<owl::core::UUID> {
	auto operator()(const owl::core::UUID& iUuid) const noexcept -> std::size_t {
		return hash<uint64_t>()(static_cast<uint64_t>(iUuid));
	}
};

/// @endcond
