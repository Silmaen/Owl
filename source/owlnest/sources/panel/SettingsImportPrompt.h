/**
 * @file SettingsImportPrompt.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include <cstdint>

namespace owl::nest::panel {

/**
 * @brief
 *  Modal shown at the first start of an installation, offering to import the settings of a previous one.
 */
class SettingsImportPrompt final {
public:
	/**
	 * @brief
	 *  What the user chose in the modal.
	 */
	enum struct Choice : uint8_t {
		None,///< No choice this frame (modal closed or still open).
		Import,///< Pick the folder of the previous installation.
		Skip,///< Start with the default settings.
	};

	/**
	 * @brief
	 *  Show the modal.
	 */
	void open();

	/**
	 * @brief
	 *  Check whether the modal waits for a choice.
	 * @return True while open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_open; }

	/**
	 * @brief
	 *  Draw the modal.
	 * @return The choice made this frame.
	 */
	auto onImGuiRender() -> Choice;

private:
	/// True while the modal waits for a choice.
	bool m_open = false;
	/// True until the ImGui popup is opened.
	bool m_openRequested = false;
};

}// namespace owl::nest::panel
