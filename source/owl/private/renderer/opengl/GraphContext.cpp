/**
 * @file GraphContext.cpp
 * @author Silmaen
 * @date 07/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/external/opengl46.h"

#include "GraphContext.h"

namespace owl::renderer::opengl {

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
	OWL_CORE_INFO(" Max texture slot per Shader: {}", textureUnits)
}

void GraphContext::swapBuffers() {
	OWL_PROFILE_FUNCTION()

	glfwSwapBuffers(mp_windowHandle);
}

auto GraphContext::getVersion() const -> GraphContext::Version {
#ifdef OLD_GLAD
	return {.major = GLVersion.major, .minor = GLVersion.minor};
#else
	return {.major = GLAD_VERSION_MAJOR(m_version), .minor = GLAD_VERSION_MINOR(m_version)};
#endif
}

}// namespace owl::renderer::opengl
