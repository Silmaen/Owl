/**
 * @file FileDialog.cpp
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "platform/FileDialog.h"

#include "core/Log.h"

// An Android game has no file dialogue: every request is cancelled.
namespace owl::platform {

auto FileDialog::openFile([[maybe_unused]] const std::string& iFilter) -> std::filesystem::path {
	OWL_CORE_WARN("FileDialog: No file dialogue on Android.")
	return {};
}

auto FileDialog::saveFile([[maybe_unused]] const std::string& iFilter, [[maybe_unused]] const std::string& iDefaultName)
		-> std::filesystem::path {
	OWL_CORE_WARN("FileDialog: No file dialogue on Android.")
	return {};
}

auto FileDialog::pickFolder() -> std::filesystem::path {
	OWL_CORE_WARN("FileDialog: No file dialogue on Android.")
	return {};
}

}// namespace owl::platform
