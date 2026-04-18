/**
 * @file DocumentTabBar.cpp
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DocumentTabBar.h"

#include "SceneDocument.h"

#include <gui/IconBank.h>
#include <gui/utils.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
OWL_DIAG_POP

namespace owl::nest {

namespace {

/// @brief Returns the icon name that best illustrates a document's kind + state.
auto tabIconName(const Document& iDoc) -> const char* {
	if (iDoc.type() == DocumentType::Scene) {
		const auto& scene = static_cast<const SceneDocument&>(iDoc);// NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		switch (scene.state()) {
			case SceneDocument::State::Play:
				return "PlayButton";
			case SceneDocument::State::Pause:
				return "PauseButton";
			case SceneDocument::State::Edit:
				return "owl_icon";
		}
		return "owl_icon";
	}
	return nullptr;
}

/// @brief Build the label shown on a tab: icon (if any) + title + optional `*` for dirty.
auto buildTabLabel(const Document& iDoc) -> std::string {
	const auto* icon = tabIconName(iDoc);
	(void) icon;// icon is rendered separately via ImGui::Image — label text only here.
	return std::format("{}{}##tab_{}", iDoc.title(), iDoc.isDirty() ? " *" : "",
					   static_cast<uint64_t>(iDoc.id()));
}

}// namespace

auto DocumentTabBar::onImGuiRender(DocumentManager& ioManager) -> core::UUID {
	core::UUID closed{0};

	if (ioManager.empty())
		return closed;

	const auto* activeDoc = ioManager.getActive();

	if (ImGui::BeginTabBar("##doc_tabs",
						   ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll |
								   ImGuiTabBarFlags_AutoSelectNewTabs)) {
		for (const auto& docPtr: ioManager.list()) {
			auto* doc = docPtr.get();
			if (doc == nullptr)
				continue;

			const auto id = doc->id();
			const bool isActive = (doc == activeDoc);

			ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
			if (doc->isDirty())
				flags |= ImGuiTabItemFlags_UnsavedDocument;

			bool open = true;// allow ImGui to show the close button
			const auto label = buildTabLabel(*doc);
			if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
				if (!isActive)
					ioManager.setActive(doc);
				ImGui::EndTabItem();
			}

			if (!open) {
				// User clicked the tab's close button: prompt if dirty, close immediately otherwise.
				if (doc->isDirty()) {
					m_pendingClose = id;
					m_showClosePrompt = true;
				} else {
					closed = id;
				}
			}
		}
		ImGui::EndTabBar();
	}

	if (m_showClosePrompt) {
		ImGui::OpenPopup("Close Unsaved Document?");
		m_showClosePrompt = false;
	}
	if (ImGui::BeginPopupModal("Close Unsaved Document?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		if (const auto* doc = ioManager.find(m_pendingClose); doc != nullptr) {
			ImGui::Text("'%s' has unsaved changes.", doc->title().c_str());
			ImGui::Spacing();
			if (gui::IconBank::instance().iconButton("delete", "Discard changes", {150, 0})) {
				closed = m_pendingClose;
				m_pendingClose = core::UUID{0};
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (gui::IconBank::instance().iconButton("close", "Cancel", {120, 0})) {
				m_pendingClose = core::UUID{0};
				ImGui::CloseCurrentPopup();
			}
		} else {
			m_pendingClose = core::UUID{0};
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	return closed;
}

}// namespace owl::nest
