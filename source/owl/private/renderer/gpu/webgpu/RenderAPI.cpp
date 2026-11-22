/**
 * @file RenderAPI.cpp
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "RenderAPI.h"

#include "Buffer.h"
#include "Context.h"
#include "DrawData.h"
#include "Texture.h"
#include "renderer/TextureDecoder.h"

#include <emscripten/emscripten.h>

#include <cstdlib>
#include <utility>
#include <vector>

// A PNG of the canvas to the page that asked for it (browser_run.py --shot); EM_JS declares extern variables.

// clang-format off
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wmissing-variable-declarations")
EM_JS(void, owlSendCapture, (const uint8_t* iData, size_t iSize), {
	if (!Module["owlSendCapture"])
		return;
	Module["owlSendCapture"](HEAPU8.slice(iData, iData + iSize));
});
OWL_DIAG_POP
// clang-format on

namespace owl::renderer::gpu::webgpu {

RenderAPI::~RenderAPI() { Context::get().release(); }

void RenderAPI::init() {
	OWL_PROFILE_FUNCTION()

	if (getState() != State::Created)
		return;
	if (Context::get().device == nullptr) {
		OWL_CORE_ERROR("WebGPU: No device, the canvas cannot draw.")
		setState(State::Error);
		return;
	}
	Context::get().initDefaults();
	if (const char* capture = std::getenv("OWL_WEBGPU_CAPTURE"); capture != nullptr)
		m_captureFrame = std::strtoll(capture, nullptr, 10);
	setState(State::Ready);
}

void RenderAPI::setViewport([[maybe_unused]] const uint32_t iX, [[maybe_unused]] const uint32_t iY,
							const uint32_t iWidth, const uint32_t iHeight) {
	Context::get().configure(iWidth, iHeight);
}

void RenderAPI::setClearColor(const math::vec4& iColor) { m_clearColor = iColor; }

auto RenderAPI::getMaxTextureSlots() const -> uint32_t { return g_textureSlots; }

auto RenderAPI::supportsFormat(const ImageFormat iFormat) const -> bool {
	return iFormat != ImageFormat::Bc7 && iFormat != ImageFormat::Astc4x4;
}

void RenderAPI::clear() { Context::get().clear(m_clearColor); }

void RenderAPI::drawData(const shared<renderer::gpu::DrawData>& iData, const uint32_t iIndexCount) {
	if (iData)
		Context::get().draw(static_cast<const DrawData&>(*iData), iIndexCount, 1);
}

void RenderAPI::drawDataInstanced(const shared<renderer::gpu::DrawData>& iData, const uint32_t iIndexCount,
								  const uint32_t iInstanceCount) {
	if (iData)
		Context::get().draw(static_cast<const DrawData&>(*iData), iIndexCount, iInstanceCount);
}

void RenderAPI::drawIndexedIndirectAt(const shared<renderer::gpu::DrawData>& iData,
									  const shared<renderer::gpu::StorageBuffer>& iCommandBuffer,
									  const uint32_t iCommandIndex) {
	const auto* commands = dynamic_cast<const StorageBuffer*>(iCommandBuffer.get());
	if (!iData || commands == nullptr)
		return;
	// `DrawIndexedIndirectCommand`: five 32-bit words.
	constexpr uint64_t commandSize = 5 * sizeof(uint32_t);
	Context::get().draw(static_cast<const DrawData&>(*iData), 0, 0, commands->getBuffer(),
						uint64_t{iCommandIndex} * commandSize);
}

void RenderAPI::bindTextures(const std::span<const shared<renderer::gpu::Texture2D>> iTextures) {
	auto& context = Context::get();
	context.boundTextures.assign(iTextures.begin(), iTextures.end());
	context.boundCubes.clear();
	for (uint32_t slot = 0; slot < g_textureSlots; ++slot) {
		const auto* texture = slot < iTextures.size() ? dynamic_cast<const Texture2D*>(iTextures[slot].get()) : nullptr;
		context.textureViews[slot] =
				texture != nullptr && texture->getView() != nullptr ? texture->getView() : context.whiteView;
		context.samplers[slot] =
				texture != nullptr && texture->getSampler() != nullptr ? texture->getSampler() : context.defaultSampler;
	}
}

void RenderAPI::bindTextures(const std::span<const shared<renderer::gpu::Texture2D>> iTextures,
							 const std::span<const shared<renderer::gpu::TextureCube>> iCubes) {
	bindTextures(iTextures);
	auto& context = Context::get();
	context.boundCubes.assign(iCubes.begin(), iCubes.end());
	for (uint32_t slot = 0; slot < g_cubeSlots; ++slot) {
		const auto* cube = slot < iCubes.size() ? dynamic_cast<const TextureCube*>(iCubes[slot].get()) : nullptr;
		context.cubeViews[slot] =
				cube != nullptr && cube->getView() != nullptr ? cube->getView() : context.whiteCubeView;
	}
}

void RenderAPI::beginFrame() {
	auto& context = Context::get();
	++context.frame;
	context.frameTexture = nullptr;
	context.frameView = nullptr;
	if (context.surface == nullptr)
		return;
	wgpu::SurfaceTexture frame;
	context.surface.GetCurrentTexture(&frame);
	if (frame.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
		frame.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
		return;
	context.frameTexture = frame.texture;
	context.frameView = frame.texture.CreateView();
}

void RenderAPI::endFrame() {
	// Every draw was submitted on its own: the browser shows the canvas when the frame returns to it.
	auto& context = Context::get();
	if (std::cmp_equal(m_captureFrame, m_frame)) {
		if (context.frameTexture != nullptr)
			captureFrame();
		else
			OWL_CORE_WARN("WebGPU: No canvas texture at frame {} to capture.", m_frame)
	}
	++m_frame;
	context.frameView = nullptr;
	context.frameTexture = nullptr;
}

void RenderAPI::captureFrame() const {
	auto& context = Context::get();
	OWL_CORE_INFO("WebGPU: Capturing frame {} ({}x{}).", m_frame, context.width, context.height)
	// Rows of a copy are 256-byte aligned.
	const uint32_t rowBytes = ((context.width * 4) + 255) & ~255U;
	const wgpu::BufferDescriptor desc{.label = "Capture",
									  .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
									  .size = uint64_t{rowBytes} * context.height};
	auto buffer = context.device.CreateBuffer(&desc);
	const wgpu::TexelCopyTextureInfo source{.texture = context.frameTexture};
	const wgpu::TexelCopyBufferInfo destination{.layout = {.bytesPerRow = rowBytes, .rowsPerImage = context.height},
												.buffer = buffer};
	const wgpu::Extent3D extent{.width = context.width, .height = context.height, .depthOrArrayLayers = 1};
	const wgpu::CommandEncoder encoder = context.device.CreateCommandEncoder();
	encoder.CopyTextureToBuffer(&source, &destination, &extent);
	const wgpu::CommandBuffer commands = encoder.Finish();
	context.queue.Submit(1, &commands);
	const uint32_t width = context.width;
	const uint32_t height = context.height;
	const bool bgra = context.format == wgpu::TextureFormat::BGRA8Unorm;
	buffer.MapAsync(wgpu::MapMode::Read, 0, static_cast<size_t>(desc.size), wgpu::CallbackMode::AllowSpontaneous,
					[buffer, width, height, rowBytes, bgra](const wgpu::MapAsyncStatus iStatus,
															const wgpu::StringView iMessage) -> void {
						if (iStatus != wgpu::MapAsyncStatus::Success) {
							OWL_CORE_WARN("WebGPU: Cannot read the canvas back ({}: {}).", static_cast<int>(iStatus),
										  std::string_view{iMessage.data, iMessage.length})
							return;
						}
						const auto* mapped = static_cast<const uint8_t*>(buffer.GetConstMappedRange());
						std::vector<uint8_t> rgba(size_t{width} * height * 4);
						for (uint32_t y = 0; y < height; ++y) {
							for (uint32_t x = 0; x < width; ++x) {
								const auto* texel = mapped + (size_t{y} * rowBytes) + (size_t{x} * 4);
								auto* out = rgba.data() + ((size_t{y} * width + x) * 4);
								out[0] = bgra ? texel[2] : texel[0];
								out[1] = texel[1];
								out[2] = bgra ? texel[0] : texel[2];
								out[3] = 255;
							}
						}
						const auto png = encodeImagePng({width, height}, rgba);
						owlSendCapture(png.data(), png.size());
						OWL_CORE_INFO("WebGPU: Canvas {}x{} captured ({} bytes).", width, height, png.size())
					});
}

}// namespace owl::renderer::gpu::webgpu
