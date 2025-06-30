/**
 * @file Shader.cpp
 * @author Silmaen
 * @date 07/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Shader.h"
#include "core/Application.h"
#include "core/external/opengl46.h"
#include "core/external/shaderc.h"
#include "core/utils/FileUtils.h"
#include "renderer/utils/shaderFileUtils.h"

namespace owl::renderer::opengl {

namespace utils {
namespace {
auto shaderStageToGlShader(const ShaderType& iStage) -> uint32_t {
	switch (iStage) {
		case ShaderType::Vertex:
			return GL_VERTEX_SHADER;
		case ShaderType::Fragment:
			return GL_FRAGMENT_SHADER;
		case ShaderType::Geometry:
			return GL_GEOMETRY_SHADER;
		case ShaderType::Compute:
			return GL_COMPUTE_SHADER;
		case ShaderType::None:
			break;
	}
	OWL_CORE_ASSERT(false, "Unsupported Shader Type")
	return 0;
}
}// namespace

}// namespace utils

Shader::Shader(const std::string& iShaderName, const std::string& iRenderer, const std::string& iVertexSrc,
			   const std::string& iFragmentSrc)
	: renderer::Shader{iShaderName, iRenderer} {
	compile({{ShaderType::Vertex, iVertexSrc}, {ShaderType::Fragment, iFragmentSrc}});
}

Shader::Shader(const std::string& iShaderName, const std::string& iRenderer,
			   const std::unordered_map<ShaderType, std::string>& iSources)
	: renderer::Shader{iShaderName, iRenderer} {
	compile(iSources);
}

Shader::Shader(const std::string& iShaderName, const std::string& iRenderer,
			   const std::vector<std::filesystem::path>& iSources)
	: renderer::Shader{iShaderName, iRenderer} {
	OWL_PROFILE_FUNCTION()

	std::unordered_map<ShaderType, std::string> strSources;
	for (const auto& src: iSources) {
		auto type = ShaderType::None;
		if (src.extension() == ".frag")
			type = ShaderType::Fragment;
		if (src.extension() == ".vert")
			type = ShaderType::Vertex;
		if (type == ShaderType::None) {
			OWL_CORE_ASSERT(false, "Unknown Shader Type")
			continue;
		}
		strSources.emplace(type, core::utils::fileToString(src));
	}
	compile(strSources);
}

Shader::~Shader() {
	OWL_PROFILE_FUNCTION()

	glDeleteProgram(m_programId);
}

void Shader::compile(const std::unordered_map<ShaderType, std::string>& iSources) {
	OWL_SCOPE_UNTRACK
	OWL_PROFILE_FUNCTION()

	const auto start = std::chrono::steady_clock::now();

	// todo: direct check for openGL binaries...
	renderer::utils::createCacheDirectoryIfNeeded(getRenderer(), "opengl_vulkan");
	compileOrGetVulkanBinaries(iSources);
	renderer::utils::createCacheDirectoryIfNeeded(getRenderer(), "opengl");
	compileOrGetOpenGlBinaries();
	createProgram();

	const auto timer = std::chrono::steady_clock::now() - start;
	double duration =
			static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(timer).count()) / 1000.0;
	OWL_CORE_INFO("Compilation of shader {} in {} ms", getName(), duration)
}

void Shader::compileOrGetVulkanBinaries(const std::unordered_map<ShaderType, std::string>& iSources) {
	OWL_PROFILE_FUNCTION()

	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
	options.SetOptimizationLevel(shaderc_optimization_level_performance);

	auto& shaderData = m_vulkanSpirv;
	shaderData.clear();
	for (auto&& [stage, source]: iSources) {
		const std::filesystem::path basePath =
				renderer::utils::getShaderPath(getName(), getRenderer(), "opengl", stage);
		const std::filesystem::path cachedPath =
				renderer::utils::getShaderCachedPath(getName(), getRenderer(), "opengl_vulkan", stage);

		if (exists(cachedPath) && (last_write_time(cachedPath) > last_write_time(basePath))) {
			// Cache exists: read it
			OWL_CORE_INFO("Using cached Vulkan Shader {}-{}", getName(), magic_enum::enum_name(stage))
			shaderData[stage] = renderer::utils::readCachedShader(cachedPath);
		} else {
			if (exists(cachedPath))
				OWL_CORE_INFO("Origin file newer than cached one, Recompiling.")
			const shaderc::Compiler compiler;
			const shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
					source, renderer::utils::shaderStageToShaderC(stage),
					renderer::utils::getShaderPath(getName(), getRenderer(), "opengl", stage).string().c_str(),
					options);
			if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
				OWL_CORE_ERROR(module.GetErrorMessage())
				OWL_CORE_ASSERT(false, "Failed Compilation")
			}
			shaderData[stage] = std::vector<uint32_t>(module.cbegin(), module.cend());
			renderer::utils::writeCachedShader(cachedPath, shaderData[stage]);
		}
	}
	for (auto&& [stage, data]: shaderData)
		renderer::utils::shaderReflect(getName(), getRenderer(), "opengl", stage, data);
}

void Shader::compileOrGetOpenGlBinaries() {
	OWL_PROFILE_FUNCTION()

	auto& shaderData = m_openGlSpirv;

	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_opengl, shaderc_env_version_opengl_4_5);
	options.SetOptimizationLevel(shaderc_optimization_level_performance);

	shaderData.clear();
	m_openGlSource.clear();
	for (auto&& [stage, spirv]: m_vulkanSpirv) {
		const std::filesystem::path basePath =
				renderer::utils::getShaderCachedPath(getName(), getRenderer(), "opengl_vulkan", stage);
		const std::filesystem::path cachedPath =
				renderer::utils::getShaderCachedPath(getName(), getRenderer(), "opengl", stage);
		if (exists(cachedPath) && (last_write_time(cachedPath) > last_write_time(basePath))) {
			OWL_CORE_INFO("Using cached OpenGL Shader {}-{}", getName(), magic_enum::enum_name(stage))
			shaderData[stage] = renderer::utils::readCachedShader(cachedPath);
		} else {
			if (exists(cachedPath))
				OWL_CORE_INFO("Origin file newer than cached one, Recompiling.")
			spirv_cross::CompilerGLSL glslCompiler(spirv);
			m_openGlSource[stage] = glslCompiler.compile();
			auto& source = m_openGlSource[stage];
			const shaderc::Compiler compiler;
			const shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
					source, renderer::utils::shaderStageToShaderC(stage),
					renderer::utils::getShaderPath(getName(), getRenderer(), "opengl", stage).string().c_str());
			if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
				OWL_CORE_ERROR(module.GetErrorMessage())
				OWL_CORE_ASSERT(false, "Compilation error")
			}
			shaderData[stage] = std::vector<uint32_t>(module.cbegin(), module.cend());
			renderer::utils::writeCachedShader(cachedPath, shaderData[stage]);
		}
	}
}

void Shader::createProgram() {
	const GLuint program = glCreateProgram();

	// list of shader's id
	std::vector<GLuint> shaderIDs;
	for (auto&& [stage, spirv]: m_openGlSpirv) {
		const GLuint shaderId = shaderIDs.emplace_back(glCreateShader(utils::shaderStageToGlShader(stage)));
		OWL_CORE_ASSERT(!spirv.empty(), "Empty shader data")
		glShaderBinary(1, &shaderId, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv.data(),
					   static_cast<GLsizei>(spirv.size() * sizeof(uint32_t)));
		glSpecializeShader(shaderId, "main", 0, nullptr, nullptr);
		glAttachShader(program, shaderId);
	}
	glLinkProgram(program);
	GLint isLinked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE) {
		OWL_CORE_ERROR("Shader linking failed ({})", getName())
		GLint maxLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
		if (maxLength > 0) {
			std::vector<GLchar> infoLog(static_cast<size_t>(maxLength));
			glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());
			OWL_CORE_ERROR("     Details: {}", infoLog.data())
		}
		glDeleteProgram(program);
		for (const auto id: shaderIDs) glDeleteShader(id);
		OWL_CORE_ASSERT(false, std::format("Failed to create shader {}", getName()))
		return;
	}
	for (const auto id: shaderIDs) {
		glDetachShader(program, id);
		glDeleteShader(id);
	}
	m_programId = program;
}

void Shader::bind() const {
	OWL_PROFILE_FUNCTION()

	glUseProgram(m_programId);
}

void Shader::unbind() const {
	OWL_PROFILE_FUNCTION()

	glUseProgram(0);
}

void Shader::setInt(const std::string& iName, const int iValue) {
	OWL_PROFILE_FUNCTION()

	uploadUniformInt(iName, iValue);
}

void Shader::setIntArray(const std::string& iName, int* iValues, const uint32_t iCount) {
	OWL_PROFILE_FUNCTION()

	uploadUniformIntArray(iName, iValues, iCount);
}

void Shader::setFloat(const std::string& iName, const float iValue) {
	OWL_PROFILE_FUNCTION()

	uploadUniformFloat(iName, iValue);
}

void Shader::setFloat2(const std::string& iName, const math::vec2& iValue) {
	OWL_PROFILE_FUNCTION()

	uploadUniformFloat2(iName, iValue);
}

void Shader::setFloat3(const std::string& iName, const math::vec3& iValue) {
	OWL_PROFILE_FUNCTION()

	uploadUniformFloat3(iName, iValue);
}

void Shader::setFloat4(const std::string& iName, const math::vec4& iValue) {
	OWL_PROFILE_FUNCTION()

	uploadUniformFloat4(iName, iValue);
}

void Shader::setMat4(const std::string& iName, const math::mat4& iValue) {
	OWL_PROFILE_FUNCTION()

	uploadUniformMat4(iName, iValue);
}

void Shader::uploadUniformInt(const std::string& iName, const int iData) const {
	const GLint location = glGetUniformLocation(m_programId, iName.c_str());
	glUniform1i(location, iData);
}

void Shader::uploadUniformIntArray(const std::string& iName, const int* iValues, const uint32_t iCount) const {
	const GLint location = glGetUniformLocation(m_programId, iName.c_str());
	glUniform1iv(location, static_cast<GLsizei>(iCount), iValues);
}

void Shader::uploadUniformFloat(const std::string& iName, const float iValue) const {
	const GLint location = glGetUniformLocation(m_programId, iName.c_str());
	glUniform1f(location, iValue);
}

void Shader::uploadUniformFloat2(const std::string& iName, const math::vec2& iValue) const {
	const GLint location = glGetUniformLocation(m_programId, iName.c_str());
	glUniform2f(location, iValue.x(), iValue.y());
}

void Shader::uploadUniformFloat3(const std::string& iName, const math::vec3& iValue) const {
	const GLint location = glGetUniformLocation(m_programId, iName.c_str());
	glUniform3f(location, iValue.x(), iValue.y(), iValue.z());
}

void Shader::uploadUniformFloat4(const std::string& iName, const math::vec4& iValue) const {
	const GLint location = glGetUniformLocation(m_programId, iName.c_str());
	glUniform4f(location, iValue.x(), iValue.y(), iValue.z(), iValue.w());
}

void Shader::uploadUniformMat3(const std::string& iName, const math::mat3& iMatrix) const {
	const GLint location = glGetUniformLocation(m_programId, iName.c_str());
	glUniformMatrix3fv(location, 1, GL_FALSE, iMatrix.data());
}

void Shader::uploadUniformMat4(const std::string& iName, const math::mat4& iMatrix) const {
	const GLint location = glGetUniformLocation(m_programId, iName.c_str());
	glUniformMatrix4fv(location, 1, GL_FALSE, iMatrix.data());
}

}// namespace owl::renderer::opengl
