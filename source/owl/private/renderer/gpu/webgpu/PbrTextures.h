/**
 * @file PbrTextures.h
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/webgpu.h"
#include "renderer/gpu/Texture.h"

#include <cstdint>
#include <span>
#include <vector>

namespace owl::renderer::gpu::webgpu {

/**
 * @brief
 *  The textures of a 3D draw in sixteen bindings, the WebGPU limit of a stage (`common/pbr.slangh`).
 *
 *  The 3D renderer binds 24 slots and 8 cubes (`Renderer3D.cpp`): 0 white, 1 to 6 maps, 7 to 12 environment levels,
 *  13 DFG, 14 and 15 area lights, 16 to 19 cascades, 20 to 23 spot shadows; cubes 0 to 3 point shadows, 4 to 7 probes.
 *  Here the table keeps slots 0 to 6, the environment levels become the mips of slot 7 and 13 to 15 move to 8 to 10;
 *  the cascades are the layers of one array, the spot shadows and the faces of the point shadows of a second, the faces
 *  and mips of the probes of a third. A map smaller than its array lies in the corner of its layer: the shaders read it
 *  at its own texel coordinates. The arrays are copied once per frame, the environment once per change of its levels.
 */
class PbrTextures {
public:
	/**
	 * @brief
	 *  Appends the group 1 entries of a 3D draw.
	 * @param[in] iTextures The 2D slots the renderer bound.
	 * @param[in] iCubes The cubes it bound.
	 * @param[in] iFrame The frame: the arrays are copied again in a new one.
	 * @param[out] oEntries The entries.
	 */
	void appendEntries(std::span<const shared<renderer::gpu::Texture2D>> iTextures,
					   std::span<const shared<renderer::gpu::TextureCube>> iCubes, uint64_t iFrame,
					   std::vector<wgpu::BindGroupEntry>& oEntries);

	/// Table slots of a 3D stage.
	static constexpr uint32_t g_tableSlots = 12;
	/// Binding of the cascade array in group 1.
	static constexpr uint32_t g_cascadeBinding = 32;
	/// Binding of the local shadow array.
	static constexpr uint32_t g_localBinding = 33;
	/// Binding of the probe array.
	static constexpr uint32_t g_probeBinding = 34;
	/// Binding of the probe sampler.
	static constexpr uint32_t g_probeSamplerBinding = 41;
	/// Binding of the uniform buffer of the cube layers (`WebGpuCubeLayers`).
	static constexpr uint32_t g_cubeLayersBinding = 42;

private:
	/// One texture array and the textures copied into it.
	struct Array {
		/// The array.
		wgpu::Texture texture;
		/// Its view.
		wgpu::TextureView view;
		/// The textures last copied, one per layer source.
		std::vector<WGPUTexture> sources;
	};

	/**
	 * @brief
	 *  Makes sure an array holds a size, layer count, mip count and format, else makes it again.
	 * @param[in,out] ioArray The array.
	 * @param[in] iWidth Width of the layers.
	 * @param[in] iHeight Height of the layers.
	 * @param[in] iLayers Layer count.
	 * @param[in] iLevels Mip count.
	 * @param[in] iFormat Format.
	 */
	static void ensure(Array& ioArray, uint32_t iWidth, uint32_t iHeight, uint32_t iLayers, uint32_t iLevels,
					   wgpu::TextureFormat iFormat);

	/**
	 * @brief
	 *  Builds the environment texture from its levels when they changed.
	 * @param[in] iLevels The six level slots.
	 */
	void packEnvironment(std::span<const shared<renderer::gpu::Texture2D>> iLevels);

	/**
	 * @brief
	 *  Copies the shadow maps and the probes into their arrays.
	 * @param[in] iTextures The 2D slots.
	 * @param[in] iCubes The cubes.
	 */
	void packArrays(std::span<const shared<renderer::gpu::Texture2D>> iTextures,
					std::span<const shared<renderer::gpu::TextureCube>> iCubes);

	/// The cascades.
	Array m_cascades;
	/// The spot shadows, then six layers per point shadow.
	Array m_local;
	/// Six layers per probe, with their mips.
	Array m_probes;
	/// The environment levels as mips.
	Array m_environment;
	/// Sampler of the environment.
	wgpu::Sampler m_environmentSampler;
	/// Sampler of the probes.
	wgpu::Sampler m_probeSampler;
	/// `WebGpuCubeLayers` of the shaders.
	wgpu::Buffer m_cubeLayers;
	/// Frame of the last copy of the arrays.
	uint64_t m_frame = ~uint64_t{0};
	/// Textures and cubes of the last copy of the arrays.
	std::vector<const void*> m_packed;
};

}// namespace owl::renderer::gpu::webgpu
