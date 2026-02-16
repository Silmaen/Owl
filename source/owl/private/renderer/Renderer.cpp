/**
 * @file Renderer.cpp
 * @author Silmaen
 * @date 08/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/BackgroundRenderer.h"
#include "renderer/Renderer.h"
#include "renderer/Renderer2D.h"

namespace owl::renderer {

Renderer::State Renderer::m_internalState = State::Created;
shared<Renderer::SceneData> Renderer::m_sceneData = nullptr;
shared<Renderer::ShaderLibrary> Renderer::m_shaderLibrary = nullptr;
shared<Renderer::TextureLibrary> Renderer::m_textureLibrary = nullptr;

void Renderer::init() {
	OWL_PROFILE_FUNCTION()

	if (m_sceneData == nullptr)
		m_sceneData = mkShared<SceneData>();
	if (m_shaderLibrary == nullptr)
		m_shaderLibrary = mkShared<ShaderLibrary>();
	if (m_textureLibrary == nullptr)
		m_textureLibrary = mkShared<TextureLibrary>();

	RenderCommand::init();
	if (RenderCommand::getState() != RenderAPI::State::Ready) {
		m_internalState = State::Error;
		return;
	}

	Renderer2D::init();
	BackgroundRenderer::init();

	m_internalState = State::Running;
}

void Renderer::shutdown() {
	BackgroundRenderer::shutdown();
	Renderer2D::shutdown();
	reset();
	m_internalState = State::Stopped;
}

void Renderer::reset() {
	m_internalState = State::Created;
	m_sceneData.reset();
	m_shaderLibrary.reset();
	m_textureLibrary.reset();
}

void Renderer::beginScene(const Camera& iCamera) { m_sceneData->viewProjectionMatrix = iCamera.getViewProjection(); }

void Renderer::endScene() {}

void Renderer::onWindowResized(const uint32_t iWidth, const uint32_t iHeight) {
	RenderCommand::setViewport(0, 0, iWidth, iHeight);
}

auto Renderer::getShaderLibrary() -> ShaderLibrary& {
	if (m_shaderLibrary == nullptr)
		m_shaderLibrary = mkShared<ShaderLibrary>();
	return *m_shaderLibrary;
}

auto Renderer::getTextureLibrary() -> TextureLibrary& {
	if (m_textureLibrary == nullptr)
		m_textureLibrary = mkShared<TextureLibrary>();
	return *m_textureLibrary;
}

}// namespace owl::renderer
