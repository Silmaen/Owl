/**
 * @file Camera.cpp
 * @author Silmaen
 * @date 1/28/25
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/Camera.h"

namespace owl::scene::component {

namespace {

template<typename T>
void readInto(const core::YamlNode& iNode, const char* iKey, T& oValue) {
	if (const auto value = iNode[iKey]; value && value.isScalar())
		oValue = value.as<T>();
}

void writePostProcess(YAML::Emitter& iOut, const renderer::PostProcessSettings& iSettings) {
	iOut << YAML::Key << "postProcess" << YAML::Value << YAML::BeginMap;
	iOut << YAML::Key << "ev100" << YAML::Value << iSettings.ev100;
	iOut << YAML::Key << "autoExposure" << YAML::Value << iSettings.autoExposure;
	iOut << YAML::Key << "autoExposureMin" << YAML::Value << iSettings.autoExposureMin;
	iOut << YAML::Key << "autoExposureMax" << YAML::Value << iSettings.autoExposureMax;
	iOut << YAML::Key << "autoExposureSpeed" << YAML::Value << iSettings.autoExposureSpeed;
	iOut << YAML::Key << "exposure" << YAML::Value << iSettings.exposure;
	iOut << YAML::Key << "bloom" << YAML::Value << iSettings.bloom;
	iOut << YAML::Key << "bloomThreshold" << YAML::Value << iSettings.bloomThreshold;
	iOut << YAML::Key << "bloomKnee" << YAML::Value << iSettings.bloomKnee;
	iOut << YAML::Key << "bloomIntensity" << YAML::Value << iSettings.bloomIntensity;
	iOut << YAML::Key << "bloomScatter" << YAML::Value << iSettings.bloomScatter;
	iOut << YAML::Key << "vignette" << YAML::Value << iSettings.vignette;
	iOut << YAML::Key << "vignetteIntensity" << YAML::Value << iSettings.vignetteIntensity;
	iOut << YAML::Key << "vignetteSmoothness" << YAML::Value << iSettings.vignetteSmoothness;
	iOut << YAML::Key << "vignetteColor" << YAML::Value << YAML::Flow << YAML::BeginSeq << iSettings.vignetteColor.x()
		 << iSettings.vignetteColor.y() << iSettings.vignetteColor.z() << YAML::EndSeq;
	iOut << YAML::Key << "lut" << YAML::Value << iSettings.lut;
	iOut << YAML::Key << "lutContribution" << YAML::Value << iSettings.lutContribution;
	iOut << YAML::Key << "chromaticAberration" << YAML::Value << iSettings.chromaticAberration;
	iOut << YAML::Key << "chromaticAberrationIntensity" << YAML::Value << iSettings.chromaticAberrationIntensity;
	iOut << YAML::Key << "filmGrain" << YAML::Value << iSettings.filmGrain;
	iOut << YAML::Key << "filmGrainIntensity" << YAML::Value << iSettings.filmGrainIntensity;
	iOut << YAML::Key << "filmGrainSize" << YAML::Value << iSettings.filmGrainSize;
	iOut << YAML::Key << "depthOfField" << YAML::Value << iSettings.depthOfField;
	iOut << YAML::Key << "focusDistance" << YAML::Value << iSettings.focusDistance;
	iOut << YAML::Key << "focusRange" << YAML::Value << iSettings.focusRange;
	iOut << YAML::Key << "bokehRadius" << YAML::Value << iSettings.bokehRadius;
	iOut << YAML::Key << "motionBlur" << YAML::Value << iSettings.motionBlur;
	iOut << YAML::Key << "motionBlurIntensity" << YAML::Value << iSettings.motionBlurIntensity;
	iOut << YAML::Key << "lensFlare" << YAML::Value << iSettings.lensFlare;
	iOut << YAML::Key << "lensFlareIntensity" << YAML::Value << iSettings.lensFlareIntensity;
	iOut << YAML::Key << "lensFlareGhosts" << YAML::Value << iSettings.lensFlareGhosts;
	iOut << YAML::Key << "lensFlareSpacing" << YAML::Value << iSettings.lensFlareSpacing;
	iOut << YAML::Key << "lensFlareHalo" << YAML::Value << iSettings.lensFlareHalo;
	iOut << YAML::Key << "lensFlareSize" << YAML::Value << iSettings.lensFlareSize;
	iOut << YAML::EndMap;
}

auto readPostProcess(const core::YamlNode& iNode) -> renderer::PostProcessSettings {
	renderer::PostProcessSettings settings;
	if (!iNode || !iNode.isMap())
		return settings;
	readInto(iNode, "ev100", settings.ev100);
	readInto(iNode, "autoExposure", settings.autoExposure);
	readInto(iNode, "autoExposureMin", settings.autoExposureMin);
	readInto(iNode, "autoExposureMax", settings.autoExposureMax);
	readInto(iNode, "autoExposureSpeed", settings.autoExposureSpeed);
	readInto(iNode, "exposure", settings.exposure);
	readInto(iNode, "bloom", settings.bloom);
	readInto(iNode, "bloomThreshold", settings.bloomThreshold);
	readInto(iNode, "bloomKnee", settings.bloomKnee);
	readInto(iNode, "bloomIntensity", settings.bloomIntensity);
	readInto(iNode, "bloomScatter", settings.bloomScatter);
	readInto(iNode, "vignette", settings.vignette);
	readInto(iNode, "vignetteIntensity", settings.vignetteIntensity);
	readInto(iNode, "vignetteSmoothness", settings.vignetteSmoothness);
	if (const auto color = iNode["vignetteColor"]; color && color.isSequence() && color.size() == 3)
		settings.vignetteColor = {color[0].as<float>(), color[1].as<float>(), color[2].as<float>()};
	readInto(iNode, "lut", settings.lut);
	readInto(iNode, "lutContribution", settings.lutContribution);
	readInto(iNode, "chromaticAberration", settings.chromaticAberration);
	readInto(iNode, "chromaticAberrationIntensity", settings.chromaticAberrationIntensity);
	readInto(iNode, "filmGrain", settings.filmGrain);
	readInto(iNode, "filmGrainIntensity", settings.filmGrainIntensity);
	readInto(iNode, "filmGrainSize", settings.filmGrainSize);
	readInto(iNode, "depthOfField", settings.depthOfField);
	readInto(iNode, "focusDistance", settings.focusDistance);
	readInto(iNode, "focusRange", settings.focusRange);
	readInto(iNode, "bokehRadius", settings.bokehRadius);
	readInto(iNode, "motionBlur", settings.motionBlur);
	readInto(iNode, "motionBlurIntensity", settings.motionBlurIntensity);
	readInto(iNode, "lensFlare", settings.lensFlare);
	readInto(iNode, "lensFlareIntensity", settings.lensFlareIntensity);
	readInto(iNode, "lensFlareGhosts", settings.lensFlareGhosts);
	readInto(iNode, "lensFlareSpacing", settings.lensFlareSpacing);
	readInto(iNode, "lensFlareHalo", settings.lensFlareHalo);
	readInto(iNode, "lensFlareSize", settings.lensFlareSize);
	return settings;
}

}// namespace

void Camera::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;// CameraComponent
	iOut.getImpl()->emitter << YAML::Key << "camera" << YAML::Value;
	iOut.getImpl()->emitter << YAML::BeginMap;// Camera
	iOut.getImpl()->emitter << YAML::Key << "projectionType" << YAML::Value
							<< std::string(magic_enum::enum_name(camera.getProjectionType()));
	iOut.getImpl()->emitter << YAML::Key << "toneMapping" << YAML::Value
							<< std::string(magic_enum::enum_name(camera.getToneMapping()));
	iOut.getImpl()->emitter << YAML::Key << "perspectiveFOV" << YAML::Value << camera.getPerspectiveVerticalFov();
	iOut.getImpl()->emitter << YAML::Key << "perspectiveNear" << YAML::Value << camera.getPerspectiveNearClip();
	iOut.getImpl()->emitter << YAML::Key << "perspectiveFar" << YAML::Value << camera.getPerspectiveFarClip();
	iOut.getImpl()->emitter << YAML::Key << "orthographicSize" << YAML::Value << camera.getOrthographicSize();
	iOut.getImpl()->emitter << YAML::Key << "orthographicNear" << YAML::Value << camera.getOrthographicNearClip();
	iOut.getImpl()->emitter << YAML::Key << "orthographicFar" << YAML::Value << camera.getOrthographicFarClip();
	writePostProcess(iOut.getImpl()->emitter, camera.getPostProcess());
	iOut.getImpl()->emitter << YAML::EndMap;// Camera
	iOut.getImpl()->emitter << YAML::Key << "primary" << YAML::Value << primary;
	iOut.getImpl()->emitter << YAML::Key << "fixedAspectRatio" << YAML::Value << fixedAspectRatio;
	iOut.getImpl()->emitter << YAML::EndMap;// Camera
}

void Camera::deserialize(const core::Serializer& iNode) {
	auto cameraProps = iNode.getImpl()->node["camera"];
	const auto projType =
			magic_enum::enum_cast<SceneCamera::ProjectionType>(cameraProps["projectionType"].as<std::string>());
	if (projType.has_value())
		camera.setProjectionType(projType.value());
	renderer::ToneMapping toneMapping = renderer::ToneMapping::None;
	if (const auto toneNode = cameraProps["toneMapping"]; toneNode && toneNode.isScalar()) {
		if (const auto parsed = magic_enum::enum_cast<renderer::ToneMapping>(toneNode.as<std::string>());
			parsed.has_value())
			toneMapping = parsed.value();
	}
	camera.setToneMapping(toneMapping);
	camera.setPostProcess(readPostProcess(cameraProps["postProcess"]));
	camera.setPerspectiveVerticalFov(cameraProps["perspectiveFOV"].as<float>());
	camera.setPerspectiveNearClip(cameraProps["perspectiveNear"].as<float>());
	camera.setPerspectiveFarClip(cameraProps["perspectiveFar"].as<float>());

	camera.setOrthographicSize(cameraProps["orthographicSize"].as<float>());
	camera.setOrthographicNearClip(cameraProps["orthographicNear"].as<float>());
	camera.setOrthographicFarClip(cameraProps["orthographicFar"].as<float>());

	primary = iNode.getImpl()->node["primary"].as<bool>();
	fixedAspectRatio = iNode.getImpl()->node["fixedAspectRatio"].as<bool>();
}

}// namespace owl::scene::component
