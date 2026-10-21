/**
 * @file PhysicalLightMigration.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "PhysicalLightMigration.h"

#include "core/SerializerImpl.h"

#include <numbers>
#include <string>

namespace owl::scene {

namespace {

// Radiance of a factor of 1 before the physical units, divided by the exposure of EV100 15 (1 / (1.2 * 2^15)).
constexpr float g_factorToPhysical = 1.2f * 32768.f;
constexpr float g_pi = std::numbers::pi_v<float>;

void scaleIntensity(YAML::Node ioComponent, const float iScale) {
	const float factor = ioComponent["intensity"] ? ioComponent["intensity"].as<float>() : 1.f;
	ioComponent["intensity"] = factor * iScale;
}

}// namespace

auto convertToPhysicalLights(const core::Serializer& ioDocument) -> bool {
	const auto entities = ioDocument.getImpl()->document["Entities"];
	if (!entities || !entities.IsSequence())
		return true;
	for (auto entity: entities) {
		if (!entity.IsMap())
			continue;
		if (auto light = entity["Light"]; light && light.IsMap()) {
			const std::string type = light["type"] ? light["type"].as<std::string>("") : "Directional";
			// The old lobe was scaled by pi and a point light did not fall off: this matches it at one metre.
			if (type == "Point")
				scaleIntensity(light, g_pi * 4.f * g_pi * g_factorToPhysical);
			else if (type == "Spot")
				scaleIntensity(light, g_pi * g_pi * g_factorToPhysical);
			else
				scaleIntensity(light, g_pi * g_factorToPhysical);
		}
		if (auto environment = entity["Environment"]; environment && environment.IsMap())
			scaleIntensity(environment, g_factorToPhysical);
	}
	return true;
}

auto hasLegacyLight(const core::YamlNode& iEntity) -> bool {
	return iEntity.isMap() && (iEntity["Light"].isMap() || iEntity["Environment"].isMap());
}

}// namespace owl::scene
