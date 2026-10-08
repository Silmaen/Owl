/**
 * @file Shader.cpp
 * @author Silmaen
 * @date 07/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Shader.h"
#include "app/Application.h"
#include "core/external/opengl46.h"
#include "platform/FileUtils.h"
#include "renderer/utils/shaderFileUtils.h"

#include <format>

namespace owl::renderer::gpu::opengl {

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

auto checkCompileStatus(const GLuint iShader, const std::string& iName) -> bool {
	GLint status = 0;
	glGetShaderiv(iShader, GL_COMPILE_STATUS, &status);
	if (status != GL_FALSE)
		return true;
	GLint logLength = 0;
	glGetShaderiv(iShader, GL_INFO_LOG_LENGTH, &logLength);
	std::string log(static_cast<size_t>(std::max(logLength, 1)), '\0');
	glGetShaderInfoLog(iShader, logLength, &logLength, log.data());
	OWL_CORE_ERROR("OpenGL Shader: Compilation of {} failed ({}).", iName, log.c_str())
	return false;
}

auto isSpirvSupported() -> bool {
	// The debug glad wrappers are never null: test the real pointers, then the version or the extension string.
	if (glad_glSpecializeShader == nullptr || glad_glShaderBinary == nullptr)
		return false;
	GLint major = 0;
	GLint minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	if (major > 4 || (major == 4 && minor >= 6))
		return true;
	GLint extensionCount = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
	for (GLint i = 0; i < extensionCount; ++i) {
		if (const auto* name = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
			name != nullptr && std::string_view{name} == "GL_ARB_gl_spirv")
			return true;
	}
	return false;
}

auto detectShaderFormat() -> ShaderFormat {
	if (const char* forced = std::getenv("OWL_OPENGL_SHADERS"); forced != nullptr) {
		const std::string_view value{forced};
		if (value == "glsl")
			return ShaderFormat::Glsl;
		if (value == "spirv")
			return ShaderFormat::Spirv;
		OWL_CORE_WARN("OpenGL Shader: Unknown OWL_OPENGL_SHADERS value '{}' (expected glsl or spirv).", value)
	}
	return isSpirvSupported() ? ShaderFormat::Spirv : ShaderFormat::Glsl;
}
}// namespace

}// namespace utils

auto getShaderFormat() -> ShaderFormat {
	static const ShaderFormat format = utils::detectShaderFormat();
	return format;
}

auto createShaderObject(const ShaderType iStage, const std::vector<uint32_t>& iSpirv, const std::string& iName)
		-> uint32_t {
	if (iSpirv.empty()) {
		OWL_CORE_ERROR("OpenGL Shader: No SPIR-V for {} ({}).", iName, magic_enum::enum_name(iStage))
		return 0;
	}
	const GLuint shaderId = glCreateShader(utils::shaderStageToGlShader(iStage));
	if (getShaderFormat() == ShaderFormat::Spirv) {
		auto spirv = iSpirv;
		renderer::utils::remapBuiltinsForOpenGl(spirv);
		glShaderBinary(1, &shaderId, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv.data(),
					   static_cast<GLsizei>(spirv.size() * sizeof(uint32_t)));
		glSpecializeShader(shaderId, "main", 0, nullptr, nullptr);
	} else {
		const auto glsl = renderer::utils::crossCompileToGlsl(iSpirv, iName);
		if (!glsl.has_value()) {
			glDeleteShader(shaderId);
			return 0;
		}
		const char* text = glsl->c_str();
		glShaderSource(shaderId, 1, &text, nullptr);
		glCompileShader(shaderId);
	}
	if (!utils::checkCompileStatus(shaderId, iName)) {
		glDeleteShader(shaderId);
		return 0;
	}
	return shaderId;
}

Shader::Shader(const std::string& iShaderName, const std::string& iRenderer, const std::string& /*iVertexSrc*/,
			   const std::string& /*iFragmentSrc*/)
	: renderer::gpu::Shader{iShaderName, iRenderer} {OWL_CORE_WARN(
			  "OpenGL Shader: Separate vertex/fragment source constructor is deprecated, use Slang source.")}

	  Shader::Shader(const std::string& iShaderName, const std::string& iRenderer, const std::string& iSlangSource)
	: renderer::gpu::Shader{iShaderName, iRenderer} {

	compile(iSlangSource);
}

Shader::Shader(const std::string& iShaderName, const std::string& iRenderer,
			   const std::vector<std::filesystem::path>& iSources)
	: renderer::gpu::Shader{iShaderName, iRenderer} {

	OWL_PROFILE_FUNCTION()

	if (iSources.size() == 1 && iSources[0].extension() == ".slang") {
		compile(platform::fileToString(iSources[0]));
	} else {
		OWL_CORE_ERROR("OpenGL Shader: Expected a single .slang file, got {} files.", iSources.size())
	}
}

Shader::~Shader() {
	OWL_PROFILE_FUNCTION()

	glDeleteProgram(m_programId);
}

void Shader::compile(const std::string& iSlangSource) {
	OWL_SCOPE_UNTRACK
	OWL_PROFILE_FUNCTION()

	const auto start = std::chrono::steady_clock::now();

	renderer::utils::createCacheDirectoryIfNeeded(getRenderer(), "opengl");
	if (auto binaries = compileOrGetOpenGlBinaries(iSlangSource); binaries.has_value())
		m_openGlSpirv = std::move(*binaries);
	m_programId = createProgram(m_openGlSpirv);
	OWL_CORE_ASSERT(m_programId != 0, std::format("Failed to create shader {}", getName()))

	const auto timer = std::chrono::steady_clock::now() - start;
	double duration =
			static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(timer).count()) / 1000.0;
	OWL_CORE_INFO("Compilation of shader {} in {} ms.", getName(), duration)
}

auto Shader::recompile(const std::string& iSlangSource) -> bool {
	OWL_PROFILE_FUNCTION()

	auto binaries = compileOrGetOpenGlBinaries(iSlangSource);
	if (!binaries.has_value())
		return false;
	const uint32_t program = createProgram(*binaries);
	if (program == 0)
		return false;
	glDeleteProgram(m_programId);
	m_programId = program;
	m_openGlSpirv = std::move(*binaries);
	return true;
}

auto Shader::compileOrGetOpenGlBinaries(const std::string& iSlangSource) const
		-> std::optional<std::unordered_map<ShaderType, std::vector<uint32_t>>> {
	OWL_PROFILE_FUNCTION()

	std::unordered_map<ShaderType, std::vector<uint32_t>> shaderData;
	const auto cacheKey =
			renderer::utils::getShaderCacheKey(iSlangSource, getRenderer() + "/" + getName(), /*iForVulkan=*/false);
	bool allCached = true;
	for (const auto stage: {ShaderType::Vertex, ShaderType::Fragment}) {
		const auto cachedPath = renderer::utils::getShaderCachedPath(getName(), getRenderer(), "opengl", stage);
		if (!renderer::utils::isShaderCacheValid(cachedPath, cacheKey)) {
			allCached = false;
			break;
		}
	}

	if (allCached) {
		for (const auto stage: {ShaderType::Vertex, ShaderType::Fragment}) {
			const auto cachedPath = renderer::utils::getShaderCachedPath(getName(), getRenderer(), "opengl", stage);

			OWL_CORE_INFO("Using cached OpenGL Shader {}-{}.", getName(), magic_enum::enum_name(stage))
			shaderData[stage] = renderer::utils::readCachedShader(cachedPath);
		}
	} else {
		OWL_CORE_TRACE("Compiling Slang shader '{}' for OpenGL...", getName())
		auto compiled = renderer::utils::compileSlangToSpirv(iSlangSource, getName(), false);
		if (!compiled.success) {
			OWL_CORE_ERROR("Slang compilation failed for shader '{}'.", getName())
			return std::nullopt;
		}
		shaderData = std::move(compiled.spirvData);
		for (auto&& [stage, data]: shaderData) {
			const auto cachedPath = renderer::utils::getShaderCachedPath(getName(), getRenderer(), "opengl", stage);
			if (!renderer::utils::writeCachedShader(cachedPath, data))
				OWL_CORE_WARN("Failed to write the compiled shader.")
			renderer::utils::writeShaderHash(cachedPath, cacheKey);
		}
	}
	for (auto&& [stage, data]: shaderData)
		renderer::utils::shaderReflect(getName(), getRenderer(), "opengl", stage, data);
	return shaderData;
}

auto Shader::createProgram(const std::unordered_map<ShaderType, std::vector<uint32_t>>& iSpirv) const -> uint32_t {
	const GLuint program = glCreateProgram();
	std::vector<GLuint> shaderIDs;
	bool stagesCompiled = !iSpirv.empty();
	for (auto&& [stage, spirv]: iSpirv) {
		const GLuint shaderId = createShaderObject(stage, spirv, getName());
		if (shaderId == 0) {
			stagesCompiled = false;
			continue;
		}
		shaderIDs.push_back(shaderId);
		glAttachShader(program, shaderId);
	}
	GLint isLinked = 0;
	if (stagesCompiled) {
		glLinkProgram(program);
		glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
	}
	if (isLinked == GL_FALSE) {
		OWL_CORE_ERROR("Shader linking failed ({}).", getName())
		GLint maxLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
		if (maxLength > 0) {
			std::vector<GLchar> infoLog(static_cast<size_t>(maxLength));
			glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());
			OWL_CORE_ERROR("     Details: {}.", infoLog.data())
		}
		glDeleteProgram(program);
		for (const auto id: shaderIDs) glDeleteShader(id);
		return 0;
	}
	for (const auto id: shaderIDs) {
		glDetachShader(program, id);
		glDeleteShader(id);
	}
	return program;
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

}// namespace owl::renderer::gpu::opengl
