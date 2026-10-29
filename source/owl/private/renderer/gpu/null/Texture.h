/**
 * @file Texture.h
 * @author Silmaen
 * @date 30/07/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/Texture.h"

#include <cstdint>
#include <filesystem>

namespace owl::renderer::gpu::null {
/**
 * @brief
 *  Specialized class managing null texture.
 */
class OWL_API Texture2D final : public renderer::gpu::Texture2D {
public:
	Texture2D(const Texture2D&) = default;

	Texture2D(Texture2D&&) = default;

	auto operator=(const Texture2D&) -> Texture2D& = default;

	auto operator=(Texture2D&&) -> Texture2D& = default;

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iPath path to the texture image file.
	 */
	explicit Texture2D(std::filesystem::path iPath);

	/**
	 * @brief
	 *  Constructor by specifications.
	 * @param[in] iSpecs The texture's specification.
	 */
	explicit Texture2D(const Specification& iSpecs);

	/**
	 * @brief
	 *  Destructor.
	 */
	~Texture2D() override;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther Other texture to compare.
	 * @return True if same.
	 */
	auto operator==(const Texture& iOther) const -> bool override {
		return m_rendererId == (dynamic_cast<const Texture2D&>(iOther)).m_rendererId;
	}

	/**
	 * @brief
	 *  Get renderer id.
	 * @return The renderer ID.
	 */
	[[nodiscard]] auto getRendererId() const -> uint64_t override { return m_rendererId; }

	/**
	 * @brief
	 *  Identity of the GPU texture in its backend.
	 * @return The backend identity of the texture.
	 */
	[[nodiscard]] auto getBackendId() const -> uint64_t override { return m_rendererId; }

	/**
	 * @brief
	 *  Activate the texture in the GPU.
	 * @param[in] iSlot Slot into put the texture.
	 */
	void bind(uint32_t iSlot) const override;

	/**
	 * @brief
	 *  Define the texture data.
	 * @param[in] iData Raw data.
	 * @param[in] iSize Size of the data.
	 */
	void setData(void* iData, uint32_t iSize) override;

	/**
	 * @brief
	 *  Replace the texture by levels given in full (mipmaps included, no mipmap generated): size and format follow.
	 * @param[in] iLevels The levels, finest first, in a format the backend supports.
	 * @return True when the levels were uploaded.
	 */
	auto setLevels(const TextureLevels& iLevels) -> bool override;

private:
	/// OpenGL binding.
	uint64_t m_rendererId = 0;
};

/**
 * @brief
 *  Cube texture of the null backend: keeps its specification, holds no pixel.
 */
class OWL_API TextureCube final : public renderer::gpu::TextureCube {
public:
	TextureCube(const TextureCube&) = default;

	TextureCube(TextureCube&&) = default;

	auto operator=(const TextureCube&) -> TextureCube& = default;

	auto operator=(TextureCube&&) -> TextureCube& = default;

	/**
	 * @brief
	 *  Constructor by specifications.
	 * @param[in] iSpecs The cube's specification.
	 */
	explicit TextureCube(const Specification& iSpecs);

	/**
	 * @brief
	 *  Destructor.
	 */
	~TextureCube() override;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther Other texture to compare.
	 * @return True if same object.
	 */
	auto operator==(const Texture& iOther) const -> bool override { return this == &iOther; }

	/**
	 * @brief
	 *  Get renderer id.
	 * @return 0, nothing is allocated.
	 */
	[[nodiscard]] auto getRendererId() const -> uint64_t override { return 0; }

	/**
	 * @brief
	 *  Identity of the GPU texture in its backend.
	 * @return 0, nothing is allocated.
	 */
	[[nodiscard]] auto getBackendId() const -> uint64_t override { return 0; }

	/**
	 * @brief
	 *  Activate the texture in the GPU (no-op).
	 * @param[in] iSlot Slot into put the texture.
	 */
	void bind(uint32_t iSlot) const override;

	/**
	 * @brief
	 *  Check the size of the six faces of level 0.
	 * @param[in] iData The pixels.
	 * @param[in] iSize Size of the data in bytes.
	 */
	void setData(void* iData, uint32_t iSize) override;

	/**
	 * @brief
	 *  Keep the level count of the given levels.
	 * @param[in] iLevels The levels, six faces each.
	 * @return True when the levels fit the cube.
	 */
	auto setLevels(const TextureLevels& iLevels) -> bool override;
};
}// namespace owl::renderer::gpu::null
