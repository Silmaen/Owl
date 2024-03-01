/**
 * @file GraphContext.h
 * @author Silmaen
 * @date 07/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::renderer {
/**
 * @brief General class for the GraphicContext.
 */
class OWL_API GraphContext {
public:
	GraphContext() = default;
	GraphContext(const GraphContext&) = delete;
	GraphContext(GraphContext&&) = delete;
	GraphContext& operator=(const GraphContext&) = delete;
	GraphContext& operator=(GraphContext&&) = delete;

	/**
	 * @brief Destructor.
	 */
	virtual ~GraphContext();

	/**
	 * @brief Initialize the context.
	 */
	virtual void init() = 0;
	/**
	 * @brief Do the buffer swap.
	 */
	virtual void swapBuffers() = 0;

	/**
	 * @brief Wait for device ready.
	 */
	virtual void waitIdle() {}
	/**
	 * @brief Create a Graphics context.
	 * @param[in,out] ioWindow The window into render context.
	 * @return The created context.
	 */
	static uniq<GraphContext> create(void* ioWindow);

	/**
	 * @brief Structure for holding version number.
	 */
	struct Version {
		int major; ///< major version number.
		int minor; ///< minor version number.
		/**
		 * @brief Check version order
		 * @param[in] iOther Version to compare
		 * @return True if this is equal.
		 */
		[[nodiscard]] bool operator==(const Version& iOther) const {
			return major == iOther.major && minor == iOther.minor;
		}

		/**
		 * @brief Check version order
		 * @param[in] iOther Version to compare
		 * @return True if this is lower.
		 */
		[[nodiscard]] bool operator<(const Version& iOther) const {
			return major < iOther.major || (major == iOther.major && minor < iOther.minor);
		}

		/**
		 * @brief Check version order
		 * @param[in] iOther Version to compare
		 * @return True if this is lower.
		 */
		[[nodiscard]] bool operator<=(const Version& iOther) const {
			return major <= iOther.major || (major == iOther.major && minor <= iOther.minor);
		}
	};

	/**
	 * @brief Get version number of the backend API.
	 * @return The version number.
	 */
	[[nodiscard]] virtual Version getVersion() const = 0;
};
} // namespace owl::renderer
