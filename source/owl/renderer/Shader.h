/**
 * @file Shader.h
 * @author Silmaen
 * @date 07/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <filesystem>

namespace owl::renderer {
class ShaderLibrary;

enum class ShaderType : uint8_t {
	None,
	Vertex,
	Fragment,
	Geometry,
	Compute,
};

/**
 * @brief Class Shader.
 */
class OWL_API Shader {
public:
	Shader(const Shader &) = default;
	Shader(Shader &&) = default;
	Shader &operator=(const Shader &) = default;
	Shader &operator=(Shader &&) = default;

	/**
	 * @brief Constructor.
	 * @param[in] iShaderName The shader's name.
	 * @param[in] iRenderer The renderer's name.
	 */
	Shader(std::string iShaderName, std::string iRenderer)
		: m_name{std::move(iShaderName)}, m_renderer{std::move(iRenderer)} {}

	/**
	 * @brief Destructor.
	 */
	virtual ~Shader();

	/**
	 * @brief Activate the shader on the GPU.
	 */
	virtual void bind() const = 0;

	/**
	 * @brief Deactivate the shader on the GPU.
	 */
	virtual void unbind() const = 0;

	/**
	* @brief Create a new shader.
	 * @param[in] iShaderName Shader's name.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 * @return Pointer to the shader.
	 */
	static shared<Shader> create(const std::string &iShaderName, const std::string &iRenderer);

	/**
	* @brief Create a new shader.
	 * @param[in] iShaderName Shader's name.
	 * @param[in] iRenderer Name of the shader's related renderer.
	 * @param[in] iFile Source of the shader.
	 * @return Pointer to the shader.
	 */
	static shared<Shader> create(const std::string &iShaderName, const std::string &iRenderer,
								 const std::filesystem::path &iFile);

	/**
	 * @brief Set shader's internal int variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setInt(const std::string &iName, int iValue) = 0;

	/**
	 * @brief Set shader's internal int variable array.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValues Shader's variable's raw values.
	 * @param[in] iCount Amount values.
	 */
	virtual void setIntArray(const std::string &iName, int *iValues, uint32_t iCount) = 0;

	/**
	 * @brief Set shader's internal int variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setFloat(const std::string &iName, float iValue) = 0;

	/**
	 * @brief Set shader's internal vector 2 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setFloat2(const std::string &iName, const math::vec2 &iValue) = 0;

	/**
	 * @brief Set shader's internal vector 3 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setFloat3(const std::string &iName, const math::vec3 &iValue) = 0;

	/**
	 * @brief Set shader's internal vector 4 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setFloat4(const std::string &iName, const math::vec4 &iValue) = 0;

	/**
	 * @brief Set shader's internal Matrix 4 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setMat4(const std::string &iName, const math::mat4 &iValue) = 0;

	/**
	 * @brief get the shader's name.
	 * @return Shader's name.
	 */
	[[nodiscard]] virtual const std::string &getName() const { return m_name; }
	/**
	 * @brief get the shader's renderer's name.
	 * @return Shader's renderer's name.
	 */
	[[nodiscard]] virtual const std::string &getRenderer() const { return m_renderer; }

	/**
	 * @brief get the shader's full name.
	 * @return Shader's full name.
	 */
	[[nodiscard]] virtual std::string getFullName() const { return fmt::format("{}_{}", m_renderer, m_name); }

private:
	/// Shader's name.
	std::string m_name;
	/// Shader's name.
	std::string m_renderer;
	/// Library is a friend to be able to modify name.
	friend class ShaderLibrary;
};
}// namespace owl::renderer
