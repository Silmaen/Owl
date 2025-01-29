/**
 * @file FileUtils.h
 * @author Silmaen
 * @date 30/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include <filesystem>

namespace owl::core::utils {

/**
 * @brief Reads a text file and return its content as a string.
 * @param[in] iFile The file to read.
 * @return The content of the file.
 */
auto OWL_API fileToString(const std::filesystem::path& iFile) -> std::string;

}// namespace owl::core::utils
