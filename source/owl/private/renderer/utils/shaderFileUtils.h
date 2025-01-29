/**
 * @file shaderFileUtils.h
 * @author Silmaen
 * @date 06/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/Shader.h"
#include "core/external/shaderc.h"

/**
 * @brief Namespace gathering utility functions used accross different renderers.
 */
namespace owl::renderer::utils {

auto getCacheDirectory(const std::string& iRenderer, const std::string& iRendererApi) -> std::filesystem::path;

void createCacheDirectoryIfNeeded(const std::string& iRenderer, const std::string& iRendererApi);

auto getShaderCachedPath(const std::string& iShaderName, const std::string& iRenderer, const std::string& iRendererApi,
						 const ShaderType& iType) -> std::filesystem::path;

auto getShaderPath(const std::string& iShaderName, const std::string& iRenderer, const std::string& iRendererApi,
				   const ShaderType& iType) -> std::filesystem::path;

auto getRelativeShaderPath(const std::string& iShaderName, const std::string& iRenderer,
						   const std::string& iRendererApi, const ShaderType& iType) -> std::filesystem::path;

auto getExtension(const ShaderType& iStage) -> std::string;

auto getCacheExtension(const ShaderType& iStage) -> std::string;

auto shaderStageToShaderC(const ShaderType& iStage) -> shaderc_shader_kind;

auto readCachedShader(const std::filesystem::path& iFile) -> std::vector<uint32_t>;

auto writeCachedShader(const std::filesystem::path& iFile, const std::vector<uint32_t>& iData) -> bool;

void shaderReflect(const std::string& iShaderName, const std::string& iRenderer, const std::string& iRendererApi,
				   ShaderType iStage, const std::vector<uint32_t>& iShaderData);

}// namespace owl::renderer::utils
