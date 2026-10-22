/**
 * @file MaterialDocument.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "MaterialDocument.h"

#include "EditorLayer.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <gui/IconFont.h>
#include <gui/component/render.h>
#include <gui/utils.h>
#include <gui/widgets/IconButton.h>
#include <renderer/MaterialLibrary.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
OWL_DIAG_POP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <tuple>
#include <utility>

namespace owl::nest {

namespace {
constexpr float g_orbitSpeed = 0.01f;
constexpr float g_maxPitch = 1.55f;
constexpr float g_zoomStep = 0.9f;
constexpr const char* g_editDescription = "Edit material";

class ModifyMaterialCommand final : public UndoCommand<renderer::MaterialAsset> {
public:
	ModifyMaterialCommand(renderer::MaterialAsset iBefore, renderer::MaterialAsset iAfter, std::string iDescription,
						  const bool iMerge)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)},
		  m_merge{iMerge} {}

	void undo(renderer::MaterialAsset& ioTarget) override { ioTarget = m_before; }

	void redo(renderer::MaterialAsset& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return m_merge ? 1 : 0; }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto& other = static_cast<const ModifyMaterialCommand&>(iOther);
		if (other.m_description != m_description)
			return false;
		m_after = other.m_after;
		return true;
	}

private:
	renderer::MaterialAsset m_before;
	renderer::MaterialAsset m_after;
	std::string m_description;
	bool m_merge;
};

auto libraryOf() -> renderer::MaterialLibrary* {
	return app::Application::instanced() ? &app::Application::get().getEngineContext().getMaterialLibrary() : nullptr;
}
}// namespace

MaterialDocument::MaterialDocument() { m_undo.markSaved(); }

MaterialDocument::~MaterialDocument() = default;

auto MaterialDocument::title() const -> std::string {
	return m_path.empty() ? std::string{"Untitled"} : m_path.filename().string();
}

void MaterialDocument::onAttach(EditorLayer* iEditor) { mp_editorLayer = iEditor; }

void MaterialDocument::onDetach() {
	mp_editorLayer = nullptr;
	m_framebuffer.reset();
	m_shapeMeshes[0].reset();
	m_shapeMeshes[1].reset();
	m_previewMaterial.reset();
}

void MaterialDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto MaterialDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto loaded = renderer::MaterialAsset::load(iPath);
	if (!loaded) {
		OWL_CORE_WARN("MaterialDocument: {} is not a material.", iPath.string())
		return false;
	}
	m_asset = std::move(*loaded);
	m_path = iPath;
	m_undo.clear();
	m_undo.markSaved();
	m_previewDirty = true;
	return true;
}

auto MaterialDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto MaterialDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (!m_asset.save(iPath)) {
		OWL_CORE_ERROR("MaterialDocument: failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_undo.markSaved();
	if (auto* library = libraryOf(); library != nullptr)
		std::ignore = library->invalidate(std::filesystem::absolute(iPath));
	m_previewDirty = true;
	return true;
}

auto MaterialDocument::recoverySnapshot() const -> std::optional<std::string> { return m_asset.toYaml(); }

auto MaterialDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	auto asset = renderer::MaterialAsset::fromYaml(iSnapshot);
	if (!asset) {
		OWL_WARN("Recovery: Cannot restore the autosave of material '{}'.", title())
		return false;
	}
	m_asset = std::move(*asset);
	m_undo.clear();
	m_undo.markUnsaved();
	m_previewDirty = true;
	return true;
}

void MaterialDocument::performUndo() {
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_asset);
	m_previewDirty = true;
}

void MaterialDocument::performRedo() {
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_asset);
	m_previewDirty = true;
}

auto MaterialDocument::commit(renderer::MaterialAsset iNext, const std::string& iDescription, const bool iMerge)
		-> bool {
	if (iNext.toYaml() == m_asset.toYaml())
		return false;
	auto before = std::move(m_asset);
	m_asset = iNext;
	m_undo.push(mkUniq<ModifyMaterialCommand>(std::move(before), std::move(iNext), iDescription, iMerge));
	m_previewDirty = true;
	return true;
}

auto MaterialDocument::setParent(const std::filesystem::path& iParent) -> bool {
	if (iParent == m_asset.parent)
		return false;
	if (!iParent.empty() && iParent.extension() != ".owlmat") {
		OWL_CORE_WARN("MaterialDocument: {} is not a material.", iParent.string())
		return false;
	}
	if (!iParent.empty() && !m_path.empty() && iParent.filename() == m_path.filename()) {
		OWL_CORE_WARN("MaterialDocument: a material cannot be its own parent.")
		return false;
	}
	auto next = m_asset;
	next.parent = iParent;
	if (iParent.empty())
		next.overrides = renderer::g_allMaterialFields;
	return commit(std::move(next), iParent.empty() ? "Clear material parent" : "Set material parent", false);
}

auto MaterialDocument::setValues(const renderer::MaterialParameters& iValues, const uint32_t iOverrides,
								 const bool iMerge) -> bool {
	auto next = m_asset;
	next.values = iValues;
	next.overrides = iOverrides;
	return commit(std::move(next), g_editDescription, iMerge);
}

auto MaterialDocument::buildMaterial() const -> renderer::Material {
	if (!m_asset.parent.empty()) {
		if (auto* library = libraryOf(); library != nullptr) {
			if (const auto parent = library->get(m_asset.parent))
				return parent->withOverrides({.fields = m_asset.overrides, .values = m_asset.values});
		}
	}
	return renderer::Material{m_asset.values};
}

void MaterialDocument::setPreviewShape(const viewer::PreviewShape iShape) {
	m_shape = iShape;
	m_orbit = {};
}

void MaterialDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {
	if (m_canvasSize.x() == 0 || m_canvasSize.y() == 0)
		return;
	if (const auto* library = libraryOf(); library != nullptr && library->getGeneration() != m_libraryGeneration) {
		m_libraryGeneration = library->getGeneration();
		m_previewDirty = true;
	}
	auto& mesh = m_shapeMeshes[m_shape == viewer::PreviewShape::Sphere ? 0 : 1];
	if (!mesh)
		mesh = viewer::makePreviewShape(m_shape);
	if (!mesh)
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
				 .debugName = "material_viewer"});
	} else if (m_framebuffer->getSpecification().size != m_canvasSize) {
		m_framebuffer->resize(m_canvasSize);
	}
	if (m_previewDirty || !m_previewMaterial) {
		m_previewMaterial = buildMaterial();
		m_previewDirty = false;
	}
	viewer::renderMaterialPreview(*m_framebuffer, *mesh, m_orbit, *m_previewMaterial);
}

void MaterialDocument::onImGuiRender() {
	const auto display =
			(mp_editorLayer != nullptr) ? mp_editorLayer->getDocumentManager().displayTitleFor(this) : title();
	const auto winTitle = documentWindowTitle(DocumentType::Material, display, static_cast<uint64_t>(id()));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;
	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);
	const bool wantFocus = consumeFocusRequest();
	if (wantFocus)
		ImGui::SetNextWindowFocus();
	const bool open = ImGui::Begin(winTitle.c_str(), &m_pOpen, flags);
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
		if (windowFocused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
			std::ignore = save();
		const float listWidth = std::max(320.f, ImGui::GetContentRegionAvail().x * 0.42f);
		ImGui::BeginChild("##materialParameters", ImVec2{listWidth, 0.f}, ImGuiChildFlags_Borders);
		renderParameters();
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginChild("##materialPreview", ImVec2{0.f, 0.f}, ImGuiChildFlags_Borders,
						  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		renderPreview();
		ImGui::EndChild();
	}
	ImGui::End();
}

void MaterialDocument::renderParameters() {
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Parent");
	ImGui::SameLine();
	const float side = ImGui::GetFrameHeight();
	std::string parent = m_asset.parent.generic_string();
	ImGui::SetNextItemWidth(std::max(side, ImGui::GetContentRegionAvail().x - side - ImGui::GetStyle().ItemSpacing.x));
	ImGui::InputTextWithHint("##parent", "none: a root material", &parent, ImGuiInputTextFlags_ReadOnly);
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const auto* const data = static_cast<const char*>(payload->Data);
			std::ignore = setParent(
					std::filesystem::path{std::string(data, ::strnlen(data, static_cast<size_t>(payload->DataSize)))});
		}
		ImGui::EndDragDropTarget();
	}
	ImGui::SameLine();
	if (gui::widgets::iconButton("##clearParent", {.glyph = gui::icon::x,
												   .tooltip = "Clear",
												   .description = "Make this a root material again.",
												   .disabled = parent.empty()}))
		std::ignore = setParent({});
	if (!m_asset.parent.empty())
		ImGui::TextDisabled("Check a field to override it; the others follow the parent.");

	auto next = m_asset;
	std::optional<renderer::MaterialParameters> inherited;
	if (auto* library = libraryOf(); library != nullptr && !m_asset.parent.empty()) {
		if (const auto material = library->get(m_asset.parent))
			inherited = material->getParameters();
	}
	if (gui::component::renderMaterialFields(next.values, next.overrides, !m_asset.parent.empty(),
											 inherited ? &*inherited : nullptr)) {
		const int frame = ImGui::GetFrameCount();
		const bool dragging = ImGui::IsMouseDown(ImGuiMouseButton_Left) && frame - m_lastEditFrame <= 1;
		m_lastEditFrame = frame;
		std::ignore = setValues(next.values, next.overrides, dragging);
	}
}

void MaterialDocument::renderPreview() {
	int shape = m_shape == viewer::PreviewShape::Sphere ? 0 : 1;
	bool changed = ImGui::RadioButton("Sphere", &shape, 0);
	ImGui::SameLine();
	changed |= ImGui::RadioButton("Cube", &shape, 1);
	if (changed)
		setPreviewShape(shape == 0 ? viewer::PreviewShape::Sphere : viewer::PreviewShape::Cube);
	ImGui::SameLine();
	ImGui::TextDisabled("Drag: orbit. Wheel: zoom. Double-click: reset.");
	const auto avail = ImGui::GetContentRegionAvail();
	m_canvasSize = {static_cast<uint32_t>(std::max(avail.x, 1.f)), static_cast<uint32_t>(std::max(avail.y, 1.f))};
	const auto texture = gui::imTexture(m_framebuffer, 0);
	if (!texture.has_value()) {
		ImGui::Dummy(avail);
		return;
	}
	ImGui::Image(texture.value(), avail, gui::vec(m_framebuffer->getLowerData()),
				 gui::vec(m_framebuffer->getUpperData()));
	if (!ImGui::IsItemHovered())
		return;
	const auto& io = ImGui::GetIO();
	if (io.MouseWheel != 0.f)
		m_orbit.zoom = std::clamp(m_orbit.zoom * std::pow(g_zoomStep, io.MouseWheel), 0.2f, 8.f);
	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		m_orbit = {};
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
		m_orbit.yaw -= io.MouseDelta.x * g_orbitSpeed;
		m_orbit.pitch = std::clamp(m_orbit.pitch - io.MouseDelta.y * g_orbitSpeed, -g_maxPitch, g_maxPitch);
	}
}

}// namespace owl::nest
