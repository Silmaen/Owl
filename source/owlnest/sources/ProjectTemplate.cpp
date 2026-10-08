/**
 * @file ProjectTemplate.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ProjectTemplate.h"

#include "Project.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

#include <algorithm>
#include <exception>
#include <system_error>

namespace owl::nest {

namespace {
constexpr const char* g_templateFile = "template.yml";
constexpr const char* g_projectFile = "owl_project.yml";
}// namespace

auto describe(const ProjectTemplateError iError) -> std::string_view {
	switch (iError) {
		case ProjectTemplateError::MissingTemplate:
			return "the template folder is missing";
		case ProjectTemplateError::DestinationNotEmpty:
			return "the destination folder is not empty";
		case ProjectTemplateError::CopyFailed:
			return "the template files cannot be copied";
		case ProjectTemplateError::InvalidProjectFile:
			return "the project file cannot be written";
	}
	return "unknown error";
}

auto listProjectTemplates(const std::filesystem::path& iRoot) -> std::vector<ProjectTemplate> {
	std::vector<ProjectTemplate> templates;
	std::error_code ec;
	if (!is_directory(iRoot, ec))
		return templates;
	for (const auto& entry: std::filesystem::directory_iterator(iRoot, ec)) {
		const auto& dir = entry.path();
		if (!entry.is_directory(ec) || !exists(dir / g_templateFile, ec) || !exists(dir / g_projectFile, ec))
			continue;
		try {
			const auto node = YAML::LoadFile((dir / g_templateFile).string())["Template"];
			templates.push_back({.id = dir.filename().string(),
								 .name = node["name"].as<std::string>(dir.filename().string()),
								 .description = node["description"].as<std::string>(""),
								 .order = node["order"].as<int>(0),
								 .directory = dir});
		} catch (const std::exception& iEx) {
			OWL_WARN("Project templates: Cannot read '{}': {}.", (dir / g_templateFile).string(), iEx.what())
		}
	}
	std::ranges::sort(templates, [](const ProjectTemplate& iLeft, const ProjectTemplate& iRight) -> bool {
		return iLeft.order != iRight.order ? iLeft.order < iRight.order : iLeft.name < iRight.name;
	});
	return templates;
}

auto createProjectFromTemplate(const ProjectTemplate& iTemplate, const std::filesystem::path& iDestination,
							   const std::string& iName) -> expected<void, ProjectTemplateError> {
	std::error_code ec;
	if (!exists(iTemplate.directory / g_projectFile, ec)) {
		OWL_WARN("New Project: Template '{}' not found in '{}'.", iTemplate.name, iTemplate.directory.string())
		return unexpected{ProjectTemplateError::MissingTemplate};
	}
	if (exists(iDestination, ec) && !is_empty(iDestination, ec)) {
		OWL_WARN("New Project: '{}' is not empty. Fix: pick a new or empty folder.", iDestination.string())
		return unexpected{ProjectTemplateError::DestinationNotEmpty};
	}
	create_directories(iDestination, ec);
	if (!ec)
		std::filesystem::copy(iTemplate.directory, iDestination, std::filesystem::copy_options::recursive, ec);
	if (ec) {
		OWL_WARN("New Project: Cannot copy template '{}' into '{}': {}.", iTemplate.name, iDestination.string(),
				 ec.message())
		return unexpected{ProjectTemplateError::CopyFailed};
	}
	remove(iDestination / g_templateFile, ec);
	Project project;
	if (!project.loadFromFile(iDestination / g_projectFile)) {
		OWL_WARN("New Project: Cannot read the project file of template '{}'.", iTemplate.name)
		return unexpected{ProjectTemplateError::InvalidProjectFile};
	}
	project.name = iName;
	if (!project.saveToFile(iDestination / g_projectFile)) {
		OWL_WARN("New Project: Cannot write the project file in '{}'.", iDestination.string())
		return unexpected{ProjectTemplateError::InvalidProjectFile};
	}
	OWL_INFO("New Project: '{}' created in '{}' from template '{}'.", iName, iDestination.string(), iTemplate.name)
	return {};
}

}// namespace owl::nest
