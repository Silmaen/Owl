/**
 * @file AssetImport.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AssetImport.h"

#include "../DockLayout.h"

#include <data/assets/AssetRegistry.h>
#include <data/assets/cook/CookCache.h>
#include <gui/IconFont.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/Toggle.h>

#include <array>
#include <format>
#include <tuple>
#include <utility>

namespace owl::nest::panel {

namespace {

constexpr std::array<const char*, 3> g_compressionNames{"None", "Uastc", "Etc1s"};
constexpr std::array<const char*, 3> g_compressionLabels{"Source file (lossless)", "UASTC (near lossless)",
														 "ETC1S (smallest)"};

}// namespace

void AssetImport::show(const std::filesystem::path& iPath) {
	setSelection(iPath);
	m_visible = true;
	m_focus = true;
}

void AssetImport::setSelection(const std::filesystem::path& iPath) {
	if (iPath == m_path)
		return;
	m_path = iPath;
	m_cached.reset();
	m_cachedFor.reset();
}

auto AssetImport::run(const std::string& iName, commands::CommandArgs iArgs) const -> bool {
	iArgs.set("path", m_path.string());
	const commands::CommandTarget target{.registry = mp_commands, .scene = m_scene, .undo = mp_undoManager};
	const auto result = target.execute(iName, iArgs);
	if (!result)
		OWL_WARN("Import settings: {} failed: {}.", iName, result.error().message)
	return result.has_value();
}

void AssetImport::renderTextureSettings(const data::assets::AssetMeta& iMeta) const {
	if (!gui::widgets::beginProperties("##textureImport"))
		return;
	bool nearest = iMeta.texture.nearest;
	gui::widgets::propertyLabel("Pixel art", "Nearest-texel filtering: sharp texels, no blending between them.");
	if (gui::widgets::checkbox("##nearest", &nearest))
		std::ignore = run("asset.set_import", {{"nearest", nearest}});
	if (gui::widgets::propertyReset(iMeta.texture.nearest))
		std::ignore = run("asset.set_import", {{"nearest", false}});

	int compression = static_cast<int>(iMeta.texture.compression);
	gui::widgets::propertyLabel("Pack storage",
								"How the cook stores the texture in a game pack. UASTC and ETC1S are transcoded to BC7 "
								"or ASTC at load.");
	if (gui::widgets::combo("##compression", &compression, g_compressionLabels))
		std::ignore = run("asset.set_import",
						  {{"compression", std::string{g_compressionNames[static_cast<size_t>(compression)]}}});
	if (gui::widgets::propertyReset(iMeta.texture.compression != data::assets::TextureCompression::None))
		std::ignore = run("asset.set_import", {{"compression", std::string{"None"}}});

	const bool mipsApply =
			iMeta.texture.compression != data::assets::TextureCompression::None && !iMeta.texture.nearest;
	bool mips = iMeta.texture.mips;
	gui::widgets::propertyLabel("Mipmaps", "Mipmap chain built by the cook; only for a compressed, filtered texture.");
	ImGui::BeginDisabled(!mipsApply);
	if (gui::widgets::checkbox("##mips", &mips))
		std::ignore = run("asset.set_import", {{"mips", mips}});
	ImGui::EndDisabled();
	if (gui::widgets::propertyReset(!iMeta.texture.mips))
		std::ignore = run("asset.set_import", {{"mips", true}});
	gui::widgets::endProperties();
}

void AssetImport::onImGuiRender() {
	if (!m_visible)
		return;
	if (m_focus) {
		ImGui::SetNextWindowFocus();
		m_focus = false;
	}
	if (!ImGui::Begin(panelWindow::title(gui::icon::slidersHorizontal, "Import", panelWindow::assetImport).c_str(),
					  &m_visible)) {
		ImGui::End();
		return;
	}
	const auto& registry = app::Application::get().getAssetRegistry();
	const auto guid = m_path.empty() ? std::nullopt : registry.findGuid(m_path.generic_string());
	const auto entry = guid ? registry.find(*guid) : std::nullopt;
	if (!entry) {
		gui::widgets::emptyState(gui::icon::slidersHorizontal, "No asset selected",
								 m_path.empty() ? "Select an asset in the content browser."
												: "This file has no sidecar: it lies outside the project, or is no "
												  "asset.");
		ImGui::End();
		return;
	}
	const auto& meta = entry->meta;
	ImGui::TextUnformatted(m_path.filename().string().c_str());
	ImGui::TextDisabled("%s asset", std::string{data::assets::importerName(meta.importer)}.c_str());
	const auto guidText = data::assets::formatGuid(meta.guid);
	ImGui::TextDisabled("GUID %s", guidText.c_str());
	ImGui::SameLine();
	if (gui::widgets::button("##copyGuid", {.glyph = gui::icon::copy, .label = "", .tooltip = "Copy the GUID"}))
		ImGui::SetClipboardText(guidText.c_str());
	ImGui::Separator();

	if (meta.importer == data::assets::Importer::Texture) {
		renderTextureSettings(meta);
		if (gui::widgets::button("Apply to Folder",
								 {.glyph = gui::icon::folder,
								  .label = "Apply to Folder",
								  .tooltip = "Apply to Folder",
								  .description = "Give these settings to every texture of the folder of this one."})) {
			commands::CommandArgs args{{"nearest", meta.texture.nearest},
									   {"compression", std::string{g_compressionNames[static_cast<size_t>(
															   meta.texture.compression)]}},
									   {"mips", meta.texture.mips}};
			args.set("path", m_path.parent_path().string());
			const commands::CommandTarget target{.registry = mp_commands, .scene = m_scene, .undo = mp_undoManager};
			std::ignore = target.execute("asset.set_import", args);
		}
	} else {
		ImGui::TextDisabled("No import setting: the asset is packed as it is.");
	}
	ImGui::Separator();

	if (!m_cachedFor || *m_cachedFor != meta) {
		data::assets::cook::CookCache cache(entry->root / ".owl" / "cache" / "cooked");
		m_cached = cache.isCached(m_path, meta);
		m_cachedFor = meta;
	}
	ImGui::TextDisabled("%s", m_cached.value_or(false) ? "Cooked, up to date" : "Not cooked for these settings");
	if (gui::widgets::button("Cook", {.glyph = gui::icon::package, .label = "Cook Now"}) && run("asset.cook", {})) {
		m_cached.reset();
		m_cachedFor.reset();
	}
	ImGui::End();
}

}// namespace owl::nest::panel
