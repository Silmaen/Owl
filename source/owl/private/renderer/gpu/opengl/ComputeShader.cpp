/**
 * @file ComputeShader.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "ComputeShader.h"

#include "core/external/opengl46.h"
#include "renderer/Renderer.h"
#include "renderer/utils/shaderFileUtils.h"

namespace owl::renderer::gpu::opengl {

namespace {
auto loadSlangSource(const std::string& iShaderName, const std::string& iRenderer) -> std::string {
	// Slang sources live at `shaders/<renderer>/slang/<name>.slang` (getShaderPath builds the wrong GLSL-style path).
	const auto relative =
			std::filesystem::path("shaders") / iRenderer / "slang" / (iShaderName + std::string(".slang"));
	const auto sourcePath =
			Renderer::getTextureLibrary().find(relative.generic_string()).value_or(std::filesystem::path{});
	if (sourcePath.empty() || !exists(sourcePath)) {
		OWL_CORE_ERROR("OpenGL compute shader: source not found for '{}' / '{}'.", iRenderer, iShaderName)
		return {};
	}
	std::ifstream in(sourcePath, std::ios::binary);
	if (!in.is_open()) {
		OWL_CORE_ERROR("OpenGL compute shader: cannot open '{}'.", sourcePath.string())
		return {};
	}
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}
}// namespace

ComputeShader::ComputeShader(const std::string& iShaderName, const std::string& iRenderer)
	: m_name{iShaderName + "@" + iRenderer} {
	const std::string source = loadSlangSource(iShaderName, iRenderer);
	if (source.empty())
		return;
	const auto compiled = renderer::utils::compileSlangToSpirv(source, iShaderName, /*iForVulkan=*/false);
	if (!compiled.success) {
		OWL_CORE_ERROR("OpenGL compute shader: Slang compilation failed for '{}'.", m_name)
		return;
	}
	const auto it = compiled.spirvData.find(ShaderType::Compute);
	if (it == compiled.spirvData.end() || it->second.empty()) {
		OWL_CORE_ERROR("OpenGL compute shader: no `computeMain` entry point in '{}'.", m_name)
		return;
	}
	const auto& spirv = it->second;

	const GLuint program = glCreateProgram();
	const GLuint shaderId = glCreateShader(GL_COMPUTE_SHADER);
	glShaderBinary(1, &shaderId, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv.data(),
				   static_cast<GLsizei>(spirv.size() * sizeof(uint32_t)));
	// Slang emits the entry point under the canonical name `main` in SPIR-V.
	glSpecializeShader(shaderId, "main", 0, nullptr, nullptr);
	GLint compileStatus = 0;
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus == GL_FALSE) {
		GLint logLen = 0;
		glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLen);
		if (logLen > 0) {
			std::vector<GLchar> log(static_cast<size_t>(logLen));
			glGetShaderInfoLog(shaderId, logLen, &logLen, log.data());
			OWL_CORE_ERROR("OpenGL compute shader: specialize failed for '{}': {}.", m_name, log.data())
		}
		glDeleteShader(shaderId);
		glDeleteProgram(program);
		return;
	}
	glAttachShader(program, shaderId);
	glLinkProgram(program);
	GLint isLinked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE) {
		GLint logLen = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
		if (logLen > 0) {
			std::vector<GLchar> log(static_cast<size_t>(logLen));
			glGetProgramInfoLog(program, logLen, &logLen, log.data());
			OWL_CORE_ERROR("OpenGL compute shader: link failed for '{}': {}.", m_name, log.data())
		}
		glDeleteShader(shaderId);
		glDeleteProgram(program);
		return;
	}
	glDetachShader(program, shaderId);
	glDeleteShader(shaderId);
	m_programId = program;
}

ComputeShader::~ComputeShader() {
	if (m_programId != 0)
		glDeleteProgram(m_programId);
	m_programId = 0;
}

void ComputeShader::bindStorageBuffer([[maybe_unused]] const uint32_t iBinding,
									  const shared<renderer::gpu::StorageBuffer>& iBuffer) {
	if (iBuffer)
		iBuffer->bind();
}

void ComputeShader::dispatch(const uint32_t iGroupsX, const uint32_t iGroupsY, const uint32_t iGroupsZ) {
	if (m_programId == 0 || iGroupsX == 0 || iGroupsY == 0 || iGroupsZ == 0)
		return;
	glUseProgram(m_programId);
	glDispatchCompute(iGroupsX, iGroupsY, iGroupsZ);
}

}// namespace owl::renderer::gpu::opengl
