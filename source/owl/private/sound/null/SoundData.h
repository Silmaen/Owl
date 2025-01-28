/**
 * @file SoundData.h
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "sound/SoundData.h"

namespace owl::sound::null {

/**
 * @brief Class representing what's required for playing sound.
 */
class OWL_API SoundData final : public sound::SoundData {
public:
	/**
	 * @brief Default constructor.
	 * @param[in] iSpecifications The specifications.
	 */
	explicit SoundData(const Specification& iSpecifications);
	/**
	 * @brief Default destructor.
	 */
	~SoundData() override;
	/**
	 * @brief Default copy constructor.
	 */
	SoundData(const SoundData&) = default;
	/**
	 * @brief Default move constructor.
	 */
	SoundData(SoundData&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const SoundData&) -> SoundData& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(SoundData&&) -> SoundData& = default;
	/**
	 * @brief Get the system id.
	 * @return The system id.
	 */
	auto getSystemId() const -> uint64_t override { return 0; }
};

}// namespace owl::sound::null
