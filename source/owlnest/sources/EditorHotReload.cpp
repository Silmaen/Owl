/**
 * @file EditorHotReload.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"

#include "document/SceneDocument.h"

#include <platform/FileWatcher.h>
#include <scene/Tileset.h>

#include <filesystem>
#include <string>

namespace owl::nest {

void EditorLayer::onAssetFileChanged(const std::filesystem::path& iFile) {
	OWL_PROFILE_FUNCTION()

	const auto extension = iFile.extension().string();
	if (extension == scene::Tileset::fileExtension())
		onTilesetSaved(iFile);
	for (const auto& docPtr: m_documents.list()) {
		if (!docPtr || docPtr->type() != DocumentType::Scene)
			continue;
		auto* sceneDoc = static_cast<SceneDocument*>(docPtr.get());
		if (extension == ".owl") {
			if (!sceneDoc->filePath().empty() && platform::isSameFile(sceneDoc->filePath(), iFile))
				static_cast<void>(sceneDoc->reloadFromDisk(activeViewportSize()));
			continue;
		}
		const auto& active = sceneDoc->getActiveScene();
		if (active)
			static_cast<void>(active->onAssetFileChanged(iFile));
		if (const auto& edited = sceneDoc->getEditorScene(); edited && edited != active)
			static_cast<void>(edited->onAssetFileChanged(iFile));
	}
}

}// namespace owl::nest
