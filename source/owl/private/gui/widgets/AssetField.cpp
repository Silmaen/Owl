/**
 * @file AssetField.cpp
 * @author Silmaen
 * @date 26/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/UiLayer.h"
#include "gui/utils.h"
#include "gui/widgets/AssetField.h"
#include "renderer/Renderer.h"

#include <algorithm>
#include <cstring>

namespace owl::gui::widgets {

namespace {
constexpr const char* kPayload = "CONTENT_BROWSER_ITEM";

auto extLower(const std::filesystem::path& iPath) -> std::string {
	auto ext = iPath.extension().string();
	std::ranges::transform(ext, ext.begin(),
						   [](const unsigned char iChar) -> char { return static_cast<char>(std::tolower(iChar)); });
	return ext;
}

auto matches(const std::string& iExt, std::initializer_list<std::string_view> iAllowed) -> bool {
	return std::ranges::any_of(iAllowed, [&](const std::string_view iCand) -> bool { return iExt == iCand; });
}

void drawLoadStateOverlay(const renderer::gpu::LoadState iState, const ImVec2 iAnchor, const ImVec2 iSize) {
	if (iState == renderer::gpu::LoadState::Ready)
		return;
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImU32 bg = IM_COL32(0, 0, 0, 160);
	drawList->AddRectFilled({iAnchor.x, iAnchor.y + iSize.y - 18.f}, {iAnchor.x + iSize.x, iAnchor.y + iSize.y}, bg);
	const char* label = iState == renderer::gpu::LoadState::Pending ? "(loading...)" : "(failed)";
	const ImU32 fg =
			iState == renderer::gpu::LoadState::Failed ? IM_COL32(255, 80, 80, 255) : IM_COL32(255, 255, 255, 220);
	drawList->AddText({iAnchor.x + 4.f, iAnchor.y + iSize.y - 16.f}, fg, label);
}

}// namespace

auto isPathOfKind(const std::filesystem::path& iPath, const AssetKind iKind) -> bool {
	if (iKind == AssetKind::Any)
		return true;
	const std::string ext = extLower(iPath);
	switch (iKind) {
		case AssetKind::Texture:
			return matches(ext, {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr"});
		case AssetKind::Font:
			return matches(ext, {".ttf", ".otf"});
		case AssetKind::Sound:
			return matches(ext, {".wav", ".mp3", ".ogg", ".flac"});
		case AssetKind::LuaScript:
			return ext == ".lua";
		case AssetKind::AnyScript:
			return matches(ext, {".lua", ".py", ".c", ".cpp", ".cc", ".cxx", ".h", ".hpp"});
		case AssetKind::Scene:
			return ext == ".owl";
		case AssetKind::Prefab:
			return ext == ".owlprefab";
		case AssetKind::Tileset:
			return ext == ".owltileset";
		case AssetKind::Tilemap:
			return ext == ".owltilemap";
		case AssetKind::Any:
			return true;
	}
	return false;
}

auto assetDropTarget(const AssetKind iKind, std::filesystem::path& oRelativePath) -> bool {
	bool accepted = false;
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayload)) {
			const auto* const data = static_cast<const char*>(payload->Data);
			const auto length = ::strnlen(data, static_cast<size_t>(payload->DataSize));
			const std::filesystem::path candidate{std::string(data, length)};
			if (isPathOfKind(candidate, iKind)) {
				oRelativePath = candidate;
				accepted = true;
			}
		}
		ImGui::EndDragDropTarget();
	}
	return accepted;
}

auto textureField(const char* iLabel, shared<renderer::gpu::Texture2D>& ioTexture, const ImVec2 iSize) -> bool {
	bool changed = false;
	const ImVec2 anchor = ImGui::GetCursorScreenPos();
	const std::string popupId = std::string{iLabel} + "##settings";
	if (const auto tex = imTexture(ioTexture); tex.has_value()) {
		if (ImGui::ImageButton(iLabel, tex.value(), iSize, {0, 1}, {1, 0}) && ioTexture != nullptr)
			ImGui::OpenPopup(popupId.c_str());
		if (ioTexture)
			drawLoadStateOverlay(ioTexture->getLoadState(), anchor, iSize);
	} else {
		if (ImGui::Button(iLabel, iSize) && ioTexture != nullptr)
			ImGui::OpenPopup(popupId.c_str());
	}
	std::filesystem::path dropped;
	if (assetDropTarget(AssetKind::Texture, dropped)) {
		if (const auto resolved = renderer::Renderer::getTextureLibrary().find(dropped.string())) {
			// The current frame may already have captured `ioTexture`'s
			// `ImTextureID` (= `VkDescriptorSet` on Vulkan) into ImGui's draw
			// data, so reassigning would free a descriptor that is still
			// scheduled for submission. Hand the old shared pointer to
			// `UiLayer` which keeps it alive until end-of-frame, then drops it.
			UiLayer::deferTextureRelease(ioTexture);
			ioTexture = renderer::gpu::Texture2D::create(*resolved);
			changed = true;
		}
	}
	if (ImGui::BeginPopup(popupId.c_str())) {
		if (ImGui::MenuItem("Remove texture")) {
			ioTexture.reset();
			changed = true;
		}
		ImGui::EndPopup();
	}
	return changed;
}

}// namespace owl::gui::widgets
