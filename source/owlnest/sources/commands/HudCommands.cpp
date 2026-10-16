/**
 * @file HudCommands.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HudCommands.h"

#include "../UndoManager.h"
#include "CommandRegistry.h"
#include "EntityCommands.h"

#include <renderer/gpu/Texture.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>
#include <scene/ui/HudLayout.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <format>
#include <functional>
#include <ranges>
#include <utility>

namespace owl::nest::commands {

namespace {

using scene::component::UiRect;
using Prepared = expected<PreparedCommand, CommandFailure>;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto sameVec(const math::vec2& iA, const math::vec2& iB) -> bool { return iA == iB; }

auto sameRect(const UiRect& iA, const UiRect& iB) -> bool {
	return sameVec(iA.anchorMin, iB.anchorMin) && sameVec(iA.anchorMax, iB.anchorMax) && sameVec(iA.pivot, iB.pivot) &&
		   sameVec(iA.size, iB.size) && sameVec(iA.anchorOffset, iB.anchorOffset);
}

auto entityArg(const scene::Scene& iScene, const CommandArgs& iArgs, const std::string& iName)
		-> expected<scene::Entity, CommandFailure> {
	const auto found = iArgs.getValues().find(iName);
	if (found == iArgs.getValues().end())
		return fail(CommandError::MissingArgument, std::format("argument `{}` is missing", iName));
	if (auto entity = CommandRegistry::resolveEntity(iScene, found->second); entity)
		return *entity;
	return fail(CommandError::UnknownEntity, std::format("no entity matches `{}`", iName));
}

auto widgetArg(const scene::Scene& iScene, const CommandArgs& iArgs, const std::string& iName)
		-> expected<scene::Entity, CommandFailure> {
	auto entity = entityArg(iScene, iArgs, iName);
	if (entity && !entity->hasComponent<UiRect>())
		return fail(CommandError::InvalidArgument, std::format("`{}` has no UI Rect", entity->getName()));
	return entity;
}

auto parseEntityList(const scene::Scene& iScene, const std::string& iText)
		-> expected<std::vector<scene::Entity>, CommandFailure> {
	std::vector<scene::Entity> entities;
	for (const auto word: std::views::split(iText, ',')) {
		std::string token{std::string_view{word.begin(), word.end()}};
		std::erase_if(token, [](const char iChar) -> bool { return std::isspace(static_cast<unsigned char>(iChar)); });
		if (token.empty())
			continue;
		ArgValue value = token;
		if (uint64_t number = 0;
			std::from_chars(token.data(), token.data() + token.size(), number).ptr == token.data() + token.size())
			value = static_cast<int64_t>(number);
		const auto entity = CommandRegistry::resolveEntity(iScene, value);
		if (!entity)
			return fail(CommandError::UnknownEntity, std::format("no entity matches `{}`", token));
		if (std::ranges::find(entities, *entity) == entities.end())
			entities.push_back(*entity);
	}
	if (entities.empty())
		return fail(CommandError::MissingArgument, "`entities` names no entity");
	return entities;
}

auto isDescendantOf(const scene::Scene& iScene, const scene::Entity& iEntity, const scene::Entity& iAncestor) -> bool {
	auto parent = iEntity.getComponent<scene::component::Hierarchy>().parentId;
	while (parent != core::UUID{0}) {
		if (parent == iAncestor.getUUID())
			return true;
		const auto above = iScene.findEntityByUUID(parent);
		if (!above)
			return false;
		parent = above.getComponent<scene::component::Hierarchy>().parentId;
	}
	return false;
}

auto topLevelOnly(const scene::Scene& iScene, const std::vector<scene::Entity>& iEntities)
		-> std::vector<scene::Entity> {
	std::vector<scene::Entity> kept;
	for (const auto& entity: iEntities) {
		const bool covered = std::ranges::any_of(iEntities, [&](const scene::Entity& iOther) -> bool {
			return iOther != entity && isDescendantOf(iScene, entity, iOther);
		});
		if (!covered)
			kept.push_back(entity);
	}
	return kept;
}

auto findCanvasAbove(const scene::Scene& iScene, scene::Entity iEntity) -> std::optional<scene::Entity> {
	while (iEntity) {
		if (iEntity.hasComponent<scene::component::Canvas>())
			return iEntity;
		const auto parent = iEntity.getComponent<scene::component::Hierarchy>().parentId;
		if (parent == core::UUID{0})
			return std::nullopt;
		iEntity = iScene.findEntityByUUID(parent);
	}
	return std::nullopt;
}

auto toPixels(const math::vec2& iSize) -> math::vec2ui {
	return {static_cast<uint32_t>(std::max(1.f, iSize.x())), static_cast<uint32_t>(std::max(1.f, iSize.y()))};
}

auto resolutionOf(const scene::Scene& iScene, const scene::Entity& iEntity, const CommandArgs& iArgs)
		-> expected<math::vec2ui, CommandFailure> {
	if (const auto given = iArgs.get<math::vec2>("resolution"); given)
		return toPixels(*given);
	if (const auto reference = getHudReferenceResolution(iScene, iEntity); reference)
		return *reference;
	return fail(CommandError::NotApplicable, std::format("`{}` is under no canvas", iEntity.getName()));
}

struct LaidOut {
	UiRect::Bounds parent;
	UiRect::Bounds self;
};

auto layOut(scene::Scene& ioScene, const scene::Entity& iEntity, const math::vec2ui& iResolution)
		-> std::optional<LaidOut> {
	auto& layout = ioScene.getHudLayout();
	layout.compute(ioScene, iResolution, true);
	const auto* node = layout.findNode(static_cast<entt::entity>(iEntity));
	if (node == nullptr)
		return std::nullopt;
	const auto inReference = [&node](const UiRect::Bounds& iBounds) -> UiRect::Bounds {
		return {.position = iBounds.position / node->scale, .size = iBounds.size / node->scale};
	};
	LaidOut result{.parent = {}, .self = inReference(node->bounds)};
	if (node->parent == scene::ui::HudNode::noParent) {
		const math::vec2 screen{static_cast<float>(iResolution.x()), static_cast<float>(iResolution.y())};
		result.parent = {.position = {0.f, 0.f}, .size = screen / node->scale};
	} else {
		result.parent = inReference(layout.getNodes()[node->parent].content);
	}
	layout.invalidate();
	return result;
}

auto keepBounds(const shared<scene::Scene>& ioScene, const scene::Entity& iEntity, const UiRect& iNew,
				const CommandArgs& iArgs) -> expected<UiRect, CommandFailure> {
	const auto resolution = resolutionOf(*ioScene, iEntity, iArgs);
	if (!resolution)
		return unexpected<CommandFailure>{resolution.error()};
	const auto placed = layOut(*ioScene, iEntity, *resolution);
	if (!placed)
		return fail(CommandError::NotApplicable,
					std::format("`{}` is not laid out (hidden?), pass `keep_bounds` false", iEntity.getName()));
	return solveHudRect(iNew, placed->parent, placed->self);
}

auto describeRects(const std::vector<scene::Entity>& iEntities, const std::string_view iVerb) -> std::string {
	if (iEntities.size() == 1)
		return std::format("{} '{}'", iVerb, iEntities.front().getName());
	return std::format("{} {} widgets", iVerb, iEntities.size());
}

auto rectsCommand(const scene::Entity& iEntity, const UiRect& iAfter, std::string iDescription) -> Prepared {
	const HudRectChange change{.entity = iEntity.getUUID(), .before = iEntity.getComponent<UiRect>(), .after = iAfter};
	if (sameRect(change.before, change.after))
		return fail(CommandError::NotApplicable, std::format("`{}` already has this rectangle", iEntity.getName()));
	return PreparedCommand{.command = mkUniq<HudRectsCommand>(std::vector{change}, std::move(iDescription)),
						   .applied = false,
						   .entity = iEntity.getUUID()};
}

auto setRect(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = widgetArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	UiRect rect = entity->getComponent<UiRect>();
	bool given = false;
	const auto assign = [&](const char* iName, math::vec2& oField) -> void {
		if (const auto value = iArgs.get<math::vec2>(iName); value) {
			oField = *value;
			given = true;
		}
	};
	assign("anchor_min", rect.anchorMin);
	assign("anchor_max", rect.anchorMax);
	assign("pivot", rect.pivot);
	assign("size", rect.size);
	assign("offset", rect.anchorOffset);
	if (!given)
		return fail(CommandError::MissingArgument, "give `anchor_min`, `anchor_max`, `pivot`, `size` or `offset`");
	return rectsCommand(*entity, rect, std::format("Edit rect of '{}'", entity->getName()));
}

auto setAnchors(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = widgetArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto anchorMin = iArgs.get<math::vec2>("anchor_min");
	const auto anchorMax = iArgs.get<math::vec2>("anchor_max");
	if (!anchorMin && !anchorMax)
		return fail(CommandError::MissingArgument, "give `anchor_min` or `anchor_max`");
	UiRect rect = entity->getComponent<UiRect>();
	if (anchorMin) {
		rect.anchorMin = *anchorMin;
		if (!anchorMax)
			rect.anchorMax = *anchorMin;
	}
	if (anchorMax)
		rect.anchorMax = *anchorMax;
	if (iArgs.getBool("keep_bounds", true)) {
		const auto kept = keepBounds(ioScene, *entity, rect, iArgs);
		if (!kept)
			return unexpected<CommandFailure>{kept.error()};
		rect = *kept;
	}
	return rectsCommand(*entity, rect, std::format("Set anchors of '{}'", entity->getName()));
}

auto setPivot(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = widgetArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto pivot = iArgs.get<math::vec2>("pivot");
	if (!pivot)
		return fail(CommandError::MissingArgument, "argument `pivot` is missing");
	UiRect rect = entity->getComponent<UiRect>();
	rect.pivot = *pivot;
	if (iArgs.getBool("keep_bounds", true)) {
		const auto kept = keepBounds(ioScene, *entity, rect, iArgs);
		if (!kept)
			return unexpected<CommandFailure>{kept.error()};
		rect = *kept;
	}
	return rectsCommand(*entity, rect, std::format("Set pivot of '{}'", entity->getName()));
}

auto moveWidgets(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto listed = parseEntityList(*ioScene, iArgs.getText("entities"));
	if (!listed)
		return unexpected<CommandFailure>{listed.error()};
	const auto delta = iArgs.get<math::vec2>("delta");
	if (!delta)
		return fail(CommandError::MissingArgument, "argument `delta` is missing");
	const auto moved = topLevelOnly(*ioScene, *listed);
	std::vector<HudRectChange> changes;
	for (const auto& entity: moved) {
		if (!entity.hasComponent<UiRect>())
			return fail(CommandError::InvalidArgument, std::format("`{}` has no UI Rect", entity.getName()));
		HudRectChange change{.entity = entity.getUUID(),
							 .before = entity.getComponent<UiRect>(),
							 .after = entity.getComponent<UiRect>()};
		change.after.anchorOffset += *delta;
		changes.push_back(change);
	}
	if (*delta == math::vec2{0.f, 0.f})
		return fail(CommandError::NotApplicable, "the move is null");
	return PreparedCommand{.command = mkUniq<HudRectsCommand>(std::move(changes), describeRects(moved, "Move")),
						   .applied = false,
						   .entity = moved.front().getUUID(),
						   .data = {{"count", static_cast<int64_t>(moved.size())}}};
}

auto deleteWidgets(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto listed = parseEntityList(*ioScene, iArgs.getText("entities"));
	if (!listed)
		return unexpected<CommandFailure>{listed.error()};
	const auto removed = topLevelOnly(*ioScene, *listed);
	std::vector<uniq<SceneUndoCommand>> commands;
	commands.reserve(removed.size());
	for (const auto& entity: removed) commands.push_back(mkUniq<DeleteSubtreeCommand>(entity, *ioScene));
	ResultData data{{"count", static_cast<int64_t>(removed.size())}};
	if (commands.size() == 1)
		return PreparedCommand{.command = std::move(commands.front()), .applied = false, .data = std::move(data)};
	return PreparedCommand{
			.command = mkUniq<UndoGroupCommand<scene::Scene>>(std::move(commands), describeRects(removed, "Delete")),
			.applied = false,
			.data = std::move(data)};
}

auto reparentWidget(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto entity = widgetArg(*ioScene, iArgs, "entity");
	if (!entity)
		return unexpected<CommandFailure>{entity.error()};
	const auto parent = entityArg(*ioScene, iArgs, "parent");
	if (!parent)
		return unexpected<CommandFailure>{parent.error()};
	if (*parent == *entity || isDescendantOf(*ioScene, *parent, *entity))
		return fail(CommandError::NotApplicable, "a widget cannot go under itself or its own children");
	if (!parent->hasComponent<scene::component::Canvas>() && !parent->hasComponent<UiRect>())
		return fail(CommandError::InvalidArgument, std::format("`{}` is not a canvas nor a widget", parent->getName()));
	const auto resolution = resolutionOf(*ioScene, *entity, iArgs);
	if (!resolution)
		return unexpected<CommandFailure>{resolution.error()};

	const auto oldSlot = HierarchySlot::capture(*entity, *ioScene);
	const auto& siblings = parent->getComponent<scene::component::Hierarchy>().childrenIds;
	const bool sameParent = oldSlot.parentUuid == parent->getUUID();
	const size_t count = siblings.size() - (sameParent ? 1 : 0);
	const auto index = static_cast<size_t>(
			std::max<int64_t>(0, iArgs.get<int64_t>("index").value_or(static_cast<int64_t>(count))));
	const HierarchySlot newSlot{.parentUuid = parent->getUUID(), .siblingIndex = std::min(index, count)};
	const auto before = entity->getComponent<UiRect>();
	UiRect after = before;
	if (iArgs.getBool("keep_bounds", true)) {
		const auto placed = layOut(*ioScene, *entity, *resolution);
		if (!placed)
			return fail(CommandError::NotApplicable,
						std::format("`{}` is not laid out (hidden?), pass `keep_bounds` false", entity->getName()));
		newSlot.restore(*entity, *ioScene);
		const auto landed = layOut(*ioScene, *entity, *resolution);
		oldSlot.restore(*entity, *ioScene);
		if (!landed)
			return fail(CommandError::NotApplicable, "the new parent is not laid out (hidden?)");
		after = solveHudRect(before, landed->parent, placed->self);
	}
	if (oldSlot.parentUuid == newSlot.parentUuid && oldSlot.siblingIndex == newSlot.siblingIndex &&
		sameRect(before, after))
		return fail(CommandError::NotApplicable, "the widget is already there");
	return PreparedCommand{.command = mkUniq<HudReparentCommand>(
								   HudRectChange{.entity = entity->getUUID(), .before = before, .after = after},
								   oldSlot, newSlot, entity->getName()),
						   .applied = false,
						   .entity = entity->getUUID()};
}

struct KindDefaults {
	std::string_view name;
	math::vec2 size;
};

constexpr std::array<KindDefaults, 9> kindDefaults{{{"canvas", {0.f, 0.f}},
													{"panel", {240.f, 160.f}},
													{"label", {160.f, 32.f}},
													{"image", {96.f, 96.f}},
													{"button", {140.f, 40.f}},
													{"slider", {200.f, 20.f}},
													{"progress_bar", {200.f, 16.f}},
													{"window", {320.f, 220.f}},
													{"layout", {240.f, 160.f}}}};

void addKindComponents(scene::Entity& ioEntity, const std::string& iKind, const std::string& iTexture) {
	namespace comp = scene::component;
	if (iKind == "panel") {
		ioEntity.addComponent<comp::UiPanel>();
	} else if (iKind == "label") {
		ioEntity.addComponent<comp::UiText>().text = "Label";
	} else if (iKind == "image") {
		auto& image = ioEntity.addComponent<comp::UiImage>();
		if (!iTexture.empty())
			image.texture = renderer::gpu::Texture2D::createFromSerializedForDeserialize(iTexture);
	} else if (iKind == "button") {
		ioEntity.addComponent<comp::UiButton>();
		auto& text = ioEntity.addComponent<comp::UiText>();
		text.text = "Button";
		text.horizontalAlignment = data::fonts::HorizontalAlignment::Center;
	} else if (iKind == "slider") {
		ioEntity.addComponent<comp::UiSlider>();
	} else if (iKind == "progress_bar") {
		ioEntity.addComponent<comp::UiProgressBar>();
	} else if (iKind == "window") {
		ioEntity.addComponent<comp::UiWindow>();
	} else if (iKind == "layout") {
		auto& panel = ioEntity.addComponent<comp::UiPanel>();
		panel.backgroundColor = {0.f, 0.f, 0.f, 0.f};
		ioEntity.addComponent<comp::UiLayout>();
	}
}

auto firstCanvas(const scene::Scene& iScene) -> std::optional<scene::Entity> {
	std::optional<scene::Entity> first;
	int32_t order = 0;
	for (const auto view = iScene.registry.view<scene::component::Canvas>(); const auto handle: view) {
		if (const auto sortOrder = view.get<scene::component::Canvas>(handle).sortOrder; !first || sortOrder < order) {
			first = scene::Entity{handle, const_cast<scene::Scene*>(&iScene)};
			order = sortOrder;
		}
	}
	return first;
}

auto addWidget(const shared<scene::Scene>& ioScene, const CommandArgs& iArgs) -> Prepared {
	const auto kind = iArgs.getText("kind");
	const auto* const defaults = std::ranges::find(kindDefaults, kind, &KindDefaults::name);
	const auto name = iArgs.getText("name", kind);
	std::vector<uniq<SceneUndoCommand>> commands;
	if (kind == "canvas") {
		auto canvas = ioScene->createEntity(name);
		canvas.addComponent<scene::component::Canvas>();
		return PreparedCommand{.command = mkUniq<CreateEntityCommand>(canvas),
							   .applied = true,
							   .entity = canvas.getUUID()};
	}
	std::optional<scene::Entity> parent;
	if (iArgs.has("parent")) {
		const auto found = entityArg(*ioScene, iArgs, "parent");
		if (!found)
			return unexpected<CommandFailure>{found.error()};
		if (!findCanvasAbove(*ioScene, *found))
			return fail(CommandError::InvalidArgument, std::format("`{}` is under no canvas", found->getName()));
		parent = *found;
	} else if (const auto canvas = firstCanvas(*ioScene); canvas) {
		parent = *canvas;
	}
	if (!parent) {
		auto canvas = ioScene->createEntity("HUD Canvas");
		canvas.addComponent<scene::component::Canvas>();
		commands.push_back(mkUniq<CreateEntityCommand>(canvas));
		parent = canvas;
	}
	auto widget = ioScene->createEntity(name);
	ioScene->setParent(widget, *parent);
	auto& rect = widget.addComponent<UiRect>();
	rect.size = iArgs.get<math::vec2>("size").value_or(defaults->size);
	if (const auto anchor = iArgs.get<math::vec2>("anchor"); anchor) {
		rect.anchorMin = *anchor;
		rect.anchorMax = *anchor;
	}
	rect.anchorOffset = iArgs.get<math::vec2>("offset").value_or(math::vec2{0.f, 0.f});
	addKindComponents(widget, kind, iArgs.getText("texture"));
	commands.push_back(mkUniq<CreateEntityCommand>(widget));
	if (commands.size() == 1)
		return PreparedCommand{.command = std::move(commands.front()), .applied = true, .entity = widget.getUUID()};
	return PreparedCommand{
			.command = mkUniq<UndoGroupCommand<scene::Scene>>(std::move(commands), std::format("Add {}", kind)),
			.applied = true,
			.entity = widget.getUUID()};
}

auto entitiesSpec(std::string iDescription) -> ArgSpec {
	return {.name = "entities", .type = ArgType::Text, .required = true, .description = std::move(iDescription)};
}

auto entitySpec(std::string iDescription) -> ArgSpec {
	return {.name = "entity", .type = ArgType::Entity, .required = true, .description = std::move(iDescription)};
}

auto keepBoundsSpec() -> ArgSpec {
	return {.name = "keep_bounds",
			.type = ArgType::Bool,
			.description = "Change the offset and the size so the widget stays where it is on screen.",
			.defaultValue = true};
}

auto resolutionSpec() -> ArgSpec {
	return {.name = "resolution",
			.type = ArgType::Vec2,
			.description = "Screen size in pixels the position is measured at (the reference resolution of the "
						   "canvas when absent)."};
}

}// namespace

HudRectsCommand::HudRectsCommand(std::vector<HudRectChange> iChanges, std::string iDescription)
	: m_changes{std::move(iChanges)}, m_description{std::move(iDescription)} {
	if (!m_changes.empty()) {
		m_selectAfterUndo = m_changes.front().entity;
		m_selectAfterRedo = m_changes.front().entity;
	}
}

HudRectsCommand::~HudRectsCommand() = default;

void HudRectsCommand::undo(scene::Scene& ioScene) {
	for (const auto& change: m_changes) {
		if (const auto entity = ioScene.findEntityByUUID(change.entity); entity && entity.hasComponent<UiRect>())
			entity.getComponent<UiRect>() = change.before;
	}
}

void HudRectsCommand::redo(scene::Scene& ioScene) {
	for (const auto& change: m_changes) {
		if (const auto entity = ioScene.findEntityByUUID(change.entity); entity && entity.hasComponent<UiRect>())
			entity.getComponent<UiRect>() = change.after;
	}
}

auto HudRectsCommand::mergeWith(const SceneUndoCommand& iOther) -> bool {
	const auto* other = dynamic_cast<const HudRectsCommand*>(&iOther);
	if (other == nullptr || other->m_changes.size() != m_changes.size())
		return false;
	for (size_t i = 0; i < m_changes.size(); ++i) {
		if (other->m_changes[i].entity != m_changes[i].entity)
			return false;
	}
	for (size_t i = 0; i < m_changes.size(); ++i) m_changes[i].after = other->m_changes[i].after;
	m_description = other->m_description;
	m_timestamp = other->m_timestamp;
	return true;
}

auto HudRectsCommand::typeId() const -> size_t {
	size_t hash = 0x48754452ULL;// "HuDR"
	for (const auto& change: m_changes)
		hash = (hash * 1099511628211ULL) ^ std::hash<uint64_t>{}(static_cast<uint64_t>(change.entity));
	return hash;
}

HudReparentCommand::HudReparentCommand(const HudRectChange& iChange, const HierarchySlot& iOldSlot,
									   const HierarchySlot& iNewSlot, std::string iName)
	: m_change{iChange}, m_oldSlot{iOldSlot}, m_newSlot{iNewSlot}, m_name{std::move(iName)} {
	m_selectAfterUndo = iChange.entity;
	m_selectAfterRedo = iChange.entity;
}

HudReparentCommand::~HudReparentCommand() = default;

void HudReparentCommand::undo(scene::Scene& ioScene) {
	const auto entity = ioScene.findEntityByUUID(m_change.entity);
	if (!entity)
		return;
	m_oldSlot.restore(entity, ioScene);
	entity.getComponent<UiRect>() = m_change.before;
}

void HudReparentCommand::redo(scene::Scene& ioScene) {
	const auto entity = ioScene.findEntityByUUID(m_change.entity);
	if (!entity)
		return;
	m_newSlot.restore(entity, ioScene);
	entity.getComponent<UiRect>() = m_change.after;
}

auto HudReparentCommand::description() const -> std::string { return std::format("Reparent '{}'", m_name); }

auto getHudWidgetKinds() -> const std::vector<std::string>& {
	static const std::vector<std::string> kinds = [] -> std::vector<std::string> {
		std::vector<std::string> names;
		names.reserve(kindDefaults.size());
		for (const auto& defaults: kindDefaults) names.emplace_back(defaults.name);
		return names;
	}();
	return kinds;
}

auto solveHudRect(const UiRect& iBase, const UiRect::Bounds& iParent, const UiRect::Bounds& iTarget) -> UiRect {
	const auto scaled = [](const math::vec2& iA, const math::vec2& iB) -> math::vec2 {
		return {iA.x() * iB.x(), iA.y() * iB.y()};
	};
	UiRect rect = iBase;
	const math::vec2 cornerMin = iParent.position + scaled(iParent.size, iBase.anchorMin);
	const math::vec2 cornerMax = iParent.position + scaled(iParent.size, iBase.anchorMax);
	const math::vec2 span = cornerMax - cornerMin;
	rect.size = iTarget.size - span;
	const math::vec2 pivotPoint = iTarget.position + scaled(iTarget.size, iBase.pivot);
	rect.anchorOffset = pivotPoint - (cornerMin + scaled(span, iBase.pivot));
	return rect;
}

auto getHudParentBounds(scene::Scene& ioScene, const scene::Entity& iEntity, const math::vec2ui& iResolution)
		-> std::optional<UiRect::Bounds> {
	if (const auto placed = layOut(ioScene, iEntity, iResolution); placed)
		return placed->parent;
	return std::nullopt;
}

auto getHudReferenceResolution(const scene::Scene& iScene, const scene::Entity& iEntity)
		-> std::optional<math::vec2ui> {
	if (const auto canvas = findCanvasAbove(iScene, iEntity); canvas)
		return toPixels(canvas->getComponent<scene::component::Canvas>().referenceResolution);
	return std::nullopt;
}

void addHudCommands(CommandRegistry& ioRegistry) {
	ioRegistry.add({.name = "hud.add_widget",
					.description = "Add a HUD widget (or a canvas) under a canvas or a widget; the first canvas is "
								   "used, and created, when `parent` is absent.",
					.args = {{.name = "kind",
							  .type = ArgType::Enum,
							  .required = true,
							  .description = "Kind of widget.",
							  .choices = getHudWidgetKinds()},
							 {.name = "name", .type = ArgType::Text, .description = "Tag (the kind when absent)."},
							 {.name = "parent",
							  .type = ArgType::Entity,
							  .description = "Canvas or widget it goes under (the first canvas when absent)."},
							 {.name = "size",
							  .type = ArgType::Vec2,
							  .description = "Size in reference pixels (a default per kind when absent)."},
							 {.name = "anchor",
							  .type = ArgType::Vec2,
							  .description = "Anchor point in the parent, (0, 0) top-left to (1, 1) bottom-right "
											 "(the centre when absent)."},
							 {.name = "offset",
							  .type = ArgType::Vec2,
							  .description = "Offset of the pivot from the anchor, in reference pixels."},
							 {.name = "texture",
							  .type = ArgType::Text,
							  .description = "Texture of an `image`, relative to an asset folder.",
							  .projectPath = true}},
					.factory = addWidget});
	ioRegistry.add({.name = "hud.set_rect",
					.description = "Set the anchors, pivot, size or offset of a widget, as given; nothing else moves.",
					.args = {entitySpec("Widget changed."),
							 {.name = "anchor_min", .type = ArgType::Vec2, .description = "Top-left anchor."},
							 {.name = "anchor_max", .type = ArgType::Vec2, .description = "Bottom-right anchor."},
							 {.name = "pivot", .type = ArgType::Vec2, .description = "Pivot in the widget."},
							 {.name = "size", .type = ArgType::Vec2, .description = "Size in reference pixels."},
							 {.name = "offset", .type = ArgType::Vec2, .description = "Offset of the pivot."}},
					.factory = setRect});
	ioRegistry.add({.name = "hud.set_anchors",
					.description = "Move the anchors of a widget; by default the widget stays where it is on screen.",
					.args = {entitySpec("Widget changed."),
							 {.name = "anchor_min",
							  .type = ArgType::Vec2,
							  .description = "Top-left anchor (both anchors when `anchor_max` is absent)."},
							 {.name = "anchor_max", .type = ArgType::Vec2, .description = "Bottom-right anchor."},
							 keepBoundsSpec(),
							 resolutionSpec()},
					.factory = setAnchors});
	ioRegistry.add({.name = "hud.set_pivot",
					.description = "Move the pivot of a widget; by default the widget stays where it is on screen.",
					.args = {entitySpec("Widget changed."),
							 {.name = "pivot",
							  .type = ArgType::Vec2,
							  .required = true,
							  .description = "Pivot in the widget, (0, 0) top-left to (1, 1) bottom-right."},
							 keepBoundsSpec(),
							 resolutionSpec()},
					.factory = setPivot});
	ioRegistry.add({.name = "hud.move",
					.description = "Move widgets by a distance in reference pixels, as one step; a widget under "
								   "another moved one follows it.",
					.args = {entitiesSpec("UUIDs or tags of the widgets, separated by commas."),
							 {.name = "delta",
							  .type = ArgType::Vec2,
							  .required = true,
							  .description = "Distance in reference pixels (positive Y goes down)."}},
					.factory = moveWidgets});
	ioRegistry.add({.name = "hud.delete",
					.description = "Delete widgets and their children, as one step.",
					.args = {entitiesSpec("UUIDs or tags of the widgets, separated by commas.")},
					.factory = deleteWidgets});
	ioRegistry.add({.name = "hud.reparent",
					.description = "Put a widget under a canvas or another widget, at an index among its children "
								   "(the last one, drawn on top, when absent).",
					.args = {entitySpec("Widget moved."),
							 {.name = "parent",
							  .type = ArgType::Entity,
							  .required = true,
							  .description = "New parent: a canvas or a widget."},
							 {.name = "index", .type = ArgType::Integer, .description = "Index among the children."},
							 keepBoundsSpec(),
							 resolutionSpec()},
					.factory = reparentWidget});
}

}// namespace owl::nest::commands
