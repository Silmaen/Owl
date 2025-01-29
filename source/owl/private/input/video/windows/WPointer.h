/**
 * @file WPointer.h
 * @author Silmaen
 * @date 29/12/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

/**
 * @brief Windows-specific functions.
 */
namespace owl::input::video::windows {
/**
 * @brief Class for managing a windows pointer.
 *
 * Simple class to handle windows object pointers like a unique_ptr
 */
template<class Ptr>
class WPointer final {
public:
	/**
	 * @brief Constructor.
	 */
	WPointer() = default;

	/**
	 * @brief Constructor with a copy.
	 * @param[in] iOther Raw pointer to affect.
	 * @param[in] iOwn If the WPoint should take the ownership of the memory.
	 */
	explicit WPointer(Ptr* iOther, const bool iOwn = false) : m_owner{iOwn}, mp_object{iOther} {}

	/**
	 * @brief Destructor.
	 */
	~WPointer() {
		if (m_owner)
			release();
	}

	WPointer(const WPointer&) = default;
	WPointer(WPointer&&) = default;
	auto operator=(const WPointer&) -> WPointer& = default;
	auto operator=(WPointer&&) -> WPointer& = default;

	/**
	 * @brief Overload of the -> operator for using thi object as if it is the pointer.
	 * @return The backend pointer.
	 */
	auto operator->() -> Ptr* { return mp_object; }

	// NOLINTBEGIN(google-explicit-constructor,hicpp-explicit-conversions)
	/**
	 * @brief Explicit type casting to bool.
	 * @return True if pointer is non-void.
	 */
	operator bool() const { return mp_object != nullptr; }
	// NOLINTEND(google-explicit-constructor,hicpp-explicit-conversions)

	/**
	 * @brief Direct access to the pointer.
	 * @return The raw pointer.
	 */
	auto get() -> Ptr* { return mp_object; }
	/**
	 * @brief Direct access to the address of the pointer.
	 * @return The address of pointer.
	 */
	auto addr() -> Ptr** { return (&mp_object); }

	/**
	 * @brief Direct access to the address of the pointer with type change.
	 * @tparam Alias The new type.
	 * @return The raw pointer with the requested type.
	 */
	template<class Alias>
	auto getAs() -> Alias* {
		return reinterpret_cast<Alias*>(mp_object);
	}

	/**
	 * @brief Direct access to the address of the pointer with type change.
	 * @tparam Alias The new type.
	 * @return The address of pointer with the requested type.
	 */
	template<class Alias>
	auto addrAs() -> Alias** {
		return reinterpret_cast<Alias**>(&mp_object);
	}

	/**
	 * @brief Request memory release.
	 */
	void release() {
		if (mp_object)
			mp_object->Release();
		mp_object = nullptr;
	}

	/**
	 * @brief This object takes the memory ownership
	 */
	void takeOwnership() { m_owner = true; }


	/**
	 * @brief This object takes the memory ownership
	 * @param[in] iOther The other object to take ownership from.
	 */
	void takeOwnershipFrom(WPointer& iOther) {
		mp_object = iOther.mp_object;
		if (iOther.isMemoryOwned()) {
			iOther.leaveOwnership();
			takeOwnership();
		} else {
			leaveOwnership();
		}
	}

	/**
	 * @brief This object leave the memory ownership
	 */
	void leaveOwnership() { m_owner = false; }
	/**
	 * @brief Check if the object has memory ownership.
	 * @return True if this object has memory ownership.
	 */
	[[nodiscard]] auto isMemoryOwned() const -> bool { return m_owner; }
	/**
	 * @brief Defines the ownership of this object on backend object.
	 * @param[in] iOwn If the object should be owned.
	 */
	void setOwner(const bool iOwn) { m_owner = iOwn; }

private:
	/// Ownership of this objet on the memory behind the pointer.
	bool m_owner = true;
	/// Pointer to the object.
	Ptr* mp_object = nullptr;
};

}// namespace owl::input::video::windows
