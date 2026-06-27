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
#include <vulkan/vulkan.h>

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
	 *  Copy `iData` into the host-mapped uniform buffer of the current
	 *  in-flight frame for `iBinding`.
	 * @param[in] iBinding Shader binding slot (must have been registered).
	 * @param[in] iData Source bytes.
	 * @param[in] iSize Byte count.
	 */
	void setUniformData(uint32_t iBinding, const void* iData, size_t iSize) const;

	/**
	 * @brief
	 *  Bind an external SSBO to a storage-buffer descriptor slot. Used by
	 *  `vulkan::StorageBuffer::bind` to attach a per-instance SSBO that lives
	 *  outside this descriptor block (the SSBO owns its `VkBuffer`; we only
	 *  retain the handle so `updateDescriptor` can write it into the per-frame
	 *  descriptor sets). Idempotent — re-binding the same slot replaces the
	 *  previous handle.
	 * @param[in] iBinding Shader binding slot (must be declared as
	 *  `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` in the layout).
	 * @param[in] iBuffer Backing `VkBuffer` (non-owning).
	 * @param[in] iSize Buffer size in bytes (`VK_WHOLE_SIZE` is also accepted).
	 */
	void bindStorageBuffer(uint32_t iBinding, VkBuffer iBuffer, VkDeviceSize iSize);

	/**
	 * @brief
	 *  Clear any storage-buffer binding that references `iBuffer` across every
	 *  registered renderer block. Call this when the backing `VkBuffer` is
	 *  destroyed so stale handles are never written into a descriptor set.
	 * @param[in] iBuffer The `VkBuffer` being destroyed.
	 */
	static void unbindStorageBuffer(VkBuffer iBuffer);

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
	 *  Refresh the per-frame descriptor set with the current UBO + texture
	 *  bind state. Call after every `beginScene` / `setUniformData` / batch
	 *  rebuild for the active frame index.
	 * @param[in] iCurrentFrame Active in-flight frame index.
	 */
	void commitTextureBind(size_t iCurrentFrame);

	/**
	 * @brief
	 *  Stable pointer to the descriptor set the current draw must bind (the set
	 *  acquired by the most recent `commitTextureBind`). The frame index is no
	 *  longer used — each draw owns a distinct set from the fence-recycled ring.
	 * @param[in] iFrame Unused (kept for call-site compatibility).
	 * @return Pointer to the current descriptor set handle.
	 */
	auto getDescriptorSet(uint32_t iFrame) -> VkDescriptorSet*;

	/**
	 * @brief
	 *  Pointer to the descriptor set layout (used when the renderer builds
	 *  its `VkPipelineLayout`).
	 * @return Pointer to the layout handle.
	 */
	auto getDescriptorSetLayout() -> VkDescriptorSetLayout* { return &m_layout; }

	/**
	 * @brief
	 *  Tag the descriptor sets every registered renderer acquired during the
	 *  just-submitted batch with that batch's fence, so they are recycled only
	 *  once the GPU is done with them. Call right after `vkQueueSubmit`.
	 * @param[in] iFence The fence the submitted command buffer signals.
	 */
	static void notifySubmitAll(VkFence iFence);

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
	/// Per-binding uniform-buffer storage (one VkBuffer + map per frame).
	struct UboBinding {
		std::vector<VkBuffer> buffers;///< Per in-flight frame.
		std::vector<VkDeviceMemory> memory;///< Backing memory per frame.
		std::vector<void*> mapped;///< Persistent host map per frame.
		uint32_t size = 0;///< Buffer size in bytes.
	};

	/**
	 * @brief
	 *  External SSBO handle attached to a storage-buffer binding. The buffer
	 *  is owned by `vulkan::StorageBuffer`; we only need the handle to write
	 *  it into the descriptor set.
	 */
	struct StorageBinding {
		VkBuffer buffer{nullptr};///< Non-owning handle.
		VkDeviceSize size{0};///< Range to bind (VK_WHOLE_SIZE accepted).
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
	/// Fence-recycled ring handing a distinct descriptor set to every draw.
	DescriptorRing m_ring;
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

	/**
	 * @brief
	 *  Write the current UBO + SSBO + texture-bind state into a freshly acquired set.
	 * @param[in] iSet The descriptor set to write.
	 * @param[in] iFrame Active in-flight frame index (selects the per-frame UBO buffers).
	 */
	void writeDescriptor(VkDescriptorSet iSet, size_t iFrame);
};

}// namespace owl::renderer::gpu::vulkan::internal
