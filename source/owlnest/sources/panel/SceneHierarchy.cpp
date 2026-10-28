/**
 * @file SceneHierarchy.cpp
 * @author Silmaen
 * @date 26/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneHierarchy.h"

#include "../DockLayout.h"

#include "../EditorLayer.h"
#include "../UndoManager.h"
#include "../commands/ComponentCommands.h"
#include "../commands/EntityCommands.h"
#include "../commands/HierarchyCommands.h"
#include "../commands/PrefabCommands.h"
#include "../document/Document.h"

#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/SearchField.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Tooltip.h>
#include <gui/widgets/TreeRow.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <platform/FileDialog.h>
#include <renderer/RenderLayer.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <scene/ComponentRegistry.h>
#include <scene/PrefabSerializer.h>
#include <scene/component/Camera.h>
#include <scene/component/Canvas.h>
#include <scene/component/Environment.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/Light.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
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

void drawOverrideMarker(const gui::widgets::SectionHeaderResult& iHeader) {
	ImGui::GetWindowDrawList()->AddRectFilled(gui::vec(iHeader.min), {iHeader.min.x() + 3.0f, iHeader.max.y()},
											  ImGui::GetColorU32(gui::vec(gui::UiLayer::getTheme().accent)));
	if (iHeader.hovered)
		ImGui::SetTooltip("Overrides the prefab: kept by Update from Prefab");
}

// Glyph and colour of an entity row, from its most telling component.
struct EntityKind {
	const char* glyph;
	uint32_t color;
};

auto entityKind(const scene::Entity& iEntity, const bool iHasChildren) -> EntityKind {
	if (iEntity.hasComponent<Camera>() || iEntity.hasComponent<FlyCamera>())
		return {gui::icon::video, 0x7fb4ff};
	if (iEntity.hasComponent<SpriteRenderer>() || iEntity.hasComponent<AnimatedSpriteRenderer>() ||
		iEntity.hasComponent<BackgroundTexture>())
		return {gui::icon::image, 0xf08ab0};
	if (iEntity.hasComponent<CircleRenderer>())
		return {gui::icon::circle, 0xf2a65a};
	if (iEntity.hasComponent<MeshRenderer>())
		return {gui::icon::package, 0x7fb83e};
	if (iEntity.hasComponent<Light>()) {
		const auto& light = iEntity.getComponent<Light>();
		const uint32_t colour = light.enabled ? 0xffd75e : 0x7a7a7a;
		switch (light.type) {
			case Light::Type::Directional:
				return {gui::icon::sun, colour};
			case Light::Type::Point:
				return {gui::icon::lightbulb, colour};
			case Light::Type::Spot:
				return {gui::icon::flashlight, colour};
		}
	}
	if (iEntity.hasComponent<Light2D>()) {
		const auto& light = iEntity.getComponent<Light2D>();
		const uint32_t colour = light.enabled ? 0xffb347 : 0x7a7a7a;
		switch (light.type) {
			case Light2D::Type::Ambient:
				return {gui::icon::sun, colour};
			case Light2D::Type::Point:
				return {gui::icon::lightbulb, colour};
			case Light2D::Type::Spot:
				return {gui::icon::flashlight, colour};
		}
	}
	if (iEntity.hasComponent<Environment>())
		return {gui::icon::globe, 0x5ec8e8};
	if (iEntity.hasComponent<Text>() || iEntity.hasComponent<UiText>())
		return {gui::icon::type, 0xc7a6ff};
	if (iEntity.hasComponent<UiButton>())
		return {gui::icon::mousePointerClick, 0x6fd6c0};
	if (iEntity.hasComponent<UiImage>())
		return {gui::icon::image, 0x6fd6c0};
	if (iEntity.hasComponent<UiWindow>())
		return {gui::icon::appWindow, 0x6fd6c0};
	if (iEntity.hasComponent<UiLayout>())
		return {gui::icon::layoutGrid, 0x6fd6c0};
	if (iEntity.hasComponent<UiList>())
		return {gui::icon::list, 0x6fd6c0};
	if (iEntity.hasComponent<UiScrollView>())
		return {gui::icon::scrollText, 0x6fd6c0};
	if (iEntity.hasComponent<UiToolCursor>())
		return {gui::icon::mousePointer2, 0x6fd6c0};
	if (iEntity.hasComponent<UiPanel>())
		return {gui::icon::panelsTopLeft, 0x6fd6c0};
	if (iEntity.hasComponent<Canvas>())
		return {gui::icon::appWindow, 0x6fd6c0};
	if (iEntity.hasComponent<Tilemap>())
		return {gui::icon::grid3x3, 0xd65fb8};
	if (iEntity.hasComponent<IsometricObject>())
		return {gui::icon::house, 0xd65fb8};
	if (iEntity.hasComponent<VoxelWorld>())
		return {gui::icon::cuboid, 0xe8c15a};
	if (iEntity.hasComponent<Player>() || iEntity.hasComponent<VoxelPlayer>())
		return {gui::icon::userRound, 0xe8c15a};
	if (iEntity.hasComponent<Trigger>())
		return {gui::icon::zap, 0xef8a3c};
	if (iEntity.hasComponent<SoundSource>())
		return {gui::icon::volume2, 0x3faa72};
	if (iEntity.hasComponent<LuaScript>())
		return {gui::icon::fileCode2, 0x2fb5a0};
	if (iEntity.hasComponent<PhysicBody>())
		return {gui::icon::box, 0x8a929e};
	return {iHasChildren ? gui::icon::folder : gui::icon::box, 0};
}

// Kind colours are tuned for the dark theme; on a light ground they are darkened to stay readable.
auto kindColor(const uint32_t iHex) -> math::vec4 {
	const auto& theme = gui::UiLayer::getTheme();
	if (iHex == 0)
		return theme.textMuted;
	const math::vec4 color{static_cast<float>((iHex >> 16U) & 0xffU) / 255.f,
						   static_cast<float>((iHex >> 8U) & 0xffU) / 255.f, static_cast<float>(iHex & 0xffU) / 255.f,
						   1.f};
	const bool light = theme.surface1.x() + theme.surface1.y() + theme.surface1.z() > 1.5f;
	return light ? gui::Theme::mix(color, theme.text, 0.45f) : color;
}

// "Light" submenu: `entity.create_light` per 3D kind, `entity.create_light2d` per 2D kind, optionally under a parent.
void drawCreateLightMenu(const auto& iTarget, const std::optional<commands::ArgValue>& iParent) {
	if (!ImGui::BeginMenu("Light"))
		return;
	for (const auto& [kind, glyph]: {std::pair{"Directional", gui::icon::sun}, std::pair{"Point", gui::icon::lightbulb},
									 std::pair{"Spot", gui::icon::flashlight}}) {
		if (!gui::widgets::menuItem({.glyph = glyph, .label = kind}))
			continue;
		const commands::CommandArgs args =
				iParent ? commands::CommandArgs{{"type", std::string{kind}}, {"parent", *iParent}}
						: commands::CommandArgs{{"type", std::string{kind}}};
		std::ignore = iTarget.execute("entity.create_light", args);
	}
	ImGui::Separator();
	for (const auto& [kind, glyph]: {std::pair{"Ambient", gui::icon::sun}, std::pair{"Point", gui::icon::lightbulb},
									 std::pair{"Spot", gui::icon::flashlight}}) {
		const std::string label = std::format("{} 2D", kind);
		if (!gui::widgets::menuItem({.glyph = glyph, .label = label.c_str()}))
			continue;
		const commands::CommandArgs args =
				iParent ? commands::CommandArgs{{"type", std::string{kind}}, {"parent", *iParent}}
						: commands::CommandArgs{{"type", std::string{kind}}};
		std::ignore = iTarget.execute("entity.create_light2d", args);
	}
	ImGui::EndMenu();
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
			{"Voxel Player", "comp_player"},     {"Mesh Renderer", "gltf_icon"},
	};
	if (const auto it = map.find(iCompName); it != map.end())
		return it->second;
	return nullptr;
}

}// namespace

[[maybe_unused]] SceneHierarchy::SceneHierarchy(const shared<scene::Scene>& iScene) { setContext(iScene); }

void SceneHierarchy::setContext(const shared<scene::Scene>& iContext) {
	m_inspector.flush(getCommandTarget());
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
	const std::string title = panelWindow::title(gui::icon::listTree,
												 mp_activeDocument != nullptr ? mp_activeDocument->hierarchyPanelTitle()
																			  : std::string{"Scene Hierarchy"},
												 panelWindow::hierarchy);
	ImGui::Begin(title.c_str());

	if (mp_activeDocument != nullptr && mp_activeDocument->overridesGlobalPanels()) {
		mp_activeDocument->renderHierarchyPanel();
		ImGui::End();
		return;
	}

	if (m_context) {
		// Rows are the only lists where the selection takes the accent; headers and combos stay grey.
		ImGui::PushStyleColor(ImGuiCol_Header, gui::vec(gui::UiLayer::getTheme().getSelection()));
		renderHierarchyToolbar();
		if (!m_searchFilter.empty()) {
			renderSearchResults();
			ImGui::PopStyleColor();
			ImGui::End();
			return;
		}
		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
			m_selection = {};

		// Right-click on blank space
		ImGui::PushID("...");
		if (ImGui::BeginPopupContextWindow(nullptr, 1)) {
			if (gui::widgets::menuItem({.glyph = gui::icon::plus, .label = "Create Empty Entity"})) {
				std::ignore = getCommandTarget().execute("entity.create", {});
			}
			drawCreateLightMenu(getCommandTarget(), std::nullopt);
			ImGui::EndPopup();
		}
		ImGui::PopID();

		if (gui::widgets::treeRow("root", {.label = "Scene",
										   .glyph = gui::icon::clapperboard,
										   .glyphColor = gui::UiLayer::getTheme().accent,
										   .hasChildren = true,
										   .defaultOpen = true})
					.open) {
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
			gui::widgets::endTreeRow();
		}
		ImGui::PopStyleColor();
	} else {
		std::ignore = gui::widgets::emptyState(gui::icon::listTree, "No scene",
											   "The active document has no entities. Open a scene to see them here.");
	}
	ImGui::End();
}

void SceneHierarchy::renderHierarchyToolbar() {
	if (gui::widgets::button("addEntity", {.glyph = gui::icon::plus,
										   .label = "Add",
										   .tooltip = "Add entity",
										   .description = "Create an empty entity, or a child of the selection."}))
		ImGui::OpenPopup("##addEntity");
	if (ImGui::BeginPopup("##addEntity")) {
		if (gui::widgets::menuItem({.glyph = gui::icon::box, .label = "Empty entity"}))
			std::ignore = getCommandTarget().execute("entity.create", {});
		if (gui::widgets::menuItem({.glyph = gui::icon::listTree,
									.label = "Child of the selection",
									.enabled = static_cast<bool>(m_selection)}))
			std::ignore = getCommandTarget().execute(
					"entity.create", {{"parent", static_cast<int64_t>(static_cast<uint64_t>(m_selection.getUUID()))}});
		drawCreateLightMenu(getCommandTarget(), std::nullopt);
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	std::ignore = gui::widgets::searchField("##hierarchySearch", m_searchFilter, "Search entities");
	ImGui::Separator();
}

void SceneHierarchy::renderSearchResults() {
	size_t matches = 0;
	for (const auto& entity: m_context->getAllEntities()) {
		if (!entity.hasComponent<Tag>())
			continue;
		const auto& tag = entity.getComponent<Tag>().tag;
		if (!gui::widgets::matchesFilter(tag, m_searchFilter))
			continue;
		++matches;
		ImGui::PushID(static_cast<int>(static_cast<uint32_t>(entity)));
		if (ImGui::Selectable(tag.c_str(), entity == m_selection))
			m_selection = entity;
		ImGui::PopID();
	}
	if (matches == 0)
		std::ignore = gui::widgets::emptyState(gui::icon::search, "No entity matches",
											   "Check the spelling, or clear the search to see the whole scene.");
}

void SceneHierarchy::renderRootEntities() {
	const auto& stack = renderer::Renderer::getRenderStack();
	const auto& layers = stack.getLayers();
	const auto roots = m_context->getRootEntities();
	// 0 or 1 layer → flat list, no extra nesting.
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
		const std::string countText = std::format("{}", count);

		ImGui::PushID(name.c_str());
		const bool open = gui::widgets::treeRow("##layer", {.label = name.c_str(),
															.glyph = gui::icon::layers,
															.glyphColor = kindColor(0xe8c15a),
															.trailing = countText.c_str(),
															.hasChildren = true,
															.defaultOpen = true})
								  .open;
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

			gui::widgets::endTreeRow();
		}

		ImGui::PopID();
	}

	if (!unrouted.empty()) {
		ImGui::PushID("__unrouted__");

		const std::string countText = std::format("{}", unrouted.size());
		const bool open = gui::widgets::treeRow("##layer_unrouted", {.label = "(unrouted)",
																	 .glyph = gui::icon::triangleAlert,
																	 .glyphColor = gui::UiLayer::getTheme().warning,
																	 .trailing = countText.c_str(),
																	 .labelColor = gui::UiLayer::getTheme().warning,
																	 .hasChildren = true,
																	 .defaultOpen = true})
								  .open;
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))

			ImGui::SetTooltip("These root entities have a RendererTag whose name does not match\n"
							  "any active layer — they will be skipped at render time.");
		if (open) {
			for (auto entity: unrouted) drawEntityNode(entity);

			gui::widgets::endTreeRow();
		}

		ImGui::PopID();
	}
}

// NOLINTNEXTLINE(misc-no-recursion)
void SceneHierarchy::drawEntityNode(const scene::Entity& iEntity) {
	const auto& theme = gui::UiLayer::getTheme();
	const auto& tag = iEntity.getComponent<Tag>().tag;
	const auto& [parentId, childrenIds] = iEntity.getComponent<Hierarchy>();
	const bool hasChildren = !childrenIds.empty();
	auto& [gameVisible, editorVisible] = iEntity.getComponent<Visibility>();
	const bool isPartOfPrefab = static_cast<bool>(findPrefabRoot(iEntity, *m_context));
	const auto kind = entityKind(iEntity, hasChildren);
	const std::string countText = hasChildren ? std::format("{}", childrenIds.size()) : std::string{};
	auto labelColor = isPartOfPrefab ? theme.info : math::vec4{0.f, 0.f, 0.f, 0.f};
	if (!editorVisible)
		labelColor = theme.textDisabled;

	ImGui::PushID(static_cast<int>(static_cast<uint32_t>(iEntity)));
	const auto row = gui::widgets::treeRow("##node", {.label = tag.c_str(),
													  .glyph = kind.glyph,
													  .glyphColor = kindColor(kind.color),
													  .trailing = hasChildren ? countText.c_str() : nullptr,
													  .labelColor = labelColor,
													  .selected = iEntity == m_selection,
													  .hasChildren = hasChildren,
													  .defaultOpen = true});
	const bool open = row.open;
	const bool treeNodeClicked = row.clicked;

	// Bind context menu to the row (must be right after it).
	ImGui::OpenPopupOnItemClick("EntityContext", ImGuiPopupFlags_MouseButtonRight);

	// Drag source for reparenting.
	if (ImGui::BeginDragDropSource()) {
		const uint64_t uuid = iEntity.getUUID();
		ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &uuid, sizeof(uuid));
		gui::widgets::dragPreview(kind.glyph, tag.c_str(), "Entity");
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

	// Visibility switches on the right of every row.
	{
		gui::UiLayer::pushTextStyle(gui::TextStyle::Label);
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		const float side = row.max.y() - row.min.y();
		const auto toggleButton = [&](const char* iId, const char* iGlyph, bool& ioFlag, const char* iTip,
									  const float iX) -> void {
			ImGui::SetCursorScreenPos({iX, row.min.y()});
			if (ImGui::InvisibleButton(iId, {side, side}))
				ioFlag = !ioFlag;
			const bool hovered = ImGui::IsItemHovered();
			const auto glyphSize = ImGui::CalcTextSize(iGlyph);
			auto color = ioFlag ? theme.textMuted : theme.textDisabled;
			if (hovered)
				color = theme.text;
			ImGui::GetWindowDrawList()->AddText({std::floor(iX + (side - glyphSize.x) * 0.5f),
												 std::floor(row.min.y() + (side - glyphSize.y) * 0.5f)},
												ImGui::GetColorU32(gui::vec(color)), iGlyph);
			gui::widgets::itemTooltip(iTip);
		};
		const float right = row.max.x() - 4.f;
		toggleButton("##edVis", editorVisible ? gui::icon::eye : gui::icon::eyeOff, editorVisible,
					 "Visible in the editor", right - side * 2.f);
		toggleButton("##gameVis", gameVisible ? gui::icon::video : gui::icon::videoOff, gameVisible,
					 "Visible in the game", right - side);
		ImGui::SetCursorScreenPos(cursor);
		gui::UiLayer::popTextStyle();
	}

	drawEntityContextMenu(iEntity, hasChildren, parentId);

	if (open) {
		// Recursively draw children.
		for (const auto childId: childrenIds) {
			if (const auto child = m_context->findEntityByUUID(childId); child)
				drawEntityNode(child);
		}
		gui::widgets::endTreeRow();
	}

	ImGui::PopID();

	if (treeNodeClicked)
		m_selection = iEntity;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void SceneHierarchy::drawEntityContextMenu(const scene::Entity& iEntity, const bool iHasChildren,
										   const core::UUID iParentId) {
	if (!ImGui::BeginPopup("EntityContext"))
		return;
	gui::widgets::menuHeader(iEntity.getName().c_str(), "Entity");
	// --- Create ---
	const auto target = getCommandTarget();
	const commands::ArgValue entityArg = static_cast<int64_t>(static_cast<uint64_t>(iEntity.getUUID()));
	if (gui::widgets::menuItem({.glyph = gui::icon::plus, .label = "Create Root Entity"}))
		std::ignore = target.execute("entity.create", {});
	if (gui::widgets::menuItem({.glyph = gui::icon::plus, .label = "Create Child Entity"}))
		std::ignore = target.execute("entity.create", {{"name", std::string{"Child Entity"}}, {"parent", entityArg}});
	drawCreateLightMenu(target, std::optional<commands::ArgValue>{entityArg});

	ImGui::Separator();
	// --- Duplicate ---
	if (gui::widgets::menuItem({.glyph = gui::icon::copy, .label = "Duplicate Entity"}))
		std::ignore = target.execute("entity.duplicate", {{"entity", entityArg}});
	if (iHasChildren && gui::widgets::menuItem({.glyph = gui::icon::copy, .label = "Duplicate Subtree"}))
		std::ignore = target.execute("entity.duplicate", {{"entity", entityArg}, {"children", true}});

	// --- Prefab ---
	ImGui::Separator();
	if (isPrefabRoot(iEntity)) {
		const auto prefabFullPath = resolvePrefabPath(iEntity.getComponent<PrefabLink>());
		const bool prefabExists = !prefabFullPath.empty();
		if (gui::widgets::menuItem(
					{.glyph = gui::icon::refreshCw, .label = "Update from Prefab", .enabled = prefabExists})) {
			auto before = SubtreeSnapshot::capture(iEntity, *m_context);
			if (scene::PrefabSerializer::applyToInstance(prefabFullPath, iEntity, *m_context) &&
				mp_undoManager != nullptr)
				mp_undoManager->push(mkUniq<commands::ApplyPrefabCommand>(
						std::move(before), SubtreeSnapshot::capture(iEntity, *m_context), "Update from Prefab"));
		}
		if (gui::widgets::menuItem(
					{.glyph = gui::icon::rotateCcw, .label = "Revert to Prefab", .enabled = prefabExists})) {
			auto before = SubtreeSnapshot::capture(iEntity, *m_context);
			if (scene::PrefabSerializer::revertInstance(prefabFullPath, iEntity, *m_context) &&
				mp_undoManager != nullptr)
				mp_undoManager->push(mkUniq<commands::ApplyPrefabCommand>(
						std::move(before), SubtreeSnapshot::capture(iEntity, *m_context), "Revert to Prefab"));
		}

		ImGui::Separator();
		if (gui::widgets::menuItem({.glyph = gui::icon::unlink, .label = "Unlink Prefab"}))
			std::ignore = target.execute("component.remove",
										 {{"entity", entityArg}, {"component", std::string{PrefabLink::name()}}});

		ImGui::Separator();
	}
	if (gui::widgets::menuItem({.glyph = gui::icon::packagePlus, .label = "Create Prefab..."})) {
		if (const auto filepath = platform::FileDialog::saveFile("Owl Prefab (*.owlprefab)|owlprefab\n");
			!filepath.empty() && !scene::PrefabSerializer::serialize(iEntity, *m_context, filepath, iEntity.getName()))
			OWL_ERROR("SceneHierarchy: Prefab {} was not created.", filepath.string())
	}
	// --- HUD: a root canvas moves to an `.owlhud` the scene then draws.
	if (iEntity.hasComponent<scene::component::Canvas>() && iParentId == core::UUID{0}) {
		if (gui::widgets::menuItem({.glyph = gui::icon::packagePlus, .label = "Extract Canvas to .owlhud..."})) {
			if (const auto filepath = platform::FileDialog::saveFile("Owl HUD (*.owlhud)|owlhud\n");
				!filepath.empty()) {
				if (m_selection == iEntity)
					m_selection = {};
				if (const auto done = target.execute("hud.extract_canvas",
													 {{"entity", entityArg}, {"path", filepath.generic_string()}});
					!done)
					OWL_ERROR("SceneHierarchy: Canvas not extracted: {}.", done.error().message)
			}
		}
	}
	// --- Camera quick actions, run on the active viewport.
	if (iEntity.hasComponent<scene::component::Camera>() && mp_parentEditor != nullptr) {
		ImGui::Separator();
		if (gui::widgets::menuItem({.glyph = gui::icon::video, .label = "Camera Viewpoint"})) {
			mp_parentEditor->setSelectedEntity(iEntity);
			std::ignore = mp_parentEditor->runAction("view.cameraViewpoint");
		}
		if (gui::widgets::menuItem({.glyph = gui::icon::move3d, .label = "Pilot Camera"})) {
			mp_parentEditor->setSelectedEntity(iEntity);
			std::ignore = mp_parentEditor->runAction("view.pilotCamera");
		}
	}
	// --- Tilemap quick action: open the referenced asset in the tilemap editor.
	if (iEntity.hasComponent<scene::component::Tilemap>() && mp_parentEditor != nullptr) {
		const auto& tilemap = iEntity.getComponent<scene::component::Tilemap>();
		if (!tilemap.tilemapPath.empty()) {
			ImGui::Separator();
			if (gui::widgets::menuItem({.glyph = gui::icon::grid3x3, .label = "Open in Tilemap Editor"})) {
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
		if (gui::widgets::menuItem({.glyph = gui::icon::arrowUp, .label = "Unparent"}))
			std::ignore = target.execute("entity.reparent", {{"entity", entityArg}});
	}

	ImGui::Separator();
	// --- Delete ---
	const bool deleteOne = gui::widgets::menuItem({.glyph = gui::icon::trash2,
												   .label = iHasChildren ? "Delete Entity Only" : "Delete Entity",
												   .danger = true});
	const bool deleteAll =
			iHasChildren &&
			gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Delete with Children", .danger = true});
	if (deleteOne || deleteAll) {
		if (m_selection == iEntity)
			m_selection = {};
		std::ignore = target.execute("entity.delete", {{"entity", entityArg}, {"children", deleteAll}});
	}

	ImGui::EndPopup();
}

// Function displaying the Entity Property panel.
void SceneHierarchy::renderProperties() {
	const std::string title = panelWindow::title(
			gui::icon::slidersHorizontal,
			mp_activeDocument != nullptr ? mp_activeDocument->propertiesPanelTitle() : std::string{"Properties"},
			panelWindow::properties);
	ImGui::Begin(title.c_str());
	if (mp_activeDocument != nullptr && mp_activeDocument->overridesGlobalPanels()) {
		m_inspector.flush(getCommandTarget());
		mp_activeDocument->renderPropertiesPanel();
	} else {
		m_inspector.beginFrame(m_selection, getCommandTarget());
		if (m_selection) {
			gui::UiLayer::pushTextStyle(gui::TextStyle::Label);
			drawComponents(m_selection);
			gui::UiLayer::popTextStyle();
		} else if (m_context) {
			std::ignore = gui::widgets::emptyState(gui::icon::mousePointer2, "No entity selected",
												   "Click an entity in the viewport or in the Hierarchy to edit its "
												   "components here.");
		} else {
			std::ignore = gui::widgets::emptyState(gui::icon::slidersHorizontal, "Nothing to inspect",
												   "The active document has no properties here.");
		}
		m_inspector.endFrame(getCommandTarget());
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
template<>
constexpr bool isUiOnlyComponent<scene::component::UiWindow> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiLayout> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiTooltip> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiToolCursor> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiFocus> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiScrollView> = true;
template<>
constexpr bool isUiOnlyComponent<scene::component::UiList> = true;

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
	if (ioEntity.hasComponent<T>()) {
		ImGui::PushID(T::name());
		auto& component = ioEntity.getComponent<T>();
		const auto header = gui::widgets::sectionHeader(T::name(), componentIconName(T::name()));
		const bool open = header.open;
		const bool overridden = isComponentOverridden(ioEntity, T::key());
		if (overridden)
			drawOverrideMarker(header);

		// Track which component header is currently hovered so F1 can open the matching help page.
		if (header.hovered)
			g_lastHoveredComponentName = T::name();
		if (header.menuClicked)
			ImGui::OpenPopup("ComponentSettings");
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
			if constexpr (requires { gui::component::renderProps(component, ioEntity); })
				gui::component::renderProps(component, ioEntity);
			else
				gui::component::renderProps(component);
			ioInspector.endComponent(T::key(), T::name(), iCommands);
			gui::widgets::endSection();
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

void addGameComponentItems(scene::Entity& ioEntity, const commands::CommandTarget& iCommands) {
	for (const auto& desc: scene::ComponentRegistry::getAll()) {
		if (desc.builtin || !desc.optional || desc.has(ioEntity))
			continue;
		if (ImGui::MenuItem(desc.name.c_str())) {
			std::ignore =
					iCommands.execute("component.add", {{"entity", entityArgOf(ioEntity)}, {"component", desc.name}});
			ImGui::CloseCurrentPopup();
		}
	}
}

void drawGameComponents(scene::Entity& ioEntity, const commands::CommandTarget& iCommands,
						InspectorEditTracker& ioInspector) {
	for (const auto& desc: scene::ComponentRegistry::getAll()) {
		if (desc.builtin || !desc.has(ioEntity))
			continue;
		ImGui::PushID(desc.key.c_str());
		const auto header = gui::widgets::sectionHeader(desc.name.c_str(), nullptr, desc.optional);
		const bool open = header.open;
		if (header.menuClicked)
			ImGui::OpenPopup("ComponentSettings");
		if (ImGui::BeginPopup("ComponentSettings")) {
			if (desc.optional && ImGui::MenuItem("Remove component"))
				std::ignore = iCommands.execute("component.remove",
												{{"entity", entityArgOf(ioEntity)}, {"component", desc.name}});
			ImGui::EndPopup();
		}
		if (open) {
			ioInspector.beginComponent(ioEntity, desc.key);
			if (desc.inspect)
				std::ignore = desc.inspect(ioEntity);
			else
				ImGui::TextDisabled("No inspector registered for this component.");
			ioInspector.endComponent(desc.key, desc.name, iCommands);
			gui::widgets::endSection();
		}
		ImGui::PopID();
	}
}

}// namespace

void SceneHierarchy::drawComponents(const scene::Entity& iEntity) {
	gui::component::setInspectedScene(m_context.get());
	if (iEntity.hasComponent<Tag>()) {
		auto& tag = iEntity.getComponent<Tag>().tag;
		const auto frameTag = tag;
		const auto& theme = gui::UiLayer::getTheme();
		const bool hasChildren =
				iEntity.hasComponent<Hierarchy>() && !iEntity.getComponent<Hierarchy>().childrenIds.empty();
		const auto kind = entityKind(iEntity, hasChildren);
		const float box = std::round(ImGui::GetFrameHeight() * 1.2f);
		ImGui::Dummy({box, box});
		{
			const ImVec2 min = ImGui::GetItemRectMin();
			auto* draw = ImGui::GetWindowDrawList();
			draw->AddRectFilled(min, {min.x + box, min.y + box}, ImGui::GetColorU32(gui::vec(theme.surface3)),
								theme.frameRounding);
			const ImVec2 glyphSize = ImGui::CalcTextSize(kind.glyph);
			draw->AddText(
					{std::floor(min.x + (box - glyphSize.x) * 0.5f), std::floor(min.y + (box - glyphSize.y) * 0.5f)},
					ImGui::GetColorU32(gui::vec(kindColor(kind.color))), kind.glyph);
		}
		ImGui::SameLine();
		const float addWidth = ImGui::CalcTextSize("Add").x + ImGui::GetFontSize() * 2.6f;
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - addWidth - ImGui::GetStyle().ItemSpacing.x);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::floor((box - ImGui::GetFrameHeight()) * 0.5f));
		gui::UiLayer::pushTextStyle(gui::TextStyle::Strong);
		ImGui::InputTextWithHint("##Tag", "Entity name", &tag);
		gui::UiLayer::popTextStyle();
		if (ImGui::IsItemActivated())
			m_renameBefore = frameTag;
		if (ImGui::IsItemDeactivatedAfterEdit() && m_renameBefore.has_value() && tag != *m_renameBefore) {
			const auto currentTag = tag;
			tag = *m_renameBefore;
			if (const auto renamed = getCommandTarget().execute(
						"entity.rename", {{"entity", entityArgOf(iEntity)}, {"name", currentTag}});
				!renamed)
				tag = currentTag;
		}
		if (ImGui::IsItemDeactivated())
			m_renameBefore.reset();
	}
	ImGui::SameLine();
	if (gui::widgets::button("addComponent", {.glyph = gui::icon::plus,
											  .label = "Add",
											  .tooltip = "Add component",
											  .description = "Pick a component to add to this entity.",
											  .primary = true}))
		ImGui::OpenPopup("AddComponent");
	gui::widgets::badge(std::format("{} {}", gui::icon::hash, static_cast<uint64_t>(iEntity.getUUID())).c_str());
	if (isComponentOverridden(iEntity, Tag::key())) {
		ImGui::SameLine();
		if (gui::widgets::iconButton("revertName", {.glyph = gui::icon::rotateCcw,
													.tooltip = "Revert the name",
													.description = "The name overrides the prefab: take the prefab "
																   "name back."}))
			revertComponentToPrefab(iEntity, Tag::key(), "name", mp_undoManager);
	}

	if (ImGui::BeginPopup("AddComponent")) {
		const std::string layerTypeKey = layerTypeKeyForEntity(m_selection);
		addComponentsFromTuple(m_selection, getCommandTarget(), layerTypeKey, OptionalComponents{});
		addGameComponentItems(m_selection, getCommandTarget());
		ImGui::EndPopup();
	}
	drawComponentsFromTuple(m_selection, mp_undoManager, getCommandTarget(), m_inspector,
							gui::component::DrawableComponents{});
	drawGameComponents(m_selection, getCommandTarget(), m_inspector);
}

}// namespace owl::nest::panel
