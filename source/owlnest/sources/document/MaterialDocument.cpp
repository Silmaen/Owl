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
#include <renderer/Renderer3D.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
OWL_DIAG_POP

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <tuple>
#include <utility>

namespace owl::nest {

namespace {
constexpr float g_orbitSpeed = 0.01f;
constexpr float g_maxPitch = 1.55f;
constexpr float g_zoomStep = 0.9f;
constexpr uint32_t g_fitFrames = 3;
constexpr uint32_t g_thumbnailSize = 96;
constexpr auto g_previewDebounce = std::chrono::milliseconds{250};
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

auto makePreviewFramebuffer(const math::vec2ui& iSize, const char* iName) -> shared<renderer::gpu::Framebuffer> {
	using Format = renderer::gpu::AttachmentSpecification::Format;
	using Tiling = renderer::gpu::AttachmentSpecification::Tiling;
	return renderer::gpu::Framebuffer::create(
			{.size = iSize,
			 .attachments = {{.format = Format::Surface, .tiling = Tiling::Optimal},
							 {.format = Format::RedInteger, .tiling = Tiling::Optimal},
							 {.format = Format::Depth32F, .tiling = Tiling::Optimal}},
			 .samples = 1,
			 .swapChainTarget = false,
			 .debugName = iName});
}
}// namespace

MaterialDocument::MaterialDocument() {
	m_undo.markSaved();
	bindCanvas();
}

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
	m_thumbnails.clear();
}

void MaterialDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto MaterialDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto loaded = renderer::MaterialAsset::load(iPath);
	if (!loaded) {
		OWL_CORE_WARN("MaterialDocument: '{}' is not a material.", iPath.string())
		return false;
	}
	m_asset = std::move(*loaded);
	m_path = iPath;
	m_undo.clear();
	m_undo.markSaved();
	m_scope.clear();
	m_fitFrames = g_fitFrames;
	onAssetChanged();
	return true;
}

auto MaterialDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto MaterialDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (!m_asset.save(iPath)) {
		OWL_CORE_ERROR("MaterialDocument: Failed to write '{}'.", iPath.string())
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
	onAssetChanged();
	return true;
}

void MaterialDocument::performUndo() {
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_asset);
	onAssetChanged();
}

void MaterialDocument::performRedo() {
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_asset);
	onAssetChanged();
}

auto MaterialDocument::commit(renderer::MaterialAsset iNext, const std::string& iDescription, const bool iMerge)
		-> bool {
	if (iNext.toYaml() == m_asset.toYaml())
		return false;
	auto before = std::move(m_asset);
	m_asset = iNext;
	m_undo.push(mkUniq<ModifyMaterialCommand>(std::move(before), std::move(iNext), iDescription, iMerge));
	onAssetChanged();
	return true;
}

auto MaterialDocument::setMaterial(const renderer::MaterialAsset& iAsset, const std::string& iDescription,
								   const bool iMerge) -> bool {
	return commit(iAsset, iDescription, iMerge);
}

void MaterialDocument::onAssetChanged() {
	m_previewDirty = true;
	m_canvasStale = true;
	m_editTime = std::chrono::steady_clock::now();
	if (isGraph()) {
		m_sharedGraph = mkShared<const renderer::MaterialGraph>(*m_asset.graph);
		m_program = renderer::compileMaterialGraph(*m_sharedGraph);
	} else {
		m_sharedGraph.reset();
		m_program = {};
	}
	if (!m_scope.empty() && (!isGraph() || m_asset.graph->findFunction(m_scope) == nullptr))
		m_scope.clear();
	if (m_selectedNode) {
		const auto* nodes = isGraph() ? commands::getMaterialScopeNodes(*m_asset.graph, m_scope) : nullptr;
		if (nodes == nullptr || !std::ranges::contains(*nodes, *m_selectedNode, &renderer::MaterialGraphNode::id))
			m_selectedNode.reset();
	}
}

auto MaterialDocument::setParent(const std::filesystem::path& iParent) -> bool {
	if (iParent == m_asset.parent)
		return false;
	if (!iParent.empty() && iParent.extension() != ".owlmat") {
		OWL_CORE_WARN("MaterialDocument: '{}' is not a material.", iParent.string())
		return false;
	}
	if (!iParent.empty() && !m_path.empty() && iParent.filename() == m_path.filename()) {
		OWL_CORE_WARN("MaterialDocument: A material cannot be its own parent.")
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
			if (const auto parent = library->get(m_asset.parent)) {
				auto material = parent->withOverrides({.fields = m_asset.overrides, .values = m_asset.values});
				if (!parent->getGraph())
					return material;
				auto values = parent->getGraphValues();
				for (const auto& [name, value]: m_asset.graphValues) values[name] = value;
				return material.withGraph(parent->getGraph(), parent->getProgram(), values);
			}
		}
		return renderer::Material{m_asset.values};
	}
	if (m_sharedGraph && m_program.isValid())
		return renderer::Material{m_asset.values}.withGraph(
				m_sharedGraph, mkShared<const renderer::MaterialProgram>(m_program), m_asset.graphValues);
	return renderer::Material{m_asset.values};
}

void MaterialDocument::setPreviewShape(const viewer::PreviewShape iShape) {
	m_shape = iShape;
	m_orbit = {};
}

void MaterialDocument::setThumbnails(const bool iShow) {
	if (m_showThumbnails == iShow)
		return;
	m_showThumbnails = iShow;
	if (!iShow)
		m_thumbnails.clear();
	m_canvasStale = true;
}

void MaterialDocument::onUpdate(const core::Timestep& iTimeStep) {
	if (m_canvasSize.x() == 0 || m_canvasSize.y() == 0)
		return;
	m_time += iTimeStep.getSeconds();
	renderer::Renderer3D::setTime(m_time);
	if (const auto* library = libraryOf(); library != nullptr && library->getGeneration() != m_libraryGeneration) {
		m_libraryGeneration = library->getGeneration();
		m_previewDirty = true;
	}
	auto& mesh = m_shapeMeshes[m_shape == viewer::PreviewShape::Sphere ? 0 : 1];
	if (!mesh)
		mesh = viewer::makePreviewShape(m_shape);
	if (!mesh)
		return;
	if (!m_framebuffer)
		m_framebuffer = makePreviewFramebuffer(m_canvasSize, "material_viewer");
	else if (m_framebuffer->getSpecification().size != m_canvasSize)
		m_framebuffer->resize(m_canvasSize);
	// A graph edit builds a shader: the preview waits for the edits to settle.
	const bool settled = !isGraph() || std::chrono::steady_clock::now() - m_editTime >= g_previewDebounce;
	if ((m_previewDirty && settled) || !m_previewMaterial) {
		m_previewMaterial = buildMaterial();
		m_previewDirty = false;
	}
	viewer::renderMaterialPreview(*m_framebuffer, *mesh, m_orbit, *m_previewMaterial);
	updateThumbnails();
}

void MaterialDocument::updateThumbnails() {
	if (!m_showThumbnails || !isGraph() || !m_scope.empty() || m_visibleNodes.empty()) {
		m_visibleNodes.clear();
		return;
	}
	auto& sphere = m_shapeMeshes[0];
	if (!sphere)
		sphere = viewer::makePreviewShape(viewer::PreviewShape::Sphere);
	if (!sphere)
		return;
	bool built = false;
	for (const auto node: m_visibleNodes) {
		auto& thumbnail = m_thumbnails[node];
		if (thumbnail.revision == m_editTime)
			continue;
		auto program = renderer::compileMaterialGraph(*m_sharedGraph,
													  renderer::MaterialPreviewTarget{.node = node, .pin = {}});
		const bool newShader = program.isValid() && program.key != thumbnail.key;
		// One new shader per frame keeps the canvas responsive while the thumbnails fill in.
		if (newShader && built)
			continue;
		thumbnail.revision = m_editTime;
		thumbnail.valid = program.isValid();
		if (!thumbnail.valid)
			continue;
		thumbnail.key = program.key;
		if (!thumbnail.framebuffer)
			thumbnail.framebuffer = makePreviewFramebuffer({g_thumbnailSize, g_thumbnailSize}, "material_node_preview");
		if (!thumbnail.framebuffer)
			continue;
		renderer::MaterialParameters unlit = m_asset.values;
		unlit.shadingModel = renderer::ShadingModel::Unlit;
		unlit.alphaMode = renderer::AlphaMode::Opaque;
		const auto material = renderer::Material{unlit}.withGraph(
				m_sharedGraph, mkShared<const renderer::MaterialProgram>(std::move(program)), m_asset.graphValues);
		viewer::renderMaterialPreview(*thumbnail.framebuffer, *sphere, {}, material);
		built = built || newShader;
	}
	std::erase_if(m_thumbnails, [this](const auto& iEntry) -> bool {
		return !std::ranges::contains(m_asset.graph->nodes, iEntry.first, &renderer::MaterialGraphNode::id);
	});
	m_visibleNodes.clear();
}

void MaterialDocument::drawThumbnail(const core::UUID iNode, const math::vec2f& iMin, const math::vec2f& iMax) {
	m_visibleNodes.insert(iNode);
	auto* draw = ImGui::GetWindowDrawList();
	const float side = std::min(iMax.x() - iMin.x(), iMax.y() - iMin.y());
	const ImVec2 min{(iMin.x() + iMax.x() - side) * 0.5f, iMin.y()};
	const ImVec2 max{min.x + side, min.y + side};
	const auto found = m_thumbnails.find(iNode);
	if (found == m_thumbnails.end() || !found->second.valid || !found->second.framebuffer) {
		draw->AddRect(min, max, IM_COL32(110, 110, 120, 160), 4.f);
		if (found != m_thumbnails.end() && found->second.revision != std::chrono::steady_clock::time_point{} &&
			!found->second.valid && side > 32.f) {
			const char* text = "no preview";
			const auto size = ImGui::CalcTextSize(text);
			draw->AddText({(min.x + max.x - size.x) * 0.5f, (min.y + max.y - size.y) * 0.5f},
						  IM_COL32(150, 150, 160, 255), text);
		}
		return;
	}
	const auto texture = gui::imTexture(found->second.framebuffer, 0);
	if (!texture.has_value())
		return;
	draw->AddImage(*texture, min, max, gui::vec(found->second.framebuffer->getLowerData()),
				   gui::vec(found->second.framebuffer->getUpperData()));
}

auto MaterialDocument::runCommand(const std::string_view iName, commands::CommandArgs iArgs)
		-> std::optional<commands::CommandResult> {
	if (mp_editorLayer == nullptr) {
		OWL_WARN("MaterialDocument: '{}' ignored, no editor to run it.", iName)
		return std::nullopt;
	}
	iArgs.set("path", m_path.generic_string());
	auto result = mp_editorLayer->execute(iName, iArgs, commands::CommandSource::Gesture);
	if (!result)
		return std::nullopt;
	return std::move(*result);
}

auto MaterialDocument::isDragging() -> bool {
	const int frame = ImGui::GetFrameCount();
	const bool dragging = ImGui::IsMouseDown(ImGuiMouseButton_Left) && frame - m_lastEditFrame <= 1;
	m_lastEditFrame = frame;
	return dragging;
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
		renderToolbar();
		if (isGraph()) {
			renderGraphView(windowFocused);
		} else {
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
			std::ignore = runCommand(
					"material.set_parent",
					{{"parent", std::string(data, ::strnlen(data, static_cast<size_t>(payload->DataSize)))}});
		}
		ImGui::EndDragDropTarget();
	}
	ImGui::SameLine();
	if (gui::widgets::iconButton("##clearParent", {.glyph = gui::icon::x,
												   .tooltip = "Clear",
												   .description = "Make this a root material again.",
												   .disabled = parent.empty()}))
		std::ignore = runCommand("material.set_parent", {{"parent", std::string{}}});
	if (!m_asset.parent.empty())
		ImGui::TextDisabled("Check a field to override it; the others follow the parent.");

	renderGraphOverrides();
	auto next = m_asset;
	std::optional<renderer::MaterialParameters> inherited;
	if (auto* library = libraryOf(); library != nullptr && !m_asset.parent.empty()) {
		if (const auto material = library->get(m_asset.parent))
			inherited = material->getParameters();
	}
	if (gui::component::renderMaterialFields(next.values, next.overrides, !m_asset.parent.empty(),
											 inherited ? &*inherited : nullptr)) {
		const renderer::MaterialOverrides values{.fields = m_asset.parent.empty() ? renderer::g_allMaterialFields
																				  : next.overrides,
												 .values = next.values};
		std::ignore = runCommand("material.set_values", {{"values", values.toYaml()}, {"merge", isDragging()}});
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
