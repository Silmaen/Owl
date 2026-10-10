/**
 * @file shaderFileUtils.h
 * @author Silmaen
 * @date 06/02/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/Shader.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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
OWL_API auto getCacheDirectory(const std::string& iRenderer, const std::string& iRendererApi) -> std::filesystem::path;

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

OWL_API auto shaderReflect(const std::string& iShaderName, const std::string& iRenderer,
						   const std::string& iRendererApi, gpu::ShaderType iStage,
						   const std::vector<uint32_t>& iShaderData) -> ShaderReflectionData;

OWL_API auto computeShaderHash(const std::string& iSource) -> std::string;

/**
 * @brief
 *  Build the cache key of a Slang shader: everything that changes the compiled output.
 *  Pass it to `isShaderCacheValid` / `writeShaderHash` instead of the bare source.
 * @param[in] iSource Slang source.
 * @param[in] iModuleName Shader name (the module path inside the asset tree).
 * @param[in] iForVulkan Target backend (selects the profile and the `BACKEND_*` macro).
 * @return The key: backend, module, Slang version, macros, profile and source.
 */
OWL_API auto getShaderCacheKey(const std::string& iSource, const std::string& iModuleName, bool iForVulkan)
		-> std::string;

/**
 * @brief
 *  Make Slang's SPIR-V valid for `GL_ARB_gl_spirv`: the Vulkan-only `InstanceIndex` / `VertexIndex` built-ins become
 *  the OpenGL `InstanceId` / `VertexId` (`gl_InstanceID`, `gl_VertexID`). Slang subtracts the base instance / vertex
 *  from them, which is exact as long as draws use a zero base, as every Owl draw does.
 * @param[in,out] ioSpirv SPIR-V words, patched in place.
 * @return Number of decorations changed.
 */
OWL_API auto remapBuiltinsForOpenGl(std::vector<uint32_t>& ioSpirv) -> uint32_t;

/**
 * @brief
 *  Translate an OpenGL SPIR-V module (`compileSlangToSpirv(..., false)`) into GLSL 4.50 core source, for OpenGL
 *  drivers without `GL_ARB_gl_spirv`. Bindings and locations are kept, so the reflection of the SPIR-V applies.
 * @param[in] iSpirv SPIR-V words of one entry point.
 * @param[in] iName Shader name, for the logs.
 * @return The GLSL source, or nothing when the translation fails.
 */
OWL_API auto crossCompileToGlsl(const std::vector<uint32_t>& iSpirv, const std::string& iName)
		-> std::optional<std::string>;

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

/**
 * @brief
 *  Path of a stage compiled at build time, relative to an asset folder.
 * @param[in] iShaderName Shader name (file stem of the Slang source).
 * @param[in] iRenderer Renderer folder (`renderer2D`, `bitonic_sort`...).
 * @param[in] iRendererApi Graphics API folder (`vulkan` or `opengl`).
 * @param[in] iType Shader stage.
 * @return `shaders/<renderer>/spirv/<api>/<name>.<stage>.spv`, with its `.hash` next to it.
 */
OWL_API auto getPrecompiledShaderPath(const std::string& iShaderName, const std::string& iRenderer,
									  const std::string& iRendererApi, const gpu::ShaderType& iType)
		-> std::filesystem::path;

/// SPIR-V per stage, as the backends consume it.
using SpirvStages = std::unordered_map<gpu::ShaderType, std::vector<uint32_t>>;

/**
 * @brief
 *  Check that a word stream is a well-formed SPIR-V module, before a driver reads it.
 *
 *  The header (magic number, non-zero id bound), instructions that fill the stream exactly and end with
 *  `OpFunctionEnd`, the `OpMemoryModel` and an `OpEntryPoint` of the expected stage. Catches truncated, garbled, foreign or misplaced files: a driver given
 *  invalid SPIR-V is in undefined behaviour (Mesa can crash on the next pipeline), so it must not see them. What only
 *  the driver can judge (types, capabilities) is caught at module or pipeline creation.
 * @param[in] iSpirv SPIR-V words.
 * @param[in] iStage Stage the module must have an entry point for, or `ShaderType::None` to accept any.
 * @return Why the module is rejected, or nothing when it is well formed.
 */
OWL_API auto checkSpirv(const std::vector<uint32_t>& iSpirv, gpu::ShaderType iStage = gpu::ShaderType::None)
		-> std::optional<std::string>;

/// SPIR-V of a shader and where it comes from.
struct LoadedSpirv {
	/// SPIR-V per stage.
	SpirvStages stages;
	/// Folder of the stored stages that were read, empty when they were just compiled from the Slang source.
	std::filesystem::path origin;
};

/**
 * @brief
 *  Get the SPIR-V of a shader without compiling it when its output is already known.
 *
 *  In order: the cache of the working directory, the stages compiled at build time (`getPrecompiledShaderPath` in an
 *  asset folder), then a Slang compilation whose output goes to that cache. Stored stages are used only when their
 *  hash matches `getShaderCacheKey` of the current source and `checkSpirv` accepts them, so an edited source (hot
 *  reload), another Slang version or a damaged file is compiled again; every rejected file is logged with its reason.
 *  The cache comes first so that a stored output a driver refused (`recompileSpirv`) stays replaced.
 * @param[in] iSource Slang source of the shader.
 * @param[in] iShaderName Shader name (Slang module name).
 * @param[in] iRenderer Renderer folder of the shader.
 * @param[in] iForVulkan True for the Vulkan target, false for OpenGL.
 * @param[in] iStages Stages that must all be found for a stored output to be used.
 * @return The SPIR-V per stage and its origin, or `std::nullopt` when the compilation fails.
 */
OWL_API auto loadOrCompileSpirv(const std::string& iSource, const std::string& iShaderName,
								const std::string& iRenderer, bool iForVulkan,
								const std::vector<gpu::ShaderType>& iStages) -> std::optional<LoadedSpirv>;

/**
 * @brief
 *  Compile a shader again after a driver refused its stored SPIR-V, and put the result in the cache.
 *
 *  Logs the rejected folder, the reason and the outcome. SPIR-V that was just compiled (empty origin) is not
 *  compiled again: the same output would be refused the same way.
 * @param[in] iSource Slang source of the shader.
 * @param[in] iShaderName Shader name (Slang module name).
 * @param[in] iRenderer Renderer folder of the shader.
 * @param[in] iForVulkan True for the Vulkan target, false for OpenGL.
 * @param[in] iOrigin `LoadedSpirv::origin` of the refused SPIR-V.
 * @param[in] iReason What refused it, for the log.
 * @return The new SPIR-V (empty origin), or `std::nullopt` when nothing better can be produced.
 */
OWL_API auto recompileSpirv(const std::string& iSource, const std::string& iShaderName, const std::string& iRenderer,
							bool iForVulkan, const std::filesystem::path& iOrigin, std::string_view iReason)
		-> std::optional<LoadedSpirv>;

/// Output of `compileSlangToSpirv`.
struct SlangCompilationResult {
	/// SPIR-V of every entry point found (`vertexMain`, `fragmentMain`, `computeMain`).
	SpirvStages spirvData;
	/// True when the module loaded and at least one entry point compiled.
	bool success = false;
};

/**
 * @brief
 *  Compile a Slang source to SPIR-V, one module per entry point.
 * @param[in] iSource Slang source.
 * @param[in] iModuleName Slang module name.
 * @param[in] iForVulkan True for `spirv_1_6` with `BACKEND_VULKAN`, false for `glsl_450` with `BACKEND_OPENGL`.
 * @return The SPIR-V per stage and the success flag.
 */
OWL_API auto compileSlangToSpirv(const std::string& iSource, const std::string& iModuleName, bool iForVulkan)
		-> SlangCompilationResult;

}// namespace owl::renderer::utils
