/**
 * @file Shader.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "DrawData.h"
#include "Shader.h"
#include "app/Application.h"
#include "internal/VulkanHandler.h"
#include "internal/utils.h"
#include "platform/FileUtils.h"
#include "renderer/utils/shaderFileUtils.h"

#include <format>

namespace owl::renderer::gpu::vulkan {

namespace utils {

namespace {
// NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
[[maybe_unused]] auto shaderStageToVkStageBit(const ShaderType& iStage) -> VkShaderStageFlagBits {
	switch (iStage) {
		case ShaderType::Vertex:
			return VK_SHADER_STAGE_VERTEX_BIT;
		case ShaderType::Fragment:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		case ShaderType::Geometry:
			return VK_SHADER_STAGE_GEOMETRY_BIT;
		case ShaderType::Compute:
			return VK_SHADER_STAGE_COMPUTE_BIT;
		case ShaderType::None:
			return static_cast<VkShaderStageFlagBits>(0);
	}
	return static_cast<VkShaderStageFlagBits>(0);
}
// NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)

auto createShaderModule(const VkDevice& iLogicalDevice, const std::vector<uint32_t>& iCode) -> VkShaderModule {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = iCode.size() * sizeof(uint32_t);
	createInfo.pCode = iCode.data();
	VkShaderModule shaderModule = nullptr;
	if (const VkResult result = vkCreateShaderModule(iLogicalDevice, &createInfo, nullptr, &shaderModule);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("failed to create shader module ({}).", internal::resultString(result))
		return nullptr;
	}
	return shaderModule;
}
}// namespace

}// namespace utils

Shader::Shader(const std::string& iShaderName, const std::string& iRenderer, const std::string& /*iVertexSrc*/,
			   const std::string& /*iFragmentSrc*/)
	: renderer::gpu::Shader{iShaderName,
							iRenderer} {OWL_PROFILE_FUNCTION()

												OWL_CORE_WARN("Vulkan Shader: Separate vertex/fragment source. "
															  "constructor is deprecated, use Slang source.")}

	  Shader::Shader(const std::string& iShaderName, const std::string& iRenderer, const std::string& iSlangSource)
	: renderer::gpu::Shader{iShaderName, iRenderer} {
	OWL_PROFILE_FUNCTION()

	createShader(iSlangSource);
}

Shader::Shader(const std::string& iShaderName, const std::string& iRenderer,
			   const std::vector<std::filesystem::path>& iSources)
	: renderer::gpu::Shader{iShaderName, iRenderer} {

	OWL_PROFILE_FUNCTION()

	if (iSources.size() == 1 && iSources[0].extension() == ".slang") {
		createShader(platform::fileToString(iSources[0]));
	} else {
		OWL_CORE_ERROR("Vulkan Shader: Expected a single .slang file, got {} files.", iSources.size())
	}
}

Shader::~Shader() = default;

void Shader::bind() const {}

void Shader::unbind() const {}

void Shader::setInt(const std::string&, int) {}

void Shader::setIntArray(const std::string&, int*, uint32_t) {}

void Shader::setFloat(const std::string&, float) {}

void Shader::setFloat2(const std::string&, const math::vec2&) {}

void Shader::setFloat3(const std::string&, const math::vec3&) {}

void Shader::setFloat4(const std::string&, const math::vec4&) {}

void Shader::setMat4(const std::string&, const math::mat4&) {}

void Shader::createShader(const std::string& iSlangSource) {
	OWL_SCOPE_UNTRACK
	OWL_PROFILE_FUNCTION()

	const auto start = std::chrono::steady_clock::now();

	m_slangSource = iSlangSource;
	if (auto binaries = compileOrGetVulkanBinaries(iSlangSource); binaries.has_value()) {
		m_vulkanSpirv = std::move(binaries->stages);
		m_spirvOrigin = std::move(binaries->origin);
	}

	const auto timer = std::chrono::steady_clock::now() - start;
	double duration =
			static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(timer).count()) / 1000.0;
	OWL_CORE_INFO("Compilation of shader {} in {} ms.", getName(), duration)
}

auto Shader::recompile(const std::string& iSlangSource) -> bool {
	OWL_PROFILE_FUNCTION()

	auto binaries = compileOrGetVulkanBinaries(iSlangSource);
	if (!binaries.has_value())
		return false;
	m_vulkanSpirv = std::move(binaries->stages);
	m_spirvOrigin = std::move(binaries->origin);
	m_slangSource = iSlangSource;
	return true;
}

auto Shader::recoverRejectedSpirv(const std::string_view iReason) -> bool {
	auto binaries = renderer::utils::recompileSpirv(m_slangSource, getName(), getRenderer(), /*iForVulkan=*/true,
													m_spirvOrigin, iReason);
	if (!binaries.has_value())
		return false;
	m_vulkanSpirv = std::move(binaries->stages);
	m_spirvOrigin = std::move(binaries->origin);
	return true;
}

void Shader::onRecompiled() { DrawData::rebuildPipelines(*this); }

auto Shader::compileOrGetVulkanBinaries(const std::string& iSlangSource) const
		-> std::optional<renderer::utils::LoadedSpirv> {
	OWL_PROFILE_FUNCTION()

	auto shaderData =
			renderer::utils::loadOrCompileSpirv(iSlangSource, getName(), getRenderer(),
												/*iForVulkan=*/true, {ShaderType::Vertex, ShaderType::Fragment});
	if (!shaderData.has_value())
		return std::nullopt;
	for (auto&& [stage, data]: shaderData->stages)
		renderer::utils::shaderReflect(getName(), getRenderer(), "vulkan", stage, data);
	return shaderData;
}

// NOLINTNEXTLINE(misc-no-recursion): one retry at most, after the rejected SPIR-V is compiled again.
auto Shader::getStagesInfo() -> std::vector<VkPipelineShaderStageCreateInfo> {
	auto& vkh = internal::VulkanHandler::get();
	const auto& vkc = internal::VulkanCore::get();
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	for (auto&& [stage, code]: m_vulkanSpirv) {
		shaderStages.emplace_back();
		shaderStages.back().sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages.back().stage = utils::shaderStageToVkStageBit(stage);
		shaderStages.back().module = utils::createShaderModule(vkc.getLogicalDevice(), code);
		if (shaderStages.back().module == nullptr) {
			shaderStages.pop_back();
			for (const auto& created: shaderStages)
				vkDestroyShaderModule(vkc.getLogicalDevice(), created.module, nullptr);
			if (recoverRejectedSpirv(
						std::format("vkCreateShaderModule failed for the {} stage", magic_enum::enum_name(stage))))
				return getStagesInfo();
			OWL_CORE_ERROR("Vulkan: Failed create shader module {} {}.", getName(), magic_enum::enum_name(stage))
			vkh.setState(internal::VulkanHandler::State::ErrorCreatingPipeline);
			return {};
		}
		shaderStages.back().pName = "main";
	}
	return shaderStages;
}

}// namespace owl::renderer::gpu::vulkan
