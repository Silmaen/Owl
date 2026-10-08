/**
 * @file RendererDescriptors.h
 * @author Silmaen
 * @date 20/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "DescriptorRing.h"
#include "Descriptors.h"// for TextureData
#include "FrameRing.h"
#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::renderer::gpu::vulkan {
class StorageBuffer;
}// namespace owl::renderer::gpu::vulkan

namespace owl::renderer::gpu::vulkan::internal {

/**
 * @brief
 *  Per-renderer Vulkan descriptor state. Each high-level renderer (`Renderer2D`,
 *  `RendererTilemap`, future `RendererRaycast` stripe-emission pipeline, …)
 *  constructs one instance with the exact set of bindings its shaders declare.
 *
 *  Splits the responsibilities that used to live in the global `Descriptors`
 *  singleton: layout + descriptor pool + per-frame sets + per-binding UBO
 *  storage + per-renderer texture-slot array. Removes the binding-slot
 *  collisions that caused the NVIDIA `vkCmdBindPipeline` crash on the
 *  `RendererTilemap` instanced path (one shared layout couldn't honour both
 *  `Renderer2D`'s `{0:UBO, 1:texArray}` and `RendererTilemap`'s
 *  `{0:UBO, 1:texArray, 2:UBO}` correctly).
 *
 *  A static registry indexed by renderer name lets `UniformBuffer::create`
 *  and `Texture2D` route their per-instance work back to the right
 *  descriptor block via the `iRenderer` namespacing string already threaded
 *  through the public API.
 */
class RendererDescriptors final {
public:
	/**
	 * @brief
	 *  Declaration of a single binding inside the descriptor set layout.
	 */
	struct BindingDecl {
		/// Shader binding slot.
		uint32_t binding = 0;
		/// Descriptor type (UBO, combined image sampler, …).
		VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		/// Number of descriptors at this slot (1 for UBO, N for sampler arrays).
		uint32_t count = 1;
		/// Shader stages the binding is visible to.
		VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT;
	};

	RendererDescriptors(const RendererDescriptors&) = delete;

	RendererDescriptors(RendererDescriptors&&) = delete;

	auto operator=(const RendererDescriptors&) -> RendererDescriptors& = delete;

	auto operator=(RendererDescriptors&&) -> RendererDescriptors& = delete;

	/**
	 * @brief
	 *  Construct an empty descriptor block tied to a renderer name. Registers
	 *  the instance in the static lookup so `UniformBuffer` / `Texture2D` can
	 *  resolve it from the `iRenderer` string. `init()` must still be called
	 *  with the binding declarations before the descriptor block is usable.
	 * @param[in] iRendererName Renderer namespace key (same string passed to
	 *  `UniformBuffer::create` and `StorageBuffer::create`).
	 */
	explicit RendererDescriptors(std::string iRendererName);

	/**
	 * @brief
	 *  Releases all Vulkan resources and unregisters from the static lookup.
	 */
	~RendererDescriptors();

	/**
	 * @brief
	 *  Build the descriptor set layout + pool + per-frame descriptor sets
	 *  matching `iBindings`. Idempotent — calling `init` twice releases the
	 *  previous resources first.
	 * @param[in] iBindings Binding declarations covering every descriptor
	 *  the renderer's shaders may sample. Bindings unused by a particular
	 *  shader are tolerated by Vulkan.
	 */
	void init(std::span<const BindingDecl> iBindings);

	/**
	 * @brief
	 *  Release every Vulkan resource owned by this block. Safe to call
	 *  multiple times.
	 */
	void release();

	/**
	 * @brief
	 *  Allocate per-in-flight-frame `VkBuffer` + memory + persistent map
	 *  for a uniform-block binding. Idempotent — calling twice with the
	 *  same binding releases the previous allocation first so callers may
	 *  resize.
	 * @param[in] iBinding Shader binding slot.
	 * @param[in] iSize Buffer size in bytes.
	 */
	void registerUniform(uint32_t iBinding, uint32_t iSize);

	/**
	 * @brief
	 *  Set the content of a uniform block. Draws recorded from now on read it: the block is copied into the frame
	 *  ring when the next descriptor set is written, so draws recorded before keep the previous content.
	 * @param[in] iBinding Shader binding slot (must have been registered).
	 * @param[in] iData Source bytes.
	 * @param[in] iSize Byte count.
	 */
	void setUniformData(uint32_t iBinding, const void* iData, size_t iSize);

	/**
	 * @brief
	 *  Bind an external SSBO to a storage-buffer descriptor slot. The region written into a descriptor set is
	 *  resolved at draw time (`StorageBuffer::resolve`), so a draw sees the content set before it. Idempotent —
	 *  re-binding the same slot replaces the previous buffer.
	 * @param[in] iBinding Shader binding slot (must be declared as
	 *  `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` in the layout).
	 * @param[in] iBuffer The storage buffer (non-owning).
	 */
	void bindStorageBuffer(uint32_t iBinding, StorageBuffer* iBuffer);

	/**
	 * @brief
	 *  Clear any storage-buffer binding that references `iBuffer` across every
	 *  registered renderer block. Call this when the buffer is destroyed so it is
	 *  never written into a descriptor set again.
	 * @param[in] iBuffer The storage buffer being destroyed.
	 */
	static void unbindStorageBuffer(const StorageBuffer* iBuffer);

	/**
	 * @brief
	 *  Reset the per-frame texture-bind list.
	 */
	void resetTextureBind();

	/**
	 * @brief
	 *  Append `iIndex` to the per-frame texture-bind list. Order matters —
	 *  the list maps directly to the shader's texture array slots.
	 * @param[in] iIndex Texture slot id.
	 */
	void textureBind(uint32_t iIndex);

	/**
	 * @brief
	 *  Mark the texture-bind list as complete. The descriptor set itself is written lazily by
	 *  `getDescriptorSet` at draw time, once the storage buffers of that draw are bound too.
	 */
	void commitTextureBind();

	/**
	 * @brief
	 *  Descriptor set the current draw must bind. Acquires and writes a fresh set from the
	 *  per-frame ring when the bind state, a uniform block or a streamed storage buffer changed
	 *  since the last draw, so each draw sees exactly the UBO, SSBO and textures bound before it.
	 * @param[in] iFrame Frame slot recording the draw.
	 * @return Pointer to the current descriptor set handle (null handle when the ring is exhausted).
	 */
	auto getDescriptorSet(uint32_t iFrame) -> VkDescriptorSet*;

	/**
	 * @brief
	 *  Destroy the default resources (white texture, empty uniform and storage buffers) written
	 *  into bindings nothing was bound to. Call once, before the logical device is destroyed.
	 */
	static void releaseDefaults();

	/**
	 * @brief
	 *  Pointer to the descriptor set layout (used when the renderer builds
	 *  its `VkPipelineLayout`).
	 * @return Pointer to the layout handle.
	 */
	auto getDescriptorSetLayout() -> VkDescriptorSetLayout* { return &m_layout; }

	/**
	 * @brief
	 *  Look up the descriptor block registered for a renderer name.
	 * @param[in] iName Renderer namespace key.
	 * @return Pointer to the instance, or nullptr when the name is unknown.
	 */
	static auto getForRenderer(const std::string& iName) -> RendererDescriptors*;

	/**
	 * @brief
	 *  Set the active descriptor block for the current thread. Texture
	 *  binds and per-draw descriptor-set selection route through this
	 *  pointer until cleared. Pass `nullptr` to fall back to the legacy
	 *  global `Descriptors` path.
	 * @param[in] iActive Active block, or nullptr.
	 */
	static void setActive(RendererDescriptors* iActive);

	/**
	 * @brief
	 *  Read the active descriptor block for the current thread.
	 * @return Active block, or nullptr when none is set.
	 */
	[[nodiscard]] static auto getActive() -> RendererDescriptors*;

	/**
	 * @brief
	 *  RAII helper that pushes a block onto the active slot for its scope
	 *  and restores the previous value on destruction.
	 */
	struct ScopedActive {
		/**
		 * @brief
		 *  Push the block.
		 * @param[in] iActive Block to make active (may be nullptr).
		 */
		explicit ScopedActive(RendererDescriptors* iActive);

		/**
		 * @brief
		 *  Restore the previous active block.
		 */
		~ScopedActive();

		ScopedActive(const ScopedActive&) = delete;

		ScopedActive(ScopedActive&&) = delete;

		auto operator=(const ScopedActive&) -> ScopedActive& = delete;

		auto operator=(ScopedActive&&) -> ScopedActive& = delete;

	private:
		/// Previous active block, restored at destruction.
		RendererDescriptors* m_previous = nullptr;
	};

private:
	/// Per-binding uniform block: CPU content, copied into the frame ring when it changes.
	struct UboBinding {
		std::vector<uint8_t> shadow;///< CPU content of the block.
		uint32_t size = 0;///< Block size in bytes.
		RingSlice slice;///< Frame-ring copy read by the draws.
		uint64_t sliceSerial = 0;///< Frame serial of the copy.
		bool dirty = true;///< True when the content changed since the copy.
	};

	/**
	 * @brief
	 *  External SSBO attached to a storage-buffer binding (owned by `vulkan::StorageBuffer`).
	 */
	struct StorageBinding {
		StorageBuffer* buffer{nullptr};///< Non-owning.
		uint64_t version{0};///< Version of its region written into the current set.
	};

	/// Renderer namespace key.
	std::string m_rendererName;
	/// Cached binding declarations (kept for updateDescriptor).
	std::vector<BindingDecl> m_bindings;
	/// Slot at which the texture array (if any) lives — set from m_bindings.
	uint32_t m_textureArrayBinding = std::numeric_limits<uint32_t>::max();
	/// Slot count of the texture array.
	uint32_t m_textureArrayCount = 0;
	/// Descriptor set layout for this renderer.
	VkDescriptorSetLayout m_layout{nullptr};
	/// Per-frame ring handing a distinct descriptor set to every draw.
	DescriptorRing m_ring;
	/// Frame serial of the current set.
	uint64_t m_setSerial = 0;
	/// Scratch buffer infos of a set write (kept to avoid per-draw allocations).
	std::vector<VkDescriptorBufferInfo> m_bufferInfos;
	/// Scratch image infos of a set write.
	std::vector<VkDescriptorImageInfo> m_imageInfos;
	/// Scratch writes of a set write.
	std::vector<VkWriteDescriptorSet> m_writes;
	/// Registered UBO bindings keyed by binding slot.
	std::unordered_map<uint32_t, UboBinding> m_uniformBindings;
	/// Registered SSBO bindings keyed by binding slot.
	std::unordered_map<uint32_t, StorageBinding> m_storageBindings;
	/**
	 * @brief
	 *  Per-frame texture-bind list. Stores texture ids that index into the
	 *  global `Descriptors::m_textures` table — texture storage stays global,
	 *  only the bind list (which textures fill which slots in this shader's
	 *  texture array) is per-renderer.
	 */
	std::vector<uint32_t> m_textureBind;
	/// True when the bind state or a uniform block changed since the current set was written.
	bool m_dirty = true;

	/**
	 * @brief
	 *  Write the current UBO + SSBO + texture-bind state into a freshly acquired set. Every
	 *  declared binding is written: bindings nothing was bound to get a default resource.
	 * @param[in] iSet The descriptor set to write.
	 */
	void writeDescriptor(VkDescriptorSet iSet);

	/**
	 * @brief
	 *  Check a streamed storage buffer bound here changed since the current set was written.
	 * @return True when a new set is needed.
	 */
	[[nodiscard]] auto hasStaleStorage() const -> bool;

	/**
	 * @brief
	 *  Region of a uniform block for the frame being recorded, copied into the frame ring if needed.
	 * @param[in,out] ioUbo The block.
	 * @return The buffer info, empty buffer on failure.
	 */
	[[nodiscard]] static auto resolveUniform(UboBinding& ioUbo) -> VkDescriptorBufferInfo;

	/**
	 * @brief
	 *  Image infos for the texture array: the bound textures in order, padded up to the array size
	 *  with the default white texture (also used for any unloaded texture).
	 * @param[in] iCount Number of slots of the texture array.
	 * @return False when the default texture is missing (nothing collected into `m_imageInfos`).
	 */
	auto collectImageInfos(uint32_t iCount) -> bool;
};

}// namespace owl::renderer::gpu::vulkan::internal
