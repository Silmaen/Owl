/**
 * @file HudEditor.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../document/HudDocument.h"

#include <owlgui.h>

#include <renderer/gpu/Framebuffer.h>
#include <renderer/gpu/Texture.h>

#include <optional>
#include <string>

namespace owl::nest {
class EditorLayer;
class SceneDocument;
}// namespace owl::nest

namespace owl::nest::panel {

/**
 * @brief
 *  The HUD view of a scene document: the preview of its canvases at a chosen resolution over a backdrop, the widget
 *  palette, the rect gizmo and the toolbar.
 *
 * It takes the place of the 3D viewport in the tab of the document while the document is in HUD mode (always for an
 * `.owlhud`). The rules live in `HudDocument`; this class draws them with ImGui and feeds them the mouse, and every
 * change goes through a `hud.*` command of the registry. Selection is shared with the Scene Hierarchy and the
 * Properties panel.
 */
class HudEditor final {
public:
	HudEditor(const HudEditor&) = delete;

	HudEditor(HudEditor&&) = delete;

	auto operator=(const HudEditor&) -> HudEditor& = delete;

	auto operator=(HudEditor&&) -> HudEditor& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	HudEditor();

	/**
	 * @brief
	 *  Destructor.
	 */
	~HudEditor();

	/**
	 * @brief
	 *  Define the editor the panel issues commands through.
	 * @param[in] iParent The editor layer (non-owning, may be null).
	 */
	void attachParent(EditorLayer* iParent) { mp_parent = iParent; }

	/**
	 * @brief
	 *  Free the GPU resources.
	 */
	void detach();

	/**
	 * @brief
	 *  Get the state of the HUD editor.
	 * @return The document logic.
	 */
	[[nodiscard]] auto getDocument() -> HudDocument& { return m_document; }

	/**
	 * @brief
	 *  Get the state of the HUD editor.
	 * @return The document logic.
	 */
	[[nodiscard]] auto getDocument() const -> const HudDocument& { return m_document; }

	/**
	 * @brief
	 *  Draw the preview into its framebuffer: backdrop, then the HUD of the scene at the preview resolution.
	 * @param[in,out] ioScene The scene.
	 */
	void onUpdate(scene::Scene& ioScene);

	/**
	 * @brief
	 *  Draw the toolbar, the palette and the preview, and handle the mouse and the keys.
	 *
	 * Call it inside the window of the document tab.
	 * @param[in,out] ioDocument The scene document edited.
	 */
	void onRender(SceneDocument& ioDocument);

private:
	/**
	 * @brief
	 *  Create the framebuffer, or resize it to the preview resolution.
	 */
	void ensureFramebuffer();

	/**
	 * @brief
	 *  Draw the toolbar: resolution, zoom, backdrop, snapping, order and deletion.
	 * @param[in,out] ioDocument The scene document edited.
	 */
	void renderToolbar(SceneDocument& ioDocument);

	/**
	 * @brief
	 *  Draw the palette of the widgets, which can be clicked or dragged into the preview.
	 * @param[in,out] ioDocument The scene document edited.
	 */
	void renderPalette(SceneDocument& ioDocument);

	/**
	 * @brief
	 *  Draw the preview and handle the gestures in it.
	 * @param[in,out] ioDocument The scene document edited.
	 */
	void renderPreview(SceneDocument& ioDocument);

	/**
	 * @brief
	 *  Draw the outlines, the gizmo, the guides and the selection box over the preview.
	 * @param[in] iScene The scene.
	 */
	void renderOverlay(const scene::Scene& iScene) const;

	/**
	 * @brief
	 *  Turn the mouse of the preview into selection, gizmo gestures and box selection.
	 * @param[in,out] ioDocument The scene document edited.
	 * @param[in] iMouse The mouse in preview pixels.
	 * @param[in] iHovered Whether the mouse is over the preview.
	 */
	void handleMouse(SceneDocument& ioDocument, const math::vec2& iMouse, bool iHovered);

	/**
	 * @brief
	 *  Run the keys of the preview (delete, arrows, escape).
	 * @param[in,out] ioDocument The scene document edited.
	 */
	void handleKeys(SceneDocument& ioDocument);

	/**
	 * @brief
	 *  Accept what is dropped on the preview: a widget of the palette, or a texture of the Content Browser.
	 * @param[in,out] ioDocument The scene document edited.
	 * @param[in] iMouse The mouse in preview pixels.
	 */
	void handleDrop(SceneDocument& ioDocument, const math::vec2& iMouse);

	/**
	 * @brief
	 *  Draw the context menu of the preview.
	 * @param[in,out] ioDocument The scene document edited.
	 */
	void renderContextMenu(SceneDocument& ioDocument);

	/**
	 * @brief
	 *  Keep the selection of the document and the one of the Scene Hierarchy the same.
	 * @param[in,out] ioDocument The scene document edited.
	 */
	void syncSelection(SceneDocument& ioDocument);

	/**
	 * @brief
	 *  Build the command target of the document.
	 * @param[in,out] ioDocument The scene document edited.
	 * @return The registry bound to the scene and its undo history.
	 */
	[[nodiscard]] auto getTarget(SceneDocument& ioDocument) const -> commands::CommandTarget;

	/**
	 * @brief
	 *  Convert a point of the screen to a point of the preview.
	 * @param[in] iScreen The point in screen pixels.
	 * @return The point in preview pixels.
	 */
	[[nodiscard]] auto toPreview(const ImVec2& iScreen) const -> math::vec2;

	/**
	 * @brief
	 *  Convert a point of the preview to a point of the screen.
	 * @param[in] iPoint The point in preview pixels.
	 * @return The point in screen pixels.
	 */
	[[nodiscard]] auto toScreen(const math::vec2& iPoint) const -> ImVec2;

	/// The rules and the state of the editor.
	HudDocument m_document;
	/// Where the preview is rendered.
	shared<renderer::gpu::Framebuffer> m_framebuffer;
	/// Image behind the HUD.
	shared<renderer::gpu::Texture2D> m_backdropTexture;
	/// Path `m_backdropTexture` was loaded from.
	std::string m_backdropLoaded;
	/// The editor layer (non-owning).
	EditorLayer* mp_parent{nullptr};
	/// Top-left corner of the preview image, in screen pixels.
	ImVec2 m_origin{0.f, 0.f};
	/// Screen pixels per preview pixel.
	float m_zoom{1.f};
	/// Offset of the image centre from the centre of the preview area, in screen pixels.
	ImVec2 m_pan{0.f, 0.f};
	/// Whether the zoom follows the size of the area.
	bool m_fit{true};
	/// Whether the outlines of every widget are drawn.
	bool m_showOutlines{true};
	/// Whether the reference resolution of the canvases is framed.
	bool m_showReference{true};
	/// Start of a selection box, in preview pixels.
	std::optional<math::vec2> m_boxStart;
	/// Current corner of the selection box, in preview pixels.
	math::vec2 m_boxEnd{0.f, 0.f};
	/// Primary widget the Scene Hierarchy last showed.
	core::UUID m_syncedPrimary{0};
	/// Point of the preview where the context menu opened.
	math::vec2 m_contextPoint{0.f, 0.f};
	/// Size of the area the preview is drawn in, in screen pixels.
	ImVec2 m_areaSize{0.f, 0.f};
};

}// namespace owl::nest::panel
