/**
 * @file Texture.h
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/Texture.h"

namespace owl::renderer::null {
/**
 * @brief Specialized class managing null texture.
 */
class OWL_API Texture2D final : public renderer::Texture2D {
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
	 * @brief Constructor by specifications.
	 * @param[in] iSpecs The texture's specification.
	 */
	explicit Texture2D(const Specification& iSpecs);

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
		return m_rendererId == (dynamic_cast<const Texture2D&>(iOther)).m_rendererId;
	}

	/**
	 * @brief Get renderer id.
	 * @return The renderer ID.
	 */
	[[nodiscard]] auto getRendererId() const -> uint64_t override { return m_rendererId; }

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
	uint64_t m_rendererId = 0;
};
}// namespace owl::renderer::null
