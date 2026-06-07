/**
 * @file GraphContext.cpp
 * @author Silmaen
 * @date 07/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/external/opengl46.h"

#include "GraphContext.h"

namespace owl::renderer::gpu::opengl {

GraphContext::GraphContext(GLFWwindow* ioWindow) : mp_windowHandle(ioWindow) {
	OWL_CORE_ASSERT(ioWindow, "Windows handle is nullptr")
}

void GraphContext::init() {
	OWL_PROFILE_FUNCTION()

	glfwMakeContextCurrent(mp_windowHandle);
#ifdef OLD_GLAD
	version = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
#else
	m_version = gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress));
#endif
	OWL_CORE_ASSERT(m_version, "Failed to initialize GLAD for OpenGL")
	OWL_CORE_INFO("OpenGL GraphContext Initiated.")
	OWL_CORE_INFO("Device Info:")
	OWL_CORE_INFO("  Vendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)))
	OWL_CORE_INFO("  Renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)))
	OWL_CORE_INFO("  Version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)))
	int32_t textureUnits = 0;
	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &textureUnits);
	OWL_CORE_INFO(" Max texture slot per Shader: {}.", textureUnits)
	int32_t combinedTextureUnits = 0;
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &combinedTextureUnits);
	int32_t vertexSsbo = 0;
	int32_t fragmentSsbo = 0;
	int32_t combinedSsbo = 0;
	glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &vertexSsbo);
	glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &fragmentSsbo);
	glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, &combinedSsbo);
	OWL_CORE_INFO("  Combined texture image units: {}.", combinedTextureUnits)
	OWL_CORE_INFO("  Shader storage blocks: vertex={}, fragment={}, combined={}.", vertexSsbo, fragmentSsbo,
				  combinedSsbo)
}

void GraphContext::swapBuffers() {
	OWL_PROFILE_FUNCTION()

	glfwSwapBuffers(mp_windowHandle);
}

auto GraphContext::getVersion() const -> Version {
#ifdef OLD_GLAD
	return {.major = GLVersion.major, .minor = GLVersion.minor};
#else
	return {.major = GLAD_VERSION_MAJOR(m_version), .minor = GLAD_VERSION_MINOR(m_version)};
#endif
}

}// namespace owl::renderer::gpu::opengl
