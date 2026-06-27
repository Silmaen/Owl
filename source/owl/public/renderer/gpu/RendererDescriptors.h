/**
 * @file RendererDescriptors.h
 * @author Silmaen
 * @date 20/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl::renderer::gpu {

/**
 * @brief
 *  Descriptor binding kind used in a `BindingDecl`.
 */
enum struct BindingType : uint8_t {
	/// Uniform buffer (small read-only host-mapped block).
	UniformBuffer,
	/// Combined image+sampler (array allowed via `count`).
	CombinedImageSampler,
	/// Shader storage buffer (read-write, larger than UBO).
	StorageBuffer,
};

/**
 * @brief
 *  Shader stage flags. Bit-field so a binding can be visible to multiple
 *  stages at once (e.g. a UBO read by both vertex and fragment shaders).
 */
struct ShaderStage {
	/// Visible in the vertex stage.
	static constexpr uint32_t Vertex = 1U << 0U;
	/// Visible in the fragment stage.
	static constexpr uint32_t Fragment = 1U << 1U;
	/// Visible in the compute stage.
	static constexpr uint32_t Compute = 1U << 2U;
	/// Convenience: vertex + fragment.
	static constexpr uint32_t VertexFragment = Vertex | Fragment;
};

/**
 * @brief
 *  Declaration of a single shader binding inside a renderer's descriptor set.
 *  Backend-neutral; the implementation translates it to `VkDescriptorSetLayoutBinding`
 *  (or equivalent) at `RendererDescriptors::declare` time.
 */
struct BindingDecl {
	/// Shader binding slot (matches `[[vk::binding(N)]]` in Slang).
	uint32_t binding = 0;
	/// Descriptor type.
	BindingType type = BindingType::UniformBuffer;
	/// Number of descriptors at this slot (1 for UBO, N for sampler arrays).
	uint32_t count = 1;
	/// Bit-field of `ShaderStage` values.
	uint32_t stages = ShaderStage::Vertex;
};

/**
 * @brief
 *  Per-renderer descriptor-set declaration API. Each high-level renderer
 *  (`Renderer2D`, `RendererTilemap`, future `RendererRaycast` stripe-emission)
 *  calls `declare()` once at init with the union of bindings used by all of
 *  its shaders, then wraps its `beginScene`/flush in a `ScopedActive` so the
 *  Vulkan backend routes per-draw descriptor-set selection, pipeline-layout
 *  creation and `Texture2D::bind` to the right per-renderer block.
 *
 *  Null and OpenGL backends no-op every call — OpenGL has no descriptor-set
 *  concept, and the headless Null backend has no GPU state to track.
 */
class OWL_API RendererDescriptors {
public:
	/**
	 * @brief
	 *  Allocate the per-renderer descriptor block (layout + pool + per-frame
	 *  sets) for the named renderer. Must be called before any
	 *  `UniformBuffer::create`, `Texture2D` allocation, or draw routed
	 *  through `iRenderer`. Idempotent — calling twice for the same name
	 *  releases the previous block first.
	 * @param[in] iRenderer Renderer namespace key.
	 * @param[in] iBindings Binding declarations covering every slot the
	 *  renderer's shaders may sample.
	 */
	static void declare(const std::string& iRenderer, std::span<const BindingDecl> iBindings);

	/**
	 * @brief
	 *  Release the per-renderer descriptor block. Safe to call when nothing
	 *  is registered. Called by each renderer's `shutdown()`.
	 * @param[in] iRenderer Renderer namespace key.
	 */
	static void release(const std::string& iRenderer);

	/**
	 * @brief
	 *  Release every per-renderer descriptor block at once.
	 *
	 * A last-resort teardown hook: the blocks otherwise live in a process-static
	 * map that would be destroyed only at program exit — after the Vulkan device
	 * is gone — leaking their layouts / pools. Called at device teardown (while
	 * the device is still valid) so no block can outlive it, regardless of
	 * per-renderer `shutdown()` ordering. Safe to call when nothing is registered.
	 */
	static void releaseAll();

	/**
	 * @brief
	 *  RAII guard that makes `iRenderer`'s descriptor block the active one
	 *  on the current thread. While alive, `Texture2D::bind`, pipeline-layout
	 *  creation and the per-draw descriptor-set bind route through this
	 *  renderer's block. The previous active is restored on destruction so
	 *  guards nest cleanly.
	 */
	class OWL_API ScopedActive final {
	public:
		/**
		 * @brief
		 *  Push the renderer's block onto the active slot.
		 * @param[in] iRenderer Renderer namespace key.
		 */
		explicit ScopedActive(const std::string& iRenderer);

		/**
		 * @brief
		 *  Restore the previously active block.
		 */
		~ScopedActive();

		ScopedActive(const ScopedActive&) = delete;

		ScopedActive(ScopedActive&&) = delete;

		auto operator=(const ScopedActive&) -> ScopedActive& = delete;

		auto operator=(ScopedActive&&) -> ScopedActive& = delete;

	private:
		/// Opaque PIMPL pointer to backend-specific state (previous active).
		void* mp_state = nullptr;
		/// True when this scope actually installed itself as active.
		bool m_engaged = false;
	};
};

}// namespace owl::renderer::gpu
