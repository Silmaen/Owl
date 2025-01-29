/**
 * @file Widget.h
 * @author Silmaen
 * @date 10/26/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Core.h"
#include <string>

/**
 * @brief Namespace for the gui widgets.
 */
namespace owl::gui::widgets {

/**
 * @brief Base widget's data information.
 */
struct WidgetData {
	std::string id{};
	bool visible = true;
};

/// Concept imposing the DataType to be derived from Widget Data.
template<typename DataType>
concept derivedFromWidgetData = std::derived_from<DataType, WidgetData>;

/**
 * @brief base widget's class.
 * @tparam DataType Type of data describing the widget.
 */
template<derivedFromWidgetData DataType = WidgetData>
class OWL_API Widget {
public:
	/**
	 * @brief Constructor.
	 */
	Widget() = default;
	/**
	 * @brief Default destructor.
	 */
	virtual ~Widget() = default;

	Widget(const Widget&) = default;
	Widget(Widget&&) = default;
	auto operator=(const Widget&) -> Widget& = default;
	auto operator=(Widget&&) -> Widget& = default;

	/**
	 * @brief Initialize the data.
	 * @param iData Data to apply to the widget.
	 */
	void init(DataType&& iData) {
		m_data = std::move(iData);
		m_initialized = true;
	}
	/**
	 * @brief Render the widget.
	 */
	void onRender() const {
		if (!m_initialized) {
			OWL_CORE_WARN("Trying to render uninitialized widget {}", m_data.id)
			return;
		}
		if (m_data.visible)
			onRenderBase();
	}

protected:
	/// If the Widget is initialized.
	bool m_initialized = false;
	/// Internal data.
	DataType m_data;

private:
	virtual void onRenderBase() const = 0;
};

}// namespace owl::gui::widgets
