/**
 * @file Shader.h
 * @author Silmaen
 * @date 07/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/Shader.h"

namespace owl::renderer::opengl {
/**
 * @brief Specialized class managing OpenGL shader.
 */
class OWL_API Shader final : public renderer::Shader {
public:
	Shader(const Shader&) = delete;
	Shader(Shader&&) = delete;
	auto operator=(const Shader&) -> Shader& = delete;
	auto operator=(Shader&&) -> Shader& = delete;

	/**
	 * @brief Constructor.
	 * @param[in] iShaderName Shader's name.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 * @param[in] iVertexSrc Source of the vertex shader.
	 * @param[in] iFragmentSrc Source of fragment shader.
	 */
	Shader(const std::string& iShaderName, const std::string& iRenderer, const std::string& iVertexSrc,
		   const std::string& iFragmentSrc);

	/**
	 * @brief Constructor.
	 * @param[in] iShaderName Shader's name.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 * @param[in] iSources The shader's sources with type.
	 */
	Shader(const std::string& iShaderName, const std::string& iRenderer,
		   const std::unordered_map<ShaderType, std::string>& iSources);

	/**
	 * @brief Constructor.
	 * @param[in] iShaderName Shader's name.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 * @param[in] iSources The shader source's path with type.
	 */
	Shader(const std::string& iShaderName, const std::string& iRenderer,
		   const std::vector<std::filesystem::path>& iSources);

	/**
	 * @brief Destructor.
	 */
	~Shader() override;

	/**
	 * @brief Activate the shader on the GPU.
	 */
	void bind() const override;

	/**
	 * @brief Deactivate the shader on the GPU.
	 */
	void unbind() const override;

	/**
	 * @brief Set shader's internal int variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	void setInt(const std::string& iName, int iValue) override;

	/**
	 * @brief Set shader's internal int variable array.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValues Shader's variable's raw values.
	 * @param[in] iCount Amount values.
	 */
	void setIntArray(const std::string& iName, int* iValues, uint32_t iCount) override;

	/**
	 * @brief Set shader's internal int variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	void setFloat(const std::string& iName, float iValue) override;

	/**
	 * @brief Set shader's internal vector 2 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	void setFloat2(const std::string& iName, const math::vec2& iValue) override;

	/**
	 * @brief Set shader's internal vector 3 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	void setFloat3(const std::string& iName, const math::vec3& iValue) override;

	/**
	 * @brief Set shader's internal vector 4 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	void setFloat4(const std::string& iName, const math::vec4& iValue) override;

	/**
	 * @brief Set shader's internal Matrix 4 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	void setMat4(const std::string& iName, const math::mat4& iValue) override;

	/**
	 * @brief Push a matrix to the GPU.
	 * @param[in] iName Variable's name.
	 * @param[in] iMatrix The matrix data.
	 */
	void uploadUniformMat3(const std::string& iName, const math::mat3& iMatrix) const;

	/**
	 * @brief Push a matrix to the GPU.
	 * @param[in] iName Variable's name.
	 * @param[in] iMatrix The matrix data.
	 */
	void uploadUniformMat4(const std::string& iName, const math::mat4& iMatrix) const;

	/**
	 * @brief Push an int to the GPU.
	 * @param[in] iName Variable's name.
	 * @param[in] iData The int to push.
	 */
	void uploadUniformInt(const std::string& iName, int iData) const;

	/**
	 * @brief Push an int variable array to the GPU.
	 * @param[in] iName Variable's name.
	 * @param[in] iValues Variable's raw values.
	 * @param[in] iCount Amount values.
	 */
	void uploadUniformIntArray(const std::string& iName, const int* iValues, uint32_t iCount) const;

	/**
	 * @brief Push a float to the GPU.
	 * @param[in] iName Variable's name.
	 * @param[in] iValue The float to push.
	 */
	void uploadUniformFloat(const std::string& iName, float iValue) const;

	/**
	 * @brief Push a 2-vector to the GPU.
	 * @param[in] iName Variable's name.
	 * @param[in] iValue The vector to push.
	 */
	void uploadUniformFloat2(const std::string& iName, const math::vec2& iValue) const;

	/**
	 * @brief Push a 3-vector to the GPU.
	 * @param[in] iName Variable's name.
	 * @param[in] iValue The vector to push.
	 */
	void uploadUniformFloat3(const std::string& iName, const math::vec3& iValue) const;

	/**
	 * @brief Push a 4-vector to the GPU.
	 * @param[in] iName Variable's name.
	 * @param[in] iValue The vector to push.
	 */
	void uploadUniformFloat4(const std::string& iName, const math::vec4& iValue) const;

private:
	/// Id of the shader in the GPU.
	uint32_t m_programId = 0;

	/**
	 * @brief Compile the given shader source code.
	 * @param[in] iSources The shader's sources with type.
	 */
	void compile(const std::unordered_map<ShaderType, std::string>& iSources);

	void compileOrGetOpenGlBinaries(const std::unordered_map<ShaderType, std::string>& iSources);
	void createProgram();

	std::unordered_map<ShaderType, std::vector<uint32_t>> m_openGlSpirv;
};
}// namespace owl::renderer::opengl
