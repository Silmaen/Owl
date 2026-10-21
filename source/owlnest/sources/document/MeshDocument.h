/**
 * @file MeshDocument.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "viewer/MeshPreview.h"

#include <math/vectors.h>
#include <renderer/MeshCache.h>
#include <renderer/gpu/Framebuffer.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace owl::nest {

/**
 * @brief
 *  Read-only viewer of a mesh file (`.gltf`, `.glb`, `.obj`): the mesh alone under an orbit camera, textured or
 *  shaded, with its vertex and triangle counts, bounds and material.
 * Nothing in it edits the file, so it has no undo stack and is never dirty.
 */
class MeshDocument final : public Document {
public:
	MeshDocument(const MeshDocument&) = delete;

	MeshDocument(MeshDocument&&) = delete;

	auto operator=(const MeshDocument&) -> MeshDocument& = delete;

	auto operator=(MeshDocument&&) -> MeshDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	MeshDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~MeshDocument() override;

	/**
	 * @brief
	 *  Document type.
	 * @return `DocumentType::Mesh`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Mesh; }

	/**
	 * @brief
	 *  Tab title: the file name.
	 * @return The title.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  Path of the mesh file.
	 * @return The path.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  A viewer has nothing to save.
	 * @return False.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return false; }

	/**
	 * @brief
	 *  Attach to the editor.
	 * @param[in] iEditor The editor layer.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Detach from the editor and release the GPU resources.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Render the mesh into the canvas framebuffer, outside any render pass.
	 * @param[in] iTimeStep The frame time.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Event hook (ImGui handles the input).
	 * @param[in,out] ioEvent The event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Draw the document window: the rendered canvas and its orbit, zoom and reset gestures.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Nothing to save.
	 * @return False.
	 */
	auto save() -> bool override { return false; }

	/**
	 * @brief
	 *  Nothing to save.
	 * @param[in] iPath Unused.
	 * @return False.
	 */
	auto saveAs([[maybe_unused]] const std::filesystem::path& iPath) -> bool override { return false; }

	/**
	 * @brief
	 *  Unused scene undo manager, required by the interface.
	 * @return The placeholder manager.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Unused scene undo manager, required by the interface.
	 * @return The placeholder manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  The document draws its own Hierarchy and Properties panels.
	 * @return True.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Hierarchy panel: file, geometry and material information.
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Properties panel: shading, view reset and placement in the active scene.
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title of the Hierarchy panel.
	 * @return The title.
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Mesh"; }

	/**
	 * @brief
	 *  Title of the Properties panel.
	 * @return The title.
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Mesh View"; }

	/**
	 * @brief
	 *  Read a mesh file.
	 * @param[in] iPath The file.
	 * @return True when the file holds a mesh.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  The mesh shown.
	 * @return The mesh, null before a successful load.
	 */
	[[nodiscard]] auto getMesh() const -> const shared<renderer::MeshResource>& { return m_mesh; }

	/**
	 * @brief
	 *  Orbit of the camera.
	 * @return The orbit.
	 */
	[[nodiscard]] auto getOrbit() const -> const viewer::MeshOrbit& { return m_orbit; }

	/**
	 * @brief
	 *  Put the camera back on its default orbit.
	 */
	void resetView() { m_orbit = {}; }

	/**
	 * @brief
	 *  How the surface is coloured.
	 * @return The shading.
	 */
	[[nodiscard]] auto getShading() const -> viewer::MeshShading { return m_shading; }

	/**
	 * @brief
	 *  Choose how the surface is coloured.
	 * @param[in] iShading The shading.
	 */
	void setShading(const viewer::MeshShading iShading) { m_shading = iShading; }

	/**
	 * @brief
	 *  Place the mesh in the active scene, at the origin (`mesh.create` through the editor).
	 */
	void placeInScene() const;

	/**
	 * @brief
	 *  Check whether the document tab is open.
	 * @return True while open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Set the open state of the tab.
	 * @param[in] iOpen The state.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/// Path of the mesh file.
	std::filesystem::path m_path;
	/// The mesh, owned by the viewer (not shared with the scenes).
	shared<renderer::MeshResource> m_mesh;
	/// Where the mesh is rendered; sized to the canvas.
	shared<renderer::gpu::Framebuffer> m_framebuffer;
	/// Size of the canvas on the last frame, in pixels.
	math::vec2ui m_canvasSize{0, 0};
	/// Orbit of the camera.
	viewer::MeshOrbit m_orbit;
	/// How the surface is coloured.
	viewer::MeshShading m_shading{viewer::MeshShading::Textured};
	/// Editor back-pointer.
	EditorLayer* mp_editorLayer{nullptr};
	/// ImGui open state of the tab.
	bool m_pOpen{true};
	/// Whether the window was focused last frame.
	bool m_wasFocused{false};
	/// Placeholder: `Document::undoManager()` returns a `SceneUndoManager&`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
