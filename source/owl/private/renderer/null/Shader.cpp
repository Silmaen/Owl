/**
 * @file Shader.cpp
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Shader.h"

namespace owl::renderer::null {

Shader::Shader(const std::string &iShaderName, const std::string &iRenderer, const std::string &,
               const std::string &) : renderer::Shader{iShaderName, iRenderer} {}

Shader::Shader(const std::string &iShaderName, const std::string &iRenderer,
               const std::unordered_map<ShaderType, std::string> &) : renderer::Shader{iShaderName, iRenderer} {}

Shader::Shader(const std::string &iShaderName, const std::string &iRenderer,
               const std::vector<std::filesystem::path> &) : renderer::Shader{iShaderName, iRenderer} {}

Shader::~Shader() = default;

void Shader::bind() const {}

void Shader::unbind() const {}

void Shader::setInt(const std::string &, int) {}

void Shader::setIntArray(const std::string &, int *, uint32_t) {}

void Shader::setFloat(const std::string &, float) {}

void Shader::setFloat2(const std::string &, const math::vec2 &) {}

void Shader::setFloat3(const std::string &, const math::vec3 &) {}

void Shader::setFloat4(const std::string &, const math::vec4 &) {}

void Shader::setMat4(const std::string &, const math::mat4 &) {}


}// namespace owl::renderer::null
