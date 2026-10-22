/**
 * @file MaterialDocument.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"
#include "viewer/MeshPreview.h"

#include <math/vectors.h>
#include <renderer/Material.h>
#include <renderer/MeshCache.h>
#include <renderer/gpu/Framebuffer.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace owl::nest {

/**
 * @brief
 *  Undo manager typed for `renderer::MaterialAsset` edits.
 *
 * Each command keeps the whole asset before and after the edit: a material is a few dozen values.
 */
using MaterialUndoManager = UndoManager<renderer::MaterialAsset>;

/**
 * @brief
 *  A `.owlmat` material edited in a tab: its parent, every value (or, with a parent, every override) and a live
 *  preview on a sphere or a cube, with its own undo stack.
 *
 * Every edit is a method returning whether it changed the asset, so the tab and the tests drive the same code; a
 * drag of a slider merges into one undo step. Saving writes the file atomically and drops the material from the
 * engine's `MaterialLibrary`, so every entity drawing it updates at once.
 */
class MaterialDocument final : public Document {
public:
	MaterialDocument(const MaterialDocument&) = delete;

	MaterialDocument(MaterialDocument&&) = delete;

	auto operator=(const MaterialDocument&) -> MaterialDocument& = delete;

	auto operator=(MaterialDocument&&) -> MaterialDocument& = delete;

	/**
	 * @brief
	 *  Create an empty material document (the default root material).
	 */
	MaterialDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~MaterialDocument() override;

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Kind of document.
	 * @return `DocumentType::Material`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Material; }

	/**
	 * @brief
	 *  Title of the tab.
	 * @return The file name, "Untitled" without a path.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  Path of the file.
	 * @return The path, empty when none was set.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Whether the material differs from the saved one.
	 * @return True when there are unsaved edits.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return m_undo.isDirty(); }

	/**
	 * @brief
	 *  Attach the document to the editor.
	 * @param[in] iEditor The editor.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Detach the document from the editor and free the preview.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Render the preview.
	 * @param[in] iTimeStep Time since the last frame.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Event hook (nothing to do: ImGui consumes its own events).
	 * @param[in,out] ioEvent The event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Draw the tab: the parameters on the left, the preview on the right.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Write the material to its file and drop it from the material library.
	 * @return True when written; false without a path or when the write failed.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Write the material to another file, which becomes the document's.
	 * @param[in] iPath The file.
	 * @return True when written.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  The material as YAML, for the autosave.
	 * @return The text.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Put back the material of an autosave.
	 * @param[in] iSnapshot The YAML text.
	 * @return True when it parsed.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Placeholder of the `Document` base (the typed manager is `materialUndoManager`).
	 * @return The empty scene undo manager.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Placeholder of the `Document` base.
	 * @return The empty scene undo manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Whether an edit can be undone.
	 * @return True when the stack is not empty.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether an undone edit can be redone.
	 * @return True when the redo stack is not empty.
	 */
	[[nodiscard]] auto canRedo() const -> bool override { return m_undo.canRedo(); }

	/**
	 * @brief
	 *  Undo the last edit.
	 */
	void performUndo() override;

	/**
	 * @brief
	 *  Redo the last undone edit.
	 */
	void performRedo() override;

	// --- Material API --------------------------------------------------------
	/**
	 * @brief
	 *  The material being edited.
	 * @return The asset.
	 */
	[[nodiscard]] auto asset() const -> const renderer::MaterialAsset& { return m_asset; }

	/**
	 * @brief
	 *  The typed undo stack.
	 * @return The manager.
	 */
	[[nodiscard]] auto materialUndoManager() const -> const MaterialUndoManager& { return m_undo; }

	/**
	 * @brief
	 *  Read a material file into the document.
	 * @param[in] iPath The `.owlmat` file.
	 * @return True when it was read.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Choose the parent material, or none.
	 * @param[in] iParent Asset path of the parent `.owlmat`, empty for a root material.
	 * @return True when it changed.
	 */
	auto setParent(const std::filesystem::path& iParent) -> bool;

	/**
	 * @brief
	 *  Replace the values and the overridden fields.
	 * @param[in] iValues The new values.
	 * @param[in] iOverrides Mask of the fields the material sets (used with a parent).
	 * @param[in] iMerge True for a drag of a slider: the edits of the next second join one undo step.
	 * @return True when it changed.
	 */
	auto setValues(const renderer::MaterialParameters& iValues, uint32_t iOverrides, bool iMerge = false) -> bool;

	/**
	 * @brief
	 *  The material as it is drawn: the parent's resolved material with the overridden fields, or the values alone.
	 * @return The material.
	 */
	[[nodiscard]] auto buildMaterial() const -> renderer::Material;

	/**
	 * @brief
	 *  Shape of the preview.
	 * @return The shape.
	 */
	[[nodiscard]] auto previewShape() const -> viewer::PreviewShape { return m_shape; }

	/**
	 * @brief
	 *  Choose the shape of the preview.
	 * @param[in] iShape The shape.
	 */
	void setPreviewShape(viewer::PreviewShape iShape);

	// --- Tab -----------------------------------------------------------------
	/**
	 * @brief
	 *  True while the user has not clicked the close button of the tab.
	 * @return True until ImGui flips the close flag.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Set the open flag.
	 * @param[in] iOpen The new value.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/**
	 * @brief
	 *  Make an asset the current one and record the step.
	 * @param[in] iNext The asset after the edit.
	 * @param[in] iDescription Description of the step.
	 * @param[in] iMerge Whether the step joins the previous one of the same description.
	 * @return True when the asset changed.
	 */
	auto commit(renderer::MaterialAsset iNext, const std::string& iDescription, bool iMerge) -> bool;

	/**
	 * @brief
	 *  Draw the parent field and the fields of the material.
	 */
	void renderParameters();

	/**
	 * @brief
	 *  Draw the preview, its shape buttons and handle its orbit.
	 */
	void renderPreview();

	/// Disk path of the material.
	std::filesystem::path m_path;
	/// Material being edited.
	renderer::MaterialAsset m_asset;
	/// Undo stack of the material.
	MaterialUndoManager m_undo;
	/// Editor back-pointer.
	EditorLayer* mp_editorLayer{nullptr};
	/// Preview framebuffer.
	shared<renderer::gpu::Framebuffer> m_framebuffer;
	/// Size of the preview area in pixels.
	math::vec2ui m_canvasSize{0, 0};
	/// Preview shape.
	viewer::PreviewShape m_shape{viewer::PreviewShape::Sphere};
	/// Meshes of the shapes, built on first use.
	shared<renderer::MeshResource> m_shapeMeshes[2];
	/// Material drawn by the preview.
	std::optional<renderer::Material> m_previewMaterial;
	/// Whether the preview material must be built again.
	bool m_previewDirty{true};
	/// Generation of the material library the preview material was built from.
	uint64_t m_libraryGeneration{0};
	/// Orbit of the preview camera.
	viewer::MeshOrbit m_orbit;
	/// Frame of the last edit from the parameters, to tell a drag from separate edits.
	int m_lastEditFrame{-2};
	/// Tracks the ImGui "Open" state of the tab.
	bool m_pOpen{true};
	/// Whether the tab was focused last frame.
	bool m_wasFocused{false};
	/// Placeholder required by `Document::undoManager()`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
