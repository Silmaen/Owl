/**
 * @file AssetLibrary.h
 * @author Silmaen
 * @date 1/9/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/assets/Asset.h"

#include "core/Application.h"

/**
 * @brief Concept tha check existence of a conversion function from string to specification.
 */
template<typename T>
concept hasStringSpec = requires {
	{ T::stringToSpecification(std::declval<const std::string&>()) } -> std::same_as<typename T::Specification>;
};

/**
 * @brief Namespace for asset management.
 */
namespace owl::core::assets {

/**
 * @brief Class managing a library of assets.
 * @tparam DataType The underlying data type.
 */
template<assetDataType DataType>
class AssetLibrary final {
public:
	using AssetType = Asset<DataType>;
	/**
	 * @brief Default constructor.
	 */
	AssetLibrary() = default;

	/**
	 * @brief Default destructor.
	 */
	~AssetLibrary() = default;

	/**
	 * @brief Default copy constructor.
	 */
	AssetLibrary(const AssetLibrary&) = default;

	/**
	 * @brief Default move constructor.
	 */
	AssetLibrary(AssetLibrary&&) = default;

	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const AssetLibrary&) -> AssetLibrary& = default;

	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(AssetLibrary&&) -> AssetLibrary& = default;

	/**
	 * @brief Add the asset to the library and name it.
	 * @param[in] iName Name of the asset.
	 * @param[in] iAsset The asset to add.
	 */
	void add(const std::string& iName, shared<DataType>& iAsset) { m_assets.emplace(iName, AssetType{iAsset}); }

	/**
	 * @brief Load an asset from a file base on name.
	 * @param[in] iName Name of the asset.
	 * @return The asset.
	 */
	auto load(const std::string& iName) -> shared<DataType> {
		if (exists(iName)) {
			OWL_CORE_WARN("AssetLibrary::load({}) already exists!", iName)
			return m_assets.at(iName).get();
		}
		shared<DataType> asset = nullptr;
		if (!DataType::extension().empty()) {
			auto assetFile = find(iName);
			if (!assetFile.has_value()) {
				OWL_CORE_WARN("AssetLibrary::load({}) does not exist in asset folders!", iName)
				return nullptr;
			}
			asset = DataType::create(assetFile.value());
		} else if constexpr (hasStringSpec<DataType>) {
			asset = DataType::create(DataType::stringToSpecification(iName));
		}
		if (asset == nullptr) {
			OWL_CORE_WARN("AssetLibrary::load({}) could not load asset!", iName)
			return nullptr;
		}
		OWL_CORE_TRACE("Asset {} Added.", iName)
		add(iName, asset);
		return asset;
	}

	/**
	 * @brief Load an asset from a file and name.
	 * @param[in] iName Name of the asset.
	 * @param[in] iFile File path to the asset.
	 * @return The asset.
	 */
	auto load(const std::string& iName, const std::filesystem::path& iFile) -> shared<DataType> {
		if (exists(iName)) {
			OWL_CORE_WARN("AssetLibrary::load({}, {}) already exists!", iName, iFile.string())
			return m_assets.at(iName).get();
		}
		if (!DataType::extension().empty()) {
			if (!std::filesystem::exists(iFile)) {
				OWL_CORE_WARN("AssetLibrary::load({}, {}) file does not exist!", iName, iFile.string())
				return nullptr;
			}
		}
		auto asset = DataType::create(iFile);
		if (asset == nullptr) {
			OWL_CORE_WARN("AssetLibrary::load({}, {}) could not load asset!", iName, iFile.string())
			return nullptr;
		}
		OWL_CORE_TRACE("Asset {} Added.", iName)
		add(iName, asset);
		return asset;
	}

	/**
	 * @brief Load an asset based on its internal specifications structure.
	 * @param[in] iName The name of the asset.
	 * @param[in] iSpec The specifications of the asset.
	 * @return The asset.
	 */
	auto load(const std::string& iName, const DataType::Specification& iSpec) -> shared<DataType> {
		if (exists(iName)) {
			OWL_CORE_WARN("AssetLibrary::load({}, <Specification>) already exists!", iName)
			return m_assets[iName].get();
		}
		auto asset = DataType::create(iSpec);
		if (asset == nullptr) {
			OWL_CORE_WARN("AssetLibrary::load({}, <Specification>) could not load asset!", iName)
			return nullptr;
		}
		OWL_CORE_TRACE("Asset {} Added.", iName)
		add(iName, asset);
		return asset;
	}

	/**
	 * @brief Access to the asset of the given name.
	 * @param[in] iName Name of the asset.
	 * @return Asset's pointer or nullptr if not exists.
	 */
	auto get(const std::string& iName) -> shared<DataType> {
		if (!exists(iName)) {
			OWL_CORE_ERROR("Asset {} not found in library", iName)
			return nullptr;
		}
		return m_assets.at(iName).get();
	}

	/**
	 * @brief Verify if an asset exists.
	 * @param[in] iName Name of the asset.
	 * @return True if the asset exists.
	 */
	[[nodiscard]] auto exists(const std::string& iName) const -> bool { return m_assets.contains(iName); }

	/**
	 * @brief Get a lis of asset file found in the asset folders
	 * @return The list of asset founds.
	 */
	[[nodiscard]] auto list() const -> std::vector<std::string> {
		if (AssetType::extensions().empty())
			return {};
		std::vector<std::string> result;
		std::vector<std::string> ext = AssetType::extensions();
		// get a list of directory to search.
		std::list<Application::AssetDirectory> assetDirectories;
		if (Application::instanced()) {
			assetDirectories = Application::get().getAssetDirectories();
		} else {
			assetDirectories.push_back({"cwd", std::filesystem::current_path()});
		}
		for (const auto& [title, assetsPath]: assetDirectories) {
			for (const auto& entry: std::filesystem::recursive_directory_iterator(assetsPath)) {
				if (std::find(ext.begin(), ext.end(), entry.path().extension()) != ext.end()) {
					result.push_back(relative(entry.path(), assetsPath).string());
				}
			}
		}
		return result;
	}

	/**
	 * @brief Find the file path of the given Asset name.
	 * @param iName Name of the asset.
	 * @return Path to the file or nullopt if not found.
	 */
	[[nodiscard]] auto find(const std::string& iName) const -> std::optional<std::filesystem::path> {
		if (AssetType::extensions().empty())
			return std::nullopt;
		const std::vector<std::string> ext = AssetType::extensions();
		// get a list of directory to search.
		std::list<Application::AssetDirectory> assetDirectories;
		if (Application::instanced()) {
			assetDirectories = Application::get().getAssetDirectories();
		} else {
			assetDirectories.push_back({"cwd", std::filesystem::current_path()});
		}
		const std::filesystem::path name(iName);
		const bool hasExtension = name.has_extension();
		for (const auto& [title, assetsPath]: assetDirectories) {
			// check base folders
			{
				std::filesystem::path filePath = assetsPath / name;
				if (hasExtension) {
					if (std::filesystem::exists(filePath))
						return filePath;
				} else {
					for (auto& e: ext) {
						std::filesystem::path filePathWithExt = filePath.string() + e;
						if (std::filesystem::exists(filePathWithExt))
							return filePathWithExt;
					}
				}
			}
			// Check sub-folders
			for (const auto& entry: std::filesystem::recursive_directory_iterator(assetsPath)) {
				if (!entry.is_directory())
					continue;
				std::filesystem::path filePath = entry.path() / name;
				if (hasExtension) {
					if (std::filesystem::exists(filePath))
						return filePath;
				} else {
					for (auto& e: ext) {
						filePath.replace_extension(e);
						if (std::filesystem::exists(filePath))
							return filePath;
					}
				}
			}
		}
		return std::nullopt;
	}

private:
	/// The list of assets.
	std::unordered_map<std::string, AssetType> m_assets;
};

}// namespace owl::core::assets
