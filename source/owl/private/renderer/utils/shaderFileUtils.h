/**
 * @file shaderFileUtils.h
 * @author Silmaen
 * @date 06/02/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/Shader.h"

/**
 * @brief
 *  Namespace gathering utility functions used across different renderers.
 */
namespace owl::renderer::utils {

/**
 * @brief
 *  Get the on-disk cache directory used for compiled SPIR-V binaries.
 * @param[in] iRenderer Engine renderer name (e.g. "vulkan", "opengl").
 * @param[in] iRendererApi Specific backend API qualifier (e.g. "1.4", "4.5").
 * @return Absolute path under the working directory.
 */
OWL_API auto getCacheDirectory(const std::string& iRenderer, const std::string& iRendererApi)
		-> std::filesystem::path;

/**
 * @brief
 *  Create the shader cache directory when it does not yet exist.
 * @param[in] iRenderer Engine renderer name.
 * @param[in] iRendererApi Specific backend API qualifier.
 */
OWL_API void createCacheDirectoryIfNeeded(const std::string& iRenderer, const std::string& iRendererApi);

/**
 * @brief
 *  Compute the cached SPIR-V file path for a given shader stage.
 * @param[in] iShaderName Logical shader name (without extension).
 * @param[in] iRenderer Engine renderer name.
 * @param[in] iRendererApi Specific backend API qualifier.
 * @param[in] iType Shader stage (vertex / fragment / ...).
 * @return Absolute path to the cache file.
 */
OWL_API auto getShaderCachedPath(const std::string& iShaderName, const std::string& iRenderer,
								 const std::string& iRendererApi, const gpu::ShaderType& iType)
		-> std::filesystem::path;

/**
 * @brief
 *  Resolve the source shader file path.
 * @param[in] iShaderName Logical shader name.
 * @param[in] iRenderer Engine renderer name.
 * @param[in] iRendererApi Specific backend API qualifier.
 * @param[in] iType Shader stage.
 * @return Absolute path to the source file (looked up via the texture/asset library).
 */
OWL_API auto getShaderPath(const std::string& iShaderName, const std::string& iRenderer,
						   const std::string& iRendererApi, const gpu::ShaderType& iType) -> std::filesystem::path;

/**
 * @brief
 *  Build the assets-relative path of a shader file (used for pack lookups).
 * @param[in] iShaderName Logical shader name.
 * @param[in] iRenderer Engine renderer name.
 * @param[in] iRendererApi Specific backend API qualifier.
 * @param[in] iType Shader stage.
 * @return Relative path under `shaders/<renderer>/<api>/`.
 */
OWL_API auto getRelativeShaderPath(const std::string& iShaderName, const std::string& iRenderer,
								   const std::string& iRendererApi, const gpu::ShaderType& iType)
		-> std::filesystem::path;

/**
 * @brief
 *  Get the source file extension for a given shader stage.
 * @param[in] iStage Shader stage.
 * @return File extension including the leading dot.
 */
OWL_API auto getExtension(const gpu::ShaderType& iStage) -> std::string;

/**
 * @brief
 *  Get the cache file extension for a given shader stage.
 * @param[in] iStage Shader stage.
 * @return Cache extension including the leading dot.
 */
OWL_API auto getCacheExtension(const gpu::ShaderType& iStage) -> std::string;

/**
 * @brief
 *  Read a cached SPIR-V binary from disk.
 * @param[in] iFile Path to the cached `.spv` file.
 * @return The SPIR-V word stream (empty on read error).
 */
OWL_API auto readCachedShader(const std::filesystem::path& iFile) -> std::vector<uint32_t>;

/**
 * @brief
 *  Write a SPIR-V binary to the cache.
 * @param[in] iFile Destination cache file.
 * @param[in] iData The SPIR-V word stream to persist.
 * @return True on success.
 */
OWL_API auto writeCachedShader(const std::filesystem::path& iFile, const std::vector<uint32_t>& iData) -> bool;

struct ShaderReflectionData {
	struct UniformBuffer {
		std::string name;
		uint32_t binding = 0;
		size_t size = 0;
		size_t memberCount = 0;
	};
	struct SampledImage {
		std::string name;
		uint32_t binding = 0;
		uint32_t descriptorCount = 0;
	};
	std::vector<UniformBuffer> uniformBuffers;
	std::vector<SampledImage> sampledImages;
};

OWL_API auto shaderReflect(const std::string& iShaderName, const std::string& iRenderer, const std::string& iRendererApi,
						   gpu::ShaderType iStage, const std::vector<uint32_t>& iShaderData) -> ShaderReflectionData;

OWL_API auto computeShaderHash(const std::string& iSource) -> std::string;

/**
 * @brief
 *  Check whether the cached binary still matches the current source hash.
 * @param[in] iCachedPath Path to the cache file.
 * @param[in] iSource Current shader source.
 * @return True when the cache is up-to-date.
 */
OWL_API auto isShaderCacheValid(const std::filesystem::path& iCachedPath, const std::string& iSource) -> bool;

/**
 * @brief
 *  Persist the source hash next to the cached binary so future reads can validate it.
 * @param[in] iCachedPath Path to the cache file.
 * @param[in] iSource Current shader source whose hash is written.
 */
OWL_API void writeShaderHash(const std::filesystem::path& iCachedPath, const std::string& iSource);

struct SlangCompilationResult {
	std::unordered_map<gpu::ShaderType, std::vector<uint32_t>> spirvData;
	bool success = false;
};

OWL_API auto compileSlangToSpirv(const std::string& iSource, const std::string& iModuleName, bool iForVulkan)
		-> SlangCompilationResult;

}// namespace owl::renderer::utils
