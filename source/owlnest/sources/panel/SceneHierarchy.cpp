/**
 * @file SceneHierarchy.cpp
 * @author Silmaen
 * @date 26/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneHierarchy.h"

#include <gui/IconBank.h>
#include <gui/utils.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <renderer/Renderer.h>

using namespace owl::scene::component;

namespace owl::nest::panel {

namespace {

/// Map component display name to icon bank name.
auto componentIconName(const char* iCompName) -> const char* {
	static const std::unordered_map<std::string_view, const char*> map = {
			{"Transform", "comp_transform"},
			{"Camera", "comp_camera"},
			{"Sprite Renderer", "comp_sprite"},
			{"Animated Sprite", "comp_animated_sprite"},
			{"Circle Renderer", "comp_circle"},
			{"Text Renderer", "comp_text"},
			{"Physical body", "comp_physics"},
			{"Native Script", "comp_script"},
			{"Trigger", "comp_trigger"},
			{"Player", "comp_player"},
			{"Entity Link", "comp_link"},
			{"Background Texture", "comp_background"},
			{"Visibility", "comp_visibility"},
			{"Sound Source", "comp_sound"},
			{"Sound Listener", "comp_sound"},
			{"Lua Script", "comp_lua_script"},
	};
	if (const auto it = map.find(iCompName); it != map.end())
		return it->second;
	return nullptr;
}

}// namespace

[[maybe_unused]] SceneHierarchy::SceneHierarchy(const shared<scene::Scene>& iScene) { setContext(iScene); }

void SceneHierarchy::setContext(const shared<scene::Scene>& iContext) {
	m_context = iContext;
	m_selection = {};
}

void SceneHierarchy::onImGuiRender() {
	renderHierarchy();
	renderProperties();
}

// Function displaying the Hierarchy panel.

void SceneHierarchy::renderHierarchy() {
	ImGui::Begin("Scene Hierarchy");

	if (m_context) {
		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
			m_selection = {};

		// Right-click on blank space
		ImGui::PushID("...");
		if (ImGui::BeginPopupContextWindow(nullptr, 1)) {
			if (gui::IconBank::instance().menuItem("add_entity", "Create Empty Entity"))
				m_context->createEntity("Empty Entity");
			ImGui::EndPopup();
		}
		ImGui::PopID();

		if (constexpr ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen; ImGui::TreeNodeEx("root", flag)) {
			// Drop target on root node: unparent dragged entity.
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
					const uint64_t droppedUuid = *static_cast<const uint64_t*>(payload->Data);
					if (const auto child = m_context->findEntityByUUID(core::UUID{droppedUuid}); child)
						m_context->unparent(child);
				}
				ImGui::EndDragDropTarget();
			}
			for (auto& entity: m_context->getRootEntities()) { drawEntityNode(entity); }
			ImGui::TreePop();
		}
	}
	ImGui::End();
}

// NOLINTNEXTLINE(misc-no-recursion)
void SceneHierarchy::drawEntityNode(const scene::Entity& iEntity) {
	const auto& tag = iEntity.getComponent<Tag>().tag;
	const auto& [parentId, childrenIds] = iEntity.getComponent<Hierarchy>();
	const bool hasChildren = !childrenIds.empty();

	ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow;
	if (!hasChildren)
		flag |= ImGuiTreeNodeFlags_Leaf;
	if (iEntity == m_selection)
		flag |= ImGuiTreeNodeFlags_Selected;

	ImGui::PushID(static_cast<int>(static_cast<uint32_t>(iEntity)));
	const bool open = ImGui::TreeNodeEx(tag.c_str(), flag);
	const bool treeNodeClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen();
	// Bind context menu to the tree node item (must be right after TreeNodeEx).
	ImGui::OpenPopupOnItemClick("EntityContext", ImGuiPopupFlags_MouseButtonRight);

	// Drag source for reparenting.
	if (ImGui::BeginDragDropSource()) {
		const uint64_t uuid = iEntity.getUUID();
		ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &uuid, sizeof(uuid));
		ImGui::Text("%s", tag.c_str());
		ImGui::EndDragDropSource();
	}

	// Drop target for reparenting.
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
			const uint64_t droppedUuid = *static_cast<const uint64_t*>(payload->Data);
			if (const auto child = m_context->findEntityByUUID(core::UUID{droppedUuid}); child && child != iEntity)
				m_context->setParent(child, iEntity);
		}
		ImGui::EndDragDropTarget();
	}

	// Visibility toggle buttons (right-aligned)
	{
		auto& [gameVisible, editorVisible] = iEntity.getComponent<Visibility>();
		const auto& iconBank = gui::IconBank::instance();

		const float btnSize = ImGui::GetTextLineHeight();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;

		// Position: right edge minus space for 2 buttons
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - (btnSize + spacing) * 2);

		// Transparent button background
		constexpr math::vec4 transparent{0.f, 0.f, 0.f, 0.f};
		constexpr math::vec4 subtleHighlight{1.f, 1.f, 1.f, 0.15f};
		ImGui::PushStyleColor(ImGuiCol_Button, gui::vec(transparent));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, gui::vec(subtleHighlight));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, gui::vec(math::vec2{0.f, 0.f}));

		constexpr math::vec4 fullTint{1.0f, 1.0f, 1.0f, 1.0f};
		constexpr math::vec4 dimTint{0.5f, 0.5f, 0.5f, 0.5f};
		const math::vec2 btnSizeVec{btnSize, btnSize};

		// --- Editor visibility button (left) — eye icon ---
		ImGui::PushID("editorVis");
		const auto* const edIconName = editorVisible ? "eye_open" : "eye_closed";
		const auto edTint = editorVisible ? fullTint : dimTint;
		if (const auto iconInfo = iconBank.getIcon(edIconName)) {
			if (ImGui::ImageButton("##edVis", static_cast<ImTextureID>(iconInfo->textureId), gui::vec(btnSizeVec),
								   gui::vec(iconInfo->uv0), gui::vec(iconInfo->uv1), gui::vec(transparent),
								   gui::vec(edTint)))
				editorVisible = !editorVisible;
		} else {
			if (ImGui::Button(editorVisible ? "E" : "-", gui::vec(btnSizeVec)))
				editorVisible = !editorVisible;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Editor Visibility");
		ImGui::PopID();

		ImGui::SameLine();

		// --- Game visibility button (right) — camera icon ---
		ImGui::PushID("gameVis");
		const auto* const gameIconName = gameVisible ? "camera_on" : "camera_off";
		const auto gameTint = gameVisible ? fullTint : dimTint;
		if (const auto iconInfo = iconBank.getIcon(gameIconName)) {
			if (ImGui::ImageButton("##gameVis", static_cast<ImTextureID>(iconInfo->textureId), gui::vec(btnSizeVec),
								   gui::vec(iconInfo->uv0), gui::vec(iconInfo->uv1), gui::vec(transparent),
								   gui::vec(gameTint)))
				gameVisible = !gameVisible;
		} else {
			if (ImGui::Button(gameVisible ? "V" : "-", gui::vec(btnSizeVec)))
				gameVisible = !gameVisible;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Game Visibility");
		ImGui::PopID();

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);
	}

	// Context menu (opened via OpenPopupOnItemClick on the tree node).
	if (ImGui::BeginPopup("EntityContext")) {
		const auto& ib = gui::IconBank::instance();
		// --- Create ---
		if (ib.menuItem("add_entity", "Create Root Entity"))
			m_context->createEntity("Empty Entity");
		if (ib.menuItem("add_child_entity", "Create Child Entity")) {
			const auto child = m_context->createEntity("Child Entity");
			m_context->setParent(child, iEntity);
		}
		ImGui::Separator();
		// --- Duplicate ---
		if (ib.menuItem("duplicate", "Duplicate Entity"))
			m_context->duplicateEntity(iEntity);
		if (hasChildren) {
			if (ib.menuItem("duplicate", "Duplicate Subtree"))
				m_context->duplicateSubtree(iEntity);
		}
		// --- Hierarchy ---
		if (parentId != core::UUID{0}) {
			if (ib.menuItem("unparent", "Unparent"))
				m_context->unparent(iEntity);
		}
		ImGui::Separator();
		// --- Delete ---
		if (ib.menuItem("delete_entity", hasChildren ? "Delete Entity Only" : "Delete Entity")) {
			if (m_selection == iEntity)
				m_selection = {};
			auto entity = iEntity;
			m_context->destroyEntity(entity);
		}
		if (hasChildren) {
			if (ib.menuItem("delete_cascade", "Delete with Children")) {
				if (m_selection == iEntity)
					m_selection = {};
				auto entity = iEntity;
				m_context->destroyEntityWithChildren(entity);
			}
		}
		ImGui::EndPopup();
	}

	if (open) {
		// Recursively draw children.
		for (const auto childId: childrenIds) {
			if (const auto child = m_context->findEntityByUUID(childId); child)
				drawEntityNode(child);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();

	if (treeNodeClicked)
		m_selection = iEntity;
}

// Function displaying the Entity Property panel.

void SceneHierarchy::renderProperties() {
	ImGui::Begin("Properties");
	if (m_selection)
		drawComponents(m_selection);
	ImGui::End();
}

namespace {

template<isNamedComponent Comp>
void addComponentPop(scene::Entity& ioEntity) {
	if (!ioEntity.hasComponent<Comp>()) {
		const auto* iconId = componentIconName(Comp::name());
		bool clicked = false;
		if (iconId)
			clicked = gui::IconBank::instance().menuItem(iconId, Comp::name());
		else
			clicked = ImGui::MenuItem(Comp::name());
		if (clicked) {
			ioEntity.addComponent<Comp>();
			ImGui::CloseCurrentPopup();
		}
	}
}

template<isNamedComponent T>
void drawComponent(scene::Entity& ioEntity) {
	constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
												 ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap |
												 ImGuiTreeNodeFlags_FramePadding;
	if (ioEntity.hasComponent<T>()) {
		auto& component = ioEntity.getComponent<T>();
		const ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
		const float lineHeight = ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
		ImGui::Separator();

		// Build label with icon spacing
		const auto* iconId = componentIconName(T::name());
		const std::string label = iconId ? std::format("     {}", T::name()) : std::string(T::name());
		const bool open = ImGui::TreeNodeEx(label.c_str(), treeNodeFlags);

		// Draw icon over the padding space in the tree node header
		if (iconId) {
			if (const auto iconInfo = gui::IconBank::instance().getIcon(iconId)) {
				constexpr float iconSz = 16.0f;
				const auto itemMin = ImGui::GetItemRectMin();
				const float iconY = itemMin.y + (lineHeight - iconSz) * 0.5f;
				const float iconX = itemMin.x + ImGui::GetTreeNodeToLabelSpacing() + 2.0f;
				ImGui::GetWindowDrawList()->AddImage(iconInfo->textureId, {iconX, iconY},
													 {iconX + iconSz, iconY + iconSz}, gui::vec(iconInfo->uv0),
													 gui::vec(iconInfo->uv1));
			}
		}

		ImGui::PopStyleVar();
		ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
		if (ImGui::Button("+", ImVec2{lineHeight, lineHeight})) {
			ImGui::OpenPopup("ComponentSettings");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Component Settings");
		bool removeComponent = false;
		if (ImGui::BeginPopup("ComponentSettings")) {
			if (ImGui::MenuItem("Remove component"))
				removeComponent = true;
			ImGui::EndPopup();
		}
		if (open) {
			gui::component::renderProps(component);
			ImGui::TreePop();
		}
		if (removeComponent)
			ioEntity.removeComponent<T>();
	}
}


template<isNamedComponent... Component>
void addComponentsFromTuple(scene::Entity& ioEntity, const std::tuple<Component...>&) {
	(..., addComponentPop<Component>(ioEntity));
}

template<isNamedComponent... Component>
void drawComponentsFromTuple(scene::Entity& ioEntity, const std::tuple<Component...>&) {
	(..., drawComponent<Component>(ioEntity));
}

}// namespace


void SceneHierarchy::drawComponents(const scene::Entity& iEntity) {
	if (iEntity.hasComponent<Tag>()) {
		auto& tag = iEntity.getComponent<Tag>().tag;
		ImGui::InputText("##Tag", &tag);
	}
	ImGui::SameLine();
	ImGui::Text("Entity name");

	ImGui::PushItemWidth(-1);
	{
		const auto& iconBank = gui::IconBank::instance();
		bool clicked = false;
		if (const auto iconInfo = iconBank.getIcon("add_component")) {
			constexpr float iconSz = 16.0f;
			const float buttonWidth = ImGui::CalcTextSize("Add Component").x + iconSz +
									  ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().FramePadding.x * 2;
			clicked = ImGui::Button("##AddComp", ImVec2{buttonWidth, 0});
			const auto btnMin = ImGui::GetItemRectMin();
			const auto btnMax = ImGui::GetItemRectMax();
			const float iconY = btnMin.y + (btnMax.y - btnMin.y - iconSz) * 0.5f;
			ImGui::GetWindowDrawList()->AddImage(iconInfo->textureId, {btnMin.x + 4, iconY},
												 {btnMin.x + 4 + iconSz, iconY + iconSz}, gui::vec(iconInfo->uv0),
												 gui::vec(iconInfo->uv1));
			const float textX = btnMin.x + iconSz + ImGui::GetStyle().ItemSpacing.x;
			const float textY = btnMin.y + ImGui::GetStyle().FramePadding.y;
			ImGui::GetWindowDrawList()->AddText({textX, textY}, IM_COL32_WHITE, "Add Component");
		} else {
			clicked = ImGui::Button("Add Component");
		}
		if (clicked)
			ImGui::OpenPopup("AddComponent");
	}
	if (ImGui::BeginPopup("AddComponent")) {
		addComponentsFromTuple(m_selection, OptionalComponents{});
		ImGui::EndPopup();
	}
	ImGui::PopItemWidth();
	drawComponentsFromTuple(m_selection, gui::component::DrawableComponents{});
}

}// namespace owl::nest::panel
