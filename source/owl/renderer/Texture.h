/**
 * @file Texture.h
 * @author Silmen
 * @date 12/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Core.h"
#include <filesystem>

namespace owl::renderer {


#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif
/**
 * @brief Class Texture
 */
class OWL_API Texture {
public:
	/**
	 * @brief Default copy constructor
	 */
	Texture(const Texture &) = default;
	/**
	 * @brief Default move constructor
	 */
	Texture(Texture &&) = default;
	/**
	 * @brief Default copy assignation
	 * @return this
	 */
	Texture &operator=(const Texture &) = default;
	/**
	 * @brief Default move assignation
	 * @return this
	 */
	Texture &operator=(Texture &&) = default;
	/**
	 * @brief Default constructor.
	 */
	Texture() = default;
	/**
	 * @brief Destructor.
	 */
	virtual ~Texture() = default;//---UNCOVER---
	/**
	 * @brief Comparison operator
	 * @param other Other texture to compare
	 * @return True if same
	 */
	virtual bool operator==(const Texture& other) const = 0;
	/**
	 * @brief Access to texture's width.
	 * @return Texture's width
	 */
	[[nodiscard]] virtual uint32_t getWidth() const = 0;
	/**
	 * @brief Access to texture's height.
	 * @return Texture's height
	 */
	[[nodiscard]] virtual uint32_t getHeight() const = 0;
	/**
	 * @brief Get renderer id
	 * @return The renderer ID
	 */
	virtual uint32_t getRendererID() const = 0;
	/**
	 * @brief Activate the texture in the GPU
	 * @param slot Slot into put the texture
	 */
	virtual void bind(uint32_t slot = 0) const = 0;
	/**
	 * @brief Define the texture data
	 * @param data Raw data
	 * @param size Size of the data
	 */
	virtual void setData(void* data, uint32_t size) = 0;
private:
};
#ifdef __clang__
#pragma clang diagnostic pop
#endif


#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif
/**
 * @brief Class texture 2D
 */
class OWL_API Texture2D : public Texture {
public:
	/**
	 * @brief Creates the texture with the given filename
	 * @param file The path to the file to load
	 * @return Resulting texture
	 */
	static shrd<Texture2D> create(const std::filesystem::path &file);
	/**
	 * @brief Creates the texture with the given size
	 * @param width The texture's width
	 * @param height The texture's height
	 * @return Resulting texture
	 */
	static shrd<Texture2D> create(uint32_t width, uint32_t height);
};
#ifdef __clang__
#pragma clang diagnostic pop
#endif

}// namespace owl::renderer
