/**
 * @file PluginCommands.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "PluginCommands.h"

#include <core/Log.h>
#include <platform/AtomicFile.h>

#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace owl::nest::commands {

PluginDocumentEditor::~PluginDocumentEditor() = default;

WriteTextFileCommand::WriteTextFileCommand(std::filesystem::path iFile, std::optional<std::string> iBefore,
										   std::string iAfter)
	: m_file{std::move(iFile)}, m_before{std::move(iBefore)}, m_after{std::move(iAfter)} {}

WriteTextFileCommand::~WriteTextFileCommand() = default;

void WriteTextFileCommand::undo(scene::Scene& /*ioScene*/) {
	if (!m_before.has_value()) {
		std::error_code error;
		if (!std::filesystem::remove(m_file, error))
			OWL_WARN("Plugins: Cannot remove '{}'.", m_file.string())
		return;
	}
	if (const auto written = platform::writeFileAtomic(m_file, *m_before); !written)
		OWL_WARN("Plugins: Cannot write '{}': {}.", m_file.string(), platform::describe(written.error()))
}

void WriteTextFileCommand::redo(scene::Scene& /*ioScene*/) {
	if (const auto written = platform::writeFileAtomic(m_file, m_after); !written)
		OWL_WARN("Plugins: Cannot write '{}': {}.", m_file.string(), platform::describe(written.error()))
}

auto WriteTextFileCommand::description() const -> std::string {
	return std::format("Import `{}`", m_file.filename().string());
}

}// namespace owl::nest::commands
