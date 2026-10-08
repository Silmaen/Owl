/**
 * @file HotReload.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "app/HotReload.h"

#include "debug/Profiler.h"
#include "renderer/Renderer.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

namespace owl::app {

namespace {

auto lowerExtension(const std::filesystem::path& iFile) -> std::string {
	auto extension = iFile.extension().string();
	std::ranges::transform(extension, extension.begin(),
						   [](const unsigned char iChar) -> char { return static_cast<char>(std::tolower(iChar)); });
	return extension;
}

auto isImage(const std::string& iExtension) -> bool {
	return iExtension == ".png" || iExtension == ".jpg" || iExtension == ".jpeg";
}

}// namespace

HotReload::HotReload(const std::chrono::milliseconds iPeriod) : m_watcher{iPeriod} {}

HotReload::~HotReload() = default;

void HotReload::setEnabled(const bool iEnabled) {
	if (iEnabled == isEnabled())
		return;
	if (iEnabled) {
		m_watcher.start();
		OWL_CORE_INFO("Hot reload: Watching {} asset directories.", m_watcher.getDirectories().size())
	} else {
		m_watcher.stop();
		OWL_CORE_INFO("Hot reload: Stopped.")
	}
}

auto HotReload::addListener(Listener iListener) -> uint32_t {
	const uint32_t id = m_nextListenerId++;
	m_listeners.emplace_back(id, std::move(iListener));
	return id;
}

void HotReload::removeListener(const uint32_t iId) {
	std::erase_if(m_listeners, [iId](const auto& iEntry) -> bool { return iEntry.first == iId; });
}

void HotReload::dispatchChanges() {
	OWL_PROFILE_FUNCTION()

	for (const auto& file: m_watcher.takeChanges()) static_cast<void>(reloadFile(file));
}

auto HotReload::reloadFile(const std::filesystem::path& iFile) -> Report {
	OWL_PROFILE_FUNCTION()

	Report report{.file = iFile};
	const auto extension = lowerExtension(iFile);
	const auto count = [&report](const bool iSuccess) -> void {
		if (iSuccess)
			++report.reloaded;
		else
			++report.failed;
	};
	if (isImage(extension)) {
		renderer::Renderer::getTextureLibrary().forEach(
				[&iFile, &count](const std::string&, const shared<renderer::gpu::Texture2D>& iTexture) -> void {
					if (iTexture && !iTexture->getPath().empty() && platform::isSameFile(iTexture->getPath(), iFile))
						count(iTexture->reloadFromFile());
				});
	} else if (extension == ".slang") {
		renderer::Renderer::getShaderLibrary().forEach(
				[&iFile, &count](const std::string&, const shared<renderer::gpu::Shader>& iShader) -> void {
					if (iShader && !iShader->getSourcePath().empty() &&
						platform::isSameFile(iShader->getSourcePath(), iFile))
						count(iShader->reload());
				});
	}
	if (report.reloaded + report.failed > 0)
		OWL_CORE_INFO("Hot reload: {} -> {} reloaded, {} kept the previous version.", iFile.string(), report.reloaded,
					  report.failed)
	// Copy: a listener may remove itself (a layer detached by a scene change).
	const auto listeners = m_listeners;
	for (const auto& [id, listener]: listeners) listener(iFile);
	m_lastReport = report;
	return report;
}

}// namespace owl::app
