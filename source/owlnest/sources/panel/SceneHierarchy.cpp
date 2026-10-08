/**
 * @file SceneHierarchy.cpp
 * @author Silmaen
 * @date 26/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneHierarchy.h"

#include "../EditorLayer.h"
#include "../UndoManager.h"
#include "../commands/ComponentCommands.h"
#include "../commands/EntityCommands.h"
#include "../commands/HierarchyCommands.h"
#include "../commands/PrefabCommands.h"
#include "../document/Document.h"

#include <gui/IconBank.h>
#include <gui/utils.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <platform/FileDialog.h>
#include <renderer/RenderLayer.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <scene/PrefabSerializer.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace owl::scene::component;

namespace owl::nest::panel {

namespace {
std::string g_lastHoveredComponentName;

auto isPrefabRoot(const scene::Entity& iEntity) -> bool { return iEntity && iEntity.hasComponent<PrefabLink>(); }

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

auto resolvePrefabPath(const PrefabLink& iLink) -> std::filesystem::path {
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (auto candidate = assetsPath / iLink.prefabAssetPath; exists(candidate))
			return candidate;
	}
	return {};
}

auto isComponentOverridden(const scene::Entity& iEntity, const std::string& iComponentKey) -> bool {
	const auto* scene = iEntity.getScene();
	if (scene == nullptr)
		return false;
	const auto root = scene::PrefabSerializer::findInstanceRoot(iEntity, *scene);
	if (!root)
		return false;
	const auto& link = root.getComponent<PrefabLink>();
	const auto canonical = link.findCanonicalUuid(static_cast<uint64_t>(iEntity.getUUID()));
	return canonical.has_value() && link.isOverridden(*canonical, iComponentKey);
}

auto entityArgOf(const scene::Entity& iEntity) -> commands::ArgValue {
	return static_cast<int64_t>(static_cast<uint64_t>(iEntity.getUUID()));
}

auto recordOverrides(const scene::Entity& iEntity, const std::string& iBeforeYaml) -> commands::PrefabOverrideChange {
	const auto* scene = iEntity.getScene();
	if (scene == nullptr)
		return {};
	return commands::PrefabOverrideChange::record(iEntity, *scene, iBeforeYaml);
}

void revertComponentToPrefab(const scene::Entity& iEntity, const std::string& iComponentKey, const char* iLabel,
							 SceneUndoManager* iUndoManager) {
	const auto* scene = iEntity.getScene();
	if (scene == nullptr)
		return;
	const auto root = scene::PrefabSerializer::findInstanceRoot(iEntity, *scene);
	if (!root)
		return;
	const auto prefabPath = resolvePrefabPath(root.getComponent<PrefabLink>());
	if (prefabPath.empty()) {
		OWL_WARN("Prefab {} not found in the asset directories.", root.getComponent<PrefabLink>().prefabAssetPath)
		return;
	}
	auto before = EntitySnapshot::capture(iEntity);
	auto change = commands::PrefabOverrideChange::capture(iEntity, *scene);
	if (!scene::PrefabSerializer::revertComponent(prefabPath, root, iEntity, iComponentKey))
		return;
	change.captureAfter(*scene);
	if (iUndoManager == nullptr)
		return;
	auto cmd = mkUniq<commands::ModifyEntityCommand>(iEntity.getUUID(), std::move(before),
													 std::format("Revert {} to Prefab", iLabel));
	cmd->captureAfter(iEntity);
	cmd->setPrefabOverrides(std::move(change));
	iUndoManager->push(std::move(cmd));
}

void drawOverrideMarker() {
	const auto itemMin = ImGui::GetItemRectMin();
	const auto itemMax = ImGui::GetItemRectMax();
	ImGui::GetWindowDrawList()->AddRectFilled(itemMin, {itemMin.x + 3.0f, itemMax.y},
											  ImGui::GetColorU32(ImGuiCol_CheckMark));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Overrides the prefab: kept by Update from Prefab");
}

auto componentIconName(const char* iCompName) -> const char* {
	static const std::unordered_map<std::string_view, const char*> map = {
			{"Transform", "comp_transform"},     {"Camera", "comp_camera"},
			{"Sprite Renderer", "comp_sprite"},  {"Animated Sprite", "comp_animated_sprite"},
			{"Circle Renderer", "comp_circle"},  {"Text Renderer", "comp_text"},
			{"Physical body", "comp_physics"},   {"Native Script", "comp_script"},
			{"Trigger", "comp_trigger"},         {"Player", "comp_player"},
			{"Entity Link", "comp_link"},        {"Background Texture", "comp_background"},
			{"Visibility", "comp_visibility"},   {"Sound Source", "comp_sound"},
			{"Sound Listener", "comp_sound"},    {"Lua Script", "comp_lua_script"},
			{"Canvas", "comp_canvas"},           {"UI Rect", "comp_ui_rect"},
			{"UI Text", "comp_ui_text"},         {"UI Image", "comp_ui_image"},
			{"UI Panel", "comp_ui_panel"},       {"UI Button", "comp_ui_button"},
			{"UI Slider", "comp_ui_slider"},     {"UI Progress Bar", "comp_ui_progress"},
			{"Prefab Link", "prefab_icon"},      {"Tilemap", "owltileset_icon"},
			{"Voxel World", "comp_voxel_world"}, {"Fly Camera", "comp_fly_camera"},
			{"Voxel Player", "comp_player"},
	};
	if (const auto it = map.find(iCompName); it != map.end())
		return it->second;
	return nullptr;
}

}// namespace

[[maybe_unused]] SceneHierarchy::SceneHierarchy(const shared<scene::Scene>& iScene) { setContext(iScene); }

void SceneHierarchy::setContext(const shared<scene::Scene>& iContext) {
	m_inspector.flush(m_context.get(), mp_undoManager);
	m_context = iContext;
	m_selection = {};
}

void SceneHierarchy::onImGuiRender() {
	g_lastHoveredComponentName.clear();
	renderHierarchy();
	renderProperties();
}

auto SceneHierarchy::lastHoveredComponentName() -> const std::string& { return g_lastHoveredComponentName; }

// Function displaying the Hierarchy panel.
void SceneHierarchy::renderHierarchy() {
	const std::string title =
			(mp_activeDocument != nullptr ? mp_activeDocument->hierarchyPanelTitle() : std::string{"Scene Hierarchy"}) +
			std::string{"###Hierarchy"};
	ImGui::Begin(title.c_str());

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
				std::ignore = getCommandTarget().execute("entity.create", {});
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();

		if (constexpr ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen; ImGui::TreeNodeEx("root", flag)) {
			// Drop target on root node: unparent dragged entity.
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
					const uint64_t droppedUuid = *static_cast<const uint64_t*>(payload->Data);
					std::ignore = getCommandTarget().execute("entity.reparent",
															 {{"entity", static_cast<int64_t>(droppedUuid)}});
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
		for (auto entity: roots) drawEntityNode(entity);
		return;
	}

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
		const bool known =
				std::ranges::any_of(layers, [&](const auto& l) -> bool { return l->getName() == effective; });
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
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
				const uint64_t droppedUuid = *static_cast<const uint64_t*>(payload->Data);
				if (auto e = m_context->findEntityByUUID(core::UUID{droppedUuid}); e) {
					auto before = EntitySnapshot::capture(e);
					if (e.getComponent<Hierarchy>().parentId != core::UUID{0})
						m_context->unparent(e);
					auto& tag = e.hasComponent<RendererTag>() ? e.getComponent<RendererTag>()
															  : e.addComponent<RendererTag>();
					tag.rendererName = name;
					auto overrides = recordOverrides(e, before.yamlData);
					if (mp_undoManager != nullptr) {
						auto cmd = mkUniq<commands::ModifyEntityCommand>(e.getUUID(), std::move(before),
																		 std::format("Route to layer '{}'", name));
						cmd->captureAfter(e);
						cmd->setPrefabOverrides(std::move(overrides));
						mp_undoManager->push(std::move(cmd));
					}
				}
			}

			ImGui::EndDragDropTarget();
		}
		if (open) {
			if (const auto it = bucketed.find(name); it != bucketed.end()) {
				for (auto entity: it->second) drawEntityNode(entity);
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
			for (auto entity: unrouted) drawEntityNode(entity);

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
			if (core::UUID{droppedUuid} != iEntity.getUUID())
				std::ignore = getCommandTarget().execute(
						"entity.reparent",
						{{"entity", static_cast<int64_t>(droppedUuid)},
						 {"parent", static_cast<int64_t>(static_cast<uint64_t>(iEntity.getUUID()))}});
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void SceneHierarchy::drawEntityContextMenu(const scene::Entity& iEntity, const bool iHasChildren,
										   const core::UUID iParentId) {
	if (!ImGui::BeginPopup("EntityContext"))
		return;
	const auto& ib = gui::IconBank::instance();
	// --- Create ---
	const auto target = getCommandTarget();
	const commands::ArgValue entityArg = static_cast<int64_t>(static_cast<uint64_t>(iEntity.getUUID()));
	if (ib.menuItem("add_entity", "Create Root Entity"))
		std::ignore = target.execute("entity.create", {});
	if (ib.menuItem("add_child_entity", "Create Child Entity"))
		std::ignore = target.execute("entity.create", {{"name", std::string{"Child Entity"}}, {"parent", entityArg}});

	ImGui::Separator();
	// --- Duplicate ---
	if (ib.menuItem("duplicate", "Duplicate Entity"))
		std::ignore = target.execute("entity.duplicate", {{"entity", entityArg}});
	if (iHasChildren && ib.menuItem("duplicate", "Duplicate Subtree"))
		std::ignore = target.execute("entity.duplicate", {{"entity", entityArg}, {"children", true}});

	// --- Prefab ---
	ImGui::Separator();
	if (isPrefabRoot(iEntity)) {
		const auto prefabFullPath = resolvePrefabPath(iEntity.getComponent<PrefabLink>());
		const bool prefabExists = !prefabFullPath.empty();
		if (ib.menuItem("prefab_icon", "Update from Prefab", nullptr, prefabExists)) {
			auto before = SubtreeSnapshot::capture(iEntity, *m_context);
			if (scene::PrefabSerializer::applyToInstance(prefabFullPath, iEntity, *m_context) &&
				mp_undoManager != nullptr)
				mp_undoManager->push(mkUniq<commands::ApplyPrefabCommand>(
						std::move(before), SubtreeSnapshot::capture(iEntity, *m_context), "Update from Prefab"));
		}
		if (ib.menuItem("prefab_icon", "Revert to Prefab", nullptr, prefabExists)) {
			auto before = SubtreeSnapshot::capture(iEntity, *m_context);
			if (scene::PrefabSerializer::revertInstance(prefabFullPath, iEntity, *m_context) &&
				mp_undoManager != nullptr)
				mp_undoManager->push(mkUniq<commands::ApplyPrefabCommand>(
						std::move(before), SubtreeSnapshot::capture(iEntity, *m_context), "Revert to Prefab"));
		}

		ImGui::Separator();
		if (ib.menuItem("prefab_icon", "Unlink Prefab"))
			std::ignore = target.execute("component.remove",
										 {{"entity", entityArg}, {"component", std::string{PrefabLink::name()}}});

		ImGui::Separator();
	}
	if (ib.menuItem("prefab_icon", "Create Prefab...")) {
		if (const auto filepath = platform::FileDialog::saveFile("Owl Prefab (*.owlprefab)|owlprefab\n");
			!filepath.empty() && !scene::PrefabSerializer::serialize(iEntity, *m_context, filepath, iEntity.getName()))
			OWL_ERROR("SceneHierarchy: Prefab {} was not created.", filepath.string())
	}
	// --- Tilemap quick action: open the referenced asset in the tilemap editor.
	if (iEntity.hasComponent<scene::component::Tilemap>() && mp_parentEditor != nullptr) {
		const auto& tilemap = iEntity.getComponent<scene::component::Tilemap>();
		if (!tilemap.tilemapPath.empty()) {
			ImGui::Separator();
			if (ib.menuItem("owltilemap_icon", "Open in Tilemap Editor")) {
				const auto& app = app::Application::get();
				std::filesystem::path resolved;
				for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
					if (const auto candidate = assetsPath / tilemap.tilemapPath; exists(candidate)) {
						resolved = candidate;
						break;
					}
				}
				if (!resolved.empty())
					mp_parentEditor->requestDeferredOpen(resolved);
				else
					OWL_CORE_WARN("Could not resolve tilemap path '{}' against any asset directory.",
								  tilemap.tilemapPath.string())
			}
		}
	}

	// --- Hierarchy ---
	if (iParentId != core::UUID{0}) {
		if (ib.menuItem("unparent", "Unparent"))
			std::ignore = target.execute("entity.reparent", {{"entity", entityArg}});
	}

	ImGui::Separator();
	// --- Delete ---
	const bool deleteOne = ib.menuItem("delete_entity", iHasChildren ? "Delete Entity Only" : "Delete Entity");
	const bool deleteAll = iHasChildren && ib.menuItem("delete_cascade", "Delete with Children");
	if (deleteOne || deleteAll) {
		if (m_selection == iEntity)
			m_selection = {};
		std::ignore = target.execute("entity.delete", {{"entity", entityArg}, {"children", deleteAll}});
	}

	ImGui::EndPopup();
}

// Function displaying the Entity Property panel.
void SceneHierarchy::renderProperties() {
	const std::string title =
			(mp_activeDocument != nullptr ? mp_activeDocument->propertiesPanelTitle() : std::string{"Properties"}) +
			std::string{"###Properties"};
	ImGui::Begin(title.c_str());
	if (mp_activeDocument != nullptr && mp_activeDocument->overridesGlobalPanels()) {
		m_inspector.flush(m_context.get(), mp_undoManager);
		mp_activeDocument->renderPropertiesPanel();
	} else {
		m_inspector.beginFrame(m_selection, m_context.get(), mp_undoManager);
		if (m_selection)
			drawComponents(m_selection);
		m_inspector.endFrame(m_context.get(), mp_undoManager);
	}
	ImGui::End();
}

namespace {
template<typename>
constexpr bool isRaycastOnlyComponent = false;
template<>
constexpr bool isRaycastOnlyComponent<scene::component::RaycastDoor> = true;
template<>
constexpr bool isRaycastOnlyComponent<scene::component::RaycastPushWall> = true;

template<typename>
constexpr bool isUiOnlyComponent = false;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiButton> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiImage> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiPanel> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiProgressBar> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiRect> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiSlider> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiText> = true;

auto layerTypeKeyForEntity(const scene::Entity& iEntity) -> std::string {
	const auto& stack = renderer::Renderer::getRenderStack();
	if (stack.isEmpty())
		return {};
	shared<renderer::RenderLayer> layer;
	if (iEntity.hasComponent<scene::component::RendererTag>()) {
		const auto& tag = iEntity.getComponent<scene::component::RendererTag>();
		if (!tag.rendererName.empty())
			layer = stack.findByName(tag.rendererName);
	}
	if (!layer)
		layer = stack.getDefaultLayer();
	return layer ? std::string{layer->getTypeKey()} : std::string{};
}

template<isNamedComponent Comp>
void addComponentPop(scene::Entity& ioEntity, const commands::CommandTarget& iCommands,
					 const std::string& iLayerTypeKey) {
	if constexpr (isRaycastOnlyComponent<Comp>) {
		if (!iLayerTypeKey.empty() && iLayerTypeKey != "RendererRaycast")
			return;
	}
	if constexpr (isUiOnlyComponent<Comp>) {
		if (!iLayerTypeKey.empty() && iLayerTypeKey != "Renderer2D")
			return;
	}
	if (!ioEntity.hasComponent<Comp>()) {
		const auto* iconId = componentIconName(Comp::name());
		bool clicked = false;
		if (iconId)
			clicked = gui::IconBank::instance().menuItem(iconId, Comp::name());
		else
			clicked = ImGui::MenuItem(Comp::name());
		if (clicked) {
			std::ignore = iCommands.execute(
					"component.add", {{"entity", entityArgOf(ioEntity)}, {"component", std::string{Comp::name()}}});
			ImGui::CloseCurrentPopup();
		}
	}
}

template<isNamedComponent T>
void drawComponent(scene::Entity& ioEntity, SceneUndoManager* iUndoManager, const commands::CommandTarget& iCommands,
				   InspectorEditTracker& ioInspector) {
	constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
												 ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap |
												 ImGuiTreeNodeFlags_FramePadding;
	if (ioEntity.hasComponent<T>()) {
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
		const bool overridden = isComponentOverridden(ioEntity, T::key());
		if (overridden)
			drawOverrideMarker();

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
		bool revertComponent = false;
		if (ImGui::BeginPopup("ComponentSettings")) {
			if (overridden && ImGui::MenuItem("Revert this component"))
				revertComponent = true;
			if (ImGui::MenuItem("Remove component"))
				removeComponent = true;
			ImGui::EndPopup();
		}
		if (open) {
			ioInspector.beginComponent(ioEntity, T::key());
			gui::component::renderProps(component);
			ioInspector.endComponent(ioEntity, T::key(), T::name(), iUndoManager);
			ImGui::TreePop();
		}
		if (revertComponent)
			revertComponentToPrefab(ioEntity, T::key(), T::name(), iUndoManager);
		if (removeComponent)
			std::ignore = iCommands.execute("component.remove",
											{{"entity", entityArgOf(ioEntity)}, {"component", std::string{T::name()}}});
		ImGui::PopID();
	}
}

template<isNamedComponent... Component>
void addComponentsFromTuple(scene::Entity& ioEntity, const commands::CommandTarget& iCommands,
							const std::string& iLayerTypeKey, const std::tuple<Component...>&) {
	(..., addComponentPop<Component>(ioEntity, iCommands, iLayerTypeKey));
}

template<isNamedComponent... Component>
void drawComponentsFromTuple(scene::Entity& ioEntity, SceneUndoManager* iUndoManager,
							 const commands::CommandTarget& iCommands, InspectorEditTracker& ioInspector,
							 const std::tuple<Component...>&) {
	(..., drawComponent<Component>(ioEntity, iUndoManager, iCommands, ioInspector));
}

}// namespace

void SceneHierarchy::drawComponents(const scene::Entity& iEntity) {
	if (iEntity.hasComponent<Tag>()) {
		auto& tag = iEntity.getComponent<Tag>().tag;
		const auto frameTag = tag;
		ImGui::InputText("##Tag", &tag);
		if (ImGui::IsItemActivated())
			m_renameBefore = frameTag;
		if (ImGui::IsItemDeactivatedAfterEdit() && m_renameBefore.has_value() && tag != *m_renameBefore) {
			const auto currentTag = tag;
			tag = *m_renameBefore;
			auto before = EntitySnapshot::capture(iEntity);
			tag = currentTag;
			auto overrides = recordOverrides(iEntity, before.yamlData);
			if (mp_undoManager != nullptr) {
				auto cmd = mkUniq<commands::ModifyEntityCommand>(iEntity.getUUID(), std::move(before), "Rename Entity");
				cmd->captureAfter(iEntity);
				cmd->setPrefabOverrides(std::move(overrides));
				mp_undoManager->push(std::move(cmd));
			}
		}
		if (ImGui::IsItemDeactivated())
			m_renameBefore.reset();
	}
	ImGui::SameLine();
	ImGui::Text("Entity name");
	if (isComponentOverridden(iEntity, Tag::key())) {
		ImGui::SameLine();
		if (ImGui::SmallButton("Revert name"))
			revertComponentToPrefab(iEntity, Tag::key(), "name", mp_undoManager);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("The name overrides the prefab: revert it to the prefab name");
	}

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
		const std::string layerTypeKey = layerTypeKeyForEntity(m_selection);
		addComponentsFromTuple(m_selection, getCommandTarget(), layerTypeKey, OptionalComponents{});
		ImGui::EndPopup();
	}
	ImGui::PopItemWidth();
	drawComponentsFromTuple(m_selection, mp_undoManager, getCommandTarget(), m_inspector,
							gui::component::DrawableComponents{});
}

}// namespace owl::nest::panel
