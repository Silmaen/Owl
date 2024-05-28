/**
 * @file shaderFileUtils.cpp
 * @author Silmaen
 * @date 06/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/Application.h"
#include "shaderFileUtils.h"

namespace owl::renderer::utils {

std::filesystem::path getCacheDirectory(const std::string &iRenderer, const std::string &iRendererApi) {
	auto output = core::Application::get().getAssetDirectory() / "cache" / "shader";
	if (!iRenderer.empty())
		output /= iRenderer;
	if (!iRendererApi.empty())
		output /= iRendererApi;
	return output;
}

void createCacheDirectoryIfNeeded(const std::string &iRenderer, const std::string &iRendererApi) {
	if (const std::filesystem::path cacheDirectory = getCacheDirectory(iRenderer, iRendererApi);
		!exists(cacheDirectory)) {
		create_directories(cacheDirectory);
		if (!exists(cacheDirectory))
			OWL_CORE_ERROR("Cannot Create directory {}.", cacheDirectory.string())
	}
}

std::filesystem::path getShaderCachedPath(const std::string &iShaderName, const std::string &iRenderer,
										  const std::string &iRendererApi, const ShaderType &iType) {
	return getCacheDirectory(iRenderer, iRendererApi) / (iShaderName + getCacheExtension(iType));
}

std::filesystem::path getShaderPath(const std::string &iShaderName, const std::string &iRenderer,
									const std::string &iRendererApi, const ShaderType &iType) {
	return core::Application::get().getAssetDirectory() / "shaders" / iRenderer / iRendererApi /
		   (iShaderName + getExtension(iType));
}

std::filesystem::path getRelativeShaderPath(const std::string &iShaderName, const std::string &iRenderer,
											const std::string &iRendererApi, const ShaderType &iType) {
	return std::filesystem::path("shaders") / iRenderer / iRendererApi / (iShaderName + getExtension(iType));
}

std::string getExtension(const ShaderType &iStage) {
	auto ext = fmt::format(".{}", magic_enum::enum_name(iStage).substr(0, 4));
	std::ranges::transform(ext.begin(), ext.end(), ext.begin(),
						   [](const unsigned char iChar) { return std::tolower(iChar); });
	return ext;
}

std::string getCacheExtension(const ShaderType &iStage) {
	auto ext = fmt::format(".{}.spv", magic_enum::enum_name(iStage).substr(0, 4));
	std::ranges::transform(ext.begin(), ext.end(), ext.begin(),
						   [](const unsigned char iChar) { return std::tolower(iChar); });
	return ext;
}


std::vector<uint32_t> readCachedShader(const std::filesystem::path &iFile) {
	OWL_PROFILE_FUNCTION()

	std::vector<uint32_t> result;
	std::ifstream in(iFile, std::ios::in | std::ios::binary);
	in.seekg(0, std::ios::end);
	const auto size = in.tellg();
	in.seekg(0, std::ios::beg);
	result.resize(static_cast<size_t>(size) / sizeof(uint32_t));
	in.read(reinterpret_cast<char *>(result.data()), size);
	in.close();
	return result;
}

bool writeCachedShader(const std::filesystem::path &iFile, const std::vector<uint32_t> &iData) {
	OWL_PROFILE_FUNCTION()
	std::ofstream out(iFile, std::ios::out | std::ios::binary);
	if (!exists(iFile.parent_path()))
		OWL_CORE_WARN("Cache directory {} does not exists, creating.", iFile.parent_path().string())

	if (out.is_open()) {
		out.write(reinterpret_cast<const char *>(iData.data()), static_cast<int64_t>(iData.size() * sizeof(uint32_t)));
		out.flush();
		out.close();
		return true;
	}
	OWL_CORE_WARN("Cannot open file {} for writting.", iFile.string())
	return false;
}

shaderc_shader_kind shaderStageToShaderC(const ShaderType &iStage) {
	switch (iStage) {
		case ShaderType::Vertex:
			return shaderc_glsl_vertex_shader;
		case ShaderType::Fragment:
			return shaderc_glsl_fragment_shader;
		case ShaderType::Geometry:
			return shaderc_glsl_geometry_shader;
		case ShaderType::Compute:
			return shaderc_glsl_compute_shader;
		case ShaderType::None:
			break;
	}
	OWL_CORE_ASSERT(false, "Unsupported Shader Type")
	return static_cast<shaderc_shader_kind>(0);
}

void shaderReflect(const std::string &iShaderName, const std::string &iRenderer, const std::string &iRendererApi,
				   const ShaderType iStage, const std::vector<uint32_t> &iShaderData) {
	const spirv_cross::Compiler compiler(iShaderData);
	const spirv_cross::ShaderResources resources = compiler.get_shader_resources();

	OWL_CORE_TRACE("Shader reflect - {0} : <assets>/{1}", magic_enum::enum_name(iStage),
				   renderer::utils::getRelativeShaderPath(iShaderName, iRenderer, iRendererApi, iStage).string())
	OWL_CORE_TRACE("    {} resources", resources.sampled_images.size())
	if (resources.uniform_buffers.empty()) {
		OWL_CORE_TRACE("  No Uniform buffer")
	} else {
		OWL_CORE_TRACE("  Uniform buffers:")
		for (const auto &resource: resources.uniform_buffers) {
			const auto &bufferType = compiler.get_type(resource.base_type_id);
			OWL_CORE_TRACE("   {}", resource.name)
			OWL_CORE_TRACE("     Size = {}", compiler.get_declared_struct_size(bufferType))
			OWL_CORE_TRACE("     Binding = {}", compiler.get_decoration(resource.id, spv::DecorationBinding))
			OWL_CORE_TRACE("     Members = {}", bufferType.member_types.size())
		}
	}
}

}// namespace owl::renderer::utils
