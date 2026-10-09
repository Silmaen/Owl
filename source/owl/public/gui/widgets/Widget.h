/**
 * @file Widget.h
 * @author Silmaen
 * @date 10/26/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Core.h"
#include <concepts>
#include <string>
#include <utility>

/**
 * @brief
 *  Namespace for the gui widgets.
 */
namespace owl::gui::widgets {
/**
 * @brief
 *  Base widget's data information.
 */
struct WidgetData {
	/// Unique ImGui identifier.
	std::string id;
	/// False hides the widget.
	bool visible = true;
};

/// Concept imposing the DataType to be derived from Widget Data.
template<typename DataType>
concept derivedFromWidgetData = std::derived_from<DataType, WidgetData>;

/**
 * @brief
 *  base widget's class.
 * @tparam DataType Type of data describing the widget.
 */
template<derivedFromWidgetData DataType = WidgetData>
class OWL_API Widget {
public:
	/**
	 * @brief
	 *  Constructor.
	 */
	Widget() = default;

	/**
	 * @brief
	 *  Default destructor.
	 */
	virtual ~Widget() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	Widget(const Widget&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	Widget(Widget&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const Widget&) -> Widget& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(Widget&&) -> Widget& = default;

	/**
	 * @brief
	 *  Initialize the data.
	 * @param iData Data to apply to the widget.
	 */
	void init(DataType&& iData) {
		m_data = std::move(iData);
		m_initialized = true;
	}

	/**
	 * @brief
	 *  Render the widget.
	 */
	void onRender() const {
		if (!m_initialized) {
			OWL_CORE_WARN("Trying to render uninitialized widget {}.", m_data.id)
			return;
		}
		if (m_data.visible)
			/**
			 * @brief
			 *  Handle the render base event.
			 */
			onRenderBase();
	}

protected:
	/// If the Widget is initialized.
	bool m_initialized = false;
	/// Internal data.
	DataType m_data;

private:
	/**
	 * @brief
	 *  Handle the render base event.
	 */
	virtual void onRenderBase() const = 0;
};

}// namespace owl::gui::widgets
