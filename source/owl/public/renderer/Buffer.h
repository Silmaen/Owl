/**
 * @file Buffer.h
 * @author Silmaen
 * @date 08/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::renderer {
/**
 * @brief Type of data.
 */
enum class ShaderDataType : uint8_t {
	None = 0,
	Float,
	Float2,
	Float3,
	Float4,
	Mat3,
	Mat4,
	Int,
	Int2,
	Int3,
	Int4,
	Bool
};

static auto shaderDataTypeSize(const ShaderDataType iType) -> uint32_t {
	switch (iType) {
		case ShaderDataType::Float:
			return 4;
		case ShaderDataType::Float2:
			return 4 * 2;
		case ShaderDataType::Float3:
			return 4 * 3;
		case ShaderDataType::Float4:
			return 4 * 4;
		case ShaderDataType::Mat3:
			return 4 * 3 * 3;
		case ShaderDataType::Mat4:
			return 4 * 4 * 4;
		case ShaderDataType::Int:
			return 4;
		case ShaderDataType::Int2:
			return 4 * 2;
		case ShaderDataType::Int3:
			return 4 * 3;
		case ShaderDataType::Int4:
			return 4 * 4;
		case ShaderDataType::Bool:
			return 1;
		case ShaderDataType::None:
			return 0;
	}
	OWL_CORE_ERROR("shaderDataTypeSize: Unknown ShaderDataType!")
	return 0;
}

/**
 * @brief Class for buffer element.
 */
struct OWL_API BufferElement {
	/// Element Name.
	std::string name;
	/// Data's type.
	ShaderDataType type;
	/// Data's size.
	uint32_t size{0};
	/// Data's offset.
	uint32_t offset{0};
	/// If data's normalized.
	bool normalized = false;

	/**
	 * @brief Constructor.
	 * @param[in] iName Element Name.
	 * @param[in] iType Data's type.
	 * @param[in] iNormalized If data's normalized.
	 */
	BufferElement(std::string&& iName, const ShaderDataType iType, const bool iNormalized = false)
		: name(std::move(iName)), type(iType), size(shaderDataTypeSize(iType)), normalized(iNormalized) {}

	/**
	 * @brief Get component's count.
	 * @return Component's count.
	 */
	[[nodiscard]] auto getComponentCount() const -> uint32_t {
		switch (type) {
			case ShaderDataType::Float:
				return 1;
			case ShaderDataType::Float2:
				return 2;
			case ShaderDataType::Float3:
				return 3;
			case ShaderDataType::Float4:
				return 4;
			case ShaderDataType::Mat3:
				return 3 * 3;
			case ShaderDataType::Mat4:
				return 4 * 4;
			case ShaderDataType::Int:
				return 1;
			case ShaderDataType::Int2:
				return 2;
			case ShaderDataType::Int3:
				return 3;
			case ShaderDataType::Int4:
				return 4;
			case ShaderDataType::Bool:
				return 1;
			case ShaderDataType::None:
				return 0;
		}
		OWL_CORE_ERROR("getComponentCount: Unknown ShaderDataType!")
		return 0;
	}
};

/**
 * @brief Class Describing the buffer layout.
 */
class OWL_API BufferLayout {
public:
	using element_type = std::vector<BufferElement>;
	using iterator = element_type::iterator;
	using const_iterator = element_type::const_iterator;

	BufferLayout() = default;
	/**
	 * @brief Constructor.
	 * @param[in] iElements Elements in the layout.
	 */
	BufferLayout(const std::initializer_list<BufferElement>& iElements) : m_elements(iElements) {
		calculateOffsetsAndStride();
	}

	/**
	 * @brief Get buffer stride.
	 * @return The buffer stride.
	 */
	[[nodiscard]] auto getStride() const -> uint32_t { return m_stride; }

	/**
	 * @brief Get the buffer Elements.
	 * @return Buffer elements.
	 */
	[[nodiscard]] auto getElements() const -> const std::vector<BufferElement>& { return m_elements; }

	[[nodiscard]] auto begin() -> iterator { return m_elements.begin(); }
	[[nodiscard]] auto end() -> iterator { return m_elements.end(); }
	[[nodiscard]] auto begin() const -> const_iterator { return m_elements.begin(); }
	[[nodiscard]] auto end() const -> const_iterator { return m_elements.end(); }

private:
	/// List of element in the buffer data.
	element_type m_elements;
	/// Stride of the data.
	uint32_t m_stride = 0;
	/**
	 * @brief Automate computation of the offsets and stride.
	 */
	void calculateOffsetsAndStride() {
		uint32_t offset = 0;
		m_stride = 0;
		for (auto& element: m_elements) {
			element.offset = offset;
			offset += element.size;
			m_stride += element.size;
		}
	}
};

/**
 * @brief Abstract class for managing vertex buffer.
 */
class OWL_API VertexBuffer {
public:
	VertexBuffer(const VertexBuffer&) = default;
	VertexBuffer(VertexBuffer&&) = default;
	auto operator=(const VertexBuffer&) -> VertexBuffer& = default;
	auto operator=(VertexBuffer&&) -> VertexBuffer& = default;
	VertexBuffer() = default;// ---UNCOVER---
	/**
	 * @brief Destructor.
	 */
	virtual ~VertexBuffer();

	/**
	 * @brief Activate the buffer in the GPU.
	 */
	virtual void bind() const = 0;

	/**
	 * @brief Deactivate the buffer in the GPU.
	 */
	virtual void unbind() const = 0;

	/**
	 * @brief Defines the data of the vertex buffer.
	 * @param[in] iData The raw data.
	 * @param[in] iSize Number of data.
	 */
	virtual void setData(const void* iData, uint32_t iSize) = 0;

	/**
	 * @brief Get the buffer data layout.
	 * @return Data layout.
	 */
	[[nodiscard]] auto getLayout() const -> const BufferLayout& { return m_layout; }

	/**
	 * @brief Define the data layout.
	 * @param[in] iLayout New data layout.
	 */
	void setLayout(const BufferLayout& iLayout) { m_layout = iLayout; }

private:
	/// Data layout description.
	BufferLayout m_layout;
};

/**
 * @brief Abstract class for managing index buffer.
 */
class OWL_API IndexBuffer {
public:
	IndexBuffer(const IndexBuffer&) = default;
	IndexBuffer(IndexBuffer&&) = default;
	auto operator=(const IndexBuffer&) -> IndexBuffer& = default;
	auto operator=(IndexBuffer&&) -> IndexBuffer& = default;
	/**
	 * @brief Default constructor.
	 */
	IndexBuffer() = default;// ---UNCOVER---
	/**
	 * @brief Destructor.
	 */
	virtual ~IndexBuffer();
	/**
	 * @brief Activate the buffer in the GPU.
	 */
	virtual void bind() const = 0;

	/**
	 * @brief Deactivate the buffer in the GPU.
	 */
	virtual void unbind() const = 0;

	/**
	 * @brief Get the number of element in the buffer.
	 * @return Number of element in the buffer.
	 */
	[[nodiscard]] virtual auto getCount() const -> uint32_t = 0;
};
}// namespace owl::renderer
