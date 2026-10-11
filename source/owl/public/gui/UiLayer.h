/**
 * @file UiLayer.h
 * @author Silmaen
 * @date 05/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "Theme.h"
#include "app/layer/Layer.h"
#include "renderer/gpu/Texture.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

struct ImFont;

/**
 * @brief
 *  Namespace for gui.
 */
namespace owl::gui {

/**
 * @brief
 *  Typographic roles of the editor text, each a font and a size relative to the UI font size.
 */
enum struct TextStyle : uint8_t {
	Body,///< Running text and values (Inter, 1x).
	Strong,///< Emphasis at body size (Inter SemiBold, 1x).
	Title,///< Panel or dialogue title (Inter SemiBold, 1.15x).
	Section,///< Section and component header (Inter SemiBold, 0.9x).
	Label,///< Property labels and values of the inspector, ribbon labels (Inter, 0.875x).
	Caption,///< Small print, group captions, sub-section titles (Inter SemiBold, 0.75x).
	Value,///< Numbers aligned in columns (JetBrains Mono, 0.92x).
	Code,///< Code editor text (JetBrains Mono at the code font size).
};
/**
 * @brief
 *  Class ImGuiLayer.
 */
class OWL_API UiLayer final : public app::layer::Layer {
public:
	UiLayer(const UiLayer&) = delete;

	UiLayer(UiLayer&&) = delete;

	auto operator=(const UiLayer&) -> UiLayer& = delete;

	auto operator=(UiLayer&&) -> UiLayer& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	UiLayer();

	/**
	 * @brief
	 *  Destructor.
	 */
	~UiLayer() override;

	/**
	 * @brief
	 *  Action on Attach.
	 */
	void onAttach() override;

	/**
	 * @brief
	 *  Action on detach.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Action on event.
	 * @param[in,out] ioEvent The Event to react.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Begin layer definition.
	 */
	void begin() const;

	/**
	 * @brief
	 *  End layer definition.
	 */
	void end() const;

	/**
	 * @brief
	 *  Defines layer as blocking events.
	 * @param[in] iBlock If layer block event or let them pass to next layer.
	 */
	void blockEvents(const bool iBlock) { m_blockEvent = iBlock; }

	/**
	 * @brief
	 *  Schedule a texture (or any GPU resource wrapped in a `shared`) to be
	 *  released only **after** the current ImGui frame has been submitted.
	 *
	 * On Vulkan, ``ImTextureID`` is a raw ``VkDescriptorSet`` pointer captured
	 * into ImGui's draw data. If the underlying texture is destroyed between
	 * the widget call (e.g. ``ImGui::Image``) and ``ImGui_ImplVulkan_RenderDrawData``,
	 * the driver dereferences a stale descriptor and crashes. Call this
	 * whenever you swap a texture mid-frame (drag-drop handlers, asset
	 * reloads): the shared pointer is kept alive until ``end()`` finishes,
	 * then dropped.
	 * @param[in] iTexture The texture handle to keep alive across the frame.
	 */
	static void deferTextureRelease(shared<renderer::gpu::Texture> iTexture);

	/**
	 * @brief
	 *  Defines the theme for the UI.
	 * @param[in] iTheme The theme to apply.
	 */
	static void setTheme(const Theme& iTheme = Theme());

	/**
	 * @brief
	 *  The theme applied last.
	 * @return The current theme.
	 */
	[[nodiscard]] static auto getTheme() -> const Theme&;

	/**
	 * @brief
	 *  Push the font and size of a typographic role; pair with `popTextStyle`.
	 * @param[in] iStyle The role.
	 */
	static void pushTextStyle(TextStyle iStyle);

	/**
	 * @brief
	 *  Pop the role pushed by `pushTextStyle`.
	 */
	static void popTextStyle();

	/**
	 * @brief
	 *  Scale every text of the interface at once (and the sizes derived from the font), applied at once.
	 * @param[in] iScale The scale, clamped to [`minTextScale`, `maxTextScale`]; 1 keeps the font size.
	 */
	static void setTextScale(float iScale);

	/**
	 * @brief
	 *  Scale given to every text by `setTextScale`.
	 * @return The scale.
	 */
	[[nodiscard]] static auto getTextScale() -> float;

	/// Smallest text scale.
	static constexpr float minTextScale = 0.75f;
	/// Largest text scale.
	static constexpr float maxTextScale = 1.5f;

	/**
	 * @brief
	 *  Content scale of the primary monitor applied to the style sizes and fonts.
	 * @return The scale, 1 on a standard-density screen and under Wayland (the framebuffer scale covers it there).
	 */
	[[nodiscard]] static auto getDpiScale() -> float;

	/**
	 * @brief
	 *  Enable docking of windows.
	 */
	void enableDocking() { m_dockingEnable = true; }

	/**
	 * @brief
	 *  Disable docking of windows.
	 */
	void disableDocking() { m_dockingEnable = false; }

	/**
	 * @brief
	 *  Assume that there is no application.
	 */
	void disableApp() { m_withApp = false; }

	/**
	 * @brief
	 *  Get the active widget id.
	 * @return The active widget id.
	 */
	static auto getActiveWidgetId() -> uint32_t;

	/**
	 * @brief
	 *  Register a callback rendered at the top of the main dockspace window.
	 *
	 * The callback runs each frame after `ImGui::Begin` of the main dockspace window
	 * and before `ImGui::DockSpace`. Intended for a ribbon or application toolbar
	 * that should sit above the docked panels. Pass an empty function to disable.
	 * @param[in] iCallback The callback, or an empty function.
	 */
	void setTopBarCallback(std::function<void()> iCallback) { m_topBarCallback = std::move(iCallback); }

	/**
	 * @brief
	 *  Register a callback rendered at the bottom of the main dockspace window, below the docked panels.
	 *
	 * The dockspace shrinks by one frame height (with spacing) to leave room for it. Intended for a status bar.
	 * Pass an empty function to disable.
	 * @param[in] iCallback The callback, or an empty function.
	 */
	void setBottomBarCallback(std::function<void()> iCallback) { m_bottomBarCallback = std::move(iCallback); }

	/**
	 * @brief
	 *  Register a callback run each frame just before `ImGui::DockSpace`, with the dockspace ID.
	 *
	 * The only point in the frame where the dock tree can be rebuilt with the `DockBuilder` API.
	 * Pass an empty function to disable.
	 * @param[in] iCallback The callback, or an empty function.
	 */
	void setDockLayoutCallback(std::function<void(uint32_t)> iCallback) { m_dockLayoutCallback = std::move(iCallback); }

	/**
	 * @brief
	 *  ID of the main dockspace, as submitted on the last frame.
	 * @return The dockspace ID, 0 before the first docked frame.
	 */
	[[nodiscard]] static auto getDockSpaceId() -> uint32_t;

	/**
	 * @brief
	 *  Pixel size at which the dedicated code-editor font was rasterised on `onAttach`.
	 *
	 * Rendering at this size is crisp; other sizes would require bitmap scaling.
	 * @return The code-editor font pixel size.
	 */
	static auto codeFontSize() -> float;

	/**
	 * @brief
	 *  ImGui font loaded at `codeFontSize()` and dedicated to the code editor tabs.
	 * @return The code-editor font, or nullptr when docking has not been initialised yet.
	 */
	[[nodiscard]] auto getCodeFont() const noexcept -> ImFont* { return mp_codeFont; }

	/**
	 * @brief
	 *  Set the UI font size used on the next `onAttach`.
	 *
	 * Clamped to `[12, 32]`. Must be called before the `UiLayer` is constructed (the font atlas
	 * is built in `onAttach` and is not rebuilt until restart).
	 * @param[in] iSize Desired pixel size.
	 */
	static void setUiFontSize(float iSize);

	/**
	 * @brief
	 *  Set the code-editor font size used on the next `onAttach`.
	 *
	 * Clamped to `[8, 48]`. Must be called before the `UiLayer` is constructed.
	 * @param[in] iSize Desired pixel size.
	 */
	static void setCodeFontSize(float iSize);

private:
	/// If event should be blocked.
	bool m_blockEvent = true;
	/// If the docking space exists.
	bool m_dockingEnable = false;
	/// If is attached.
	bool m_withApp = true;
	/// Path string for ImGui ini file (must outlive ImGui context).
	std::string m_iniFilePath;
	/// Optional top-bar rendered between the dockspace window `Begin` and `DockSpace()`.
	std::function<void()> m_topBarCallback;
	/// Optional bottom bar rendered after `DockSpace()`, below the docked panels.
	std::function<void()> m_bottomBarCallback;
	/// Optional callback run before `DockSpace()` to (re)build the dock tree.
	std::function<void(uint32_t)> m_dockLayoutCallback;
	/// Dedicated code-editor font (JetBrains Mono at `codeFontSize()`). Set in `onAttach`.
	ImFont* mp_codeFont = nullptr;

	/**
	 * @brief
	 *  Initializes the docking port (calls `m_topBarCallback` if set).
	 */
	void initializeDocking() const;
};
}// namespace owl::gui
