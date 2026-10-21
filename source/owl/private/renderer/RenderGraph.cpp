/**
 * @file RenderGraph.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/RenderGraph.h"

#include "renderer/gpu/RenderCommand.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <span>
#include <utility>

namespace owl::renderer {

namespace {
// Pooled framebuffers unused for that many runs of the graph are released (a viewport resized, a preview closed).
constexpr uint32_t g_maxIdleRuns = 120;

auto srgbToLinear(const float iValue) -> float {
	return iValue <= 0.04045f ? iValue / 12.92f : std::pow((iValue + 0.055f) / 1.055f, 2.4f);
}

auto toFormat(const RenderGraph::ColorSpace iColorSpace) -> gpu::AttachmentSpecification::Format {
	switch (iColorSpace) {
		case RenderGraph::ColorSpace::Linear:
			return gpu::AttachmentSpecification::Format::Rgba16F;
		case RenderGraph::ColorSpace::Display:
			return gpu::AttachmentSpecification::Format::Surface;
	}
	return gpu::AttachmentSpecification::Format::Surface;
}

void clearTarget(const RenderGraph::ColorSpace iColorSpace, gpu::Framebuffer& ioFramebuffer) {
	const math::vec4 clearColor = gpu::RenderCommand::getClearColor();
	math::vec4 color = clearColor;
	if (iColorSpace == RenderGraph::ColorSpace::Linear)
		color = {srgbToLinear(color.r()), srgbToLinear(color.g()), srgbToLinear(color.b()), color.a()};
	gpu::RenderCommand::setClearColor(color);
	gpu::RenderCommand::clear();
	gpu::RenderCommand::setClearColor(clearColor);
	ioFramebuffer.clearAttachment(1, -1);
}

}// namespace

// ---------------------------------------------------------------- PassBuilder
void RenderGraph::PassBuilder::read(const Resource iResource) {
	if (iResource.isValid())
		m_graph.m_passes[m_pass].reads.push_back(iResource.index);
}

void RenderGraph::PassBuilder::write(const Resource iResource) { m_graph.m_passes[m_pass].write = iResource; }

// ---------------------------------------------------------------- PassContext
auto RenderGraph::PassContext::getFramebuffer(const Resource iResource) const -> gpu::Framebuffer* {
	if (!iResource.isValid() || iResource.index >= m_graph.m_targetCount)
		return nullptr;
	return m_graph.m_targets[iResource.index].framebuffer;
}

auto RenderGraph::PassContext::getTexture(const Resource iResource, const uint32_t iAttachment) const
		-> shared<gpu::Texture2D> {
	auto* const framebuffer = getFramebuffer(iResource);
	if (framebuffer == nullptr) {
		OWL_CORE_WARN("RenderGraph: No framebuffer to sample for target {}.", iResource.index)
		return nullptr;
	}
	return framebuffer->getAttachmentTexture(iAttachment);
}

auto RenderGraph::PassContext::getSize() const -> const math::vec2ui& { return m_graph.m_size; }

// ---------------------------------------------------------------- RenderGraph
void RenderGraph::reset() {
	m_targetCount = 0;
	m_passCount = 0;
	m_order.clear();
}

auto RenderGraph::addTarget(const std::string_view iName, const bool iImported, const ColorSpace iColorSpace)
		-> Resource {
	if (m_targetCount == m_targets.size())
		m_targets.emplace_back();
	auto& target = m_targets[m_targetCount];
	target.name.assign(iName);
	target.imported = iImported;
	target.colorSpace = iColorSpace;
	target.framebuffer = nullptr;
	return {.index = m_targetCount++};
}

auto RenderGraph::importTarget(const std::string_view iName) -> Resource {
	return addTarget(iName, true, ColorSpace::Display);
}

auto RenderGraph::createTarget(const std::string_view iName, const ColorSpace iColorSpace) -> Resource {
	return addTarget(iName, false, iColorSpace);
}

auto RenderGraph::beginPass(const std::string_view iName, Execute iExecute) -> uint32_t {
	if (m_passCount == m_passes.size())
		m_passes.emplace_back();
	auto& pass = m_passes[m_passCount];
	pass.name.assign(iName);
	pass.reads.clear();
	pass.write = {};
	pass.execute = std::move(iExecute);
	return m_passCount++;
}

auto RenderGraph::compile() -> bool {
	OWL_PROFILE_FUNCTION()

	m_order.clear();
	const uint32_t passCount = m_passCount;
	const size_t targetCount = m_targetCount;
	// The work arrays keep their capacity from frame to frame: compiling allocates nothing once warm.
	auto& scratch = m_scratch;
	scratch.written.assign(targetCount, 0);
	for (const auto& pass: std::span{m_passes}.first(passCount)) {
		if (!pass.write.isValid() || pass.write.index >= targetCount) {
			OWL_CORE_WARN("RenderGraph: Pass '{}' writes no target.", pass.name)
			return false;
		}
		for (const uint32_t read: pass.reads) {
			if (read >= targetCount || read == pass.write.index ||
				(scratch.written[read] == 0 && !m_targets[read].imported)) {
				OWL_CORE_WARN("RenderGraph: Pass '{}' reads a target no earlier pass writes.", pass.name)
				return false;
			}
		}
		scratch.written[pass.write.index] = 1;
	}

	// A pass is kept when it writes a target a kept pass (or the frame) needs: walk back from the imported targets.
	scratch.needed.assign(targetCount, 0);
	for (size_t i = 0; i < targetCount; ++i) scratch.needed[i] = m_targets[i].imported ? 1 : 0;
	scratch.kept.assign(passCount, 0);
	for (uint32_t index = passCount; index-- > 0;) {
		const auto& pass = m_passes[index];
		if (scratch.needed[pass.write.index] == 0)
			continue;
		scratch.kept[index] = 1;
		for (const uint32_t read: pass.reads) scratch.needed[read] = 1;
	}

	// Dependencies: read after write, write after write, write after read; ties keep the declaration order.
	const auto resetLists = [](std::vector<std::vector<uint32_t>>& ioLists, const size_t iCount) -> void {
		if (ioLists.size() < iCount)
			ioLists.resize(iCount);
		for (auto& list: ioLists) list.clear();
	};
	resetLists(scratch.successors, passCount);
	resetLists(scratch.readersSinceWrite, targetCount);
	scratch.predecessorCount.assign(passCount, 0);
	scratch.lastWriter.assign(targetCount, -1);
	const auto addEdge = [&scratch](const uint32_t iFrom, const uint32_t iTo) -> void {
		auto& next = scratch.successors[iFrom];
		if (std::ranges::find(next, iTo) != next.end())
			return;
		next.push_back(iTo);
		++scratch.predecessorCount[iTo];
	};
	for (uint32_t index = 0; index < passCount; ++index) {
		if (scratch.kept[index] == 0)
			continue;
		const auto& pass = m_passes[index];
		for (const uint32_t read: pass.reads) {
			if (scratch.lastWriter[read] >= 0)
				addEdge(static_cast<uint32_t>(scratch.lastWriter[read]), index);
			scratch.readersSinceWrite[read].push_back(index);
		}
		const uint32_t write = pass.write.index;
		if (scratch.lastWriter[write] >= 0)
			addEdge(static_cast<uint32_t>(scratch.lastWriter[write]), index);
		for (const uint32_t reader: scratch.readersSinceWrite[write]) addEdge(reader, index);
		scratch.readersSinceWrite[write].clear();
		scratch.lastWriter[write] = index;
	}
	// Kahn's sort, the ready pass with the smallest declaration index first (a frame has a few dozen passes).
	auto& ready = scratch.ready;
	ready.clear();
	for (uint32_t index = 0; index < passCount; ++index) {
		if (scratch.kept[index] != 0 && scratch.predecessorCount[index] == 0)
			ready.push_back(index);
	}
	while (!ready.empty()) {
		const auto first = std::ranges::min_element(ready);
		const uint32_t index = *first;
		ready.erase(first);
		m_order.push_back(index);
		for (const uint32_t next: scratch.successors[index]) {
			if (--scratch.predecessorCount[next] == 0)
				ready.push_back(next);
		}
	}

	for (auto& target: std::span{m_targets}.first(targetCount)) target.lastUse = 0;
	for (uint32_t position = 0; position < m_order.size(); ++position) {
		const auto& pass = m_passes[m_order[position]];
		m_targets[pass.write.index].lastUse = position;
		for (const uint32_t read: pass.reads) m_targets[read].lastUse = position;
	}
	return true;
}

void RenderGraph::execute(const math::vec2ui& iSize) {
	OWL_PROFILE_FUNCTION()

	m_size = iSize;
	if (m_order.empty() || iSize.x() == 0 || iSize.y() == 0)
		return;
	for (auto& pooled: m_pool) {
		pooled.used = false;
		++pooled.idleRuns;
	}
	gpu::Framebuffer* const imported = gpu::Framebuffer::getBound();
	const auto targets = std::span{m_targets}.first(m_targetCount);
	for (auto& target: targets) {
		target.written = false;
		target.framebuffer = target.imported ? imported : nullptr;
	}
	gpu::Framebuffer* bound = imported;
	const PassContext context{*this};
	for (uint32_t position = 0; position < m_order.size(); ++position) {
		auto& pass = m_passes[m_order[position]];
		auto& target = m_targets[pass.write.index];
		if (!target.imported && target.framebuffer == nullptr)
			target.framebuffer = acquire(target.colorSpace, iSize);
		if (target.framebuffer != bound) {
			if (bound != nullptr)
				bound->unbind();
			if (target.framebuffer != nullptr)
				target.framebuffer->bind();
			bound = target.framebuffer;
		}
		if (!target.imported && !target.written && target.framebuffer != nullptr)
			clearTarget(target.colorSpace, *target.framebuffer);
		target.written = true;
		if (pass.execute)
			pass.execute(context);
		for (auto& used: targets) {
			if (!used.imported && used.framebuffer != nullptr && used.lastUse == position && &used != &target)
				release(used.framebuffer);
		}
		if (!target.imported && target.lastUse == position)
			release(target.framebuffer);
	}
	if (bound != imported) {
		if (bound != nullptr)
			bound->unbind();
		if (imported != nullptr)
			imported->bind();
	}
	std::erase_if(m_pool, [](const PooledTarget& iPooled) -> bool { return iPooled.idleRuns > g_maxIdleRuns; });
}

auto RenderGraph::getCompiledPasses() const -> std::vector<std::string> {
	std::vector<std::string> names;
	names.reserve(m_order.size());
	for (const uint32_t index: m_order) names.push_back(m_passes[index].name);
	return names;
}

auto RenderGraph::acquire(const ColorSpace iColorSpace, const math::vec2ui& iSize) -> gpu::Framebuffer* {
	for (auto& pooled: m_pool) {
		if (pooled.used || pooled.colorSpace != iColorSpace || pooled.framebuffer->getSpecification().size != iSize)
			continue;
		pooled.used = true;
		pooled.idleRuns = 0;
		return pooled.framebuffer.get();
	}
	auto framebuffer = gpu::Framebuffer::create(
			{.size = iSize,
			 .attachments = {{.format = toFormat(iColorSpace), .tiling = gpu::AttachmentSpecification::Tiling::Optimal},
							 {.format = gpu::AttachmentSpecification::Format::RedInteger,
							  .tiling = gpu::AttachmentSpecification::Tiling::Optimal},
							 {.format = gpu::AttachmentSpecification::Format::Depth24Stencil8,
							  .tiling = gpu::AttachmentSpecification::Tiling::Optimal}},
			 .samples = 1,
			 .swapChainTarget = false,
			 .debugName = std::format("graph.{}", iColorSpace == ColorSpace::Linear ? "linear" : "display")});
	if (framebuffer == nullptr) {
		OWL_CORE_ERROR("RenderGraph: Failed to create a transient target.")
		return nullptr;
	}
	m_pool.push_back({.framebuffer = std::move(framebuffer), .colorSpace = iColorSpace, .used = true, .idleRuns = 0});
	return m_pool.back().framebuffer.get();
}

void RenderGraph::release(const gpu::Framebuffer* iFramebuffer) {
	for (auto& pooled: m_pool) {
		if (pooled.framebuffer.get() == iFramebuffer)
			pooled.used = false;
	}
}

}// namespace owl::renderer
