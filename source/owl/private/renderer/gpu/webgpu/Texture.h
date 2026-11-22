/**
 * @file Texture.h
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/webgpu.h"
#include "renderer/gpu/Texture.h"

#include <cstdint>
#include <filesystem>

namespace owl::renderer::gpu::webgpu {

/**
 * @brief
 *  WebGPU 2D texture with its sampler.
 */
class OWL_API Texture2D final : public renderer::gpu::Texture2D {
public:
	Texture2D(const Texture2D&) = delete;
	Texture2D(Texture2D&&) = delete;
	auto operator=(const Texture2D&) -> Texture2D& = delete;
	auto operator=(Texture2D&&) -> Texture2D& = delete;

	/**
	 * @brief
	 *  Loads an image file.
	 * @param[in] iPath Path of the image.
	 */
	explicit Texture2D(std::filesystem::path iPath);

	/**
	 * @brief
	 *  Makes an empty texture.
	 * @param[in] iSpecs The specification.
	 */
	explicit Texture2D(const Specification& iSpecs);

	/**
	 * @brief
	 *  Wraps a texture made elsewhere (a framebuffer attachment), sampled without filtering for an integer one.
	 * @param[in] iTexture The texture.
	 * @param[in] iSpecs Its specification.
	 */
	Texture2D(wgpu::Texture iTexture, const Specification& iSpecs);

	/**
	 * @brief
	 *  Destructor.
	 */
	~Texture2D() override = default;

	/**
	 * @brief
	 *  Compares two textures.
	 * @param[in] iOther The other texture.
	 * @return True for the same GPU texture.
	 */
	auto operator==(const Texture& iOther) const -> bool override { return m_id == iOther.getBackendId(); }

	/**
	 * @brief
	 *  Identifier of the texture.
	 * @return The identifier.
	 */
	[[nodiscard]] auto getRendererId() const -> uint64_t override { return m_id; }

	/**
	 * @brief
	 *  Identity of the GPU texture.
	 * @return The identifier.
	 */
	[[nodiscard]] auto getBackendId() const -> uint64_t override { return m_id; }

	/**
	 * @brief
	 *  Binds the texture at a slot for the next draws.
	 * @param[in] iSlot The slot.
	 */
	void bind(uint32_t iSlot) const override;

	/**
	 * @brief
	 *  Uploads the pixels of the first level.
	 * @param[in] iData The pixels, in the format of the texture.
	 * @param[in] iSize Their size in bytes.
	 */
	void setData(void* iData, uint32_t iSize) override;

	/**
	 * @brief
	 *  Uploads every level.
	 * @param[in] iLevels The levels.
	 * @return True when uploaded.
	 */
	auto setLevels(const TextureLevels& iLevels) -> bool override;

	/**
	 * @brief
	 *  Changes the filtering.
	 * @param[in] iMode The filter mode.
	 */
	void setFilterMode(FilterMode iMode) override;

	/**
	 * @brief
	 *  Makes the texture again for another specification.
	 * @param[in] iSpecs The specification.
	 */
	void reallocate(const Specification& iSpecs) override;

	/**
	 * @brief
	 *  View of the texture.
	 * @return The view.
	 */
	[[nodiscard]] auto getView() const -> const wgpu::TextureView& { return m_view; }

	/**
	 * @brief
	 *  Sampler of the texture.
	 * @return The sampler.
	 */
	[[nodiscard]] auto getSampler() const -> const wgpu::Sampler& { return m_sampler; }

	/**
	 * @brief
	 *  The GPU texture.
	 * @return The texture.
	 */
	[[nodiscard]] auto getTexture() const -> const wgpu::Texture& { return m_texture; }

private:
	/**
	 * @brief
	 *  Makes the GPU texture, its view and its sampler from the specification.
	 */
	void create();

	/**
	 * @brief
	 *  Makes the sampler from the filter mode.
	 */
	void createSampler();

	/// The GPU texture.
	wgpu::Texture m_texture;
	/// Its view.
	wgpu::TextureView m_view;
	/// Its sampler.
	wgpu::Sampler m_sampler;
	/// Unique identifier.
	uint64_t m_id = 0;
};

/**
 * @brief
 *  WebGPU cube texture: six layers seen as a cube, sampled through the cube tables of the shaders.
 */
class OWL_API TextureCube final : public renderer::gpu::TextureCube {
public:
	TextureCube(const TextureCube&) = delete;
	TextureCube(TextureCube&&) = delete;
	auto operator=(const TextureCube&) -> TextureCube& = delete;
	auto operator=(TextureCube&&) -> TextureCube& = delete;

	/**
	 * @brief
	 *  Makes a cube, every face black.
	 * @param[in] iSpecs Face size, format and level count.
	 */
	explicit TextureCube(const Specification& iSpecs);

	/**
	 * @brief
	 *  Destructor.
	 */
	~TextureCube() override = default;

	/**
	 * @brief
	 *  Compares with another texture.
	 * @param[in] iOther The other texture.
	 * @return True for the same cube.
	 */
	auto operator==(const Texture& iOther) const -> bool override { return m_id == iOther.getBackendId(); }

	/**
	 * @brief
	 *  Identifier of the cube.
	 * @return The identifier.
	 */
	[[nodiscard]] auto getRendererId() const -> uint64_t override { return m_id; }

	/**
	 * @brief
	 *  Identifier of the cube.
	 * @return The identifier.
	 */
	[[nodiscard]] auto getBackendId() const -> uint64_t override { return m_id; }

	/**
	 * @brief
	 *  A cube has no 2D slot: it is bound with the cubes of a draw.
	 * @param[in] iSlot The slot.
	 */
	void bind(uint32_t iSlot) const override;

	/**
	 * @brief
	 *  Writes level 0 of the six faces.
	 * @param[in] iData The faces, one after the other.
	 * @param[in] iSize Their size in bytes.
	 */
	void setData(void* iData, uint32_t iSize) override;

	/**
	 * @brief
	 *  Writes every level of the six faces.
	 * @param[in] iLevels The levels.
	 * @return True when they match the cube.
	 */
	auto setLevels(const TextureLevels& iLevels) -> bool override;

	/**
	 * @brief
	 *  The GPU texture, six layers.
	 * @return The texture.
	 */
	[[nodiscard]] auto getTexture() const -> const wgpu::Texture& { return m_texture; }

	/**
	 * @brief
	 *  The cube view.
	 * @return The view.
	 */
	[[nodiscard]] auto getView() const -> const wgpu::TextureView& { return m_view; }

private:
	/// The GPU texture.
	wgpu::Texture m_texture;
	/// Its cube view.
	wgpu::TextureView m_view;
	/// Unique identifier.
	uint64_t m_id = 0;
};

}// namespace owl::renderer::gpu::webgpu
