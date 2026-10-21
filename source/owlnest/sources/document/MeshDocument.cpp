/**
 * @file MeshDocument.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "MeshDocument.h"

#include "EditorLayer.h"

#include <data/geometry/MeshLoader.h>
#include <gui/IconFont.h>
#include <gui/utils.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <tuple>

namespace owl::nest {

namespace {
constexpr float g_orbitSpeed = 0.01f;
constexpr float g_maxPitch = 1.55f;
constexpr float g_zoomStep = 0.9f;
}// namespace

MeshDocument::MeshDocument() = default;

MeshDocument::~MeshDocument() = default;

auto MeshDocument::title() const -> std::string {
	return m_path.empty() ? std::string{"Untitled"} : m_path.filename().string();
}

void MeshDocument::onAttach(EditorLayer* iEditor) { mp_editorLayer = iEditor; }

void MeshDocument::onDetach() {
	mp_editorLayer = nullptr;
	m_framebuffer.reset();
	m_mesh.reset();
}

auto MeshDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	const auto loaded = data::geometry::MeshLoader::loadMesh(iPath);
	if (!loaded.mesh) {
		OWL_WARN("MeshDocument: {} holds no mesh.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_mesh = mkShared<renderer::MeshResource>(iPath, loaded);
	return true;
}

void MeshDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {
	if (!m_mesh || m_canvasSize.x() == 0 || m_canvasSize.y() == 0)
		return;
	if (!m_framebuffer) {
		using Format = renderer::gpu::AttachmentSpecification::Format;
		using Tiling = renderer::gpu::AttachmentSpecification::Tiling;
		m_framebuffer = renderer::gpu::Framebuffer::create(
				{.size = m_canvasSize,
				 .attachments = {{.format = Format::Surface, .tiling = Tiling::Optimal},
								 {.format = Format::RedInteger, .tiling = Tiling::Optimal},
								 {.format = Format::Depth24Stencil8, .tiling = Tiling::Optimal}},
				 .samples = 1,
				 .swapChainTarget = false,
				 .debugName = "mesh_viewer"});
	} else if (m_framebuffer->getSpecification().size != m_canvasSize) {
		m_framebuffer->resize(m_canvasSize);
	}
	viewer::renderMeshPreview(*m_framebuffer, *m_mesh, m_orbit, m_shading);
}

void MeshDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

void MeshDocument::onImGuiRender() {
	const auto display =
			(mp_editorLayer != nullptr) ? mp_editorLayer->getDocumentManager().displayTitleFor(this) : title();
	const auto winTitle = documentWindowTitle(DocumentType::Mesh, display, static_cast<uint64_t>(id()));
	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);
	const bool wantFocus = consumeFocusRequest();
	if (wantFocus)
		ImGui::SetNextWindowFocus();
	const bool open = ImGui::Begin(winTitle.c_str(), &m_pOpen,
								   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
										   ImGuiWindowFlags_NoScrollWithMouse);
	if (mp_editorLayer != nullptr)
		mp_editorLayer->renderDocumentTabMenu(*this);
	if (wantFocus) {
		ImGui::SetWindowFocus();
		if (mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
	}
	if (open) {
		const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (windowFocused && !m_wasFocused && mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
		m_wasFocused = windowFocused;
		const auto avail = ImGui::GetContentRegionAvail();
		m_canvasSize = {static_cast<uint32_t>(std::max(avail.x, 1.f)), static_cast<uint32_t>(std::max(avail.y, 1.f))};
		if (!m_mesh) {
			std::ignore = gui::widgets::emptyState(gui::icon::box, "Mesh not loaded", "The file holds no mesh.");
		} else if (const auto texture = gui::imTexture(m_framebuffer, 0); !texture.has_value()) {
			ImGui::Dummy(avail);
		} else {
			ImGui::Image(texture.value(), avail, gui::vec(m_framebuffer->getLowerData()),
						 gui::vec(m_framebuffer->getUpperData()));
			const auto& io = ImGui::GetIO();
			if (ImGui::IsItemHovered()) {
				if (io.MouseWheel != 0.f)
					m_orbit.zoom = std::clamp(m_orbit.zoom * std::pow(g_zoomStep, io.MouseWheel), 0.2f, 8.f);
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					resetView();
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
					m_orbit.yaw -= io.MouseDelta.x * g_orbitSpeed;
					m_orbit.pitch = std::clamp(m_orbit.pitch - io.MouseDelta.y * g_orbitSpeed, -g_maxPitch, g_maxPitch);
				}
			}
		}
	}
	ImGui::End();
}

void MeshDocument::renderHierarchyPanel() {
	if (!gui::widgets::sectionHeader("Mesh", gui::icon::box, false).open)
		return;
	if (m_mesh && gui::widgets::beginProperties("##meshInfo")) {
		const auto& geometry = m_mesh->getGeometry();
		const math::vec3 size = geometry.boundsMax - geometry.boundsMin;
		gui::widgets::propertyLabel("File");
		ImGui::TextUnformatted(m_path.filename().string().c_str());
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::propertyLabel("Vertices", "Distinct corners (position, normal, UV) sent to the GPU.");
		ImGui::Text("%zu", geometry.vertices.size());
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::propertyLabel("Triangles");
		ImGui::Text("%zu", geometry.indices.size() / 3);
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::propertyLabel("Size", "Extent of the bounding box in object space.");
		ImGui::Text("%.3g x %.3g x %.3g", static_cast<double>(size.x()), static_cast<double>(size.y()),
					static_cast<double>(size.z()));
		std::ignore = gui::widgets::propertyReset(false);
		const auto& factor = m_mesh->getBaseColorFactor();
		gui::widgets::propertyLabel("Base colour", "Base-colour factor of the material.");
		ImGui::ColorButton("##baseColor", gui::vec(factor), ImGuiColorEditFlags_AlphaPreview);
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::propertyLabel("Texture", "Base-colour texture of the material.");
		if (const auto texture = gui::imTexture(m_mesh->getBaseColorTexture()); texture.has_value()) {
			const float side = ImGui::GetFontSize() * 4.f;
			ImGui::Image(texture.value(), {side, side}, {0.f, 1.f}, {1.f, 0.f});
		} else {
			ImGui::TextDisabled("none");
		}
		std::ignore = gui::widgets::propertyReset(false);
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

void MeshDocument::renderPropertiesPanel() {
	if (!gui::widgets::sectionHeader("View", gui::icon::scan, false).open)
		return;
	if (gui::widgets::beginProperties("##meshView")) {
		bool textured = m_shading == viewer::MeshShading::Textured;
		if (gui::widgets::propertyBool("Textured", textured, true,
									   "Draw the material; off, a plain grey shows the shape alone."))
			m_shading = textured ? viewer::MeshShading::Textured : viewer::MeshShading::Shaded;
		gui::widgets::endProperties();
	}
	ImGui::TextDisabled("Drag: orbit. Wheel: zoom. Double-click: reset.");
	if (gui::widgets::iconButton("##placeMesh", {.glyph = gui::icon::plus,
												 .tooltip = "Place in Scene",
												 .description = "Add the mesh to the active scene, at the origin.",
												 .disabled = !m_mesh || mp_editorLayer == nullptr}))
		placeInScene();
	gui::widgets::endSection();
}

void MeshDocument::placeInScene() const {
	if (mp_editorLayer != nullptr && m_mesh)
		mp_editorLayer->placeMesh(m_path);
}

}// namespace owl::nest
