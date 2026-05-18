/**
 * @file ComputeShader.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/ComputeShader.h"

namespace owl::renderer::gpu::null {

/**
 * @brief
 *  Null-backend compute shader stub — all operations are no-ops. Used by
 *  headless tests and the renderer fallback when no GPU API is available.
 */
class OWL_API ComputeShader final : public renderer::gpu::ComputeShader {
public:
	ComputeShader(const ComputeShader&) = delete;

	ComputeShader(ComputeShader&&) = delete;

	auto operator=(const ComputeShader&) -> ComputeShader& = delete;

	auto operator=(ComputeShader&&) -> ComputeShader& = delete;

	/**
	 * @brief
	 *  Stub constructor.
	 */
	ComputeShader() = default;

	/**
	 * @brief
	 *  Destructor (defined out-of-line in the .cpp to anchor the vtable).
	 */
	~ComputeShader() override;

	/**
	 * @brief
	 *  No-op SSBO bind.
	 */
	void bindStorageBuffer(uint32_t iBinding, const shared<renderer::gpu::StorageBuffer>& iBuffer) override;

	/**
	 * @brief
	 *  No-op dispatch.
	 */
	void dispatch(uint32_t iGroupsX, uint32_t iGroupsY, uint32_t iGroupsZ) override;
};

}// namespace owl::renderer::gpu::null
