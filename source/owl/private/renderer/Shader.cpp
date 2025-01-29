/**
 * @file Shader.cpp
 * @author Silmaen
 * @date 07/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Shader.h"
#include "core/Application.h"
#include "null/Shader.h"
#include "opengl/Shader.h"
#include "renderer/Renderer.h"
#include "vulkan/Shader.h"

#include "utils/shaderFileUtils.h"

#include <magic_enum/magic_enum.hpp>

namespace owl::renderer {

auto Shader::create(const Specification& iShaderName) -> shared<Shader> {
	const auto type = RenderCommand::getApi();
	std::filesystem::path shaderDir = std::filesystem::path{"shaders"};
	if (!iShaderName.shaderName.renderer.empty()) {
		shaderDir /= iShaderName.shaderName.renderer;
	}
	if (type == RenderAPI::Type::OpenGL)
		shaderDir /= "opengl";
	else if (type == RenderAPI::Type::Vulkan)
		shaderDir /= "vulkan";
	else
		shaderDir /= "null";
	shaderDir /= iShaderName.shaderName.name;
	if (RenderCommand::requireInit()) {
		const auto result = Renderer::getTextureLibrary().find(fmt::format("{}.vert", shaderDir.string()));
		if (!result.has_value()) {
			OWL_CORE_ERROR("Shader::Create: Failed to load shader {}, unable to find assets",
						   iShaderName.shaderName.name)
			return nullptr;
		}
		shaderDir = result.value().parent_path() / iShaderName.shaderName.name;
	}
	return create(shaderDir);
}

auto Shader::create(const std::filesystem::path& iFile) -> shared<Shader> {
	std::vector<std::filesystem::path> sources;
	auto name = iFile.stem().string();
	auto renderer = iFile.parent_path().parent_path().filename().string();
	auto shad = composeName({name, renderer});
	if (RenderCommand::requireInit()) {
		if (exists(iFile) && is_regular_file(iFile)) {
			sources = {iFile};
		} else if (is_directory(iFile)) {
			for (const auto& f: std::filesystem::directory_iterator(iFile)) {
				if (f.path().stem() != name)
					continue;
				sources.push_back(f);
			}
			if (sources.empty()) {
				OWL_CORE_WARN("Not able to find Shader {} in directory {}", shad, iFile.string())
				return nullptr;
			}
		} else {
			if (exists(iFile.parent_path())) {
				for (const auto& f: std::filesystem::directory_iterator(iFile.parent_path())) {
					if (f.path().stem() != name)
						continue;
					sources.push_back(f);
				}
			}
			if (sources.empty()) {
				OWL_CORE_WARN("Not able to find Shader {} for {}", shad, iFile.string())
				return nullptr;
			}
		}
	}
	OWL_CORE_TRACE("Try to create shader {} for renderer {} / API {}.", name, renderer,
				   magic_enum::enum_name(RenderCommand::getApi()))
	shared<Shader> shader = nullptr;
	switch (RenderCommand::getApi()) {
		case RenderAPI::Type::Null:
			shader = mkShared<null::Shader>(name, renderer, sources);
			break;
		case RenderAPI::Type::OpenGL:
			shader = mkShared<opengl::Shader>(name, renderer, sources);
			break;
		case RenderAPI::Type::Vulkan:
			shader = mkShared<vulkan::Shader>(name, renderer, sources);
			break;
	}
	sources.clear();
	sources.shrink_to_fit();
	return shader;
}

Shader::~Shader() = default;

auto Shader::decomposeName(const std::string& iFullName) -> ShaderName {
	ShaderName result;
	auto pos = iFullName.find('#');
	if (pos == std::string::npos) {
		result.name = iFullName;
		result.renderer = "";
	} else {
		result.renderer = iFullName.substr(0, pos);
		result.name = iFullName.substr(pos + 1);
		if (result.name.empty()) {
			OWL_CORE_ERROR("Shader::decomposeName: Failed to decompose shader name {}", iFullName)
			result.renderer = "";
		}
	}
	return result;
}

auto Shader::composeName(const ShaderName& iNames) -> std::string {
	if (iNames.renderer.empty())
		return iNames.name;
	return fmt::format("{}#{}", iNames.renderer, iNames.name);
}

}// namespace owl::renderer
