/**
 * @file MeshCursor.h
 * @author Silmaen
 * @date 20/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/component/MeshComponentExtraData.h"
#include "data/geometry/MeshCursorBase.h"

namespace owl::data::geometry {

template<bool IsConst, MeshElementType ElementType, typename... Components>
class MeshRangeIterator;

/**
 * @brief Namespace for implementation details.
 */
namespace details {

/**
 * @brief
 *  Helper structure to check if the component is writable or not
 * @tparam T The component type.
 * @tparam ElementType The type of element.
 */
template<typename T, MeshElementType ElementType>
struct IsWriteExtraData
	: std::integral_constant<bool,
							 std::is_base_of_v<component::WriteMeshExtraData<typename T::DataType, ElementType>, T>> {};

/**
 * @brief
 *  Helper structure to check if the component is writable or not
 * @tparam T The component type.
 * @tparam ElementType The type of element.
 */
template<typename T, MeshElementType ElementType>
struct IsWriteExtraDataPid
	: std::integral_constant<
			  bool, std::is_base_of_v<component::WriteMeshExtraDataPid<typename T::DataType, ElementType>, T>> {};

/**
 * @brief
 *  Helper structure to initialize a mesh cursor.
 * @tparam IsConst     True if the cursor is const, false otherwise.
 * @tparam ElementType The type of element to iterate.
 * @tparam Components  The components to iterate.
 */
template<bool IsConst, MeshElementType ElementType, typename... Components>
struct OWL_API MeshCursorInitializer {
	using MeshType = MeshCursorBase<IsConst, ElementType>::MeshType;

	MeshCursorInitializer() = default;
	MeshCursorInitializer(MeshType& iMesh, const std::tuple<Components...>& iComponents) {
		prepareComponents(iMesh, std::get<Components>(iComponents)...);
	}
	/**
	 * @brief
	 *  Initialize components.
	 * @param[in] iMesh 		The cloud on which to prepare components.
	 * @param[in] iComponent 	First cloud's component to prepare.
	 * @param[in] iComponents 	Cloud's components to prepare.
	 * @tparam T The component type.
	 * @tparam Args Remaining components to prepare.
	 */
	template<typename T, typename... Args>
	void prepareComponents(MeshType& iMesh, T&& iComponent, Args&&... iComponents) {
		using RawType = std::decay_t<T>;

		if constexpr (IsWriteExtraData<RawType, ElementType>()) {
			if (isExtraDataDefined<typename RawType::DataType>(iMesh))
				addExtraData<typename RawType::DataType>(iMesh);
		} else if constexpr (IsWriteExtraDataPid<RawType, ElementType>()) {
			if (const core::FactoryPid pid = iComponent.GetPID(); core::hasFactoryPid(pid)) {
				if (!isExtraDataDefined(iMesh, pid))
					addExtraData(iMesh, pid);
			}
		}

		if constexpr (sizeof...(Args) > 0)
			prepareComponents(iMesh, std::forward<Args>(iComponents)...);
	}

	/**
	 * @brief
	 *  Check if extra data of given type is defined on all elements.
	 * @tparam DataType Type of the extra data.
	 * @param[in] iMesh The mesh to check.
	 * @return True if the extra data is defined on all elements, false otherwise.
	 */
	template<typename DataType>
	static auto isExtraDataDefined(MeshType& iMesh) -> bool {
		if constexpr (ElementType == MeshElementType::Vertex) {
			return iMesh.template isExtraDataDefinedOnAllVertices<DataType>();
		} else {
			return iMesh.template isExtraDataDefinedOnAllTriangles<DataType>();
		}
	}

	/**
	 * @brief
	 *  Add extra data of given type to all elements.
	 * @tparam DataType Type of the extra data.
	 * @param[in] iMesh The mesh to modify.
	 */
	template<typename DataType>
	static void addExtraData(MeshType& iMesh) {
		if constexpr (ElementType == MeshElementType::Vertex) {
			iMesh.template addVertexExtraData<DataType>();
		} else {
			iMesh.template addTriangleExtraData<DataType>();
		}
	}

	/**
	 * @brief
	 *  Check if extra data with given PID is defined on all elements.
	 * @param[in] iMesh The mesh to check.
	 * @param[in] iPid  The factory PID of the extra data.
	 * @return True if the extra data is defined on all elements, false otherwise.
	 */
	static auto isExtraDataDefined(MeshType& iMesh, const core::FactoryPid iPid) -> bool {
		if constexpr (ElementType == MeshElementType::Vertex) {
			return iMesh.isExtraDataDefinedOnAllVertices(iPid);
		} else {
			return iMesh.isExtraDataDefinedOnAllTriangles(iPid);
		}
	}

	/**
	 * @brief
	 *  Add extra data with given PID to all elements.
	 * @param[in] iMesh The mesh to modify.
	 * @param[in] iPid  The factory PID of the extra data.
	 */
	static void addExtraData(MeshType& iMesh, const core::FactoryPid iPid) {
		if constexpr (ElementType == MeshElementType::Vertex) {
			iMesh.addVertexExtraData(iPid);
		} else {
			iMesh.addTriangleExtraData(iPid);
		}
	}
};

}// namespace details

/**
 * @brief
 *  Cursor for mesh ranges.
 * @tparam IsConst     True if the cursor is const, false otherwise.
 * @tparam ElementType The type of element to iterate.
 * @tparam Components  The components to iterate.
 */
template<bool IsConst, MeshElementType ElementType, typename... Components>
class OWL_API MeshCursor : details::MeshCursorInitializer<IsConst, ElementType, Components...>,
						   public MeshCursorBase<IsConst, ElementType> {
	friend MeshRangeIterator<IsConst, ElementType, Components...>;
	using InitializerType = details::MeshCursorInitializer<IsConst, ElementType, Components...>;
	using CursorType = MeshCursorBase<IsConst, ElementType>;
	using MeshType = MeshCursorBase<IsConst, ElementType>::MeshType;

public:
	using ComponentsT = std::tuple<typename std::remove_cvref_t<Components>::ComponentType...>;
	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iMesh 		The mesh on which to iterate.
	 * @param[in] iIndex 		Starting index of the cursor.
	 */
	MeshCursor(MeshType& iMesh, size_t iIndex) : InitializerType(), CursorType::MeshCursorBase(iMesh, iIndex) {}

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iMesh 		The cloud on which to iterate.
	 * @param[in] iPointIndex 	Starting index of the cursor.
	 * @param[in] iComponents 	Cloud's components on which to iterate.
	*/
	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wundefined-func-template")
	MeshCursor(MeshType& iMesh, size_t iPointIndex, const std::tuple<Components...>& iComponents)
		: InitializerType(iMesh, iComponents), CursorType::MeshCursorBase(iMesh, iPointIndex),
		  m_components(std::make_optional<ComponentsT>(
				  typename Components::ComponentType(std::get<Components>(iComponents), *this)...)) {
		init();
	}
	/**
	 * @brief
	 *  Copy constructor.
	 * @param[in] iOther Cursor to copy.
	 */
	MeshCursor(const MeshCursor& iOther)
		: InitializerType(), CursorType::MeshCursorBase(iOther), m_components(iOther.m_components) {
		if (!iOther.m_components)
			return;
		std::apply([&](auto&... iComp) -> auto { (iComp.setCursor(*this), ...); }, *m_components);
		init();
	}
	OWL_DIAG_POP
	/**
	 * @brief
	 *  Move constructor.
	 */
	MeshCursor(MeshCursor&&) noexcept = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~MeshCursor() = default;

	/**
	 * @brief
	 *  Assignment operator.
	 * @param[in] iOther Cursor to copy.
	 * @return Current cursor.
	 */
	auto operator=(const MeshCursor& iOther) -> MeshCursor& {
		if (this == &iOther)
			return *this;
		CursorType::operator=(iOther);
		if (!iOther.m_components)
			return *this;

		m_components = iOther.m_components;

		std::apply([&](auto&... iComp) -> auto { (iComp.setCursor(*this), ...); }, *m_components);
		return *this;
	}

	/**
	 * @brief
	 *  Assignment operator.
	 * @return Current cursor.
	 */
	auto operator=(MeshCursor&&) noexcept -> MeshCursor& = default;

	/**
	 *  @brief
	 *  Extracts the i-th component from the tuple in the cursor.
	 *
	 *  This function is used to automatically transform the cursor to a structured binding.
	 * @tparam N Index of the component in the tuple.
	 * @return A reference to the selected component.
	 */
	template<std::size_t N>
	auto get() -> decltype(auto) {
		static_assert(sizeof...(Components) > 0);
		OWL_CORE_ASSERT(m_components, "")
		return std::get<N>(*m_components);
	}

	/**
	 * @brief
	 *  Extracts the i-th component from the tuple in the cursor.
	 *
	 *  This function is used for automatically transform the cursor to a structured binding.
	 * @tparam N Index of the component in the tuple.
	 * @return A const reference to the selected component.
	 */
	template<std::size_t N>
	auto get() const -> decltype(auto) {
		static_assert(sizeof...(Components) > 0);
		OWL_CORE_ASSERT(m_components, "")
		return std::get<N>(*m_components);
	}

private:
	/**
	 * @brief
	 *  Addition assignment operator.
	 * @param[in] iIncrement Increment.
	 * @return A reference to the current cursor.
	 */
	auto operator+=(size_t iIncrement) -> MeshCursor& {
		CursorType::moveNext(iIncrement);
		if (!m_components)
			return *this;
		return *this;
	}

	/**
	 * @brief
	 *  Initialize additional components.
	 *
	 *  If a scanning direction component is requested, create or get additional components.
	 */
	void init() {}

	/**
	 * @brief
	 *  Move to the next element.
	 * @param[in] iIncrement Increment value.
	 */
	void moveNext(size_t iIncrement = 1) { CursorType::moveNext(iIncrement); }

	/**
	 * @brief
	 *  Check if the cursor has a specific component.
	 * @tparam T Component type to find.
	 * @retval True if the component has been found.
	 * @retval False otherwise.
	 */
	template<typename T>
	static constexpr auto hasComponent() -> bool {
		return std::disjunction_v<std::is_same<Components, T>...>;
	}

	/**
	 * @brief
	 *  Get a specific component from the tuple.
	 * @tparam T Component type to find
	 * @return A reference on the component.
	 */
	template<typename T>
	auto getComponent() -> T& {
		return std::get<T>(*m_components);
	}

	/// Component's tuple.
	std::optional<ComponentsT> m_components{};
};

}// namespace owl::data::geometry

// Don't add this part to the documentation
/// @cond

namespace std {

template<typename... Components>
struct tuple_size<owl::data::geometry::MeshCursor<false, owl::data::geometry::MeshElementType::Vertex, Components...>>
	: std::integral_constant<std::size_t, sizeof...(Components)> {};

template<typename... Components>
struct tuple_size<owl::data::geometry::MeshCursor<true, owl::data::geometry::MeshElementType::Vertex, Components...>>
	: std::integral_constant<std::size_t, sizeof...(Components)> {};


template<typename... Components>
struct tuple_size<owl::data::geometry::MeshCursor<false, owl::data::geometry::MeshElementType::Triangle, Components...>>
	: std::integral_constant<std::size_t, sizeof...(Components)> {};

template<typename... Components>
struct tuple_size<owl::data::geometry::MeshCursor<true, owl::data::geometry::MeshElementType::Triangle, Components...>>
	: std::integral_constant<std::size_t, sizeof...(Components)> {};

template<std::size_t N, typename... Components>
struct tuple_element<
		N, owl::data::geometry::MeshCursor<false, owl::data::geometry::MeshElementType::Vertex, Components...>> {
	using type = std::tuple_element_t<
			N, typename owl::data::geometry::MeshCursor<false, owl::data::geometry::MeshElementType::Vertex,
														Components...>::ComponentsT>;
};

template<std::size_t N, typename... Components>
struct tuple_element<
		N, owl::data::geometry::MeshCursor<true, owl::data::geometry::MeshElementType::Vertex, Components...>> {
	using type = std::tuple_element_t<
			N, typename owl::data::geometry::MeshCursor<true, owl::data::geometry::MeshElementType::Vertex,
														Components...>::ComponentsT>;
};
template<std::size_t N, typename... Components>
struct tuple_element<
		N, owl::data::geometry::MeshCursor<false, owl::data::geometry::MeshElementType::Triangle, Components...>> {
	using type = std::tuple_element_t<
			N, typename owl::data::geometry::MeshCursor<false, owl::data::geometry::MeshElementType::Vertex,
														Components...>::ComponentsT>;
};

template<std::size_t N, typename... Components>
struct tuple_element<
		N, owl::data::geometry::MeshCursor<true, owl::data::geometry::MeshElementType::Triangle, Components...>> {
	using type = std::tuple_element_t<
			N, typename owl::data::geometry::MeshCursor<true, owl::data::geometry::MeshElementType::Vertex,
														Components...>::ComponentsT>;
};
/// @endcond

}// namespace std
