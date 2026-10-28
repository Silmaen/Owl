/**
 * @file Texture.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "internal/Descriptors.h"
#include "renderer/gpu/Texture.h"

#include <cstdint>
#include <filesystem>
#include <span>

namespace owl::renderer::gpu::vulkan {
/**
 * @brief
 *  Specialized class managing Vulkan texture.
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
	 *  Wrap an image view owned elsewhere (a framebuffer attachment) in a texture slot; the wrapper never destroys it.
	 * @param[in] iView The image view, in the shader-read layout when draws sample it.
	 * @param[in] iSampler The sampler.
	 * @param[in] iSize Size of the image.
	 */
	Texture2D(VkImageView iView, VkSampler iSampler, const math::vec2ui& iSize);

	/**
	 * @brief
	 *  Point a wrapper at another view owned elsewhere (the attachment was recreated by a resize).
	 * @param[in] iView The image view.
	 * @param[in] iSampler The sampler.
	 * @param[in] iSize Size of the image.
	 */
	void setBorrowed(VkImageView iView, VkSampler iSampler, const math::vec2ui& iSize);

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
	auto operator==(const Texture& iOther) const -> bool override;

	/**
	 * @brief
	 *  Get renderer id.
	 * @return The renderer ID.
	 */
	[[nodiscard]] auto getRendererId() const -> uint64_t override;

	/**
	 * @brief
	 *  Identity of the GPU texture in its backend.
	 * @return The backend identity of the texture.
	 */
	[[nodiscard]] auto getBackendId() const -> uint64_t override { return m_textureId; }

	/**
	 * @brief
	 *  Get the identifier of the texture in the descriptor registry.
	 * @return The registry identifier (0 when the texture was never registered).
	 */
	[[nodiscard]] auto getTextureId() const -> uint32_t { return m_textureId; }

	/**
	 * @brief
	 *  Put the texture in one sampler slot of the active renderer block (the other slots are kept).
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
	 *  Switch the sampler between linear and nearest filtering.
	 * Waits for the device to be idle before replacing the sampler: meant for load time and editor actions.
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
	/// Vulkan-side texture identifier (registered with the descriptor pool / bindless table).
	uint32_t m_textureId = 0;
};

/**
 * @brief
 *  Vulkan cube texture: a cube-compatible image of six layers, kept in `SHADER_READ_ONLY_OPTIMAL` between copies,
 *  cleared to zero at creation.
 */
class OWL_API TextureCube final : public renderer::gpu::TextureCube {
public:
	TextureCube(const TextureCube&) = delete;

	TextureCube(TextureCube&&) = delete;

	auto operator=(const TextureCube&) -> TextureCube& = delete;

	auto operator=(TextureCube&&) -> TextureCube& = delete;

	/**
	 * @brief
	 *  Allocate the image, its cube view and its sampler.
	 * @param[in] iSpecs The cube's specification.
	 */
	explicit TextureCube(const Specification& iSpecs);

	/**
	 * @brief
	 *  Destructor: the image is released once the frames using it are done.
	 */
	~TextureCube() override;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther Other texture to compare.
	 * @return True if same registered texture.
	 */
	auto operator==(const Texture& iOther) const -> bool override;

	/**
	 * @brief
	 *  Get renderer id.
	 * @return The registered texture id.
	 */
	[[nodiscard]] auto getRendererId() const -> uint64_t override { return m_textureId; }

	/**
	 * @brief
	 *  Identity of the GPU texture in its backend.
	 * @return The registered texture id.
	 */
	[[nodiscard]] auto getBackendId() const -> uint64_t override { return m_textureId; }

	/**
	 * @brief
	 *  Get the id of the texture in the descriptor registry.
	 * @return The texture id.
	 */
	[[nodiscard]] auto getTextureId() const -> uint32_t { return m_textureId; }

	/**
	 * @brief
	 *  Bind to a 2D slot: a cube is bound with `RenderCommand::bindTextures(textures, cubes)`, so this warns.
	 * @param[in] iSlot The slot.
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
	 *  Record the upload of levels from memory.
	 * @param[in] iLevels Data of each level from level 0, six faces each.
	 * @return True when the upload is recorded.
	 */
	auto upload(std::span<const std::span<const uint8_t>> iLevels) const -> bool;

	/// Id of the texture in the descriptor registry.
	uint32_t m_textureId = 0;
};

}// namespace owl::renderer::gpu::vulkan
