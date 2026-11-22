/**
 * @file Shader.h
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/webgpu.h"
#include "renderer/gpu/Shader.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace owl::renderer::gpu::webgpu {

/**
 * @brief
 *  WebGPU shader: the WGSL stages baked on the desktop (`shaders/<renderer>/wgsl/<name>.<stage>.wgsl`).
 */
class OWL_API Shader final : public renderer::gpu::Shader {
public:
	Shader(const Shader&) = delete;
	Shader(Shader&&) = delete;
	auto operator=(const Shader&) -> Shader& = delete;
	auto operator=(Shader&&) -> Shader& = delete;

	/**
	 * @brief
	 *  Loads the baked stages of a shader.
	 * @param[in] iShaderName Name of the shader.
	 * @param[in] iRenderer Its renderer.
	 */
	Shader(const std::string& iShaderName, const std::string& iRenderer);

	/**
	 * @brief
	 *  Destructor.
	 */
	~Shader() override;

	/**
	 * @brief
	 *  Nothing to do: the pipeline of the draw holds the stages.
	 */
	void bind() const override {}

	/**
	 * @brief
	 *  Nothing to do: the pipeline of the draw holds the stages.
	 */
	void unbind() const override {}

	/**
	 * @brief
	 *  Uniform values go through uniform buffers on WebGPU.
	 * @param[in] iName Unused.
	 * @param[in] iValue Unused.
	 */
	void setInt([[maybe_unused]] const std::string& iName, [[maybe_unused]] int iValue) override {}

	/**
	 * @brief
	 *  Uniform values go through uniform buffers on WebGPU.
	 * @param[in] iName Unused.
	 * @param[in] iValues Unused.
	 * @param[in] iCount Unused.
	 */
	void setIntArray([[maybe_unused]] const std::string& iName, [[maybe_unused]] int* iValues,
					 [[maybe_unused]] uint32_t iCount) override {}

	/**
	 * @brief
	 *  Uniform values go through uniform buffers on WebGPU.
	 * @param[in] iName Unused.
	 * @param[in] iValue Unused.
	 */
	void setFloat([[maybe_unused]] const std::string& iName, [[maybe_unused]] float iValue) override {}

	/**
	 * @brief
	 *  Uniform values go through uniform buffers on WebGPU.
	 * @param[in] iName Unused.
	 * @param[in] iValue Unused.
	 */
	void setFloat2([[maybe_unused]] const std::string& iName, [[maybe_unused]] const math::vec2& iValue) override {}

	/**
	 * @brief
	 *  Uniform values go through uniform buffers on WebGPU.
	 * @param[in] iName Unused.
	 * @param[in] iValue Unused.
	 */
	void setFloat3([[maybe_unused]] const std::string& iName, [[maybe_unused]] const math::vec3& iValue) override {}

	/**
	 * @brief
	 *  Uniform values go through uniform buffers on WebGPU.
	 * @param[in] iName Unused.
	 * @param[in] iValue Unused.
	 */
	void setFloat4([[maybe_unused]] const std::string& iName, [[maybe_unused]] const math::vec4& iValue) override {}

	/**
	 * @brief
	 *  Uniform values go through uniform buffers on WebGPU.
	 * @param[in] iName Unused.
	 * @param[in] iValue Unused.
	 */
	void setMat4([[maybe_unused]] const std::string& iName, [[maybe_unused]] const math::mat4& iValue) override {}

	/**
	 * @brief
	 *  No compiler on the Web: a shader changes with a new export only.
	 * @param[in] iSlangSource Unused.
	 * @return False.
	 */
	auto recompile([[maybe_unused]] const std::string& iSlangSource) -> bool override { return false; }

	/**
	 * @brief
	 *  Module of the vertex stage.
	 * @return The module, null when the stage was not baked.
	 */
	[[nodiscard]] auto getVertex() const -> const wgpu::ShaderModule& { return m_vertex; }

	/**
	 * @brief
	 *  Module of the fragment stage.
	 * @return The module, null when the stage was not baked.
	 */
	[[nodiscard]] auto getFragment() const -> const wgpu::ShaderModule& { return m_fragment; }

	/**
	 * @brief
	 *  Module of the compute stage.
	 * @return The module, null when the stage was not baked.
	 */
	[[nodiscard]] auto getCompute() const -> const wgpu::ShaderModule& { return m_compute; }

	/**
	 * @brief
	 *  Location of a vertex input: the Slang WGSL output numbers the inputs by their semantics, not by
	 *  `[[vk::location]]`, so the layout element is found by its name (`i_TexCoord` for the field `texCoord`).
	 * @param[in] iElement Name of the layout element.
	 * @return The location, or nothing when the vertex stage has no input of that name.
	 */
	[[nodiscard]] auto getInputLocation(std::string_view iElement) const -> std::optional<uint32_t>;

private:
	/// Vertex stage.
	wgpu::ShaderModule m_vertex;
	/// Fragment stage.
	wgpu::ShaderModule m_fragment;
	/// Compute stage.
	wgpu::ShaderModule m_compute;
	/// Locations of the vertex inputs, by lower-case name.
	std::unordered_map<std::string, uint32_t> m_inputLocations;
};

}// namespace owl::renderer::gpu::webgpu
