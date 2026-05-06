/**
 * @file Renderer.cpp
 * @author Silmaen
 * @date 08/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer.h"

#include "renderer/BackgroundRenderer.h"
#include "renderer/Renderer2D.h"
#include "renderer/Renderer2DLayer.h"
#include "renderer/RendererRaycast.h"
#include "renderer/RendererRaycastLayer.h"

namespace owl::renderer {

Renderer::State Renderer::m_internalState = State::Created;
shared<Renderer::SceneData> Renderer::m_sceneData = nullptr;
shared<Renderer::ShaderLibrary> Renderer::m_shaderLibrary = nullptr;
shared<Renderer::TextureLibrary> Renderer::m_textureLibrary = nullptr;
RenderStack Renderer::m_renderStack;

void Renderer::init() {
	OWL_PROFILE_FUNCTION()
	initContext();
	if (m_internalState == State::Error)
		return;
	initShaders();
}

void Renderer::initContext() {
	OWL_PROFILE_FUNCTION()
	m_internalState = State::Created;

	if (m_sceneData == nullptr)
		m_sceneData = mkShared<SceneData>();
	if (m_shaderLibrary == nullptr)
		m_shaderLibrary = mkShared<ShaderLibrary>();
	if (m_textureLibrary == nullptr)
		m_textureLibrary = mkShared<TextureLibrary>();

	gpu::RenderCommand::init();
	if (gpu::RenderCommand::getState() != gpu::RenderAPI::State::Ready) {
		m_internalState = State::Error;
		return;
	}
}

void Renderer::initShaders(const ShaderProgressCallback& iProgress) {
	OWL_PROFILE_FUNCTION()

	if (iProgress)
		iProgress(0, 5, "renderer2D (quad, circle, line, text)");
	Renderer2D::init();

	if (iProgress)
		iProgress(4, 5, "background");
	BackgroundRenderer::init();

	RendererRaycast::init();

	// Register built-in render layer types with the factory. Adding more layer types
	// (voxel, ...) is just another registerWithFactory() call here.
	Renderer2DLayer::registerWithFactory();
	RendererRaycastLayer::registerWithFactory();

	m_internalState = State::Running;
}

void Renderer::shutdown() {
	RendererRaycast::shutdown();
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
	m_renderStack = RenderStack{};
}

void Renderer::setRenderStack(RenderStack iStack) { m_renderStack = std::move(iStack); }

auto Renderer::getRenderStack() -> const RenderStack& { return m_renderStack; }

void Renderer::beginScene(const Camera& iCamera) { m_sceneData->viewProjectionMatrix = iCamera.getViewProjection(); }

void Renderer::endScene() {}

void Renderer::onWindowResized(const uint32_t iWidth, const uint32_t iHeight) {
	gpu::RenderCommand::setViewport(0, 0, iWidth, iHeight);
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
