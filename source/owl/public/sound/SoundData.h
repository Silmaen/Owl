/**
 * @file SoundData.h
 * @author Silmaen
 * @date 11/5/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace owl::sound {
/**
 * @brief
 *  Abstract class representing what's required for playing sound.
 */
class OWL_API SoundData {
public:
	/**
	 * @brief
	 *  Definition of a sound.
	 */
	struct Specification {
		/// Sound's name, used to find the file in the assets.
		std::filesystem::path file;
		/// First loaded second of the file.
		float trimStart{0.f};
		/// Last loaded second of the file, 0 for its end.
		float trimEnd{0.f};
		/// Start of the looped part, in seconds from the start of the file, 0 with `loopEnd` 0 to loop it all.
		float loopStart{0.f};
		/// End of the looped part, in seconds from the start of the file, 0 for the end of the loaded part.
		float loopEnd{0.f};
	};

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iSpec the sound specifications.
	 */
	explicit SoundData(const Specification& iSpec);

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iSpec the sound specifications.
	 */
	explicit SoundData(Specification&& iSpec);

	/**
	 * @brief
	 *  Default destructor.
	 */
	virtual ~SoundData();

	/**
	 * @brief
	 *  Default copy constructor.
	 */
	SoundData(const SoundData&) = default;

	/**
	 * @brief
	 *  Default move constructor.
	 */
	SoundData(SoundData&&) = default;

	/**
	 * @brief
	 *  Default copy affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(const SoundData&) -> SoundData& = default;

	/**
	 * @brief
	 *  Default move affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(SoundData&&) -> SoundData& = default;

	/**
	 * @brief
	 *  Get the system id.
	 * @return The system id.
	 */
	[[nodiscard]] virtual auto getSystemId() const -> uint64_t = 0;

	/**
	 * @brief
	 *  Get the file the sound was read from.
	 * @return The file, empty for a sound made in memory.
	 */
	[[nodiscard]] auto getFile() const -> const std::filesystem::path& { return m_specification.file; }

	/**
	 * @brief
	 *  Create a new sound data buffer base on specifications.
	 * @param[in] iSpec The specifications of the sound.
	 * @return Pointer to the created buffer.
	 */
	static auto create(const Specification& iSpec) -> shared<SoundData>;

	/**
	 * @brief
	 *  Create a new sound data buffer base on a file.
	 * @param[in] iPath The file to load as data.
	 * @return Pointer to the created buffer.
	 */
	static auto create(const std::filesystem::path& iPath) -> shared<SoundData>;

	/**
	 * @brief
	 *  Defines the possible extensions type for this dta.
	 * @return List of supported extensions.
	 */
	static auto extension() -> std::vector<std::string> { return {".wav", ".ogg", ".flac", ".mp3"}; }

protected:
	/// The sound's data specification.
	Specification m_specification;
};

}// namespace owl::sound
