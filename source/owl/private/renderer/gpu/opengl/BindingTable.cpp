/**
 * @file BindingTable.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "BindingTable.h"

#include "core/external/opengl46.h"

#include <algorithm>
#include <ranges>
#include <unordered_map>

namespace owl::renderer::gpu::opengl {

namespace {

auto getTables() -> std::unordered_map<std::string, uniq<BindingTable>>& {
	static std::unordered_map<std::string, uniq<BindingTable>> sTables;
	return sTables;
}

// Table whose bindings are the current GL state, nullptr when unknown.
const BindingTable* g_applied = nullptr;
// Table of the active renderer block.
BindingTable* g_active = nullptr;

void forgetName(std::vector<uint32_t>& ioNames, const uint32_t iName) { std::ranges::replace(ioNames, iName, 0u); }

}// namespace

auto BindingTable::getForRenderer(const std::string& iRenderer) -> BindingTable& {
	auto& slot = getTables()[iRenderer];
	if (!slot)
		slot = mkUniq<BindingTable>();
	return *slot;
}

void BindingTable::release(const std::string& iRenderer) {
	const auto it = getTables().find(iRenderer);
	if (it == getTables().end())
		return;
	if (g_applied == it->second.get())
		g_applied = nullptr;
	if (g_active == it->second.get())
		g_active = nullptr;
	getTables().erase(it);
}

void BindingTable::releaseAll() {
	g_applied = nullptr;
	g_active = nullptr;
	getTables().clear();
}

auto BindingTable::getActive() -> BindingTable* { return g_active; }

void BindingTable::setActive(BindingTable* iTable) { g_active = iTable; }

void BindingTable::recordUniformBuffer(const uint32_t iBinding, const uint32_t iBuffer) {
	if (g_active == nullptr) {
		g_applied = nullptr;
		return;
	}
	if (iBinding >= g_active->m_uniformBuffers.size())
		g_active->m_uniformBuffers.resize(iBinding + 1, 0);
	g_active->m_uniformBuffers[iBinding] = iBuffer;
	g_active->touch();
}

void BindingTable::recordTexture(const uint32_t iSlot, const uint32_t iTexture) {
	if (g_active == nullptr) {
		g_applied = nullptr;
		return;
	}
	if (iSlot >= g_active->m_textures.size())
		g_active->m_textures.resize(iSlot + 1, 0);
	g_active->m_textures[iSlot] = iTexture;
	g_active->touch();
}

void BindingTable::bindTextures(const std::span<const uint32_t> iTextures) {
	if (!iTextures.empty())
		glBindTextures(0, static_cast<GLsizei>(iTextures.size()), iTextures.data());
	if (g_active == nullptr) {
		g_applied = nullptr;
		return;
	}
	g_active->m_textures.assign(iTextures.begin(), iTextures.end());
	g_active->touch();
}

void BindingTable::forgetBuffer(const uint32_t iBuffer) {
	for (const auto& table: getTables() | std::views::values) forgetName(table->m_uniformBuffers, iBuffer);
}

void BindingTable::forgetTexture(const uint32_t iTexture) {
	for (const auto& table: getTables() | std::views::values) forgetName(table->m_textures, iTexture);
}

void BindingTable::applyActive() {
	if (g_active == nullptr || g_applied == g_active)
		return;
	g_active->apply();
	g_applied = g_active;
}

void BindingTable::apply() const {
	for (uint32_t binding = 0; binding < m_uniformBuffers.size(); ++binding) {
		if (m_uniformBuffers[binding] != 0)
			glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_uniformBuffers[binding]);
	}
	if (!m_textures.empty())
		glBindTextures(0, static_cast<GLsizei>(m_textures.size()), m_textures.data());
}

void BindingTable::touch() const {
	if (g_applied != this)
		g_applied = nullptr;
}

}// namespace owl::renderer::gpu::opengl
