/**
 * @file ExtraDataRegisterScope.h
 * @author Silmaen
 * @date 19/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include <functional>

namespace owl::data::extradata {

/**
 * @brief
 *  Scope class for extra data registration.
 */
class OWL_API ExtraDataRegisterScope {
public:
	/**
	 * @brief
	 *  Default constructor. Resolves to failed registration (isRegistered() returns false).
	 */
	ExtraDataRegisterScope() = default;

	/**
	 * @brief
	 *  Constructor by move.
	 */
	ExtraDataRegisterScope(ExtraDataRegisterScope&&) noexcept = default;
	/**
	 * @brief
	 *  Constructor by copy deleted.
	 */
	ExtraDataRegisterScope(const ExtraDataRegisterScope&) = delete;
	/**
	 * @brief
	 *  Assignment by move.
	 */
	auto operator=(ExtraDataRegisterScope&&) noexcept -> ExtraDataRegisterScope& = default;
	/**
	 * @brief
	 *  Assignment by copy deleted.
	 */
	auto operator=(const ExtraDataRegisterScope&) -> ExtraDataRegisterScope& = delete;
	/**
	 * @brief
	 *  Check if the registration function did succeed.
	 */
	[[nodiscard]] auto isRegistered() const -> bool { return m_isRegistered; }

	/**
	 * @brief
	 *  Constructor. Resolves to successful registration (isRegistered() returns true).
	 * @param[in] iUnregisterFct Callback that will unregister an extra data.
	 */
	explicit ExtraDataRegisterScope(std::function<void()> iUnregisterFct)
		: m_isRegistered(true), m_unregisterFct(std::move(iUnregisterFct)) {}

	/**
	 * @brief
	 *  Destructor. Unregister the extra data.
	 */
	~ExtraDataRegisterScope() {
		if (m_unregisterFct)
			m_unregisterFct();
	}

private:
	/// True if the registration succeeded.
	bool m_isRegistered{false};
	/// Unregister function.
	std::function<void()> m_unregisterFct;
};

}// namespace owl::data::extradata
