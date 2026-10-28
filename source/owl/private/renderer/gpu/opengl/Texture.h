/**
 * @file Texture.h
 * @author Silmaen
 * @date 12/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/Texture.h"

#include <cstdint>
#include <filesystem>
#include <span>

namespace owl::renderer::gpu::opengl {
/**
 * @brief
 *  Specialized class managing OpenGL texture.
 */
class Texture2D final : public renderer::gpu::Texture2D {
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
	 *  Wrap a texture owned elsewhere (a framebuffer attachment); the wrapper never deletes it.
	 * @param[in] iTextureId OpenGL name of the texture.
	 * @param[in] iSpecs Size and format of the texture.
	 */
	Texture2D(uint32_t iTextureId, const Specification& iSpecs);

	/**
	 * @brief
	 *  Point a wrapper at another texture owned elsewhere (the attachment was recreated by a resize).
	 * @param[in] iTextureId OpenGL name of the texture.
	 * @param[in] iSize Size of the texture.
	 */
	void setBorrowed(uint32_t iTextureId, const math::vec2ui& iSize);

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
		return m_textureId == (dynamic_cast<const Texture2D&>(iOther)).m_textureId;
	}

	/**
	 * @brief
	 *  Get renderer id.
	 * @return The renderer ID.
	 */
	[[nodiscard]] auto getRendererId() const -> uint64_t override { return m_textureId; }

	/**
	 * @brief
	 *  Identity of the GPU texture in its backend.
	 * @return The backend identity of the texture.
	 */
	[[nodiscard]] auto getBackendId() const -> uint64_t override { return m_textureId; }

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

	/**
	 * @brief
	 *  Re-apply the GL sampler parameters for the requested filter mode.
	 * @param[in] iMode The new filter mode.
	 */
	void setFilterMode(FilterMode iMode) override;

protected:
	/**
	 * @brief
	 *  Replace the GPU image by a new one of the given specification; the old one is released safely.
	 * @param[in] iSpecs Specification of the new image.
	 */
	void reallocate(const Specification& iSpecs) override;

private:
	/// OpenGL binding.
	uint32_t m_textureId = 0;
	/// Mipmap levels allocated, fixed at creation (`Specification::getMipLevelCount`).
	uint32_t m_mipLevels = 1;
	/// Whether the texture is owned elsewhere (never deleted here).
	bool m_borrowed = false;
};

/**
 * @brief
 *  OpenGL cube texture (`GL_TEXTURE_CUBE_MAP`), sampled seamlessly across its faces.
 */
class TextureCube final : public renderer::gpu::TextureCube {
public:
	TextureCube(const TextureCube&) = delete;

	TextureCube(TextureCube&&) = delete;

	auto operator=(const TextureCube&) -> TextureCube& = delete;

	auto operator=(TextureCube&&) -> TextureCube& = delete;

	/**
	 * @brief
	 *  Allocate the six faces and their levels.
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
	 * @return True if same GL texture.
	 */
	auto operator==(const Texture& iOther) const -> bool override {
		const auto* other = dynamic_cast<const TextureCube*>(&iOther);
		return other != nullptr && m_textureId == other->m_textureId;
	}

	/**
	 * @brief
	 *  Get renderer id.
	 * @return The GL texture name.
	 */
	[[nodiscard]] auto getRendererId() const -> uint64_t override { return m_textureId; }

	/**
	 * @brief
	 *  Identity of the GPU texture in its backend.
	 * @return The GL texture name.
	 */
	[[nodiscard]] auto getBackendId() const -> uint64_t override { return m_textureId; }

	/**
	 * @brief
	 *  Get the GL texture name.
	 * @return The texture name.
	 */
	[[nodiscard]] auto getTextureId() const -> uint32_t { return m_textureId; }

	/**
	 * @brief
	 *  Bind the cube to a texture unit.
	 * @param[in] iSlot The unit.
	 */
	void bind(uint32_t iSlot) const override;

	/**
	 * @brief
	 *  Upload the six faces of level 0.
	 * @param[in] iData The faces, one after the other.
	 * @param[in] iSize Size of the data in bytes.
	 */
	void setData(void* iData, uint32_t iSize) override;

	/**
	 * @brief
	 *  Upload every level of the cube.
	 * @param[in] iLevels The levels, six faces each, as many as the cube has.
	 * @return True when the levels fit the cube.
	 */
	auto setLevels(const TextureLevels& iLevels) -> bool override;

private:
	/**
	 * @brief
	 *  Upload the six faces of one level.
	 * @param[in] iLevel The level.
	 * @param[in] iFaces The faces, one after the other.
	 */
	void uploadLevel(uint32_t iLevel, std::span<const uint8_t> iFaces) const;

	/// OpenGL texture name.
	uint32_t m_textureId = 0;
};
}// namespace owl::renderer::gpu::opengl
