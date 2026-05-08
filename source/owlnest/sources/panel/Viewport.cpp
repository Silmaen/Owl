/**
 * @file Viewport.cpp
 * @author Silmaen
 * @date 10/16/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Viewport.h"

#include "EditorLayer.h"
#include "document/DocumentManager.h"
#include "document/SceneDocument.h"

#include "../UndoManager.h"
#include "../commands/ComponentCommands.h"

#include <owl.h>
#include <scene/SceneSerializer.h>
#include <scene/TilemapAsset.h>
#include <scene/component/Tilemap.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

namespace owl::nest::panel {

namespace {
/**
 * @brief
 *  Build the ImGui window title: display text + stable `##{uuid}` id.
 *
 * The display text is delegated to the document manager so identical-named tabs (e.g. a
 * `world_map.owl` scene next to a `world_map.owltilemap`) automatically get a type prefix.
 */
auto makeWindowTitle(const SceneDocument& iDoc, const EditorLayer* iParent) -> std::string {
	std::string base;
	if (iParent != nullptr)
		base = iParent->getDocumentManager().displayTitleFor(&iDoc);
	if (base.empty())
		base = iDoc.title();
	return std::format("{}##scene_{:x}", base.empty() ? std::string{"Untitled"} : base,
					   static_cast<uint64_t>(iDoc.id()));
}

/**
 * @brief
 *  Snapshot of the relevant fields of the first `Tilemap` component in a scene.
 *
 * Used to drive both the snap step (cellSize × multiplier) and the cell-center
 * realignment applied at the end of a translation drag.
 */
struct TilemapSnapInfo {
	math::vec3 worldTranslation{0.f, 0.f, 0.f};///< World-space translation of the tilemap entity.
	float cellSize = 1.f;///< Cell size in world units.
	uint32_t width = 1;///< Grid width in cells.
	uint32_t height = 1;///< Grid height in cells.
};

/**
 * @brief
 *  Find the first `Tilemap` component in the scene and capture its grid info.
 * @param[in] iScene Scene to scan (may be null).
 * @return A populated `TilemapSnapInfo` if a tilemap is found, `std::nullopt` otherwise.
 */
auto findFirstTilemapInfo(const scene::Scene* iScene) -> std::optional<TilemapSnapInfo> {
	if (iScene == nullptr)
		return std::nullopt;
	const auto view = iScene->registry.view<scene::component::Tilemap>();
	for (const auto entity: view) {
		const auto& tilemap = view.get<scene::component::Tilemap>(entity);
		if (!tilemap.asset)
			continue;
		const scene::Entity sceneEntity{entity, const_cast<scene::Scene*>(iScene)};
		TilemapSnapInfo info;
		info.worldTranslation = iScene->getWorldTransform(sceneEntity).translation();
		info.cellSize = std::max(0.0001f, tilemap.asset->cellSize);
		info.width = std::max(1u, tilemap.asset->width);
		info.height = std::max(1u, tilemap.asset->height);
		return info;
	}
	return std::nullopt;
}

/**
 * @brief
 *  Resolve the snap step to use for a translation gizmo.
 * @param[in] iSettings Editor settings carrying snap preferences.
 * @param[in] iTilemap Optional tilemap info (when auto-from-tilemap resolves).
 * @return World-unit step (clamped to a positive value).
 */
auto resolveTranslationSnapStep(const EditorSettings& iSettings, const std::optional<TilemapSnapInfo>& iTilemap)
		-> float {
	if (iSettings.snapAutoFromTilemap && iTilemap.has_value())
		return std::max(0.0001f, iTilemap->cellSize * iSettings.snapMultiplier);
	return std::max(0.0001f, iSettings.snapStep);
}

/**
 * @brief
 *  Snap a world-space translation to the nearest tilemap cell-center grid.
 *
 * The cell-center grid lies at `tilemap.worldTranslation + (k - (width - 1) / 2) * cellSize`
 * along each axis. `k` is integer when the size is odd (centers fall on integer
 * multiples of `cellSize`) and half-integer when even (centers fall on the
 * half-cell offset). This function preserves the input's z component.
 * @param[in] iWorldTranslation The current world translation to snap.
 * @param[in] iTilemap The tilemap info providing the grid.
 * @param[in] iStep The snap step (must already include the snap multiplier).
 * @return The translation snapped to the nearest cell-center-aligned grid point.
 */
auto snapToTilemapCellCenter(const math::vec3& iWorldTranslation, const TilemapSnapInfo& iTilemap, const float iStep)
		-> math::vec3 {
	const float offsetX = (iTilemap.width % 2 == 0) ? iTilemap.cellSize * 0.5f : 0.f;
	const float offsetY = (iTilemap.height % 2 == 0) ? iTilemap.cellSize * 0.5f : 0.f;
	const float relX = iWorldTranslation.x() - iTilemap.worldTranslation.x() - offsetX;
	const float relY = iWorldTranslation.y() - iTilemap.worldTranslation.y() - offsetY;
	const float kX = std::round(relX / iStep);
	const float kY = std::round(relY / iStep);
	return math::vec3{
			iTilemap.worldTranslation.x() + offsetX + kX * iStep,
			iTilemap.worldTranslation.y() + offsetY + kY * iStep,
			iWorldTranslation.z(),
	};
}

}// namespace

Viewport::Viewport() : BasePanel{"SceneView"} {}

Viewport::~Viewport() = default;

void Viewport::setDocument(SceneDocument* iDocument) {
	mp_document = iDocument;
	if (iDocument != nullptr) {
		// Stable ImGui id (after ##) so ImGui preserves dock state across renames.
		m_name = makeWindowTitle(*iDocument, m_parent);
	} else {
		m_name = "SceneView";
	}
}

void Viewport::onRender() {
	if (mp_document == nullptr) {
		BasePanel::onRender();
		return;
	}

	// Refresh display title each frame so filename / play badge changes show up on the tab;
	// the stable `##<uuid>` suffix keeps the ImGui window id unchanged.
	m_name = makeWindowTitle(*mp_document, m_parent);

	ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;
	if (mp_document->isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;

	// First-time docking hint: dock newly-opened viewports into the central node of the main
	// dockspace instead of letting them float.
	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);

	// The tab shows the native ImGui close X.  When the user clicks it `m_pOpen` is set
	// to false; `EditorLayer` polls this after rendering and routes through the dirty
	// confirmation modal before actually closing the document.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	const bool visible = ImGui::Begin(m_name.c_str(), &m_pOpen, flags);
	m_focused = ImGui::IsWindowFocused();
	m_hovered = ImGui::IsWindowHovered();
	core::Application::get().getImGuiLayer()->blockEvents(!m_focused && !m_hovered);

	// Focus transition → this viewport's document becomes the active document.
	if (m_focused && !m_wasFocused && m_parent != nullptr)
		m_parent->getDocumentManager().setActive(mp_document);
	m_wasFocused = m_focused;

	if (visible) {
		const auto cursor = ImGui::GetCursorScreenPos();
		const auto avail = ImGui::GetContentRegionAvail();
		const float safeW = std::max(0.f, avail.x);
		const float safeH = std::max(0.f, avail.y);
		m_lower = {cursor.x, cursor.y};
		m_upper = {cursor.x + safeW, cursor.y + safeH};
		m_size = {static_cast<uint32_t>(safeW), static_cast<uint32_t>(safeH)};
		onRenderInternal();
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void Viewport::attach() {
	const renderer::gpu::FramebufferSpecification specs{
			.size = {1280, 720},
			.attachments =
					{
							{.format = renderer::gpu::AttachmentSpecification::Format::Surface,
							 .tiling = renderer::gpu::AttachmentSpecification::Tiling::Optimal},
							{.format = renderer::gpu::AttachmentSpecification::Format::RedInteger,
							 .tiling = renderer::gpu::AttachmentSpecification::Tiling::Optimal},
					},
			.samples = 1,
			.swapChainTarget = false,
			.debugName = "editor"};
	m_framebuffer = renderer::gpu::Framebuffer::create(specs);
	m_editorCamera = renderer::CameraEditor(30.0f, 1.778f, 0.1f, 1000.0f);
}

void Viewport::detach() {
	OWL_TRACE("Viewport: framebuffer freed.")
	m_framebuffer.reset();
}

void Viewport::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	if (mp_document == nullptr)
		return;
	const auto& activeScene = mp_document->getActiveScene();
	if (!activeScene)
		return;

	// resize frame buffer if needed.
	if (const auto spec = m_framebuffer->getSpecification(); getSize().surface() > 0 && getSize() != spec.size) {
		m_framebuffer->resize(getSize());
		m_editorCamera.setViewportSize(getSize());
	}
	activeScene->onViewportResize(getSize());

	if (mp_document->state() == SceneDocument::State::Edit) {
		if (isHovered())
			m_editorCamera.onUpdate(iTimeStep);
	}

	// Render
	m_framebuffer->bind();
	renderer::gpu::RenderCommand::setClearColor({0.1f, 0.1f, 0.1f, 1});
	renderer::gpu::RenderCommand::clear();

	// Get Mouse position.
	auto [mx, my] = ImGui::GetMousePos();
	mx -= m_lower.x();
	my -= m_lower.y();
	const math::vec2 viewportSizeInternal = m_upper - m_lower;
	if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::OpenGL)
		my = viewportSizeInternal.y() - my;
	const int mouseX = static_cast<int>(mx);
	const int mouseY = static_cast<int>(my);

	// Clear our entity ID attachment to -1
	m_framebuffer->clearAttachment(1, -1);

	switch (mp_document->state()) {
		case SceneDocument::State::Edit:
			{
				activeScene->onUpdateEditor(iTimeStep, m_editorCamera);
				break;
			}
		case SceneDocument::State::Play:
			{
				// UiRect uses Y=0 at bottom; ImGui mouse Y=0 at top → always flip.
				const math::vec2 vpMouse = {mx, viewportSizeInternal.y() - my};
				const bool mousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
				scene::UiInputSystem::update(activeScene.get(), m_framebuffer->getSpecification().size, vpMouse,
											 mousePressed);
				activeScene->onUpdateRuntime(iTimeStep);
				// Handle quit request from Lua (scene.quit()) → request stop.
				if (activeScene->quitRequested)
					mp_document->requestStop();
				// `scene.transition_to(...)` from Lua schedules a load through the
				// engine-side `ScreenTransition` orchestrator; once the out-anim
				// completes, the path lands here as a teleport request, the
				// existing handler does the swap, and the Loading-phase screen
				// stays up until `minHoldDuration` elapses.
				if (auto pending = scene::ScreenTransition::pendingLoadPath(); pending) {
					activeScene->teleportRequest.pending = true;
					activeScene->teleportRequest.levelName = *pending;
					activeScene->teleportRequest.targetName.clear();
					activeScene->teleportRequest.initialVelocity = {0.f, 0.f};
					activeScene->teleportRequest.rotationDelta = 0.f;
				}
				mp_document->handleTeleportRequest(getSize());
				mp_document->handleSaveLoadRequest(getSize());
				break;
			}
		case SceneDocument::State::Pause:
			{
				if (mp_document->consumeStepRequest()) {
					activeScene->onUpdateRuntime(iTimeStep);
					mp_document->handleTeleportRequest(getSize());
				} else {
					activeScene->onRenderRuntime();
				}
				break;
			}
	}

	if (mouseX >= 0 && mouseY >= 0 && mouseX < static_cast<int>(viewportSizeInternal.x()) &&
		mouseY < static_cast<int>(viewportSizeInternal.y())) {
		const int pixelData = m_framebuffer->readPixel(1, mouseX, mouseY);
		m_hoveredEntity = pixelData == -1 ? scene::Entity()
										  : scene::Entity(static_cast<entt::entity>(pixelData), activeScene.get());
	}
	renderOverlay();

	m_framebuffer->unbind();
}

void Viewport::onRenderInternal() {
	OWL_PROFILE_FUNCTION()

	if (const auto tex = gui::imTexture(m_framebuffer, 0); tex.has_value())
		ImGui::Image(tex.value(), gui::vec(getSize()), gui::vec(m_framebuffer->getLowerData()),
					 gui::vec(m_framebuffer->getUpperData()));
	if (m_parent != nullptr) {
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				const auto* path = static_cast<const char*>(payload->Data);
				m_parent->handleContentBrowserDrop(std::filesystem::path{path});
			}
			ImGui::EndDragDropTarget();
		}
	}
	renderGizmo();
}

void Viewport::renderOverlay() const {
	OWL_PROFILE_FUNCTION()

	if (mp_document == nullptr)
		return;
	const auto& activeScene = mp_document->getActiveScene();
	if (!activeScene)
		return;

	if (mp_document->state() != SceneDocument::State::Edit) {
		const scene::Entity camera = activeScene->getPrimaryCamera();
		auto& cam = camera.getComponent<scene::component::Camera>().camera;
		cam.setTransform(activeScene->getWorldTransform(camera)());
		renderer::Renderer2D::beginScene(cam);
	} else {
		renderer::Renderer2D::beginScene(m_editorCamera);
	}

	if (mp_document->state() == SceneDocument::State::Edit) {
		// Draw selected entity outline
		if (const scene::Entity selectedEntity =
					m_parent != nullptr ? m_parent->getSelectedEntity() : scene::Entity{}) {
			const math::Transform worldTransform = activeScene->getWorldTransform(selectedEntity);
			renderer::Renderer2D::drawRect({.transform = worldTransform, .color = math::vec4(0.95f, 0.55f, 0.f, 1)});
			renderer::Renderer2D::drawQuad({.transform = worldTransform, .color = math::vec4{0.95f, 0.55f, 0.f, 0.2f}});
		}

		// Draw trigger type icons
		auto& textureLibrary = renderer::Renderer::getTextureLibrary();
		for (const auto view = activeScene->registry.view<scene::component::Trigger, scene::component::Transform>();
			 const auto entity: view) {
			auto [trigger, tc] = view.get<scene::component::Trigger, scene::component::Transform>(entity);
			std::string iconName;
			switch (trigger.trigger.type) {
				case scene::SceneTrigger::TriggerType::Victory:
					iconName = "icons/triggers/victory";
					break;
				case scene::SceneTrigger::TriggerType::Death:
					iconName = "icons/triggers/death";
					break;
				case scene::SceneTrigger::TriggerType::Target:
					iconName = "icons/triggers/target";
					break;
				case scene::SceneTrigger::TriggerType::Teleport:
					iconName = "icons/triggers/teleport";
					break;
				case scene::SceneTrigger::TriggerType::Timer:
					iconName = "icons/triggers/timer";
					break;
				case scene::SceneTrigger::TriggerType::Interaction:
					iconName = "icons/triggers/interaction";
					break;
				case scene::SceneTrigger::TriggerType::LuaCallback:
					iconName = "icons/triggers/lua_callback";
					break;
			}
			if (!iconName.empty()) {
				if (const auto icon = textureLibrary.get(iconName); icon != nullptr) {
					const scene::Entity ent{entity, activeScene.get()};
					math::Transform iconTransform = activeScene->getWorldTransform(ent);
					iconTransform.translation().z() += 0.01f;
					iconTransform.scale() = {0.5f, 0.5f, 1.f};
					renderer::Renderer2D::drawQuad({.transform = iconTransform,
													.color = {1.f, 1.f, 1.f, 0.7f},
													.texture = icon,
													.entityId = static_cast<int>(entity)});
				}
			}
		}
	}

	renderer::Renderer2D::endScene();
}

void Viewport::setGuizmoType(const gui::Guizmo::Type& iType) { m_gizmoType = iType; }

auto Viewport::getGuizmoType() const -> gui::Guizmo::Type { return m_gizmoType; }

auto Viewport::getGuizmoTypeI() const -> uint16_t { return static_cast<uint16_t>(m_gizmoType); }

void Viewport::renderGizmo() {
	// Gizmos
	if (mp_document == nullptr || m_parent == nullptr)
		return;
	if (mp_document->state() != SceneDocument::State::Edit)
		return;
	const auto& activeScene = mp_document->getActiveScene();
	if (!activeScene)
		return;

	if (const scene::Entity selectedEntity = m_parent->getSelectedEntity();
		selectedEntity && m_gizmoType != gui::Guizmo::Type::None) {
		gui::Guizmo::initialize(m_lower, m_upper - m_lower);

		math::mat4 cameraProjection = m_editorCamera.getProjection();
		if (renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan)
			cameraProjection(1, 1) *= -1.f;
		const math::mat4 cameraView = m_editorCamera.getView();

		auto& tc = selectedEntity.getComponent<scene::component::Transform>();
		math::mat4 transform = activeScene->getWorldTransform(selectedEntity)();

		const bool ctrlHeld = input::Input::isKeyPressed(input::key::LeftControl);
		const bool snapEnabledInSettings = m_parent != nullptr && m_parent->getSettings().snapEnabled;
		const bool snap = ctrlHeld || snapEnabledInSettings;
		const auto tilemapInfo = findFirstTilemapInfo(activeScene.get());
		float snapValue = 0.5f;
		if (gui::Guizmo::isRotate(m_gizmoType)) {
			snapValue = 45.0f;
		} else if (m_parent != nullptr) {
			snapValue = resolveTranslationSnapStep(m_parent->getSettings(), tilemapInfo);
		}

		gui::Guizmo::manipulate(cameraView, cameraProjection, m_gizmoType, transform, snap ? snapValue : 0.f);

		const bool isUsing = gui::Guizmo::isUsing();

		if (isUsing && !m_gizmoWasUsing && mp_undoManager != nullptr)
			m_gizmoBeforeYaml = scene::SceneSerializer::serializeEntityToString(selectedEntity);

		if (isUsing) {
			const auto& hierarchy = selectedEntity.getComponent<scene::component::Hierarchy>();
			math::mat4 localMat = transform;
			if (hierarchy.parentId != core::UUID{0}) {
				if (const auto parent = activeScene->findEntityByUUID(hierarchy.parentId); parent) {
					const math::mat4 parentWorldInv = math::inverse(activeScene->getWorldTransform(parent)());
					localMat = parentWorldInv * transform;
				}
			}
			const math::Transform newLocal{localMat};
			const math::vec3 deltaRotation = newLocal.rotation() - tc.transform.rotation();
			tc.transform.translation() = newLocal.translation();
			tc.transform.rotation() += deltaRotation;
			tc.transform.scale() = newLocal.scale();
		}

		// Drag-end correction: when snapping a translation against a tilemap, finalise the
		// world position to the nearest cell-center grid point so entities visually align
		// with the cells (ImGuizmo only snaps the cumulative drag delta, not the absolute
		// world position).
		if (!isUsing && m_gizmoWasUsing && snap && (m_gizmoType == gui::Guizmo::Type::Translation) &&
			tilemapInfo.has_value() && m_parent != nullptr && m_parent->getSettings().snapAutoFromTilemap) {
			const math::Transform worldT = activeScene->getWorldTransform(selectedEntity);
			const float effectiveStep =
					std::max(0.0001f, tilemapInfo->cellSize * m_parent->getSettings().snapMultiplier);
			const math::vec3 snappedWorld = snapToTilemapCellCenter(worldT.translation(), *tilemapInfo, effectiveStep);
			if (snappedWorld != worldT.translation()) {
				const math::Transform targetWorld{snappedWorld, worldT.rotation(), worldT.scale()};
				math::mat4 newLocalMat = targetWorld();
				const auto& hierarchy = selectedEntity.getComponent<scene::component::Hierarchy>();
				if (hierarchy.parentId != core::UUID{0}) {
					if (const auto parent = activeScene->findEntityByUUID(hierarchy.parentId); parent) {
						const math::mat4 parentWorldInv = math::inverse(activeScene->getWorldTransform(parent)());
						newLocalMat = parentWorldInv * targetWorld();
					}
				}
				const math::Transform newLocal{newLocalMat};
				tc.transform.translation() = newLocal.translation();
			}
		}

		if (!isUsing && m_gizmoWasUsing && mp_undoManager != nullptr && !m_gizmoBeforeYaml.empty()) {
			const auto afterYaml = scene::SceneSerializer::serializeEntityToString(selectedEntity);
			if (m_gizmoBeforeYaml != afterYaml) {
				auto cmd = mkUniq<commands::ModifyEntityCommand>(
						selectedEntity.getUUID(), EntitySnapshot{selectedEntity.getUUID(), m_gizmoBeforeYaml},
						"Transform (Gizmo)");
				cmd->captureAfter(selectedEntity);
				mp_undoManager->push(std::move(cmd));
			}
			m_gizmoBeforeYaml.clear();
		}
		m_gizmoWasUsing = isUsing;
	}
}

void Viewport::onEvent(event::Event& ioEvent) {
	if (mp_document != nullptr && mp_document->state() == SceneDocument::State::Edit && isHovered())
		m_editorCamera.onEvent(ioEvent);

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[this]<typename T0>(T0&& ioPh1) -> auto { return onMouseButtonPressed(std::forward<T0>(ioPh1)); });
}

auto Viewport::onMouseButtonPressed(const event::MouseButtonPressedEvent& ioEvent) -> bool {
	if (mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return false;
	if (ioEvent.getMouseButton() == input::mouse::ButtonLeft) {
		if (isHovered() && !gui::Guizmo::isOver() && !input::Input::isKeyPressed(input::key::LeftAlt))
			if (m_parent != nullptr)
				m_parent->setSelectedEntity(getHoveredEntity());
	}
	return false;
}

}// namespace owl::nest::panel
