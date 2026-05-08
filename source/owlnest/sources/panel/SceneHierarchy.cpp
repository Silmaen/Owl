/**
 * @file SceneHierarchy.cpp
 * @author Silmaen
 * @date 26/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneHierarchy.h"

#include "../UndoManager.h"
#include "../commands/ComponentCommands.h"
#include "../commands/EntityCommands.h"
#include "../commands/HierarchyCommands.h"
#include "../commands/PrefabCommands.h"
#include "../document/Document.h"

#include <core/utils/FileDialog.h>
#include <gui/IconBank.h>
#include <gui/utils.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <scene/PrefabSerializer.h>
#include <scene/SceneSerializer.h>

using namespace owl::scene::component;

namespace owl::nest::panel {

namespace {
/**
 * @brief
 *  Component name written by `drawComponent` while its header is hovered.
 *
 * Read by `EditorLayer::onContextualHelp` so pressing F1 over a specific
 * component header opens the documentation page that covers it. Empty when
 * no component is being hovered.
 */
std::string g_lastHoveredComponentName;

/**
 * @brief
 *  Check if an entity is the root of a prefab instance.
 */
auto isPrefabRoot(const scene::Entity& iEntity) -> bool {
	return iEntity && iEntity.hasComponent<PrefabLink>();
}

/**
 * @brief
 *  Walk the parent chain to find the prefab root (entity with PrefabLink), or return invalid entity.
 */
auto findPrefabRoot(const scene::Entity& iEntity, const scene::Scene& iScene) -> scene::Entity {
	auto current = iEntity;
	while (current) {
		if (current.hasComponent<PrefabLink>())
			return current;
		const auto pid = current.getComponent<Hierarchy>().parentId;
		if (pid == core::UUID{0})
			break;
		current = iScene.findEntityByUUID(pid);
	}
	return {};
}

/**
 * @brief
 *  Map component display name to icon bank name.
 */
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
			{"Canvas", "comp_canvas"},
			{"UI Rect", "comp_ui_rect"},
			{"UI Text", "comp_ui_text"},
			{"UI Image", "comp_ui_image"},
			{"UI Panel", "comp_ui_panel"},
			{"UI Button", "comp_ui_button"},
			{"UI Slider", "comp_ui_slider"},
			{"UI Progress Bar", "comp_ui_progress"},
			{"Prefab Link", "prefab_icon"},
			{"Tilemap", "owltileset_icon"},
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
	// Reset the hovered-component sink at the start of every frame; `drawComponent`
	// repopulates it as the user moves the mouse over individual component headers.
	g_lastHoveredComponentName.clear();
	renderHierarchy();
	renderProperties();
}

auto SceneHierarchy::lastHoveredComponentName() -> const std::string& { return g_lastHoveredComponentName; }

// Function displaying the Hierarchy panel.
void SceneHierarchy::renderHierarchy() {
	ImGui::Begin("Scene Hierarchy");

	if (mp_activeDocument != nullptr && mp_activeDocument->overridesGlobalPanels()) {
		mp_activeDocument->renderHierarchyPanel();
		ImGui::End();
		return;
	}

	if (m_context) {
		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
			m_selection = {};

		// Right-click on blank space
		ImGui::PushID("...");
		if (ImGui::BeginPopupContextWindow(nullptr, 1)) {
			if (gui::IconBank::instance().menuItem("add_entity", "Create Empty Entity")) {
				auto entity = m_context->createEntity("Empty Entity");
				if (mp_undoManager != nullptr)
					mp_undoManager->push(mkUniq<commands::CreateEntityCommand>(entity));
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();

		if (constexpr ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen; ImGui::TreeNodeEx("root", flag)) {
			// Drop target on root node: unparent dragged entity.
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
					const uint64_t droppedUuid = *static_cast<const uint64_t*>(payload->Data);
					if (const auto child = m_context->findEntityByUUID(core::UUID{droppedUuid}); child) {
						if (mp_undoManager != nullptr)
							mp_undoManager->push(mkUniq<commands::UnparentCommand>(child));
						m_context->unparent(child);
					}
				}
				ImGui::EndDragDropTarget();
			}
			renderRootEntities();
			ImGui::TreePop();
		}
	}
	ImGui::End();
}

void SceneHierarchy::renderRootEntities() {
	const auto& stack = renderer::Renderer::getRenderStack();
	const auto& layers = stack.getLayers();
	const auto roots = m_context->getRootEntities();
	// 0 or 1 layer → flat list (legacy behaviour, no extra nesting).
	if (layers.size() < 2) {
		for (auto entity: roots)

			drawEntityNode(entity);
		return;
	}

	// > 1 layer → bucket roots by their effective layer name. Untagged (or empty
	// `rendererName`) entities land under the first layer; entities with an unknown
	// name land in a trailing "(unrouted)" group.
	const std::string firstLayerName = layers.front()->getName();
	std::unordered_map<std::string, std::vector<scene::Entity>> bucketed;
	std::vector<scene::Entity> unrouted;
	for (auto entity: roots) {
		std::string effective;
		if (entity.hasComponent<RendererTag>()) {
			const auto& name = entity.getComponent<RendererTag>().rendererName;
			effective = name.empty() ? firstLayerName : name;
		} else {
			effective = firstLayerName;
		}
		const bool known = std::ranges::any_of(layers, [&](const auto& l) -> bool {
			return l->getName() == effective;
		});
		if (known)
			bucketed[effective].push_back(entity);
		else
			unrouted.push_back(entity);
	}
	for (const auto& layer: layers) {
		const auto& name = layer->getName();
		const auto count = bucketed.contains(name) ? bucketed.at(name).size() : 0u;
		const std::string label = std::format("{}  ({})###layer_{}", name, count, name);

		ImGui::PushID(name.c_str());
		const bool open = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
		// Drop target: route the dropped entity to this layer (unparent if needed, then set
		// `RendererTag.rendererName`). Both mutations are captured in a single
		// `ModifyEntityCommand` so one Ctrl+Z reverts the whole drop.
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
				const uint64_t droppedUuid = *static_cast<const uint64_t*>(payload->Data);
				if (auto e = m_context->findEntityByUUID(core::UUID{droppedUuid}); e) {
					auto before = mp_undoManager != nullptr ? EntitySnapshot::capture(e) : EntitySnapshot{};
					if (e.getComponent<Hierarchy>().parentId != core::UUID{0})
						m_context->unparent(e);
					auto& tag = e.hasComponent<RendererTag>() ? e.getComponent<RendererTag>()
															  : e.addComponent<RendererTag>();
					tag.rendererName = name;
					if (mp_undoManager != nullptr) {
						auto cmd = mkUniq<commands::ModifyEntityCommand>(
								e.getUUID(), std::move(before), std::format("Route to layer '{}'", name));
						cmd->captureAfter(e);
						mp_undoManager->push(std::move(cmd));
					}
				}
			}

			ImGui::EndDragDropTarget();
		}
		if (open) {
			if (const auto it = bucketed.find(name); it != bucketed.end()) {
				for (auto entity: it->second)

					drawEntityNode(entity);
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	if (!unrouted.empty()) {
		ImGui::PushID("__unrouted__");

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.20f, 1.0f));
		const std::string label = std::format("(unrouted)  ({})###layer_unrouted", unrouted.size());
		const bool open = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))

			ImGui::SetTooltip("These root entities have a RendererTag whose name does not match\n"
							  "any active layer — they will be skipped at render time.");
		if (open) {
			for (auto entity: unrouted)

				drawEntityNode(entity);

			ImGui::TreePop();
		}

		ImGui::PopID();
	}
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
	// Tint prefab instance entities with a distinct colour.
	const bool isPartOfPrefab = static_cast<bool>(findPrefabRoot(iEntity, *m_context));
	if (isPartOfPrefab)

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.4f, 0.7f, 1.0f, 1.0f});

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
			if (const auto child = m_context->findEntityByUUID(core::UUID{droppedUuid}); child && child != iEntity) {
				if (mp_undoManager != nullptr)
					mp_undoManager->push(mkUniq<commands::ReparentCommand>(child, iEntity.getUUID()));
				m_context->setParent(child, iEntity);
			}
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

	drawEntityContextMenu(iEntity, hasChildren, parentId);

	if (open) {
		// Recursively draw children.
		for (const auto childId: childrenIds) {
			if (const auto child = m_context->findEntityByUUID(childId); child)

				drawEntityNode(child);
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
	if (isPartOfPrefab)

		ImGui::PopStyleColor();

	if (treeNodeClicked)
		m_selection = iEntity;
}

void SceneHierarchy::drawEntityContextMenu(const scene::Entity& iEntity, const bool iHasChildren,
											const core::UUID iParentId) {
	if (!ImGui::BeginPopup("EntityContext"))
		return;
	const auto& ib = gui::IconBank::instance();
	// --- Create ---
	if (ib.menuItem("add_entity", "Create Root Entity")) {
		auto entity = m_context->createEntity("Empty Entity");
		if (mp_undoManager != nullptr)
			mp_undoManager->push(mkUniq<commands::CreateEntityCommand>(entity));
	}
	if (ib.menuItem("add_child_entity", "Create Child Entity")) {
		auto child = m_context->createEntity("Child Entity");
		m_context->setParent(child, iEntity);
		if (mp_undoManager != nullptr)
			mp_undoManager->push(mkUniq<commands::CreateEntityCommand>(child));
	}

	ImGui::Separator();
	// --- Duplicate ---
	if (ib.menuItem("duplicate", "Duplicate Entity")) {
		auto dup = m_context->duplicateEntity(iEntity);
		if (mp_undoManager != nullptr)
			mp_undoManager->push(mkUniq<commands::DuplicateEntityCommand>(iEntity, dup));
	}
	if (iHasChildren) {
		if (ib.menuItem("duplicate", "Duplicate Subtree")) {
			auto dup = m_context->duplicateSubtree(iEntity);
			if (mp_undoManager != nullptr)
				mp_undoManager->push(mkUniq<commands::DuplicateSubtreeCommand>(iEntity, dup, *m_context));
		}
	}

	// --- Prefab ---
	ImGui::Separator();
	if (isPrefabRoot(iEntity)) {
		const auto& link = iEntity.getComponent<PrefabLink>();
		// Resolve the full path from asset directories.
		std::filesystem::path prefabFullPath;
		for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
			if (const auto p = assetsPath / link.prefabAssetPath; exists(p)) {
				prefabFullPath = p;
				break;
			}
		}
		const bool prefabExists = !prefabFullPath.empty();
		if (ib.menuItem("prefab_icon", "Update from Prefab", nullptr, prefabExists)) {
			auto before = SubtreeSnapshot::capture(iEntity, *m_context);
			if (scene::PrefabSerializer::applyToInstance(prefabFullPath, iEntity, *m_context)) {
				auto root = m_context->findEntityByUUID(before.entities[0].uuid);
				if (root && mp_undoManager != nullptr) {
					auto after = SubtreeSnapshot::capture(root, *m_context);
					mp_undoManager->push(mkUniq<commands::ApplyPrefabCommand>(

							std::move(before), std::move(after), "Update from Prefab"));
				}
			}
		}
		if (ib.menuItem("prefab_icon", "Revert to Prefab", nullptr, prefabExists)) {
			auto before = SubtreeSnapshot::capture(iEntity, *m_context);
			if (scene::PrefabSerializer::revertInstance(prefabFullPath, iEntity, *m_context)) {
				auto root = m_context->findEntityByUUID(before.entities[0].uuid);
				if (root && mp_undoManager != nullptr) {
					auto after = SubtreeSnapshot::capture(root, *m_context);
					mp_undoManager->push(mkUniq<commands::ApplyPrefabCommand>(

							std::move(before), std::move(after), "Revert to Prefab"));
				}
			}
		}

		ImGui::Separator();
		if (ib.menuItem("prefab_icon", "Unlink Prefab"))
			iEntity.removeComponent<PrefabLink>();

		ImGui::Separator();
	}
	if (ib.menuItem("prefab_icon", "Create Prefab...")) {
		if (const auto filepath =

					core::utils::FileDialog::saveFile("Owl Prefab (*.owlprefab)|owlprefab\n");
			!filepath.empty())

			scene::PrefabSerializer::serialize(iEntity, *m_context, filepath, iEntity.getName());
	}
	// --- Hierarchy ---
	if (iParentId != core::UUID{0}) {
		if (ib.menuItem("unparent", "Unparent")) {
			if (mp_undoManager != nullptr)
				mp_undoManager->push(mkUniq<commands::UnparentCommand>(iEntity));
			m_context->unparent(iEntity);
		}
	}

	ImGui::Separator();
	// --- Delete ---
	if (ib.menuItem("delete_entity", iHasChildren ? "Delete Entity Only" : "Delete Entity")) {
		if (mp_undoManager != nullptr)
			mp_undoManager->push(mkUniq<commands::DeleteEntityCommand>(iEntity));
		if (m_selection == iEntity)
			m_selection = {};
		auto entity = iEntity;
		m_context->destroyEntity(entity);
	}
	if (iHasChildren) {
		if (ib.menuItem("delete_cascade", "Delete with Children")) {
			if (mp_undoManager != nullptr)
				mp_undoManager->push(mkUniq<commands::DeleteSubtreeCommand>(iEntity, *m_context));
			if (m_selection == iEntity)
				m_selection = {};
			auto entity = iEntity;
			m_context->destroyEntityWithChildren(entity);
		}
	}

	ImGui::EndPopup();
}

// Function displaying the Entity Property panel.
void SceneHierarchy::renderProperties() {
	ImGui::Begin("Properties");
	if (mp_activeDocument != nullptr && mp_activeDocument->overridesGlobalPanels())
		mp_activeDocument->renderPropertiesPanel();
	else if (m_selection)
		drawComponents(m_selection);
	ImGui::End();
}

namespace {
template<isNamedComponent Comp>
void addComponentPop(scene::Entity& ioEntity, SceneUndoManager* iUndoManager) {
	if (!ioEntity.hasComponent<Comp>()) {
		const auto* iconId = componentIconName(Comp::name());
		bool clicked = false;
		if (iconId)
			clicked = gui::IconBank::instance().menuItem(iconId, Comp::name());
		else
			clicked = ImGui::MenuItem(Comp::name());
		if (clicked) {
			auto before = iUndoManager != nullptr ? EntitySnapshot::capture(ioEntity) : EntitySnapshot{};
			ioEntity.addComponent<Comp>();
			if (iUndoManager != nullptr) {
				auto after = EntitySnapshot::capture(ioEntity);
				iUndoManager->push(
						mkUniq<commands::AddComponentCommand>(std::move(before), std::move(after), Comp::name()));
			}
			ImGui::CloseCurrentPopup();
		}
	}
}

template<isNamedComponent T>
void drawComponent(scene::Entity& ioEntity, SceneUndoManager* iUndoManager) {
	constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
												 ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap |
												 ImGuiTreeNodeFlags_FramePadding;
	if (ioEntity.hasComponent<T>()) {
		// Component-scoped ID prevents label collisions across different component types
		// that share field names (e.g. "Colour" in SpriteRenderer and CircleRenderer).
		ImGui::PushID(T::name());
		auto& component = ioEntity.getComponent<T>();
		const ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
		const float lineHeight = ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
		ImGui::Separator();

		// Build label with icon spacing
		const auto* iconId = componentIconName(T::name());
		const std::string label = iconId ? std::format("     {}", T::name()) : std::string(T::name());
		const bool open = ImGui::TreeNodeEx(label.c_str(), treeNodeFlags);

		// Track which component header is currently hovered so F1 can open the matching help page.
		if (ImGui::IsItemHovered())
			g_lastHoveredComponentName = T::name();

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
			// Capture state before renderProps for undo tracking.
			const auto beforeYaml =
					iUndoManager != nullptr ? scene::SceneSerializer::serializeEntityToString(ioEntity) : std::string{};
			gui::component::renderProps(component);
			// Compare entity state after renderProps — push undo command if changed.
			// Merge coalescing handles rapid DragFloat changes automatically.
			if (iUndoManager != nullptr) {
				const auto afterYaml = scene::SceneSerializer::serializeEntityToString(ioEntity);
				if (beforeYaml != afterYaml) {
					auto cmd = mkUniq<commands::ModifyEntityCommand>(
							ioEntity.getUUID(),
							EntitySnapshot{ioEntity.getUUID(), beforeYaml},
							std::format("Modify {}", T::name()));
					cmd->captureAfter(ioEntity);
					iUndoManager->push(std::move(cmd));
				}
			}
			ImGui::TreePop();
		}
		if (removeComponent) {
			auto before = iUndoManager != nullptr ? EntitySnapshot::capture(ioEntity) : EntitySnapshot{};
			ioEntity.removeComponent<T>();
			if (iUndoManager != nullptr) {
				auto after = EntitySnapshot::capture(ioEntity);
				iUndoManager->push(mkUniq<commands::RemoveComponentCommand>(std::move(before), std::move(after),
																			T::name()));
			}
		}
		ImGui::PopID();
	}
}

template<isNamedComponent... Component>
void addComponentsFromTuple(scene::Entity& ioEntity, SceneUndoManager* iUndoManager, const std::tuple<Component...>&) {
	(..., addComponentPop<Component>(ioEntity, iUndoManager));
}

template<isNamedComponent... Component>
void drawComponentsFromTuple(scene::Entity& ioEntity, SceneUndoManager* iUndoManager, const std::tuple<Component...>&) {
	(..., drawComponent<Component>(ioEntity, iUndoManager));
}

}// namespace

void SceneHierarchy::drawComponents(const scene::Entity& iEntity) {
	if (iEntity.hasComponent<Tag>()) {
		auto& tag = iEntity.getComponent<Tag>().tag;
		const auto beforeTag = mp_undoManager != nullptr ? tag : std::string{};
		ImGui::InputText("##Tag", &tag);
		if (mp_undoManager != nullptr && ImGui::IsItemDeactivatedAfterEdit() && tag != beforeTag) {
			// Capture after state (tag already changed in-place by InputText).
			auto cmd = mkUniq<commands::ModifyEntityCommand>(
					iEntity.getUUID(),
					EntitySnapshot{iEntity.getUUID(),
								   scene::SceneSerializer::serializeEntityToString(iEntity)},
					"Rename Entity");
			// The "before" snapshot has the old tag — reconstruct it.
			auto& tagRef = iEntity.getComponent<Tag>().tag;
			const auto currentTag = tagRef;
			tagRef = beforeTag;
			cmd = mkUniq<commands::ModifyEntityCommand>(
					iEntity.getUUID(), EntitySnapshot::capture(iEntity), "Rename Entity");
			tagRef = currentTag;
			cmd->captureAfter(iEntity);
			mp_undoManager->push(std::move(cmd));
		}
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
		addComponentsFromTuple(m_selection, mp_undoManager, OptionalComponents{});
		ImGui::EndPopup();
	}
	ImGui::PopItemWidth();
	drawComponentsFromTuple(m_selection, mp_undoManager, gui::component::DrawableComponents{});
}

}// namespace owl::nest::panel
