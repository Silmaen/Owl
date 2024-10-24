/**
 * @file ContentBrowser.h
 * @author Silmaen
 * @date 10/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::nest::panel {

/**
 * @brief Class ContentBrowser
 */
class ContentBrowser {
public:
	/**
	 * @brief Default copy constructor
	 */
	ContentBrowser(const ContentBrowser&) = default;
	/**
	 * @brief Default move constructor
	 */
	ContentBrowser(ContentBrowser&&) = default;
	/**
	 * @brief Default copy assignation
	 * @return this
	 */
	auto operator=(const ContentBrowser&) -> ContentBrowser& = default;
	/**
	 * @brief Default move assignation
	 * @return this
	 */
	auto operator=(ContentBrowser&&) -> ContentBrowser& = default;
	/**
	 * @brief Default constructor.
	 */
	ContentBrowser();
	/**
	 * @brief Destructor.
	 */
	~ContentBrowser() = default;

	/**
	 * @brief The render command
	 */
	void onImGuiRender();

	void detach();
	void attach();

private:
	/// The actual folder
	std::filesystem::path m_currentPath;
	std::filesystem::path m_currentRootPath;

	void renderTopBand();
	void renderContent();
};

}// namespace owl::nest::panel
