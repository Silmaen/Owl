/**
 * @file expected.h
 * @author Silmaen
 * @date 22/07/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl {

#ifdef __cpp_lib_expected

#include <expected>
using std::expected;

#else

#if !defined(OWL_USE_CLANG_TIDY)
#include <zeus/expected.hpp>
using zeus::expected;
using zeus::unexpected;
#else

template<typename E>
class unexpected {
private:
	E m_error;

public:
	// Constructeurs
	constexpr explicit unexpected(const E& e) : m_error(e) {}
	constexpr explicit unexpected(E&& e) : m_error(std::move(e)) {}

	// Accesseurs
	constexpr const E& error() const& { return m_error; }
	constexpr E& error() & { return m_error; }
	constexpr const E&& error() const&& { return std::move(m_error); }
	constexpr E&& error() && { return std::move(m_error); }

	// Opérateurs de comparaison
	template<typename G>
	friend constexpr bool operator==(const unexpected& lhs, const unexpected<G>& rhs) {
		return lhs.error() == rhs.error();
	}
};

// Tag pour les constructeurs d'erreur
struct unexpect_t {};
inline constexpr unexpect_t unexpect{};

// Exception levée lors de l'accès à un expected sans valeur
template<typename E>
class bad_expected_access : public std::exception {
private:
	E m_error;

public:
	explicit bad_expected_access(E error) : m_error(std::move(error)) {}

	const E& error() const& { return m_error; }
	E& error() & { return m_error; }
	const E&& error() const&& { return std::move(m_error); }
	E&& error() && { return std::move(m_error); }

	const char* what() const noexcept override { return "bad expected access"; }
};

// Classe principale expected<T, E>
template<typename T, typename E>
class expected {
private:
	union {
		T m_value;
		E m_error;
	};
	bool m_has_value;

	void destroy() {
		if (m_has_value) {
			m_value.~T();
		} else {
			m_error.~E();
		}
	}

public:
	// Types
	using value_type = T;
	using error_type = E;
	using unexpected_type = unexpected<E>;

	// Constructeurs
	constexpr expected() : m_value(), m_has_value(true) {}

	constexpr expected(const T& v) : m_value(v), m_has_value(true) {}

	constexpr expected(T&& v) : m_value(std::move(v)), m_has_value(true) {}

	constexpr expected(const unexpected<E>& u) : m_error(u.error()), m_has_value(false) {}

	constexpr expected(unexpected<E>&& u) : m_error(std::move(u.error())), m_has_value(false) {}

	// Destructeur
	~expected() { destroy(); }

	// Constructeur de copie
	expected(const expected& rhs) : m_has_value(rhs.m_has_value) {
		if (m_has_value)
			new (&m_value) T(rhs.m_value);
		else
			new (&m_error) E(rhs.m_error);
	}

	// Constructeur de déplacement
	expected(expected&& rhs) noexcept : m_has_value(rhs.m_has_value) {
		if (m_has_value)
			new (&m_value) T(std::move(rhs.m_value));
		else
			new (&m_error) E(std::move(rhs.m_error));
	}

	// Opérateur d'affectation
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

	// Accesseurs
	constexpr bool has_value() const { return m_has_value; }
	constexpr explicit operator bool() const { return m_has_value; }

	constexpr const T& value() const& {
		if (!m_has_value)
			throw bad_expected_access<E>(m_error);
		return m_value;
	}

	constexpr T& value() & {
		if (!m_has_value)
			throw bad_expected_access<E>(m_error);
		return m_value;
	}

	constexpr const T&& value() const&& {
		if (!m_has_value)
			throw bad_expected_access<E>(std::move(m_error));
		return std::move(m_value);
	}

	constexpr T&& value() && {
		if (!m_has_value)
			throw bad_expected_access<E>(std::move(m_error));
		return std::move(m_value);
	}

	constexpr const E& error() const& { return m_error; }
	constexpr E& error() & { return m_error; }

	// Opérateurs d'accès
	constexpr const T* operator->() const { return &m_value; }
	constexpr T* operator->() { return &m_value; }
	constexpr const T& operator*() const& { return m_value; }
	constexpr T& operator*() & { return m_value; }

	// Méthode utilitaire
	template<typename U>
	constexpr T value_or(U&& default_value) const& {
		return m_has_value ? m_value : static_cast<T>(std::forward<U>(default_value));
	}

	template<typename U>
	constexpr T value_or(U&& default_value) && {
		return m_has_value ? std::move(m_value) : static_cast<T>(std::forward<U>(default_value));
	}
};

// Spécialisation pour void
template<typename E>
class expected<void, E> {
private:
	union {
		char m_dummy;
		E m_error;
	};
	bool m_has_value;

public:
	// Constructeurs
	constexpr expected() : m_dummy{}, m_has_value(true) {}

	constexpr expected(const unexpected<E>& u) : m_error(u.error()), m_has_value(false) {}

	constexpr expected(unexpected<E>&& u) : m_error(std::move(u.error())), m_has_value(false) {}

	// Accesseurs
	constexpr bool has_value() const { return m_has_value; }
	constexpr explicit operator bool() const { return m_has_value; }

	constexpr void value() const {
		if (!m_has_value)
			throw bad_expected_access<E>(m_error);
	}

	constexpr const E& error() const& { return m_error; }
	constexpr E& error() & { return m_error; }
};

#endif
// NOLINTEND (clang-tidy suppression)


#endif

}// namespace owl
