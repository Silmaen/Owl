/**
 * @file ParticleYaml.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "core/SerializerImpl.h"
#include "math/Curve.h"
#include "renderer/ParticleSystem.h"

namespace owl::scene::component::utils {

/**
 * @brief
 *  Write a curve under a key (nothing when it is empty).
 * @param[in,out] ioEmitter The YAML emitter.
 * @param[in] iKey The key.
 * @param[in] iCurve The curve.
 */
void emitCurve(YAML::Emitter& ioEmitter, const char* iKey, const math::Curve& iCurve);

/**
 * @brief
 *  Read a curve written by `emitCurve` (empty when the node is missing).
 * @param[in] iNode The node.
 * @param[out] oCurve The curve.
 */
void readCurve(const core::YamlNode& iNode, math::Curve& oCurve);

/**
 * @brief
 *  Write a colour gradient under a key (nothing when it is empty).
 * @param[in,out] ioEmitter The YAML emitter.
 * @param[in] iKey The key.
 * @param[in] iGradient The gradient.
 */
void emitGradient(YAML::Emitter& ioEmitter, const char* iKey, const renderer::ColorGradient& iGradient);

/**
 * @brief
 *  Read a gradient written by `emitGradient` (empty when the node is missing).
 * @param[in] iNode The node.
 * @param[out] oGradient The gradient.
 */
void readGradient(const core::YamlNode& iNode, renderer::ColorGradient& oGradient);

}// namespace owl::scene::component::utils
