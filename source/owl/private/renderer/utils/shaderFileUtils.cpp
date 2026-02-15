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

#include "renderer/Renderer.h"

namespace owl::renderer::utils {

auto getCacheDirectory(const std::string& iRenderer, const std::string& iRendererApi) -> std::filesystem::path {
	auto output = core::Application::get().getWorkingDirectory() / "cache" / "shader";
	if (!iRenderer.empty())
		output /= iRenderer;
	if (!iRendererApi.empty())
		output /= iRendererApi;
	return output;
}

void createCacheDirectoryIfNeeded(const std::string& iRenderer, const std::string& iRendererApi) {
	if (const std::filesystem::path cacheDirectory = getCacheDirectory(iRenderer, iRendererApi);
		!exists(cacheDirectory)) {
		create_directories(cacheDirectory);
		if (!exists(cacheDirectory))
			OWL_CORE_ERROR("Cannot Create directory {}.", cacheDirectory.string())
	}
}

auto getShaderCachedPath(const std::string& iShaderName, const std::string& iRenderer, const std::string& iRendererApi,
						 const ShaderType& iType) -> std::filesystem::path {
	return getCacheDirectory(iRenderer, iRendererApi) / (iShaderName + getCacheExtension(iType));
}

auto getShaderPath(const std::string& iShaderName, const std::string& iRenderer, const std::string& iRendererApi,
				   const ShaderType& iType) -> std::filesystem::path {
	return Renderer::getTextureLibrary()
			.find(std::format("shaders/{}/{}/{}{}", iRenderer, iRendererApi, iShaderName, getExtension(iType)))
			.value_or(std::filesystem::path{});
}

auto getRelativeShaderPath(const std::string& iShaderName, const std::string& iRenderer,
						   const std::string& iRendererApi, const ShaderType& iType) -> std::filesystem::path {
	return std::filesystem::path("shaders") / iRenderer / iRendererApi / (iShaderName + getExtension(iType));
}

auto getExtension(const ShaderType& iStage) -> std::string {
	auto ext = std::format(".{}", magic_enum::enum_name(iStage).substr(0, 4));
	std::ranges::transform(ext.begin(), ext.end(), ext.begin(),
						   [](const unsigned char iChar) { return std::tolower(iChar); });
	return ext;
}

auto getCacheExtension(const ShaderType& iStage) -> std::string {
	auto ext = std::format(".{}.spv", magic_enum::enum_name(iStage).substr(0, 4));
	std::ranges::transform(ext.begin(), ext.end(), ext.begin(),
						   [](const unsigned char iChar) { return std::tolower(iChar); });
	return ext;
}


auto readCachedShader(const std::filesystem::path& iFile) -> std::vector<uint32_t> {
	OWL_PROFILE_FUNCTION()

	std::vector<uint32_t> result;
	std::ifstream in(iFile, std::ios::in | std::ios::binary);
	in.seekg(0, std::ios::end);
	const auto size = in.tellg();
	in.seekg(0, std::ios::beg);
	result.resize(static_cast<size_t>(size) / sizeof(uint32_t));
	in.read(reinterpret_cast<char*>(result.data()), size);
	in.close();
	return result;
}

auto writeCachedShader(const std::filesystem::path& iFile, const std::vector<uint32_t>& iData) -> bool {
	OWL_PROFILE_FUNCTION()
	std::ofstream out(iFile, std::ios::out | std::ios::binary);
	if (!exists(iFile.parent_path()))
		OWL_CORE_WARN("Cache directory {} does not exists, creating.", iFile.parent_path().string())

	if (out.is_open()) {
		out.write(reinterpret_cast<const char*>(iData.data()), static_cast<int64_t>(iData.size() * sizeof(uint32_t)));
		out.flush();
		out.close();
		return true;
	}
	OWL_CORE_WARN("Cannot open file {} for writting.", iFile.string())
	return false;
}

auto shaderStageToShaderC(const ShaderType& iStage) -> shaderc_shader_kind {
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

auto shaderReflect(const std::string& iShaderName, const std::string& iRenderer, const std::string& iRendererApi,
				   const ShaderType iStage, const std::vector<uint32_t>& iShaderData) -> ShaderReflectionData {
	ShaderReflectionData result;
	OWL_CORE_TRACE("Shader reflect - {0} : <assets>/{1}", magic_enum::enum_name(iStage),
				   renderer::utils::getRelativeShaderPath(iShaderName, iRenderer, iRendererApi, iStage).string())
	if (iShaderData.empty())
		return result;
	const spirv_cross::Compiler compiler(iShaderData);
	const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
	OWL_CORE_TRACE("    {} sampled images", resources.sampled_images.size())
	for (const auto& resource: resources.uniform_buffers) {
		const auto& bufferType = compiler.get_type(resource.base_type_id);
		const auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
		const auto size = compiler.get_declared_struct_size(bufferType);
		OWL_CORE_TRACE("  Uniform buffer: {} Size={} Binding={} Members={}", resource.name, size, binding,
					   bufferType.member_types.size())
		result.uniformBuffers.push_back({.name = resource.name,
										 .binding = binding,
										 .size = size,
										 .memberCount = bufferType.member_types.size()});
	}
	for (const auto& resource: resources.sampled_images) {
		const auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
		const auto& type = compiler.get_type(resource.type_id);
		const uint32_t descriptorCount = type.array.empty() ? 1 : type.array[0];
		OWL_CORE_TRACE("  Sampled image: {} Binding={} Count={}", resource.name, binding, descriptorCount)
		result.sampledImages.push_back(
				{.name = resource.name, .binding = binding, .descriptorCount = descriptorCount});
	}
	return result;
}

auto computeShaderHash(const std::string& iSource) -> std::string {
	std::hash<std::string> hasher;
	return std::to_string(hasher(iSource));
}

auto isShaderCacheValid(const std::filesystem::path& iCachedPath, const std::string& iSource) -> bool {
	if (!exists(iCachedPath))
		return false;
	const auto hashPath = std::filesystem::path(iCachedPath.string() + ".hash");
	if (!exists(hashPath))
		return false;
	std::ifstream in(hashPath, std::ios::in);
	if (!in.is_open())
		return false;
	std::string storedHash;
	std::getline(in, storedHash);
	in.close();
	return storedHash == computeShaderHash(iSource);
}

void writeShaderHash(const std::filesystem::path& iCachedPath, const std::string& iSource) {
	const auto hashPath = std::filesystem::path(iCachedPath.string() + ".hash");
	std::ofstream out(hashPath, std::ios::out);
	if (out.is_open()) {
		out << computeShaderHash(iSource);
		out.close();
	}
}

}// namespace owl::renderer::utils
