/**
 * @file Texture.h
 * @author Silmaen
 * @date 12/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/Texture.h"

namespace owl::renderer::opengl {
/**
 * @brief Class Texture.
 */
class Texture2D final : public renderer::Texture2D {
public:
	Texture2D(const Texture2D&) = default;
	Texture2D(Texture2D&&) = default;
	auto operator=(const Texture2D&) -> Texture2D& = default;
	auto operator=(Texture2D&&) -> Texture2D& = default;

	/**
	 * @brief Default constructor.
	 * @param[in] iPath path to the texture image file.
	 */
	explicit Texture2D(std::filesystem::path iPath);
	/**
	 * @brief Constructor by size.
	 * @param[in] iWidth Texture's width.
	 * @param[in] iHeight Texture's height.
	 * @param[in] iWithAlpha If the texture has alpha channel.
	 */
	Texture2D(uint32_t iWidth, uint32_t iHeight, bool iWithAlpha = true);
	/**
	 * @brief Constructor by size.
	 * @param[in] iSize Texture's width.
	 * @param[in] iWithAlpha If the texture has alpha channel.
	 */
	explicit Texture2D(math::vec2ui iSize, bool iWithAlpha = true);

	/**
	 * @brief Destructor.
	 */
	~Texture2D() override;

	/**
	 * @brief Comparison operator.
	 * @param[in] iOther Other texture to compare.
	 * @return True if same.
	 */
	auto operator==(const Texture& iOther) const -> bool override {
		return m_textureId == (dynamic_cast<const Texture2D&>(iOther)).m_textureId;
	}

	/**
	 * @brief Get renderer id.
	 * @return The renderer ID.
	 */
	[[nodiscard]] auto getRendererId() const -> uint64_t override { return m_textureId; }

	/**
	 * @brief Activate the texture in the GPU.
	 * @param[in] iSlot Slot into put the texture.
	 */
	void bind(uint32_t iSlot) const override;

	/**
	 * @brief Define the texture data.
	 * @param[in] iData Raw data.
	 * @param[in] iSize Size of the data.
	 */
	void setData(void* iData, uint32_t iSize) override;

private:
	/// OpenGL binding.
	uint32_t m_textureId = 0;
};
}// namespace owl::renderer::opengl
