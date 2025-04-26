/**
 * @file Descriptors.h
 * @author Silmaen
 * @date 13/03/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"
#include <vulkan/vulkan.h>

namespace owl::renderer::vulkan::internal {

/**
 * @brief Data for Texture description.
 */
struct TextureData {
	VkImage textureImage = nullptr;
	VkDeviceMemory textureImageMemory = nullptr;
	VkImageView textureImageView = nullptr;
	VkSampler textureSampler = nullptr;
	VkDescriptorSet textureDescriptorSet = nullptr;
	VkDescriptorSetLayout textureDescriptorSetLayout = nullptr;

	void freeTexture();
	void createDescriptorSet();
	void createView();
	void createSampler();
	void createImage(const math::vec2ui& iDimensions);
};

/**
 * @brief Class handling vulkan descriptors.
 */
class Descriptors {
public:
	Descriptors(const Descriptors&) = delete;
	Descriptors(Descriptors&&) = delete;
	auto operator=(const Descriptors&) -> Descriptors& = delete;
	auto operator=(Descriptors&&) -> Descriptors& = delete;
	/**
	 * @brief Destructor.
	 */
	~Descriptors();

	/**
	 * @brief Singleton's accessor.
	 * @return The instance of this object.
	 */
	static auto get() -> Descriptors& {
		static Descriptors instance;
		return instance;
	}

	/**
	 * @brief Create the descriptor sets
	 */
	void createDescriptors();

	/**
	 * @brief Update the descriptor sets.
	 */
	void updateDescriptors();

	/**
	 * @brief Destroy everything.
	 */
	void release();

	void registerUniform(uint32_t iSize);

	/**
	 * @brief Register new texture data in the
	 * @return Id of the created texture.
	 */
	auto registerNewTexture() -> uint32_t;

	auto getTextureData(uint32_t iIndex) -> TextureData&;

	void bindTextureImage(uint32_t iIndex);

	void unregisterTexture(uint32_t iIndex);

	[[nodiscard]] auto isTextureRegistered(uint32_t iIndex) const -> bool;

	void setUniformData(const void* iData, size_t iSize) const;

	void resetTextureBind();
	void commitTextureBind(size_t iCurrentFrame);
	void textureBind(uint32_t iIndex);

	[[nodiscard]] auto getDescriptorPool() const -> VkDescriptorPool { return m_descriptorPool; }

	auto getDescriptorSetLayout() -> VkDescriptorSetLayout* { return &m_descriptorSetLayout; }
	auto getDescriptorSet(const uint32_t iFrame) -> VkDescriptorSet* { return &m_descriptorSets[iFrame]; }

	void createImguiDescriptorPool();
	[[nodiscard]] auto getImguiDescriptorPool() const -> VkDescriptorPool { return m_imguiDescriptorPool; }
	void createSingleImageDescriptorPool();

	[[nodiscard]] auto getSingleImageDescriptorPool() -> VkDescriptorPool {
		if (m_singleImageDescriptorPool == nullptr)
			createSingleImageDescriptorPool();
		return m_singleImageDescriptorPool;
	}

private:
	/**
	 * @brief Default Constructor.
	 */
	Descriptors();

	/**
	 * @brief Vulkan management of the texture list.
	 */
	struct TextureList {
		using tex = shared<TextureData>;
		using elem = std::pair<uint32_t, tex>;
		using list = std::vector<elem>;
		list textures;
		uint32_t nextId = 0;
		[[nodiscard]] auto empty() const -> bool { return textures.empty(); }
		void clear() {
			textures.clear();
			textures.shrink_to_fit();
		}
		auto begin() -> list::iterator { return textures.begin(); }
		auto end() -> list::iterator { return textures.begin(); }
		[[nodiscard]] auto begin() const -> list::const_iterator { return textures.begin(); }
		[[nodiscard]] auto end() const -> list::const_iterator { return textures.begin(); }
		[[nodiscard]] auto contains(uint32_t iIndex) const -> bool;
		auto registerNewTexture() -> uint32_t;
		void unregisterTexture(uint32_t iIndex);
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

	void updateDescriptor(size_t iFrame);
};

}// namespace owl::renderer::vulkan::internal
