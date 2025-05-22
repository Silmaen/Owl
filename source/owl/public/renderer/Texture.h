/**
 * @file Texture.h
 * @author Silmaen
 * @date 12/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Macros.h"
#include "math/vectors.h"

#include <filesystem>

namespace owl::renderer {

/**
 * @brief Pixel format of images.
 */
enum struct ImageFormat : uint8_t {
	None,///< Nothing.
	R8,///< One 8bits channel
	Rgb8,///< Three 8bits-channels.
	Rgba8,///< Four 8bits-channels.
	Rgba32F,///< Four float channels.
};

class TextureLibrary;
/**
 * @brief Abstract class managing texture.
 */
class OWL_API Texture {
public:
	Texture() = default;
	Texture(const Texture&) = default;
	Texture(Texture&&) = default;
	auto operator=(const Texture&) -> Texture& = default;
	auto operator=(Texture&&) -> Texture& = default;
	/**
	 * @brief Destructor.
	 */
	virtual ~Texture();
	/**
	 * @brief Default constructor.
	 * @param[in] iPath path to the texture image file.
	 */
	explicit Texture(std::filesystem::path iPath);

	/// Texture specifications.
	struct OWL_API Specification {
		/// Texture size.
		math::vec2ui size;
		/// Pixel format.
		ImageFormat format = ImageFormat::Rgba8;
		/// If mips should be generated
		bool generateMips = true;
		/**
		 * @brief Express specifications as strings.
		 * @return Specs as string.
		 */
		[[nodiscard]] auto toString() const -> std::string;
		/**
		 * @brief Decode a string to set specifications.
		 * @param iString String to decode.
		 */
		void fromString(const std::string& iString);
		/**
		 * @brief Compute size of data per pixel.
		 * @return Pixel's data size..
		 */
		[[nodiscard]] auto getPixelSize() const -> uint8_t;
	};
	/**
	 * @brief Constructor by specifications.
	 * @param[in] iSpecs The texture's specification.
	 */
	explicit Texture(const Specification& iSpecs);
	/**
	 * @brief Comparison operator.
	 * @param[in] iOther Other texture to compare.
	 * @return True if same.
	 */
	virtual auto operator==(const Texture& iOther) const -> bool = 0;

	/**
	 * @brief Access to texture's size.
	 * @return Texture's size.
	 */
	[[nodiscard]] auto getSize() const -> math::vec2ui { return m_specification.size; }

	/**
	 * @brief Tells if the data effectively loaded.
	 * @return True if texture contains data.
	 */
	[[nodiscard]] auto isLoaded() const -> bool { return m_specification.size.surface() > 0; }

	/**
	 * @brief Get renderer id.
	 * @return The renderer ID.
	 */
	[[nodiscard]] virtual auto getRendererId() const -> uint64_t = 0;

	/**
	 * @brief Activate the texture in the GPU.
	 * @param[in] iSlot Slot into put the texture.
	 */
	virtual void bind(uint32_t iSlot) const = 0;

	/**
	 * @brief Get Path to texture file.
	 * @return Path to texture file.
	 */
	[[nodiscard]] auto getPath() const -> const std::filesystem::path& { return m_path; }

	/**
	 * @brief Define the texture data.
	 * @param[in] iData Raw data.
	 * @param[in] iSize Size of the data.
	 */
	virtual void setData(void* iData, uint32_t iSize) = 0;

	/**
	 * @brief Get access to the texture's name.
	 * @return The texture's name.
	 */
	[[nodiscard]] auto getName() const -> const std::string& { return m_name; }

	/**
	 * @brief Get a string that can be serialized.
	 * @return A serialized String.
	 */
	[[nodiscard]] auto getSerializeString() const -> std::string;

	/**
	 * @brief Access to the texture's specifications.
	 * @return Specifications.
	 */
	[[nodiscard]] auto getSpecification() const -> const Specification& { return m_specification; }

protected:
	/// Eventually the specifications.
	Specification m_specification;
	/// Path to the texture file.
	std::filesystem::path m_path;

private:
	/// The texture's name.
	std::string m_name;
	/// Library is a friend to be able to modify name.
	friend class TextureLibrary;
};

/**
 * @brief Class texture 2D
 */
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class OWL_API Texture2D : public Texture {
public:
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
	 * @brief Creates the texture with the given filename.
	 * @param[in] iFile The path to the file to load.
	 * @return Resulting texture.
	 */
	static auto create(const std::filesystem::path& iFile) -> shared<Texture2D>;

	/**
	 * @brief Create a new texture.
	 * @param[in] iTextureSerializedName Name of the files in the standard path.
	 * @return Pointer to the texture.
	 */
	static auto createFromSerialized(const std::string& iTextureSerializedName) -> shared<Texture2D>;

	/**
	 * @brief Creates the texture with the given size.
	 * @param[in] iSpecs The texture's specification.
	 * @return Resulting texture.
	 */
	static auto create(const Specification& iSpecs) -> shared<Texture2D>;

	/**
	 * @brief Get the possible file extension for this dataset.
	 * @return The datasets possible extension.
	 */
	static auto extension() -> std::vector<std::string> { return {".jpg", ".png"}; }
};
OWL_DIAG_POP

}// namespace owl::renderer
