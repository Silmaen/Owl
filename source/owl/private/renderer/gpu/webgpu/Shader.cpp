/**
 * @file Shader.cpp
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Shader.h"

#include "Context.h"
#include "data/assets/AssetSearchPaths.h"
#include "renderer/utils/shaderFileUtils.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <optional>
#include <regex>
#include <string>

namespace owl::renderer::gpu::webgpu {

namespace {

auto lowerName(const std::string_view iName) -> std::string {
	std::string lower(iName);
	std::ranges::transform(lower, lower.begin(),
						   [](const char iChar) -> char { return static_cast<char>(std::tolower(iChar)); });
	return lower;
}

auto readStage(const std::string& iName, const std::string& iRenderer, const ShaderType iStage)
		-> std::optional<std::string> {
	const auto bytes = data::assets::readAssetBytes(utils::getPrecompiledWgslPath(iName, iRenderer, iStage));
	if (!bytes.has_value())
		return std::nullopt;
	return std::string(bytes->begin(), bytes->end());
}

// The `@location(N) name_K : type` fields of the input structure of the vertex entry point (`vertexInput_K`).
auto readInputLocations(const std::string& iWgsl) -> std::unordered_map<std::string, uint32_t> {
	std::unordered_map<std::string, uint32_t> locations;
	const auto start = iWgsl.find("struct vertexInput");
	if (start == std::string::npos)
		return locations;
	const auto end = iWgsl.find("};", start);
	static const std::regex field{R"(@location\((\d+)\)\s+(\w+?)(_\d+)?\s*:)"};
	const auto body = iWgsl.substr(start, end == std::string::npos ? std::string::npos : end - start);
	for (std::sregex_iterator it{body.begin(), body.end(), field}; it != std::sregex_iterator{}; ++it)
		locations[lowerName((*it)[2].str())] = static_cast<uint32_t>(std::stoul((*it)[1].str()));
	return locations;
}

auto loadStage(const std::string& iName, const std::string& iRenderer, const std::optional<std::string>& iWgsl)
		-> wgpu::ShaderModule {
	if (!iWgsl.has_value())
		return nullptr;
	// Texture tables sample in a switch on a varying index, where WGSL (not SPIR-V nor GLSL) refuses derivatives.
	const std::string code = "diagnostic(off, derivative_uniformity);\n" + *iWgsl;
	wgpu::ShaderSourceWGSL wgsl{};
	wgsl.code = code.c_str();
	const auto label = std::format("{}/{}", iRenderer, iName);
	const wgpu::ShaderModuleDescriptor desc{.nextInChain = &wgsl, .label = label.c_str()};
	return Context::get().device.CreateShaderModule(&desc);
}

}// namespace

Shader::Shader(const std::string& iShaderName, const std::string& iRenderer)
	: renderer::gpu::Shader{iShaderName, iRenderer} {
	const auto vertex = readStage(iShaderName, iRenderer, ShaderType::Vertex);
	m_vertex = loadStage(iShaderName, iRenderer, vertex);
	m_fragment = loadStage(iShaderName, iRenderer, readStage(iShaderName, iRenderer, ShaderType::Fragment));
	m_compute = loadStage(iShaderName, iRenderer, readStage(iShaderName, iRenderer, ShaderType::Compute));
	if (vertex.has_value())
		m_inputLocations = readInputLocations(*vertex);
	if (m_vertex == nullptr && m_compute == nullptr)
		OWL_CORE_ERROR("WebGPU: No baked WGSL for shader '{}/{}'; export the game again.", iRenderer, iShaderName)
}

Shader::~Shader() = default;

auto Shader::getInputLocation(const std::string_view iElement) const -> std::optional<uint32_t> {
	const auto name = lowerName(iElement.starts_with("i_") ? iElement.substr(2) : iElement);
	if (const auto found = m_inputLocations.find(name); found != m_inputLocations.end())
		return found->second;
	return std::nullopt;
}

}// namespace owl::renderer::gpu::webgpu
