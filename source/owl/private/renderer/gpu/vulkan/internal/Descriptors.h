/**
 * @file Descriptors.h
 * @author Silmaen
 * @date 13/03/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"
#include <vulkan/vulkan.h>

namespace owl::renderer::gpu::vulkan::internal {
/**
 * @brief
 *  Data for Texture description.
 */
struct TextureData {
	VkImage textureImage = nullptr;
	VkDeviceMemory textureImageMemory = nullptr;
	VkImageView textureImageView = nullptr;
	VkSampler textureSampler = nullptr;
	VkDescriptorSet textureDescriptorSet = nullptr;
	VkDescriptorSetLayout textureDescriptorSetLayout = nullptr;

	/**
	 * @brief
	 *  Free texture.
	 */
	void freeTexture();

	/**
	 * @brief
	 *  Create descriptor set.
	 */
	void createDescriptorSet();

	/**
	 * @brief
	 *  Create view.
	 */
	void createView();

	/**
	 * @brief
	 *  Create sampler.
	 */
	void createSampler();

	void createImage(const math::vec2ui& iDimensions);
};

/**
 * @brief
 *  Class handling vulkan descriptors.
 */
class Descriptors {
public:
	Descriptors(const Descriptors&) = delete;

	Descriptors(Descriptors&&) = delete;

	auto operator=(const Descriptors&) -> Descriptors& = delete;

	auto operator=(Descriptors&&) -> Descriptors& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~Descriptors();

	/**
	 * @brief
	 *  Singleton's accessor.
	 * @return The instance of this object.
	 */
	static auto get() -> Descriptors& {
		static Descriptors instance;
		return instance;
	}

	/**
	 * @brief
	 *  Create the descriptor sets.
	 */
	void createDescriptors();

	/**
	 * @brief
	 *  Update the descriptor sets.
	 */
	void updateDescriptors();

	/**
	 * @brief
	 *  Destroy everything.
	 */
	void release();

	void registerUniform(uint32_t iSize);

	/**
	 * @brief
	 *  Register new texture data in the.
	 * @return Id of the created texture.
	 */
	auto registerNewTexture() -> uint32_t;

	/**
	 * @brief
	 *  Get the texture data for a given slot.
	 * @param[in] iIndex Texture slot id.
	 * @return Reference to the texture data.
	 */
	auto getTextureData(uint32_t iIndex) -> TextureData&;

	void bindTextureImage(uint32_t iIndex);

	void unregisterTexture(uint32_t iIndex);

	/**
	 * @brief
	 *  Check whether the given texture slot is currently registered.
	 * @param[in] iIndex Texture slot id.
	 * @return True when the slot is registered.
	 */
	[[nodiscard]] auto isTextureRegistered(uint32_t iIndex) const -> bool;

	/**
	 * @brief
	 *  Set the uniform data.
	 * @param[in] iData The data buffer.
	 * @param[in] iSize Target size.
	 */
	void setUniformData(const void* iData, size_t iSize) const;

	/**
	 * @brief
	 *  Reset texture bind.
	 */
	void resetTextureBind();

	/**
	 * @brief
	 *  Commit texture bind.
	 * @param[in] iCurrentFrame Current in-flight frame index.
	 */
	void commitTextureBind(size_t iCurrentFrame);

	void textureBind(uint32_t iIndex);

	/**
	 * @brief
	 *  Get the descriptor pool.
	 * @return The descriptor pool.
	 */
	[[nodiscard]] auto getDescriptorPool() const -> VkDescriptorPool { return m_descriptorPool; }

	/**
	 * @brief
	 *  Get the descriptor set layout.
	 * @return The descriptor set layout.
	 */
	auto getDescriptorSetLayout() -> VkDescriptorSetLayout* { return &m_descriptorSetLayout; }

	/**
	 * @brief
	 *  Get a pointer to the descriptor set for the given in-flight frame.
	 * @param[in] iFrame The frame index.
	 * @return Pointer to the descriptor set.
	 */
	auto getDescriptorSet(const uint32_t iFrame) -> VkDescriptorSet* { return &m_descriptorSets[iFrame]; }

	/**
	 * @brief
	 *  Create imgui descriptor pool.
	 */
	void createImguiDescriptorPool();

	/**
	 * @brief
	 *  Get the imgui descriptor pool.
	 * @return The imgui descriptor pool.
	 */
	[[nodiscard]] auto getImguiDescriptorPool() const -> VkDescriptorPool { return m_imguiDescriptorPool; }

	/**
	 * @brief
	 *  Create single image descriptor pool.
	 */
	void createSingleImageDescriptorPool();

	/**
	 * @brief
	 *  Get the single image descriptor pool.
	 * @return The single image descriptor pool.
	 */
	[[nodiscard]] auto getSingleImageDescriptorPool() -> VkDescriptorPool {
		if (m_singleImageDescriptorPool == nullptr)
			createSingleImageDescriptorPool();
		return m_singleImageDescriptorPool;
	}

private:
	/**
	 * @brief
	 *  Default Constructor.
	 */
	Descriptors();

	/**
	 * @brief
	 *  Vulkan management of the texture list.
	 */
	struct TextureList {
		using tex = shared<TextureData>;
		using elem = std::pair<uint32_t, tex>;
		using list = std::vector<elem>;
		list textures;
		uint32_t nextId = 0;

		/**
		 * @brief
		 *  Whether the texture list is empty.
		 * @return True when no texture is registered.
		 */
		[[nodiscard]] auto empty() const -> bool { return textures.empty(); }

		/**
		 * @brief
		 *  Drop every registered texture and shrink the backing storage.
		 */
		void clear() {
			textures.clear();
			textures.shrink_to_fit();
		}

		/**
		 * @brief
		 *  Iterator to the first component.
		 * @return Iterator pointing to the beginning.
		 */
		auto begin() -> list::iterator { return textures.begin(); }

		/**
		 * @brief
		 *  Iterator past the last component.
		 * @return Iterator pointing to the end.
		 */
		auto end() -> list::iterator { return textures.begin(); }

		/**
		 * @brief
		 *  Iterator to the first component.
		 * @return Iterator pointing to the beginning.
		 */
		[[nodiscard]] auto begin() const -> list::const_iterator { return textures.begin(); }

		/**
		 * @brief
		 *  Iterator past the last component.
		 * @return Iterator pointing to the end.
		 */
		[[nodiscard]] auto end() const -> list::const_iterator { return textures.begin(); }

		/**
		 * @brief
		 *  Whether the given texture slot id is in the list.
		 * @param[in] iIndex Texture slot id.
		 * @return True when the slot is present.
		 */
		[[nodiscard]] auto contains(uint32_t iIndex) const -> bool;

		/**
		 * @brief
		 *  Allocate a fresh texture slot.
		 * @return The newly allocated slot id.
		 */
		auto registerNewTexture() -> uint32_t;

		void unregisterTexture(uint32_t iIndex);

		/**
		 * @brief
		 *  Look up the texture data for a given slot id.
		 * @param[in] iIndex Texture slot id.
		 * @return Shared pointer to the texture data (null when the slot does not exist).
		 */
		auto getTextureData(uint32_t iIndex) -> tex;
	};

	/// The list of texture bindings.
	TextureList m_textures;
	/// The descriptor set layout.
	VkDescriptorSetLayout m_descriptorSetLayout{nullptr};
	/// The main descriptor pool.
	VkDescriptorPool m_descriptorPool{nullptr};
	/// The descriptor pool for Imgui.
	VkDescriptorPool m_imguiDescriptorPool{nullptr};
	/// The descriptor pool for the single images.
	VkDescriptorPool m_singleImageDescriptorPool{nullptr};
	/// List of descriptors
	std::vector<VkDescriptorSet> m_descriptorSets;
	std::vector<VkBuffer> m_uniformBuffers;
	std::vector<VkDeviceMemory> m_uniformBuffersMemory;
	std::vector<void*> m_uniformBuffersMapped;
	uint32_t m_uniformSize = 0;
	uint32_t m_bindedTexture = 0;
	std::vector<uint32_t> m_textureBind;

	/**
	 * @brief
	 *  Update descriptor.
	 * @param[in] iFrame The frame index.
	 */
	void updateDescriptor(size_t iFrame);
};

}// namespace owl::renderer::gpu::vulkan::internal
