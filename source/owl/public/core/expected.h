/**
 * @file expected.h
 * @author Silmaen
 * @date 22/07/2025
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Public alias for `std::expected` (C++23) with a fallback to `zeus::expected`
 * on toolchains that lack the standard implementation. Use `owl::expected<T, E>`
 * and `owl::unexpected{...}` everywhere instead of picking the underlying type
 * directly.
 */

#pragma once

#if defined(__has_include) && __has_include(<expected>) && (defined(__cpp_lib_expected) || __cplusplus >= 202302L)

#include <expected>

namespace owl {

using std::expected;
using std::unexpected;

}// namespace owl

#elif defined(__has_include) && __has_include(<zeus/expected.hpp>) && !defined(OWL_USE_CLANG_TIDY)

#include <zeus/expected.hpp>

namespace owl {

using zeus::expected;
using zeus::unexpected;

}// namespace owl

#else

#include <exception>
#include <utility>

namespace owl {

/**
 * @brief
 *  Unexpected result.
 * @tparam E Error Type.
 */
template<typename E>
class unexpected {
private:
	/// Wrapped error value.
	E m_error;

public:
	/**
	 * @brief
	 *  Copy Constructor.
	 * @param e The error.
	 */
	constexpr explicit unexpected(const E& e) : m_error(e) {}

	/**
	 * @brief
	 *  Move Constructor.
	 * @param e The error.
	 */
	constexpr explicit unexpected(E&& e) : m_error(std::move(e)) {}

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	constexpr const E& error() const& { return m_error; }

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	constexpr E& error() & { return m_error; }

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	constexpr const E&& error() const&& { return std::move(m_error); }

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	constexpr E&& error() && { return std::move(m_error); }

	/**
	 * @brief
	 *  Equality comparison.
	 * @tparam G Compared error type.
	 * @param[in] lhs Left-hand operand.
	 * @param[in] rhs Right-hand operand.
	 * @return True when both operands compare equal.
	 */
	template<typename G>
	friend constexpr bool operator==(const unexpected& lhs, const unexpected<G>& rhs) {
		return lhs.error() == rhs.error();
	}
};

/// Tag type used by error-only constructors.
struct unexpect_t {};
inline constexpr unexpect_t unexpect{};

/**
 * @brief
 *  Exception thrown when accessing the value of an `expected` that holds an error.
 * @tparam E Error type.
 */
template<typename E>
class bad_expected_access : public std::exception {
private:
	/// Stored error that was accessed via `.value()` instead of `.error()`.
	E m_error;

public:
	explicit bad_expected_access(E error) : m_error(std::move(error)) {}

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	const E& error() const& { return m_error; }

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	E& error() & { return m_error; }

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	const E&& error() const&& { return std::move(m_error); }

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	E&& error() && { return std::move(m_error); }

	/**
	 * @brief
	 *  Exception message describing the access violation.
	 * @return Static message text.
	 */
	const char* what() const noexcept override { return "bad expected access"; }
};

/**
 * @brief
 *  Lightweight `std::expected<T, E>` fallback used when the standard header is unavailable.
 * @tparam T Stored value type.
 * @tparam E Stored error type.
 */
template<typename T, typename E>
class expected {
private:
	/// Active member is `m_value` when `m_has_value` is true, `m_error` otherwise.
	union {
		/// Held value (active when `m_has_value` is true).
		T m_value;
		/// Held error (active when `m_has_value` is false).
		E m_error;
	};
	/// Selects the active union member.
	bool m_has_value;

	/**
	 * @brief
	 *  Run the destructor of the active member of the union (value or error).
	 */
	void destroy() {
		if (m_has_value) {
			m_value.~T();
		} else {
			m_error.~E();
		}
	}

public:
	using value_type = T;
	using error_type = E;
	using unexpected_type = unexpected<E>;

	/**
	 * @brief
	 *  Default constructor.
	 */
	constexpr expected() : m_value(), m_has_value(true) {}

	constexpr expected(const T& v) : m_value(v), m_has_value(true) {}

	constexpr expected(T&& v) : m_value(std::move(v)), m_has_value(true) {}

	constexpr expected(const unexpected<E>& u) : m_error(u.error()), m_has_value(false) {}

	constexpr expected(unexpected<E>&& u) : m_error(std::move(u.error())), m_has_value(false) {}

	/**
	 * @brief
	 *  Destructor.
	 */
	~expected() { destroy(); }

	expected(const expected& rhs) : m_has_value(rhs.m_has_value) {
		if (m_has_value)
			new (&m_value) T(rhs.m_value);
		else
			new (&m_error) E(rhs.m_error);
	}

	/**
	 * @brief
	 *  Move constructor.
	 * @param[in,out] rhs Source expected (left in a valid-but-unspecified state).
	 */
	expected(expected&& rhs) noexcept : m_has_value(rhs.m_has_value) {
		if (m_has_value)
			new (&m_value) T(std::move(rhs.m_value));
		else
			new (&m_error) E(std::move(rhs.m_error));
	}

	/**
	 * @brief
	 *  Copy assignment.
	 * @param[in] rhs Source expected to copy from.
	 * @return A reference to this object.
	 */
	expected& operator=(const expected& rhs) {
		if (this != &rhs) {
			if (m_has_value && rhs.m_has_value) {
				m_value = rhs.m_value;
			} else if (!m_has_value && !rhs.m_has_value) {
				m_error = rhs.m_error;
			} else {
				destroy();
				m_has_value = rhs.m_has_value;
				if (m_has_value)
					new (&m_value) T(rhs.m_value);
				else
					new (&m_error) E(rhs.m_error);
			}
		}
		return *this;
	}

	/**
	 * @brief
	 *  Check whether value is present.
	 * @return True when value is present.
	 */
	constexpr bool has_value() const { return m_has_value; }

	/**
	 * @brief
	 *  Convert to bool — true when a value is present.
	 * @return True when a value is present.
	 */
	constexpr explicit operator bool() const { return m_has_value; }

	/**
	 * @brief
	 *  Access the stored value.
	 * @return The value.
	 */
	constexpr const T& value() const& {
		if (!m_has_value)
			throw bad_expected_access<E>(m_error);
		return m_value;
	}

	/**
	 * @brief
	 *  Access the stored value.
	 * @return The value.
	 */
	constexpr T& value() & {
		if (!m_has_value)
			throw bad_expected_access<E>(m_error);
		return m_value;
	}

	/**
	 * @brief
	 *  Access the stored value.
	 * @return The value.
	 */
	constexpr const T&& value() const&& {
		if (!m_has_value)
			throw bad_expected_access<E>(std::move(m_error));
		return std::move(m_value);
	}

	/**
	 * @brief
	 *  Access the stored value.
	 * @return The value.
	 */
	constexpr T&& value() && {
		if (!m_has_value)
			throw bad_expected_access<E>(std::move(m_error));
		return std::move(m_value);
	}

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	constexpr const E& error() const& { return m_error; }

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	constexpr E& error() & { return m_error; }

	/**
	 * @brief
	 *  Pointer-to-value access (assumes a value is present).
	 * @return Pointer to the stored value.
	 */
	constexpr const T* operator->() const { return &m_value; }

	/**
	 * @brief
	 *  Pointer-to-value access (assumes a value is present).
	 * @return Pointer to the stored value.
	 */
	constexpr T* operator->() { return &m_value; }

	/**
	 * @brief
	 *  Dereference access (assumes a value is present).
	 * @return Reference to the stored value.
	 */
	constexpr const T& operator*() const& { return m_value; }

	/**
	 * @brief
	 *  Dereference access (assumes a value is present).
	 * @return Reference to the stored value.
	 */
	constexpr T& operator*() & { return m_value; }

	/**
	 * @brief
	 *  Return the value when present, otherwise the supplied default.
	 * @tparam U Source type for conversion.
	 * @param[in] default_value Fallback value used when no value is held.
	 */
	template<typename U>
	constexpr T value_or(U&& default_value) const& {
		return m_has_value ? m_value : static_cast<T>(std::forward<U>(default_value));
	}

	/**
	 * @brief
	 *  Return the value when present, otherwise the supplied default (rvalue overload).
	 * @tparam U Source type for conversion.
	 * @param[in] default_value Fallback value used when no value is held.
	 * @return The stored value or the fallback.
	 */
	template<typename U>
	constexpr T value_or(U&& default_value) && {
		return m_has_value ? std::move(m_value) : static_cast<T>(std::forward<U>(default_value));
	}
};

/**
 * @brief
 *  `expected<void, E>` specialisation — value-less success/failure carrier.
 * @tparam E Error type.
 */
template<typename E>
class expected<void, E> {
private:
	/// Active member is `m_dummy` when `m_has_value` is true, `m_error` otherwise.
	union {
		/// Placeholder for the "void value" case (active when `m_has_value` is true).
		char m_dummy;
		/// Held error (active when `m_has_value` is false).
		E m_error;
	};
	/// Selects the active union member.
	bool m_has_value;

public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	constexpr expected() : m_dummy{}, m_has_value(true) {}

	constexpr expected(const unexpected<E>& u) : m_error(u.error()), m_has_value(false) {}

	constexpr expected(unexpected<E>&& u) : m_error(std::move(u.error())), m_has_value(false) {}

	/**
	 * @brief
	 *  Check whether value is present.
	 * @return True when value is present.
	 */
	constexpr bool has_value() const { return m_has_value; }

	/**
	 * @brief
	 *  Convert to bool — true when a value is present.
	 * @return True when a value is present.
	 */
	constexpr explicit operator bool() const { return m_has_value; }

	/**
	 * @brief
	 *  Throw when no value is present (no-op otherwise).
	 */
	constexpr void value() const {
		if (!m_has_value)
			throw bad_expected_access<E>(m_error);
	}

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	constexpr const E& error() const& { return m_error; }

	/**
	 * @brief
	 *  Access the stored error.
	 * @return The error.
	 */
	constexpr E& error() & { return m_error; }
};

}// namespace owl

#endif
