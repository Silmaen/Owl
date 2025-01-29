/**
 * @file Shader.h
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/Shader.h"

namespace owl::renderer::null {
/**
 * @brief Specialized class managing null shader.
 */
class OWL_API Shader final : public owl::renderer::Shader {
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
};
}// namespace owl::renderer::null
