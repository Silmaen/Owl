/**
 * @file yaml.h
 * @author Silmaen
 * @date 15/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

template<class T>
void get(const YAML::Node& iNode, const std::string& iKey, T& oValue) {
	if (const auto val = iNode[iKey]; val)
		oValue = val.as<T>();
}
