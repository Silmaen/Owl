/**
 * @file Texture.h
 * @author Silmaen
 * @date 12/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Macros.h"
#include "math/vectors.h"

#include <filesystem>
#include <string>
#include <vector>

namespace owl::core::task {

class Scheduler;
}// namespace owl::core::task

namespace owl::renderer::gpu {
/**
 * @brief
 *  Async load progress of a texture created via `Texture2D::createFromSerializedAsync`.
 *
 * Textures created synchronously keep the default value `Ready`.
 */
enum struct LoadState : uint8_t {
	Pending,///< Decoder still running on a worker; GPU currently holds the placeholder pixels.
	Ready,///< Real pixels uploaded.
	Failed,///< Worker failed (bytes missing, decode error); placeholder stays visible.
};

/**
 * @brief
 *  Pixel format of images.
 */
enum struct ImageFormat : uint8_t {
	None,///< Nothing.
	R8,///< One 8bits channel
	Rgb8,///< Three 8bits-channels.
	Rgba8,///< Four 8bits-channels.
	Rgba32F,///< Four float channels.
};

/**
 * @brief
 *  Sampler filtering policy.
 *
 * `Linear` (the default) gives smooth interpolation when the texture is shrunk
 * or stretched — appropriate for most sprite / UI / world rendering.
 * `Nearest` snaps to the closest texel and disables mipmaps; pick it when
 * pixel-perfect output matters (raycaster wall stripes, pixel art tilemaps).
 */
enum struct FilterMode : uint8_t {
	Linear,///< Smooth bilinear filtering, mipmaps generated when requested.
	Nearest,///< Hard nearest-texel filtering, no mipmaps. Pixel-art friendly.
};

/**
 * @brief
 *  Abstract class managing texture.
 */
class OWL_API Texture {
public:
	Texture() = default;

	Texture(const Texture&) = default;

	Texture(Texture&&) = default;

	auto operator=(const Texture&) -> Texture& = default;

	auto operator=(Texture&&) -> Texture& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~Texture();

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iPath path to the texture image file.
	 */
	explicit Texture(std::filesystem::path iPath);

	/// Texture specifications.
	struct OWL_API Specification {
		/// Texture size.
		math::vec2ui size;
		/// Pixel format.
		ImageFormat format = ImageFormat::Rgba8;
		/// If mips should be generated. Force-disabled when `filterMode == Nearest`.
		bool generateMips = true;
		/// Sampler filtering — `Nearest` disables mipmaps and snaps to texels.
		FilterMode filterMode = FilterMode::Linear;

		/**
		 * @brief
		 *  Express specifications as strings.
		 * @return Specs as string.
		 */
		[[nodiscard]] auto toString() const -> std::string;

		/**
		 * @brief
		 *  Decode a string to set specifications.
		 * @param iString String to decode.
		 */
		void fromString(const std::string& iString);

		/**
		 * @brief
		 *  Compute size of data per pixel.
		 * @return Pixel's data size..
		 */
		[[nodiscard]] auto getPixelSize() const -> uint8_t;
	};

	/**
	 * @brief
	 *  Constructor by specifications.
	 * @param[in] iSpecs The texture's specification.
	 */
	explicit Texture(const Specification& iSpecs);

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther Other texture to compare.
	 * @return True if same.
	 */
	virtual auto operator==(const Texture& iOther) const -> bool = 0;

	/**
	 * @brief
	 *  Access to texture's size.
	 * @return Texture's size.
	 */
	[[nodiscard]] auto getSize() const -> math::vec2ui { return m_specification.size; }

	/**
	 * @brief
	 *  Tells if the data effectively loaded.
	 * @return True if texture contains data.
	 */
	[[nodiscard]] auto isLoaded() const -> bool { return m_specification.size.surface() > 0; }

	/**
	 * @brief
	 *  Get renderer id.
	 * @return The renderer ID.
	 */
	[[nodiscard]] virtual auto getRendererId() const -> uint64_t = 0;

	/**
	 * @brief
	 *  Activate the texture in the GPU.
	 * @param[in] iSlot Slot into put the texture.
	 */
	virtual void bind(uint32_t iSlot) const = 0;

	/**
	 * @brief
	 *  Get Path to texture file.
	 * @return Path to texture file.
	 */
	[[nodiscard]] auto getPath() const -> const std::filesystem::path& { return m_path; }

	/**
	 * @brief
	 *  Define the texture data.
	 * @param[in] iData Raw data.
	 * @param[in] iSize Size of the data.
	 */
	virtual void setData(void* iData, uint32_t iSize) = 0;

	/**
	 * @brief
	 *  Switch the sampler filter mode at runtime.
	 *
	 * Useful when the texture is loaded by an asset path (which uses the default
	 * `Linear` filter) but the consumer wants pixel-perfect output (e.g. the
	 * raycaster wall atlas). Updates the GPU sampler parameters in-place; a
	 * Vulkan backend that uses a shared sampler may treat this as a no-op for
	 * now (TODO: per-texture sampler).
	 * @param[in] iMode The new filter mode.
	 */
	virtual void setFilterMode([[maybe_unused]] FilterMode iMode) { m_specification.filterMode = iMode; }

	/**
	 * @brief
	 *  Read the active filter mode.
	 * @return The active filter mode.
	 */
	[[nodiscard]] auto getFilterMode() const -> FilterMode { return m_specification.filterMode; }

	/**
	 * @brief
	 *  Get access to the texture's name.
	 * @return The texture's name.
	 */
	[[nodiscard]] auto getName() const -> const std::string& { return m_name; }

	/**
	 * @brief
	 *  Get a string that can be serialized.
	 * @return A serialized String.
	 */
	[[nodiscard]] auto getSerializeString() const -> std::string;

	/**
	 * @brief
	 *  Access to the texture's specifications.
	 * @return Specifications.
	 */
	[[nodiscard]] auto getSpecification() const -> const Specification& { return m_specification; }

	/**
	 * @brief
	 *  Current async-load state.
	 * @return `Ready` for synchronously loaded textures; `Pending`/`Failed` for textures created via `Texture2D::createFromSerializedAsync`.
	 */
	[[nodiscard]] auto getLoadState() const -> LoadState { return m_loadState; }

protected:
	/// Eventually the specifications.
	Specification m_specification;
	/// Path to the texture file.
	std::filesystem::path m_path;
	/// Current async-load state (only written from the main thread).
	LoadState m_loadState{LoadState::Ready};
private:
	/// The texture's name.
	std::string m_name;
	/// Texture2D needs access to set name during deserialization.
	friend class Texture2D;
};
/**
 * @brief
 *  Class texture 2D
 */

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class OWL_API Texture2D : public Texture {
public:
	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iPath path to the texture image file.
	 */
	explicit Texture2D(std::filesystem::path iPath);
	/**
	 * @brief
	 *  Constructor by specifications.
	 * @param[in] iSpecs The texture's specification.
	 */
	explicit Texture2D(const Specification& iSpecs);
	/**
	 * @brief
	 *  Creates the texture with the given filename.
	 * @param[in] iFile The path to the file to load.
	 * @return Resulting texture.
	 */
	static auto create(const std::filesystem::path& iFile) -> shared<Texture2D>;

	/**
	 * @brief
	 *  Create a new texture.
	 * @param[in] iTextureSerializedName Name of the files in the standard path.
	 * @return Pointer to the texture.
	 */
	static auto createFromSerialized(const std::string& iTextureSerializedName) -> shared<Texture2D>;

	/**
	 * @brief
	 *  Create a texture from a serialized name without blocking on decode.
	 * @param[in] iTextureSerializedName Serialized reference (`nam:`, `pat:`, `siz:`, `emp:`).
	 * @param[in] ioScheduler Scheduler used to run the decode on a worker thread.
	 * @return A valid texture with `LoadState::Pending` whose GPU contents currently hold a 1-channel
	 *         white placeholder sized to the peeked image dimensions. Contents are swapped on the
	 *         main thread when the decode termination callback fires, moving the state to
	 *         `Ready` (on success) or `Failed` (placeholder kept).
	 *
	 * Falls back to the synchronous `createFromSerialized` when the name does not designate a
	 * decodable image (`emp:`, `siz:`) or when dimensions cannot be peeked cheaply.
	 */
	static auto createFromSerializedAsync(const std::string& iTextureSerializedName, core::task::Scheduler& ioScheduler)
			-> shared<Texture2D>;

	/**
	 * @brief
	 *  Convenience wrapper used by scene component deserializers.
	 * @param[in] iTextureSerializedName Serialized reference.
	 * @return Async texture when an `Application` with a scheduler is alive; otherwise the
	 *         synchronous `createFromSerialized` result.
	 *
	 * Lets deserialization code stay a single call regardless of whether the host process has
	 * booted an `Application` (runner, editor) or not (unit tests, offline tools such as `PackWriter`).
	 */
	static auto createFromSerializedForDeserialize(const std::string& iTextureSerializedName) -> shared<Texture2D>;

	/**
	 * @brief
	 *  Creates the texture with the given size.
	 * @param[in] iSpecs The texture's specification.
	 * @return Resulting texture.
	 */
	static auto create(const Specification& iSpecs) -> shared<Texture2D>;

	/**
	 * @brief
	 *  Get the possible file extension for this dataset.
	 * @return The datasets possible extension.
	 */
	static auto extension() -> std::vector<std::string> { return {".jpg", ".png"}; }
};
OWL_DIAG_POP
}// namespace owl::renderer::gpu
