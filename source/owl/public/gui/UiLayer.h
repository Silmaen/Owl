/**
 * @file UiLayer.h
 * @author Silmaen
 * @date 05/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "Theme.h"
#include "core/layer/Layer.h"

#include <functional>
#include <string>

struct ImFont;

/**
 * @brief Namespace for gui
 */
namespace owl::gui {
/**
 * @brief Class ImGuiLayer
 */
class OWL_API UiLayer final : public core::layer::Layer {
public:
	UiLayer(const UiLayer&) = delete;
	UiLayer(UiLayer&&) = delete;
	auto operator=(const UiLayer&) -> UiLayer& = delete;
	auto operator=(UiLayer&&) -> UiLayer& = delete;

	/**
	 * @brief Default constructor.
	 */
	UiLayer();

	/**
	 * @brief Destructor.
	 */
	~UiLayer() override;

	/**
	 * @brief Action on Attach.
	 */
	void onAttach() override;

	/**
	 * @brief Action on detach.
	 */
	void onDetach() override;

	/**
	 * @brief Action on event.
	 * @param[in,out] ioEvent The Event to react.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief Begin layer definition.
	 */
	void begin() const;

	/**
	 * @brief End layer definition.
	 */
	void end() const;

	/**
	 * @brief Defines layer as blocking events.
	 * @param[in] iBlock If layer block event or let them pass to next layer.
	 */
	void blockEvents(const bool iBlock) { m_blockEvent = iBlock; }

	/**
	 * @brief Defines the theme for the UI.
	 * @param[in] iTheme The theme to apply.
	 */
	static void setTheme(const Theme& iTheme = Theme());

	/**
	 * @brief Enable docking of windows.
	 */
	void enableDocking() { m_dockingEnable = true; }

	/**
	 * @brief Disable docking of windows.
	 */
	void disableDocking() { m_dockingEnable = false; }

	/**
	 * @brief Assume that there is no application.
	 */
	void disableApp() { m_withApp = false; }

	/**
	 * @brief Get the active widget id.
	 * @return The active widget id.
	 */
	static auto getActiveWidgetId() -> uint32_t;

	/**
	 * @brief Register a callback rendered at the top of the main dockspace window.
	 *
	 * The callback runs each frame after `ImGui::Begin` of the main dockspace window
	 * and before `ImGui::DockSpace`. Intended for a ribbon or application toolbar
	 * that should sit above the docked panels. Pass an empty function to disable.
	 */
	void setTopBarCallback(std::function<void()> iCallback) { m_topBarCallback = std::move(iCallback); }

	/// @brief Pixel size at which the dedicated code-editor font was rasterised on `onAttach`.
	/// Rendering at this size is crisp; other sizes would require bitmap scaling.
	static auto codeFontSize() -> float;

	/// @brief ImGui font loaded at `codeFontSize()` and dedicated to the code editor tabs.
	/// Null when docking has not been initialised yet.
	[[nodiscard]] auto getCodeFont() const -> ImFont* { return mp_codeFont; }

	/// @brief Set the UI font size used on the next `onAttach`.  Clamped to `[12, 32]`.
	/// @param[in] iSize Desired pixel size.
	/// Must be called before the `UiLayer` is constructed (the font atlas is built in `onAttach`
	/// and is not rebuilt until restart).
	static void setUiFontSize(float iSize);

	/// @brief Set the code-editor font size used on the next `onAttach`.  Clamped to `[8, 48]`.
	/// @param[in] iSize Desired pixel size.
	/// Must be called before the `UiLayer` is constructed.
	static void setCodeFontSize(float iSize);

private:
	/// If event should be bocked.
	bool m_blockEvent = true;
	/// If the docking space exists.
	bool m_dockingEnable = false;
	/// If is attached.
	bool m_withApp = true;
	/// Path string for ImGui ini file (must outlive ImGui context).
	std::string m_iniFilePath;
	/// Optional top-bar rendered between the dockspace window `Begin` and `DockSpace()`.
	std::function<void()> m_topBarCallback;
	/// Dedicated code-editor font (Roboto at `codeFontSize()`).  Set in `onAttach`.
	ImFont* mp_codeFont = nullptr;

	/// Function that initializes the docking port (calls `m_topBarCallback` if set).
	void initializeDocking() const;
};
}// namespace owl::gui
