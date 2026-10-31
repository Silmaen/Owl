/**
 * @file Renderer.cpp
 * @author Silmaen
 * @date 08/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer.h"

#include "renderer/BackgroundRenderer.h"
#include "renderer/ColorEncoder.h"
#include "renderer/ColorVision.h"
#include "renderer/LensFlare.h"
#include "renderer/OutlinePass.h"
#include "renderer/PostProcess.h"
#include "renderer/ReflectionProbes.h"
#include "renderer/Renderer2D.h"
#include "renderer/Renderer2DLayer.h"
#include "renderer/Renderer3D.h"
#include "renderer/RendererIsometric.h"
#include "renderer/RendererIsometricLayer.h"
#include "renderer/RendererMesh.h"
#include "renderer/RendererMeshLayer.h"
#include "renderer/RendererParticles.h"
#include "renderer/RendererParticlesLayer.h"
#include "renderer/RendererPlanet.h"
#include "renderer/RendererPlanetLayer.h"
#include "renderer/RendererRaycast.h"
#include "renderer/RendererRaycastLayer.h"
#include "renderer/RendererTilemap.h"
#include "renderer/RendererVoxelLayer.h"

#include <cstdint>

namespace owl::renderer {

Renderer::State Renderer::m_internalState = State::Created;
shared<Renderer::SceneData> Renderer::m_sceneData = nullptr;
shared<Renderer::ShaderLibrary> Renderer::m_shaderLibrary = nullptr;
RendererSet Renderer::m_rendererSet;
shared<Renderer::TextureLibrary> Renderer::m_textureLibrary = nullptr;
RenderStack Renderer::m_renderStack;
RenderGraph Renderer::m_renderGraph;

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

void Renderer::initShaders(const ShaderProgressCallback& iProgress, const RendererSet& iRenderers) {
	OWL_PROFILE_FUNCTION()

	m_rendererSet = iRenderers;
	if (iProgress)
		iProgress(0, 5, "renderer2D (quad, circle, line, text)");
	Renderer2D::init();

	if (iProgress)
		iProgress(4, 5, "background");
	BackgroundRenderer::init();
	ColorEncoder::init();
	ColorVision::init();
	OutlinePass::init();
	PostProcess::init();
	LensFlare::init();
	RendererParticles::init();

	if (iRenderers.raycast)
		RendererRaycast::init();
	RendererTilemap::init();
	if (iRenderers.isometric)
		RendererIsometric::init();
	if (iRenderers.renderer3D) {
		Renderer3D::init();
		RendererMesh::init();
		ReflectionProbes::init();
		RendererPlanet::init();
	}

	Renderer2DLayer::registerWithFactory();
	RendererParticlesLayer::registerWithFactory();
	if (iRenderers.raycast)
		RendererRaycastLayer::registerWithFactory();
	if (iRenderers.renderer3D) {
		RendererVoxelLayer::registerWithFactory();
		RendererMeshLayer::registerWithFactory();
		RendererPlanetLayer::registerWithFactory();
	}
	if (iRenderers.isometric)
		RendererIsometricLayer::registerWithFactory();

	m_internalState = State::Running;
}

auto Renderer::getRendererSet() -> const RendererSet& { return m_rendererSet; }

void Renderer::shutdown() {
	// A 3D family started by an earlier set is stopped too: the current set may not hold it.
	if (m_rendererSet.renderer3D || Renderer3D::isReady()) {
		RendererPlanet::shutdown();
		ReflectionProbes::shutdown();
		RendererMesh::shutdown();
		Renderer3D::shutdown();
	}
	RendererIsometric::shutdown();
	RendererTilemap::shutdown();
	RendererRaycast::shutdown();
	m_renderGraph.releasePool();
	RendererParticles::shutdown();
	LensFlare::shutdown();
	PostProcess::shutdown();
	OutlinePass::shutdown();
	ColorVision::shutdown();
	ColorEncoder::shutdown();
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

auto Renderer::getRenderGraph() -> RenderGraph& { return m_renderGraph; }

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
