/**
 * @file MeshDocument.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "MeshDocument.h"

#include "EditorLayer.h"

#include <data/assets/AssetMeta.h>
#include <data/geometry/MeshLoader.h>
#include <gui/IconFont.h>
#include <gui/utils.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Sequencer.h>
#include <renderer/gpu/RenderCommand.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

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
	data::geometry::LodSettings lods;
	if (const auto meta = data::assets::readMeta(data::assets::metaPathOf(iPath));
		meta && meta->importer == data::assets::Importer::Mesh)
		lods = {.levelCount = meta->mesh.lods,
				.reduction = meta->mesh.lodReduction,
				.screenSize = meta->mesh.lodScreenSize};
	m_mesh = mkShared<renderer::MeshResource>(iPath, loaded, lods);
	m_clip.reset();
	m_clipTime = 0.f;
	m_lod = 0;
	m_selectedBone.reset();
	updatePose(0.f);
	return true;
}

void MeshDocument::setClip(const std::optional<size_t> iClip) {
	m_clip = m_mesh && iClip && *iClip < m_mesh->getClips().size() ? iClip : std::nullopt;
	m_clipTime = 0.f;
	updatePose(0.f);
}

void MeshDocument::setClipTime(const float iSeconds) {
	m_clipTime = iSeconds;
	updatePose(0.f);
}

void MeshDocument::setLod(const size_t iLod) { m_lod = m_mesh ? std::min(iLod, m_mesh->getLodCount()) : 0; }

void MeshDocument::updatePose(const float iSeconds) {
	if (!m_mesh || !m_mesh->getSkeleton()) {
		m_boneMatrices.clear();
		m_skinMatrices.clear();
		return;
	}
	const auto& skeleton = *m_mesh->getSkeleton();
	m_pose = skeleton.getRestPose();
	if (m_clip) {
		const auto& clip = m_mesh->getClips()[*m_clip];
		if (m_playing)
			m_clipTime += iSeconds;
		m_clipTime = clip.getLocalTime(m_clipTime, true);
		clip.sample(m_clipTime, true, m_pose);
	}
	m_boneMatrices.resize(skeleton.getBoneCount());
	m_skinMatrices.resize(skeleton.getBoneCount());
	skeleton.computeModelMatrices(m_pose, m_boneMatrices);
	skeleton.computeSkinMatrices(m_boneMatrices, m_skinMatrices);
}

void MeshDocument::drawSkeletonOverlay(const math::vec2& iMin, const math::vec2& iSize) const {
	if (!m_showSkeleton || !m_mesh)
		return;
	const bool vulkan = renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan;
	const auto camera = viewer::makeMeshPreviewCamera(m_mesh->getGeometry(), m_orbit,
													  iSize.y() > 0.f ? iSize.x() / iSize.y() : 1.f, vulkan);
	const math::mat4 viewProjection = camera.getViewProjection();
	auto* draw = ImGui::GetWindowDrawList();
	const auto toScreen = [&](const math::mat4& iMatrix) -> std::optional<ImVec2> {
		const auto point =
				viewer::projectToCanvas(viewProjection, {iMatrix(0, 3), iMatrix(1, 3), iMatrix(2, 3)}, iSize, vulkan);
		if (!point)
			return std::nullopt;
		return ImVec2{iMin.x() + point->x(), iMin.y() + point->y()};
	};
	const ImU32 boneColor = IM_COL32(120, 200, 255, 220);
	const ImU32 selectedColor = IM_COL32(255, 200, 60, 255);
	if (const auto skeleton = m_mesh->getSkeleton(); skeleton && m_boneMatrices.size() == skeleton->getBoneCount()) {
		const auto& bones = skeleton->getBones();
		for (size_t index = 0; index < bones.size(); ++index) {
			const auto tip = toScreen(m_boneMatrices[index]);
			if (!tip)
				continue;
			const bool selected = m_selectedBone == index;
			draw->AddCircleFilled(*tip, selected ? 5.f : 3.f, selected ? selectedColor : boneColor);
			if (bones[index].parent < 0)
				continue;
			if (const auto root = toScreen(m_boneMatrices[static_cast<size_t>(bones[index].parent)]); root)
				draw->AddLine(*root, *tip, selected ? selectedColor : boneColor, selected ? 3.f : 2.f);
		}
	}
	for (const auto& socket: m_mesh->getSockets()) {
		math::mat4 matrix = socket.offset;
		if (const auto skeleton = m_mesh->getSkeleton(); skeleton && !socket.bone.empty()) {
			if (const auto bone = skeleton->findBone(socket.bone); bone && *bone < m_boneMatrices.size())
				matrix = m_boneMatrices[*bone] * socket.offset;
		}
		if (const auto at = toScreen(matrix); at) {
			draw->AddRect({at->x - 4.f, at->y - 4.f}, {at->x + 4.f, at->y + 4.f}, IM_COL32(120, 255, 140, 255));
			draw->AddText({at->x + 6.f, at->y - 7.f}, IM_COL32(120, 255, 140, 255), socket.name.c_str());
		}
	}
}

void MeshDocument::onUpdate(const core::Timestep& iTimeStep) {
	if (!m_mesh || m_canvasSize.x() == 0 || m_canvasSize.y() == 0)
		return;
	updatePose(iTimeStep.getSeconds());
	if (!m_framebuffer) {
		using Format = renderer::gpu::AttachmentSpecification::Format;
		using Tiling = renderer::gpu::AttachmentSpecification::Tiling;
		m_framebuffer = renderer::gpu::Framebuffer::create(
				{.size = m_canvasSize,
				 .attachments = {{.format = Format::Surface, .tiling = Tiling::Optimal},
								 {.format = Format::RedInteger, .tiling = Tiling::Optimal},
								 {.format = Format::Depth32F, .tiling = Tiling::Optimal}},
				 .samples = 1,
				 .swapChainTarget = false,
				 .debugName = "mesh_viewer"});
	} else if (m_framebuffer->getSpecification().size != m_canvasSize) {
		m_framebuffer->resize(m_canvasSize);
	}
	viewer::renderMeshPreview(*m_framebuffer, *m_mesh, m_orbit, m_shading,
							  {.lod = m_lod,
							   .skin = m_mesh->isSkinned() ? std::span<const math::mat4>{m_skinMatrices}
														   : std::span<const math::mat4>{}});
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
			const auto imageMin = ImGui::GetItemRectMin();
			drawSkeletonOverlay({imageMin.x, imageMin.y}, {avail.x, avail.y});
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
	renderSkeletonSection();
}

void MeshDocument::renderSkeletonSection() {
	if (!m_mesh || (!m_mesh->getSkeleton() && m_mesh->getSockets().empty()))
		return;
	if (!gui::widgets::sectionHeader("Skeleton", gui::icon::gitBranch, false).open)
		return;
	if (const auto skeleton = m_mesh->getSkeleton(); skeleton) {
		const auto& bones = skeleton->getBones();
		std::vector<size_t> depth(bones.size(), 0);
		for (size_t index = 0; index < bones.size(); ++index) {
			if (bones[index].parent >= 0)
				depth[index] = depth[static_cast<size_t>(bones[index].parent)] + 1;
			ImGui::PushID(static_cast<int>(index));
			ImGui::Indent(static_cast<float>(depth[index]) * ImGui::GetFontSize());
			if (ImGui::Selectable(bones[index].name.c_str(), m_selectedBone == index))
				m_selectedBone = m_selectedBone == index ? std::nullopt : std::optional<size_t>{index};
			ImGui::Unindent(static_cast<float>(depth[index]) * ImGui::GetFontSize());
			ImGui::PopID();
		}
	}
	if (!m_mesh->getSockets().empty()) {
		ImGui::SeparatorText("Sockets");
		for (const auto& socket: m_mesh->getSockets())
			ImGui::BulletText("%s%s%s", socket.name.c_str(), socket.bone.empty() ? "" : " on ", socket.bone.c_str());
	}
	gui::widgets::endSection();
}

void MeshDocument::renderAnimationSection() {
	if (!m_mesh)
		return;
	const auto& clips = m_mesh->getClips();
	if (!clips.empty() && gui::widgets::sectionHeader("Animation", gui::icon::play, false).open) {
		const std::string preview = m_clip ? clips[*m_clip].name : std::string{"(bind pose)"};
		if (ImGui::BeginCombo("Clip", preview.c_str())) {
			if (ImGui::Selectable("(bind pose)", !m_clip))
				setClip(std::nullopt);
			for (size_t index = 0; index < clips.size(); ++index) {
				if (ImGui::Selectable(clips[index].name.c_str(), m_clip == index))
					setClip(index);
			}
			ImGui::EndCombo();
		}
		if (gui::widgets::iconButton("##playClip", {.glyph = m_playing ? gui::icon::pause : gui::icon::play,
													.tooltip = m_playing ? "Pause" : "Play",
													.description = "Play the clip in a loop.",
													.disabled = !m_clip}))
			m_playing = !m_playing;
		ImGui::SameLine();
		ImGui::Checkbox("Bones", &m_showSkeleton);
		constexpr float framesPerSecond = 30.f;
		std::vector<gui::widgets::SequencerEntry> entries;
		int32_t frameMax = 1;
		for (const auto& clip: clips) {
			const auto last = static_cast<int32_t>(std::ceil(clip.duration * framesPerSecond));
			entries.push_back({.label = clip.name, .startFrame = 0, .endFrame = last, .color = {0.4f, 0.6f, 1.f, 1.f}});
			frameMax = std::max(frameMax, last);
		}
		auto frame = static_cast<int32_t>(m_clipTime * framesPerSecond);
		if (gui::widgets::sequencer(
					"##clips", entries, frame, m_firstVisibleFrame,
					{.frameMin = 0, .frameMax = frameMax, .allowEditStartEnd = false, .showCurrentFrame = true},
					{0.f, std::min(40.f + 20.f * static_cast<float>(clips.size()), 220.f)}))
			setClipTime(static_cast<float>(frame) / framesPerSecond);
		ImGui::Text("%.2f s", static_cast<double>(m_clipTime));
		gui::widgets::endSection();
	}
	if (m_mesh->getLodCount() > 0 && gui::widgets::sectionHeader("Levels of detail", gui::icon::layers, false).open) {
		for (size_t level = 0; level <= m_mesh->getLodCount(); ++level) {
			const auto label = level == 0 ? std::format("Full: {} triangles", m_mesh->getLodTriangleCount(0))
										  : std::format("Level {}: {} triangles, under {:.0f} % of the screen", level,
														m_mesh->getLodTriangleCount(level),
														static_cast<double>(m_mesh->getLodScreenSize(level)) * 100.0);
			if (ImGui::RadioButton(label.c_str(), m_lod == level))
				setLod(level);
		}
		ImGui::TextDisabled("Set the levels in the Import panel of the mesh.");
		gui::widgets::endSection();
	}
}

void MeshDocument::renderPropertiesPanel() {
	renderAnimationSection();
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
