/**
 * @file PostProcess.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/PostProcess.h"

#include "app/Application.h"
#include "platform/FileUtils.h"
#include "renderer/EnvironmentMap.h"
#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/Framebuffer.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/UniformBuffer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::renderer {

namespace {
constexpr auto g_rendererKey = "post_process";

auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(g_rendererKey);
	return sId;
}

struct BloomUbo {
	math::vec4 texel{0.f, 0.f, 0.f, 0.f};
	math::vec4 threshold{1.f, 0.5f, 0.7f, 0.f};
	std::array<int32_t, 4> mode{0, 0, 0, 0};
	math::vec4 target{1.f, 1.f, 0.f, 0.f};
};

// One pass of the bloom chain of the frame, read by its pass body through its index.
struct BloomStep {
	RenderGraph::Resource source;
	RenderGraph::Resource added;
	math::vec2ui sourceSize{0, 0};
	math::vec2ui targetSize{0, 0};
	int32_t mode = 0;
	// Mode 5: pre-exposure EV100, darkest and brightest EV100, largest change; mode 0: pre-exposure EV100, auto flag.
	math::vec4 exposure{0.f, 0.f, 0.f, 0.f};
	bool reset = false;
};

// Number of passes of the automatic exposure: the measure, the halvings down to one texel, the adaptation.
constexpr size_t g_exposureStepCount = 8;

struct InternalData {
	shared<gpu::DrawData> drawData;
	shared<gpu::UniformBuffer> uniformBuffer;
	BloomUbo params;
	math::vec4 threshold{1.f, 0.5f, 0.7f, 0.f};
	std::array<BloomStep, size_t{2} * PostProcess::g_maxBloomLevels> steps{};
	std::array<BloomStep, g_exposureStepCount> exposureSteps{};
	std::unordered_map<std::string, PostProcess::Lut> luts;
	// Adapted EV100 of the previous and of the current frame, swapped each frame.
	std::array<shared<gpu::Framebuffer>, 2> exposure;
	uint32_t exposureFrame = 0;
	std::chrono::steady_clock::time_point lastExposure;
	bool exposureReset = true;
};

shared<InternalData> g_data;

void drawStep(const RenderGraph::PassContext& iContext, const BloomStep& iStep) {
	const auto& step = iStep;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	auto& params = g_data->params;
	params.texel = math::vec4{1.f / static_cast<float>(step.sourceSize.x()),
							  1.f / static_cast<float>(step.sourceSize.y()), 0.f, 0.f};
	params.threshold = step.mode == 5 ? step.exposure : g_data->threshold;
	params.mode = {step.mode, step.reset ? 1 : 0, 0, 0};
	params.target = math::vec4{static_cast<float>(step.targetSize.x()), static_cast<float>(step.targetSize.y()),
							   step.exposure.x(), step.mode == 0 ? step.exposure.w() : 0.f};
	g_data->uniformBuffer->setData(&params, sizeof(BloomUbo), 0);
	const auto source = iContext.getTexture(step.source);
	const auto added = step.added.isValid() ? iContext.getTexture(step.added) : source;
	const std::array textures{source, added};
	gpu::RenderCommand::bindTextures(textures);
	gpu::RenderCommand::drawData(g_data->drawData, 6);
}

auto readLut(const std::filesystem::path& iAssetPath) -> std::optional<ColorLut> {
	if (app::Application::instanced()) {
		const auto& app = app::Application::get();
		if (const std::string rel = iAssetPath.generic_string(); app.hasOpenPack() && app.packContains(rel)) {
			if (const auto bytes = app.loadFromPack(rel); bytes)
				return ColorLut::parse(std::string_view{reinterpret_cast<const char*>(bytes->data()), bytes->size()});
		}
		if (iAssetPath.is_relative()) {
			for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
				if (const auto full = assetsPath / iAssetPath; exists(full))
					return ColorLut::load(full);
			}
		}
	}
	return ColorLut::load(iAssetPath);
}

}// namespace

void PostProcess::init() {
	OWL_PROFILE_FUNCTION()

	g_data = mkShared<InternalData>();
	const std::array<gpu::BindingDecl, 2> bindings{
			gpu::BindingDecl{.binding = 0,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Fragment},
			gpu::BindingDecl{.binding = 1,
							 .type = gpu::BindingType::CombinedImageSampler,
							 .count = 2,
							 .stages = gpu::ShaderStage::Fragment},
	};
	gpu::RendererDescriptors::declare(g_rendererKey, bindings);
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	std::vector<uint32_t> indices{0, 1, 2, 2, 3, 0};
	g_data->drawData = gpu::DrawData::create();
	g_data->drawData->init({{"i_Position", gpu::ShaderDataType::Float2}}, g_rendererKey, indices, "bloom",
						   {.blendMode = gpu::BlendMode::Opaque, .depthTest = false, .depthWrite = false});
	constexpr std::array<float, 8> corners{-1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f};
	g_data->drawData->setVertexData(corners.data(), static_cast<uint32_t>(corners.size() * sizeof(float)));
	g_data->uniformBuffer = gpu::UniformBuffer::create(sizeof(BloomUbo), 0, g_rendererKey);
	g_data->uniformBuffer->bind();
}

void PostProcess::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_data)
		return;
	g_data->drawData.reset();
	g_data->uniformBuffer.reset();
	g_data->luts.clear();
	g_data->exposure = {};
	g_data.reset();
	gpu::RendererDescriptors::release(g_rendererKey);
}

auto PostProcess::getBloomLevelCount(const math::vec2ui& iFrameSize) -> uint32_t {
	uint32_t count = 0;
	math::vec2ui size{iFrameSize.x() / 2, iFrameSize.y() / 2};
	while (count < g_maxBloomLevels && size.x() >= 8 && size.y() >= 8) {
		++count;
		size = math::vec2ui{size.x() / 2, size.y() / 2};
	}
	return count;
}

auto PostProcess::recordBloom(RenderGraph& ioGraph, const RenderGraph::Resource iSceneColor,
							  const PostProcessSettings& iSettings, const math::vec2ui& iFrameSize,
							  const RenderGraph::Resource iExposure) -> RenderGraph::Resource {
	using Graph = RenderGraph;
	const uint32_t levels = getBloomLevelCount(iFrameSize);
	if (!g_data || !iSettings.bloom || levels == 0)
		return {};
	g_data->threshold = math::vec4{std::max(iSettings.bloomThreshold, 0.f), std::clamp(iSettings.bloomKnee, 0.f, 1.f),
								   std::clamp(iSettings.bloomScatter, 0.f, 1.f), 0.f};
	std::array<Graph::Resource, g_maxBloomLevels> down{};
	std::array<math::vec2ui, g_maxBloomLevels> sizes{};
	math::vec2ui size{iFrameSize.x() / 2, iFrameSize.y() / 2};
	uint32_t step = 0;
	const auto addStep = [&ioGraph, &step](const std::string_view iName, const BloomStep& iStep,
										   const Graph::Resource iTarget) -> void {
		g_data->steps[step] = iStep;
		const auto source = iStep.source;
		const auto added = iStep.added;
		ioGraph.addPass(
				iName,
				[source, added, iTarget](Graph::PassBuilder& ioBuilder) -> void {
					ioBuilder.read(source);
					if (added.isValid())
						ioBuilder.read(added);
					ioBuilder.write(iTarget);
				},
				[index = step](const Graph::PassContext& iContext) -> void {
					if (g_data)
						drawStep(iContext, g_data->steps[index]);
				});
		++step;
	};
	math::vec2ui sourceSize = iFrameSize;
	for (uint32_t level = 0; level < levels; ++level) {
		sizes[level] = size;
		down[level] = ioGraph.createTarget("bloom down", Graph::TargetFormat::Linear, size);
		const bool exposed = level == 0 && iExposure.isValid();
		addStep("bloom down",
				{.source = level == 0 ? iSceneColor : down[level - 1],
				 .added = exposed ? iExposure : Graph::Resource{},
				 .sourceSize = sourceSize,
				 .targetSize = size,
				 .mode = level == 0 ? 0 : 1,
				 .exposure = {iSettings.ev100, 0.f, 0.f, exposed ? 1.f : 0.f}},
				down[level]);
		sourceSize = size;
		size = math::vec2ui{size.x() / 2, size.y() / 2};
	}
	Graph::Resource current = down[levels - 1];
	for (uint32_t level = levels - 1; level-- > 0;) {
		const auto up = ioGraph.createTarget("bloom up", Graph::TargetFormat::Linear, sizes[level]);
		addStep("bloom up",
				{.source = current,
				 .added = down[level],
				 .sourceSize = sizes[level + 1],
				 .targetSize = sizes[level],
				 .mode = 2},
				up);
		current = up;
	}
	return current;
}

auto PostProcess::recordExposure(RenderGraph& ioGraph, const RenderGraph::Resource iSceneColor,
								 const PostProcessSettings& iSettings, const math::vec2ui& iFrameSize)
		-> RenderGraph::Resource {
	using Graph = RenderGraph;
	if (!g_data)
		return {};
	if (!iSettings.autoExposure || iFrameSize.x() == 0 || iFrameSize.y() == 0) {
		g_data->exposureReset = true;
		return {};
	}
	for (auto& framebuffer: g_data->exposure) {
		if (!framebuffer)
			framebuffer = gpu::Framebuffer::create(
					{.size = {1, 1},
					 .attachments = {{.format = gpu::AttachmentSpecification::Format::Rgba16F,
									  .tiling = gpu::AttachmentSpecification::Tiling::Optimal},
									 {.format = gpu::AttachmentSpecification::Format::RedInteger,
									  .tiling = gpu::AttachmentSpecification::Tiling::Optimal},
									 {.format = gpu::AttachmentSpecification::Format::Depth32F,
									  .tiling = gpu::AttachmentSpecification::Tiling::Optimal}},
					 .samples = 1,
					 .swapChainTarget = false,
					 .debugName = "auto exposure"});
		if (!framebuffer)
			return {};
	}
	const auto now = std::chrono::steady_clock::now();
	float elapsed = std::chrono::duration<float>(now - g_data->lastExposure).count();
	g_data->lastExposure = now;
	// A first frame, or one after a pause, starts at the measured exposure instead of fading to it.
	const bool reset = g_data->exposureReset || elapsed > 0.5f;
	g_data->exposureReset = false;
	elapsed = std::clamp(elapsed, 0.f, 0.1f);
	const uint32_t previousIndex = g_data->exposureFrame;
	g_data->exposureFrame = 1 - previousIndex;
	const auto previous = ioGraph.importFramebuffer("exposure previous", *g_data->exposure[previousIndex]);
	const auto adapted = ioGraph.importFramebuffer("exposure", *g_data->exposure[g_data->exposureFrame]);
	uint32_t step = 0;
	const auto addStep = [&ioGraph, &step](const BloomStep& iStep, const Graph::Resource iTarget) -> void {
		g_data->exposureSteps[step] = iStep;
		const auto source = iStep.source;
		const auto added = iStep.added;
		ioGraph.addPass(
				"auto exposure",
				[source, added, iTarget](Graph::PassBuilder& ioBuilder) -> void {
					ioBuilder.read(source);
					if (added.isValid())
						ioBuilder.read(added);
					ioBuilder.write(iTarget);
				},
				[index = step](const Graph::PassContext& iContext) -> void {
					if (g_data)
						drawStep(iContext, g_data->exposureSteps[index]);
				});
		++step;
	};
	uint32_t size = g_exposureMeasureSize;
	auto current = ioGraph.createTarget("exposure measure", Graph::TargetFormat::Linear, {size, size});
	addStep({.source = iSceneColor,
			 .added = {},
			 .sourceSize = iFrameSize,
			 .targetSize = {size, size},
			 .mode = 3,
			 .exposure = {},
			 .reset = false},
			current);
	while (size > 1) {
		const uint32_t half = size / 2;
		const auto next = ioGraph.createTarget("exposure reduce", Graph::TargetFormat::Linear, {half, half});
		addStep({.source = current,
				 .added = {},
				 .sourceSize = {size, size},
				 .targetSize = {half, half},
				 .mode = 4,
				 .exposure = {},
				 .reset = false},
				next);
		current = next;
		size = half;
	}
	// A reset starts from the measure: the previous value is not read (a new target was never written).
	addStep({.source = current,
			 .added = reset ? current : previous,
			 .sourceSize = {1, 1},
			 .targetSize = {1, 1},
			 .mode = 5,
			 .exposure = {iSettings.ev100, iSettings.autoExposureMin,
						  std::max(iSettings.autoExposureMax, iSettings.autoExposureMin),
						  std::max(iSettings.autoExposureSpeed, 0.f) * elapsed},
			 .reset = reset},
			adapted);
	return adapted;
}

auto PostProcess::getLut(const std::filesystem::path& iAssetPath) -> const Lut& {
	static const Lut s_none;
	if (!g_data || iAssetPath.empty())
		return s_none;
	const auto key = iAssetPath.generic_string();
	if (const auto found = g_data->luts.find(key); found != g_data->luts.end())
		return found->second;
	auto& lut = g_data->luts[key];
	const auto parsed = readLut(iAssetPath);
	if (!parsed) {
		OWL_CORE_WARN("PostProcess: LUT {} cannot be read.", key)
		return lut;
	}
	const auto strip = parsed->toStrip();
	std::vector<float> pixels;
	pixels.reserve(strip.size() * 4);
	for (const auto& value: strip) pixels.insert(pixels.end(), {value.x(), value.y(), value.z(), 1.f});
	lut.size = parsed->size;
	lut.texture =
			gpu::Texture2D::create(gpu::Texture2D::Specification{.size = {parsed->size * parsed->size, parsed->size},
																 .format = gpu::ImageFormat::Rgba32F});
	lut.texture->setData(pixels.data(), static_cast<uint32_t>(pixels.size() * sizeof(float)));
	return lut;
}

}// namespace owl::renderer
