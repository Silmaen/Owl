/**
 * @file Framebuffer.cpp
 * @author Silmaen
 * @date 21/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "Framebuffer.h"
#include "core/external/opengl46.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace owl::renderer::gpu::opengl {

namespace {
constexpr uint32_t g_maxFramebufferSize = 8192;

}// namespace

namespace utils {

namespace {
auto textureTarget(const bool iMultisampled) -> GLenum {
	return iMultisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
}

void createTextures(const bool iMultisampled, uint32_t* oId, const uint32_t iCount) {
	glCreateTextures(textureTarget(iMultisampled), static_cast<GLsizei>(iCount), oId);
}

void bindTexture(const bool iMultisampled, const uint32_t iId) { glBindTexture(textureTarget(iMultisampled), iId); }

void attachColorTexture(const uint32_t iId, const int iSamples, const GLenum iInternalFormat, const GLenum iFormat,
						const uint32_t iWidth, const uint32_t iHeight, const int iIndex) {
	const bool multisampled = iSamples > 1;
	if (multisampled) {
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, iSamples, iInternalFormat, static_cast<GLsizei>(iWidth),
								static_cast<GLsizei>(iHeight), GL_FALSE);
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(iInternalFormat), static_cast<GLsizei>(iWidth),
					 static_cast<GLsizei>(iHeight), 0, iFormat, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + iIndex, textureTarget(multisampled), iId, 0);
}

void attachDepthTexture(const uint32_t iId, const int iSamples, const GLenum iFormat, const GLenum iAttachmentType,
						const uint32_t iWidth, const uint32_t iHeight) {
	const bool multisampled = iSamples > 1;
	if (multisampled) {
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, iSamples, iFormat, static_cast<GLsizei>(iWidth),
								static_cast<GLsizei>(iHeight), GL_FALSE);
	} else {
		glTexStorage2D(GL_TEXTURE_2D, 1, iFormat, static_cast<GLsizei>(iWidth), static_cast<GLsizei>(iHeight));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	glFramebufferTexture2D(GL_FRAMEBUFFER, iAttachmentType, textureTarget(multisampled), iId, 0);
}

auto isDepthFormat(const AttachmentSpecification::Format iFormat) -> bool {
	switch (iFormat) {
		case AttachmentSpecification::Format::Depth24Stencil8:
			return true;
		case AttachmentSpecification::Format::None:
		case AttachmentSpecification::Format::Rgba8:
		case AttachmentSpecification::Format::RedInteger:
		case AttachmentSpecification::Format::Surface:
			return false;
	}
	return false;
}

auto fbTextureFormatToGl(const AttachmentSpecification::Format iFormat) -> GLenum {
	switch (iFormat) {
		case AttachmentSpecification::Format::Rgba8:
		case AttachmentSpecification::Format::Surface:
			return GL_RGBA8;
		case AttachmentSpecification::Format::RedInteger:
			return GL_RED_INTEGER;
		case AttachmentSpecification::Format::None:
		case AttachmentSpecification::Format::Depth24Stencil8:
			break;
	}
	OWL_CORE_ASSERT(false, "Bad Texture format")
	return 0;
}
}// namespace

}// namespace utils

Framebuffer::Framebuffer(FramebufferSpecification iSpec) : m_specs{std::move(iSpec)} {
	for (auto spec: m_specs.attachments) {
		if (!utils::isDepthFormat(spec.format))
			m_colorAttachmentSpecifications.emplace_back(spec);
		else
			m_depthAttachmentSpecification = spec;
	}

	invalidate();
}

Framebuffer::~Framebuffer() {
	for (auto& read: m_pixelReads) {
		if (read.fence != nullptr)
			glDeleteSync(static_cast<GLsync>(read.fence));
		if (read.buffer != 0)
			glDeleteBuffers(1, &read.buffer);
	}
	glDeleteFramebuffers(1, &m_rendererId);
	glDeleteTextures(static_cast<GLsizei>(m_colorAttachments.size()), m_colorAttachments.data());
	glDeleteTextures(1, &m_depthAttachment);
}

void Framebuffer::invalidate() {
	if (m_rendererId > 0) {
		glDeleteFramebuffers(1, &m_rendererId);
		glDeleteTextures(static_cast<GLsizei>(m_colorAttachments.size()), m_colorAttachments.data());
		glDeleteTextures(1, &m_depthAttachment);
		m_colorAttachments.clear();
		m_depthAttachment = 0;
	}

	glCreateFramebuffers(1, &m_rendererId);
	glBindFramebuffer(GL_FRAMEBUFFER, m_rendererId);

	const bool multisample = m_specs.samples > 1;

	// Attachments
	if (!m_colorAttachmentSpecifications.empty()) {
		m_colorAttachments.resize(m_colorAttachmentSpecifications.size());
		utils::createTextures(multisample, m_colorAttachments.data(), static_cast<uint32_t>(m_colorAttachments.size()));

		for (size_t i = 0; i < m_colorAttachments.size(); i++) {
			utils::bindTexture(multisample, m_colorAttachments[i]);
			switch (m_colorAttachmentSpecifications[i].format) {
				case AttachmentSpecification::Format::Rgba8:
				case AttachmentSpecification::Format::Surface:
					utils::attachColorTexture(m_colorAttachments[i], static_cast<int>(m_specs.samples), GL_RGBA8,
											  GL_RGBA, m_specs.size.x(), m_specs.size.y(), static_cast<int>(i));
					break;
				case AttachmentSpecification::Format::RedInteger:
					utils::attachColorTexture(m_colorAttachments[i], static_cast<int>(m_specs.samples), GL_R32I,
											  GL_RED_INTEGER, m_specs.size.x(), m_specs.size.y(), static_cast<int>(i));
					break;
				case AttachmentSpecification::Format::None:
				case AttachmentSpecification::Format::Depth24Stencil8:
					break;
			}
		}
	}

	if (m_depthAttachmentSpecification.format != AttachmentSpecification::Format::None) {
		utils::createTextures(multisample, &m_depthAttachment, 1);
		utils::bindTexture(multisample, m_depthAttachment);
		switch (m_depthAttachmentSpecification.format) {
			case AttachmentSpecification::Format::Depth24Stencil8:
				utils::attachDepthTexture(m_depthAttachment, static_cast<int>(m_specs.samples), GL_DEPTH24_STENCIL8,
										  GL_DEPTH_STENCIL_ATTACHMENT, m_specs.size.x(), m_specs.size.y());
				break;
			case AttachmentSpecification::Format::None:
			case AttachmentSpecification::Format::Rgba8:
			case AttachmentSpecification::Format::RedInteger:
			case AttachmentSpecification::Format::Surface:
				break;
		}
	}

	if (m_colorAttachments.size() > 1) {
		OWL_CORE_ASSERT(m_colorAttachments.size() <= 4, "Bad color attachment size")
		constexpr GLenum buffers[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
									   GL_COLOR_ATTACHMENT3};
		glDrawBuffers(static_cast<GLsizei>(m_colorAttachments.size()), buffers);
	} else if (m_colorAttachments.empty()) {
		// Only depth-pass
		glDrawBuffer(GL_NONE);
	}

	const bool completeFramebuffer = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	OWL_CORE_ASSERT(completeFramebuffer, "Framebuffer is incomplete!")
	if (!completeFramebuffer) {
		OWL_CORE_WARN("Incomplete Framebuffer.")
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::bind() {
	glBindFramebuffer(GL_FRAMEBUFFER, m_rendererId);
	glViewport(0, 0, static_cast<GLsizei>(m_specs.size.x()), static_cast<GLsizei>(m_specs.size.y()));
}

void Framebuffer::unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void Framebuffer::resize(const math::vec2ui iSize) {
	if (iSize.x() == 0 || iSize.y() == 0 || iSize.x() > g_maxFramebufferSize || iSize.y() > g_maxFramebufferSize) {
		OWL_CORE_WARN("Attempt to resize frame buffer to {} {}.", iSize.x(), iSize.y())
		return;
	}
	m_specs.size = iSize;
	invalidate();
}

auto Framebuffer::readPixel(const uint32_t iAttachmentIndex, const int iX, const int iY) -> int {
	OWL_CORE_ASSERT(iAttachmentIndex < m_colorAttachments.size(), "ReadPixel bad attachment index")

	for (auto& read: m_pixelReads) {
		if (read.fence == nullptr)
			continue;
		if (const GLenum status = glClientWaitSync(static_cast<GLsync>(read.fence), 0, 0);
			status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED)
			continue;
		glDeleteSync(static_cast<GLsync>(read.fence));
		read.fence = nullptr;
		if (read.order > m_pixelValueOrder) {
			glGetNamedBufferSubData(read.buffer, 0, sizeof(int), &m_pixelValue);
			m_pixelValueOrder = read.order;
		}
	}
	auto* const slot =
			std::ranges::find_if(m_pixelReads, [](const PixelRead& iRead) -> bool { return iRead.fence == nullptr; });
	if (slot == m_pixelReads.end())
		return m_pixelValue;
	if (slot->buffer == 0) {
		glCreateBuffers(1, &slot->buffer);
		glNamedBufferData(slot->buffer, sizeof(int), nullptr, GL_STREAM_READ);
	}
	glReadBuffer(GL_COLOR_ATTACHMENT0 + iAttachmentIndex);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, slot->buffer);
	glReadPixels(iX, iY, 1, 1, GL_RED_INTEGER, GL_INT, nullptr);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	slot->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	slot->order = ++m_pixelReadCount;
	return m_pixelValue;
}

auto Framebuffer::readColorAttachment(const uint32_t iAttachmentIndex) -> std::vector<uint8_t> {
	if (iAttachmentIndex >= m_colorAttachments.size()) {
		OWL_CORE_WARN("OpenGL Framebuffer: No colour attachment {} to read back.", iAttachmentIndex)
		return {};
	}
	if (const auto format = m_colorAttachmentSpecifications[iAttachmentIndex].format;
		format != AttachmentSpecification::Format::Surface && format != AttachmentSpecification::Format::Rgba8) {
		OWL_CORE_WARN("OpenGL Framebuffer: Attachment {} is not an 8-bit colour attachment.", iAttachmentIndex)
		return {};
	}
	const auto width = static_cast<size_t>(m_specs.size.x());
	const auto height = static_cast<size_t>(m_specs.size.y());
	const size_t rowSize = width * 4;
	std::vector<uint8_t> pixels(rowSize * height);
	GLint previous = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_rendererId);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + iAttachmentIndex);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE,
				 pixels.data());
	glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous));
	// OpenGL stores the bottom row first.
	std::vector<uint8_t> flipped(pixels.size());
	for (size_t row = 0; row < height; ++row)
		std::ranges::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(row * rowSize),
							static_cast<std::ptrdiff_t>(rowSize),
							flipped.begin() + static_cast<std::ptrdiff_t>((height - 1 - row) * rowSize));
	return flipped;
}

void Framebuffer::clearAttachment(const uint32_t iAttachmentIndex, const int iValue) {
	OWL_CORE_ASSERT(iAttachmentIndex < m_colorAttachments.size(), "clearAttachment bad attachment index")
	const auto& spec = m_colorAttachmentSpecifications[iAttachmentIndex];
	glClearTexImage(m_colorAttachments[iAttachmentIndex], 0, utils::fbTextureFormatToGl(spec.format), GL_INT, &iValue);
}

}// namespace owl::renderer::gpu::opengl
