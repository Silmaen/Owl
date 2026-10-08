/**
 * @file BindingTable.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace owl::renderer::gpu::opengl {

/**
 * @brief
 *  OpenGL side of a renderer block (`RendererDescriptors`): the uniform buffers and sampled textures one renderer
 *  binds, applied again before its draws whenever another renderer changed the global GL bindings since.
 *
 * OpenGL bindings are global: `Renderer2D`, `RendererTilemap` and `Renderer3D` all use uniform binding 0 and texture
 * units from 0. Each bind made while a block is active (`RendererDescriptors::ScopedActive`) is applied at once and
 * recorded in the block; a draw re-applies its block only when the GL state belongs to another one. Storage buffers
 * are not recorded: every renderer binds them right before each draw, as on Vulkan.
 */
class BindingTable final {
public:
	BindingTable() = default;

	~BindingTable() = default;

	BindingTable(const BindingTable&) = delete;

	BindingTable(BindingTable&&) = delete;

	auto operator=(const BindingTable&) -> BindingTable& = delete;

	auto operator=(BindingTable&&) -> BindingTable& = delete;

	/**
	 * @brief
	 *  Get the table of a renderer block, created on first use.
	 * @param[in] iRenderer Renderer key (the `RendererDescriptors` name).
	 * @return The table.
	 */
	static auto getForRenderer(const std::string& iRenderer) -> BindingTable&;

	/**
	 * @brief
	 *  Destroy the table of a renderer block (no-op when it does not exist).
	 * @param[in] iRenderer Renderer key.
	 */
	static void release(const std::string& iRenderer);

	/**
	 * @brief
	 *  Destroy every table.
	 */
	static void releaseAll();

	/**
	 * @brief
	 *  Get the active table (the block of the current `ScopedActive`).
	 * @return The active table, or `nullptr` outside any block.
	 */
	[[nodiscard]] static auto getActive() -> BindingTable*;

	/**
	 * @brief
	 *  Make a table the active one.
	 * @param[in] iTable The table, or `nullptr`.
	 */
	static void setActive(BindingTable* iTable);

	/**
	 * @brief
	 *  Record a uniform buffer bound to `iBinding` by the active block (the caller already bound it).
	 * @param[in] iBinding Uniform binding point.
	 * @param[in] iBuffer GL buffer name.
	 */
	static void recordUniformBuffer(uint32_t iBinding, uint32_t iBuffer);

	/**
	 * @brief
	 *  Record a texture bound to unit `iSlot` by the active block (the caller already bound it).
	 * @param[in] iSlot Texture unit.
	 * @param[in] iTexture GL texture name.
	 */
	static void recordTexture(uint32_t iSlot, uint32_t iTexture);

	/**
	 * @brief
	 *  Replace the textures of the active block and bind them (units past the span keep their texture).
	 * @param[in] iTextures GL texture names in unit order (0 unbinds the unit).
	 */
	static void bindTextures(std::span<const uint32_t> iTextures);

	/**
	 * @brief
	 *  Forget a deleted buffer in every table, so its name is never bound again.
	 * @param[in] iBuffer GL buffer name.
	 */
	static void forgetBuffer(uint32_t iBuffer);

	/**
	 * @brief
	 *  Forget a deleted texture in every table.
	 * @param[in] iTexture GL texture name.
	 */
	static void forgetTexture(uint32_t iTexture);

	/**
	 * @brief
	 *  Before a draw: apply the active table when the GL bindings belong to another table.
	 */
	static void applyActive();

private:
	/**
	 * @brief
	 *  Bind every recorded uniform buffer and texture of this table.
	 */
	void apply() const;

	/**
	 * @brief
	 *  Track that a bind of this table reached the GL state.
	 */
	void touch() const;

	/// Uniform buffer name per binding point (0 = none).
	std::vector<uint32_t> m_uniformBuffers;
	/// Texture name per unit (0 = none).
	std::vector<uint32_t> m_textures;
};

}// namespace owl::renderer::gpu::opengl
