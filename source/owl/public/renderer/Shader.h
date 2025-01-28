/**
 * @file Shader.h
 * @author Silmaen
 * @date 07/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/matrices.h"
#include "core/Core.h"

#include <filesystem>

namespace owl::renderer {
/**
 * @brief Types of Shader.
 */
enum class ShaderType : uint8_t {
	None,///< No type of unknown.
	Vertex,///< Vertex shader.
	Fragment,///< Fragment shader.
	Geometry,///< Geometry shader.
	Compute,///< Compute shader.
};

/**
 * @brief Abstract class managing shader.
 */
class OWL_API Shader {
public:
	Shader(const Shader&) = default;
	Shader(Shader&&) = default;
	auto operator=(const Shader&) -> Shader& = default;
	auto operator=(Shader&&) -> Shader& = default;

	/**
	 * @brief Structure holding shader components name.
	 */
	struct ShaderName {
		std::string name;///< The shader base name.
		std::string renderer;///< The name of the renderer.
	};

	/**
	 * @brief the shader's specifications.
	 */
	struct Specification {
		ShaderName shaderName;
	};

	/**
	 * @brief Constructor.
	 * @param[in] iShaderName The shader's name.
	 * @param[in] iRenderer The renderer's name.
	 */
	Shader(std::string iShaderName, std::string iRenderer) {
		m_specs.shaderName.name = std::move(iShaderName);
		m_specs.shaderName.renderer = std::move(iRenderer);
	}

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
	 * @return Pointer to the shader.
	 */
	static auto create(const Specification& iShaderName) -> shared<Shader>;

	/**
	* @brief Create a new shader.
	 * @param[in] iFile Source of the shader.
	 * @return Pointer to the shader.
	 */
	static auto create(const std::filesystem::path& iFile) -> shared<Shader>;

	/**
	 * @brief Set shader's internal int variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setInt(const std::string& iName, int iValue) = 0;

	/**
	 * @brief Set shader's internal int variable array.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValues Shader's variable's raw values.
	 * @param[in] iCount Amount values.
	 */
	virtual void setIntArray(const std::string& iName, int* iValues, uint32_t iCount) = 0;

	/**
	 * @brief Set shader's internal int variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setFloat(const std::string& iName, float iValue) = 0;

	/**
	 * @brief Set shader's internal vector 2 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setFloat2(const std::string& iName, const math::vec2& iValue) = 0;

	/**
	 * @brief Set shader's internal vector 3 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setFloat3(const std::string& iName, const math::vec3& iValue) = 0;

	/**
	 * @brief Set shader's internal vector 4 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setFloat4(const std::string& iName, const math::vec4& iValue) = 0;

	/**
	 * @brief Set shader's internal Matrix 4 variable.
	 * @param[in] iName Shader's variable's name.
	 * @param[in] iValue Shader's variable's value.
	 */
	virtual void setMat4(const std::string& iName, const math::mat4& iValue) = 0;

	/**
	 * @brief get the shader's name.
	 * @return Shader's name.
	 */
	[[nodiscard]] virtual auto getName() const -> const std::string& { return m_specs.shaderName.name; }
	/**
	 * @brief get the shader's renderer's name.
	 * @return Shader's renderer's name.
	 */
	[[nodiscard]] virtual auto getRenderer() const -> const std::string& { return m_specs.shaderName.renderer; }

	/**
	 * @brief get the shader's full name.
	 * @return Shader's full name.
	 */
	[[nodiscard]] virtual auto getFullName() const -> std::string { return composeName(m_specs.shaderName); }


	/**
	 * @brief Decode a string to a composed shader name.
	 * @param iFullName The string to decompose.
	 * @return The shader names.
	 */
	static auto decomposeName(const std::string& iFullName) -> ShaderName;

	/**
	 * @brief Compose a string with shader naming.
	 * @param iNames the shader components name.
	 * @return The composed name.
	 */
	static auto composeName(const ShaderName& iNames) -> std::string;

	/**
	 * @brief Gets the extension for this assets, return empty list for other management.
	 * @return Empty list.
	 */
	static auto extension() -> std::vector<std::string> { return {}; }

	static auto stringToSpecification(const std::string& iString) -> Specification {
		Specification specification;
		specification.shaderName = decomposeName(iString);
		return specification;
	}

private:
	/// Shader's name.
	Specification m_specs;
};
}// namespace owl::renderer
