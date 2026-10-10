/**
 * @file FormatVersion.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/FormatVersion.h"
#include "core/FormatVersionYaml.h"
#include "core/SerializerImpl.h"

#include <cstdint>
#include <exception>
#include <limits>

namespace owl::core {

namespace {

constexpr const char* g_versionKey = "FormatVersion";
static_assert(std::string_view{g_versionKey} == g_FormatVersionKey);

auto readVersionNode(const YAML::Node& iRoot) -> expected<uint32_t, FormatError> {
	if (!iRoot.IsMap() || !iRoot[g_versionKey])
		return g_UnversionedFormat;
	const auto node = iRoot[g_versionKey];
	int64_t value = 0;
	if (!node.IsScalar() || !YAML::convert<int64_t>::decode(node, value) || value < 1 ||
		value > std::numeric_limits<uint32_t>::max()) {
		OWL_CORE_ERROR("FormatVersion: Invalid {} value '{}'.", g_FormatVersionKey,
					   node.IsScalar() ? node.Scalar() : std::string{"<not a scalar>"})
		return unexpected{FormatError::InvalidVersion};
	}
	return static_cast<uint32_t>(value);
}

auto runStep(const MigrationStep iStep, YAML::Node& ioRoot) -> bool {
	try {
		const Serializer document;
		document.getImpl()->document.reset(ioRoot);
		const bool done = iStep(document);
		ioRoot.reset(document.getImpl()->document);
		return done;
	} catch (const std::exception& iEx) {
		OWL_CORE_ERROR("FormatVersion: Migration step threw: {}.", iEx.what())
		return false;
	} catch (...) {
		OWL_CORE_ERROR("FormatVersion: Migration step threw an unknown exception.")
		return false;
	}
}

}// namespace

auto describe(const FormatError iError) -> std::string_view {
	switch (iError) {
		case FormatError::InvalidVersion:
			return "its format version is not a positive integer";
		case FormatError::NewerVersion:
			return "the file was created by a newer version of Owl";
		case FormatError::MigrationFailed:
			return "it could not be migrated to the current format";
	}
	return "unknown error";
}

void emitFormatVersion(YAML::Emitter& ioEmitter, const DocumentFormat& iFormat) {
	ioEmitter << YAML::Key << g_versionKey << YAML::Value << iFormat.currentVersion();
}

auto readFormatVersion(const Serializer& iDocument) -> expected<uint32_t, FormatError> {
	return readVersionNode(iDocument.getImpl()->document);
}

auto upgradeYamlDocument(const DocumentFormat& iFormat, YAML::Node& ioRoot, const std::string_view iSourceName)
		-> expected<uint32_t, FormatError> {
	const auto version = readVersionNode(ioRoot);
	if (!version) {
		OWL_CORE_ERROR("FormatVersion: {} '{}' cannot be loaded: {}.", iFormat.name, iSourceName,
					   describe(version.error()))
		return version;
	}
	const uint32_t current = iFormat.currentVersion();
	if (*version > current) {
		OWL_CORE_ERROR("FormatVersion: {} '{}' cannot be loaded: {} (format {}, this build reads up to {}).",
					   iFormat.name, iSourceName, describe(FormatError::NewerVersion), *version, current)
		return unexpected{FormatError::NewerVersion};
	}
	for (uint32_t from = *version; from < current; ++from) {
		if (!runStep(iFormat.migrations[from - 1], ioRoot)) {
			OWL_CORE_ERROR("FormatVersion: {} '{}' cannot be loaded: migration {} -> {} failed.", iFormat.name,
						   iSourceName, from, from + 1)
			return unexpected{FormatError::MigrationFailed};
		}
	}
	if (*version < current) {
		ioRoot[g_versionKey] = current;
		OWL_CORE_INFO("FormatVersion: {} '{}' migrated from format {} to {}.", iFormat.name, iSourceName, *version,
					  current)
	}
	return version;
}

auto upgradeDocument(const DocumentFormat& iFormat, const Serializer& ioDocument, const std::string_view iSourceName)
		-> expected<uint32_t, FormatError> {
	return upgradeYamlDocument(iFormat, ioDocument.getImpl()->document, iSourceName);
}

auto upgradeDocumentText(const DocumentFormat& iFormat, std::string& ioYaml, const std::string_view iSourceName)
		-> expected<uint32_t, FormatError> {
	YAML::Node root;
	try {
		root = YAML::Load(ioYaml);
	} catch (const std::exception& iEx) {
		OWL_CORE_ERROR("FormatVersion: {} '{}' is not valid YAML: {}.", iFormat.name, iSourceName, iEx.what())
		return unexpected{FormatError::InvalidVersion};
	}
	const auto version = upgradeYamlDocument(iFormat, root, iSourceName);
	if (version && *version < iFormat.currentVersion()) {
		YAML::Emitter out;
		out << root;
		ioYaml = out.c_str();
	}
	return version;
}

}// namespace owl::core
