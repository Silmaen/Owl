/**
 * @file Device.cpp
 * @author Silmaen
 * @date 03/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Device.h"

#if defined(OWL_PLATFORM_WINDOWS)

#include "WPointer.h"
#include <initguid.h>
#include <mfapi.h>
#include <windows.h>

namespace owl::io::video::windows {

namespace {

constexpr uint64_t g_mask32 = 0xffffffff;

struct ComControl {
	auto addRef() -> bool {
		if (refCount == 0) {
			HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			if (FAILED(hr)) {
				OWL_CORE_ERROR("Unable to Initialize COM")
				return false;
			}
			hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
			if (FAILED(hr)) {
				OWL_CORE_ERROR("Unable to Initialize Media Foundation")
				return false;
			}
		}
		++refCount;
		return true;
	}

	void delRef() {
		if (refCount == 0)
			return;
		--refCount;
		if (refCount == 0) {
			MFShutdown();
			CoUninitialize();
		}
	}

private:
	uint64_t refCount = 0;
};

ComControl cc;

auto getPixelFormatString(const GUID& videoFormat) -> std::string {
	if (videoFormat == MFVideoFormat_AI44)
		return "AI44";
	if (videoFormat == MFVideoFormat_ARGB32)
		return "ARGB32";
	if (videoFormat == MFVideoFormat_AYUV)
		return "AYUV";
	if (videoFormat == MFVideoFormat_DV25)
		return "DV25";
	if (videoFormat == MFVideoFormat_DV50)
		return "DV50";
	if (videoFormat == MFVideoFormat_MJPG)
		return "MJPG";
	if (videoFormat == MFVideoFormat_RGB32)
		return "RGB32";
	if (videoFormat == MFVideoFormat_RGB24)
		return "RGB24";
	if (videoFormat == MFVideoFormat_RGB555)
		return "RGB555";
	if (videoFormat == MFVideoFormat_RGB565)
		return "RGB565";
	if (videoFormat == MFVideoFormat_UYVY)
		return "UYVY";
	//if (videoFormat == MFVideoFormat_Y411) return "Y411";
	if (videoFormat == MFVideoFormat_Y41P)
		return "Y41P";
	//if (videoFormat == MFVideoFormat_YUV) return "YUV";
	if (videoFormat == MFVideoFormat_YUY2)
		return "YUY2";
	if (videoFormat == MFVideoFormat_YV12)
		return "YV12";
	if (videoFormat == MFVideoFormat_YVU9)
		return "YVU9";
	if (videoFormat == MFVideoFormat_NV11)
		return "NV11";
	if (videoFormat == MFVideoFormat_NV12)
		return "NV12";
	if (videoFormat == MFVideoFormat_P010)
		return "P010";
	if (videoFormat == MFVideoFormat_P016)
		return "P016";
	if (videoFormat == MFVideoFormat_P210)
		return "P210";
	if (videoFormat == MFVideoFormat_P216)
		return "P216";
	if (videoFormat == MFVideoFormat_v210)
		return "v210";
	if (videoFormat == MFVideoFormat_v216)
		return "v216";
	if (videoFormat == MFVideoFormat_v410)
		return "v410";
	if (videoFormat == MFVideoFormat_I420)
		return "I420";
	if (videoFormat == MFVideoFormat_IYUV)
		return "IYUV";
	//if (videoFormat == MFVideoFormat_U420) return "U420";
	if (videoFormat == MFVideoFormat_Y210)
		return "Y210";
	if (videoFormat == MFVideoFormat_Y216)
		return "Y216";
	if (videoFormat == MFVideoFormat_Y410)
		return "Y410";
	if (videoFormat == MFVideoFormat_Y416)
		return "Y416";
	if (videoFormat == MFVideoFormat_Y41T)
		return "Y41T";
	if (videoFormat == MFVideoFormat_Y42T)
		return "Y42T";
	//if (videoFormat == MFVideoFormat_YVU420_MPEG2) return "YVU420_MPEG2";
	if (videoFormat == MFVideoFormat_DVHD)
		return "DVHD";
	if (videoFormat == MFVideoFormat_DVSD)
		return "DVSD";
	if (videoFormat == MFVideoFormat_DVSL)
		return "DVSL";
	if (videoFormat == MFVideoFormat_H264)
		return "H264";
	if (videoFormat == MFVideoFormat_H265)
		return "H265";
	if (videoFormat == MFVideoFormat_MPEG2)
		return "MPEG2";
	if (videoFormat == MFVideoFormat_MPG1)
		return "MPG1";
	if (videoFormat == MFVideoFormat_MPG2)
		return "MPG2";
	//if (videoFormat == MFVideoFormat_MPG3) return "MPG3";
	//if (videoFormat == MFVideoFormat_MPG4) return "MPG4";
	if (videoFormat == MFVideoFormat_WMV1)
		return "WMV1";
	if (videoFormat == MFVideoFormat_WMV2)
		return "WMV2";
	if (videoFormat == MFVideoFormat_WMV3)
		return "WMV3";
	if (videoFormat == MFVideoFormat_WVC1)
		return "WVC1";
	//if (videoFormat == MFVideoFormat_FLV1) return "FLV1";
	if (videoFormat == MFVideoFormat_HEVC)
		return "HEVC";
	// Ajoutez d'autres formats vidéo au besoin

	// Format inconnu
	return "Unknown";
}

auto getDevicePixelFormat(const GUID& videoFormat) -> Device::PixelFormat {
	if (videoFormat == MFVideoFormat_RGB24)
		return Device::PixelFormat::Rgb24;
	if (videoFormat == MFVideoFormat_NV12)
		return Device::PixelFormat::Nv12;
	if (videoFormat == MFVideoFormat_MJPG)
		return Device::PixelFormat::MJpeg;
	if (videoFormat == MFVideoFormat_YUY2)
		return Device::PixelFormat::YuYv;

	// Format inconnu
	return Device::PixelFormat::Unknwon;
}

}// namespace

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void updateList(std::vector<shared<video::Device>>& ioList) {
	if (!cc.addRef())
		return;
	// check if all listed devices still exists
	if (std::remove_if(ioList.begin(), ioList.end(), [](const shared<video::Device>& iDev) {
			return !std::static_pointer_cast<Device>(iDev)->isValid();
		}) != ioList.end()) {
		OWL_CORE_WARN("Possible problems during video input listing.")
	}

	WPointer<IMFAttributes> pConfig;
	HRESULT hr = MFCreateAttributes(pConfig.addr(), 1);

	// Request video capture devices.
	if (FAILED(hr)) {
		OWL_CORE_ERROR("input::video::windows::updateList: Unable to create MFAttribute.")
		return;
	}
	// define what we search: Video input!
	hr = pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
	if (FAILED(hr)) {
		OWL_CORE_ERROR("input::video::windows::updateList: Unable to setup attributes.")
		return;
	}
	std::vector<WPointer<IMFActivate>> devices;
	{
		IMFActivate** pDevices = nullptr;
		uint32_t count = 0;
		hr = MFEnumDeviceSources(pConfig.get(), &pDevices, &count);
		if (FAILED(hr)) {
			OWL_CORE_ERROR("input::video::windows::updateList: Unable to Enum Device Sources.")
			return;
		}
		devices.reserve(count);
		for (uint32_t i = 0; i < count; ++i) devices.emplace_back(pDevices[i]);
	}
	if (devices.empty()) {
		OWL_CORE_WARN("input::video::windows::updateList: No devices found.")
		return;
	}
	// search for new devices.
	size_t devCounter = 0;
	for (auto& devivce: devices) {
		auto testDev = mkShared<Device>(devivce);
		if (!testDev->isValid()) {
			continue;
		}
		// don't add a device that already exists
		if (std::ranges::find_if(ioList.begin(), ioList.end(), [&testDev](const shared<video::Device>& dev) {
				return testDev->getBusInfo() == static_pointer_cast<Device>(dev)->getBusInfo();
			}) != ioList.end())
			continue;
		OWL_CORE_TRACE("Found: {} ({}) [{}] ", devCounter, testDev->getName(), testDev->getBusInfo())
		ioList.push_back(testDev);
		++devCounter;
	}
	cc.delRef();
}
OWL_DIAG_POP

Device::Device(WPointer<IMFActivate>& iMfa) : video::Device("") {
	if (!cc.addRef())
		return;
	m_devActive.takeOwnershipFrom(iMfa);
	{
		wchar_t* fName = nullptr;
		uint32_t len = 0;
		m_devActive->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &fName, &len);
		std::wstring ws(fName);
		m_name = std::string(ws.begin(), ws.end());
		CoTaskMemFree(fName);
	}
	{
		wchar_t* fName = nullptr;
		uint32_t len = 0;
		m_devActive->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &fName, &len);
		std::wstring ws(fName);
		m_busInfo = std::string(ws.begin(), ws.end());
		CoTaskMemFree(fName);
	}
}

Device::~Device() {
	close();
	cc.delRef();
}

void Device::open() {
	OWL_CORE_INFO("Opening device {}", m_name)
	// Media source activation!
	HRESULT hr = m_devActive->ActivateObject(IID_PPV_ARGS(m_mediaSource.addr()));
	if (FAILED(hr)) {
		OWL_CORE_WARN("Device ({}): Unable to activate media source.", m_name)
		close();
		return;
	}
	// Creation of media reader
	WPointer<IMFAttributes> pConfig;
	hr = MFCreateAttributes(pConfig.addr(), 1);
	if (FAILED(hr)) {
		OWL_CORE_ERROR("Device ({}): Unable to create MFAttribute.", m_name)
		return;
	}
	// define what we search: Video input!
	hr = pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
	if (FAILED(hr)) {
		OWL_CORE_ERROR("Device ({}): Unable to setup attributes.", m_name)
		return;
	}
	hr = MFCreateSourceReaderFromMediaSource(m_mediaSource.get(), pConfig.get(), m_sourceReader.addr());
	if (FAILED(hr)) {
		OWL_CORE_WARN("Device ({}): Unable to create media reader.", m_name)
		close();
		return;
	}
	WPointer<IMFMediaType> mediaType;
	hr = m_sourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, MF_SOURCE_READER_CURRENT_TYPE_INDEX,
											mediaType.addr());
	if (FAILED(hr)) {
		OWL_CORE_WARN("Device ({}): Unable to get native media type.", m_name)
		close();
		return;
	}
	uint64_t superSize = 0;
	mediaType->GetUINT64(MF_MT_FRAME_SIZE, &superSize);
	m_size = {static_cast<uint32_t>((superSize >> 32) & g_mask32), static_cast<uint32_t>(superSize & g_mask32)};
	OWL_CORE_INFO("Device ({}): Frame size seen: {} {}.", m_name, m_size.x(), m_size.y())
	GUID format;
	mediaType->GetGUID(MF_MT_SUBTYPE, &format);
	OWL_CORE_INFO("Device ({}): Native media sub type: {}.", m_name, getPixelFormatString(format))
	m_pixFormat = getDevicePixelFormat(format);
	mediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	mediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
	hr = m_sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mediaType.get());
	if (FAILED(hr)) {
		OWL_CORE_WARN("Device ({}): Unable to set native media type.", m_name)
		close();
	}
}

void Device::close() {
	if (m_sourceReader) {
		m_sourceReader.release();
	}
	if (m_mediaSource) {
		m_mediaSource->Shutdown();
		m_mediaSource.release();
	}
	m_size = {1, 1};
}

auto Device::isOpened() const -> bool { return m_size.surface() > 1; }

void Device::fillFrame(shared<renderer::Texture>& iFrame) {
	if (!isOpened())
		return;
	// Resizing the frame.
	if (m_size.surface() == 0)
		return;
	if (!iFrame || iFrame->getSize() != m_size) {
		if (iFrame)
			iFrame.reset();
		iFrame = renderer::Texture2D::create({m_size, renderer::ImageFormat::Rgb8});
	}
	// For now only fill with black
	WPointer<IMFSample> sample;
	int64_t timestamp = 0;
	u_long actualIndex = 0;
	u_long sampleFlags = MF_SOURCE_READERF_STREAMTICK;
	while ((sampleFlags & MF_SOURCE_READERF_STREAMTICK) != 0u) {
		if (const HRESULT hr = m_sourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &actualIndex,
														  &sampleFlags, &timestamp, sample.addr());
			FAILED(hr)) {
			OWL_CORE_WARN("Device ({}): Unable to read sample from device.", m_name)
			return;
		}
	}
	u_long count = 0;
	if (FAILED(sample->GetBufferCount(&count)) || count == 0) {
		OWL_CORE_WARN("Device ({}): No  buffer found in the sample from device.", m_name)
		return;
	}
	WPointer<IMFMediaBuffer> buffer;
	if (FAILED(sample->ConvertToContiguousBuffer(buffer.addr()))) {
		OWL_CORE_WARN("Device ({}): Unable to Convert buffer.", m_name)
		return;
	}
	byte* byteBuffer = nullptr;
	u_long bCurLen = 0;
	buffer->Lock(&byteBuffer, nullptr, &bCurLen);
	std::vector<byte> convertedBuffer = getRgbBuffer(byteBuffer, static_cast<int32_t>(bCurLen));
	if (m_size.surface() * 3ull != convertedBuffer.size()) {
		OWL_CORE_WARN("Frame size missmatch {} buffer: {}.", m_size.surface() * 3ull, convertedBuffer.size())
		buffer->Unlock();
		return;
	}
	iFrame->setData(convertedBuffer.data(), static_cast<uint32_t>(convertedBuffer.size()));
	buffer->Unlock();
}

auto Device::isValid() const -> bool { return !m_name.empty() && !m_busInfo.empty(); }

}// namespace owl::io::video::windows

#endif
