/**
 * @file Device.cpp
 * @author Silmaen
 * @date 03/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Device.h"

#if defined(OWL_PLATFORM_LINUX)
#include "input/video/Manager.h"

#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <linux/media.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace owl::input::video::linux64 {

namespace {

auto getPixelFormatString(const uint32_t iPixelFormat) -> std::string {
	switch (iPixelFormat) {
		case V4L2_PIX_FMT_RGB24:
			return "RGB24";
		case V4L2_PIX_FMT_YUV420:
			return "YUV420";
		case V4L2_PIX_FMT_YUYV:
			return "YUYV";
		case V4L2_PIX_FMT_UYVY:
			return "UYVY";
		case V4L2_PIX_FMT_NV12:
			return "NV12";
		case V4L2_PIX_FMT_YVU420:
			return "YVU420";
		case V4L2_PIX_FMT_YUV422P:
			return "YUV422P";
		case V4L2_PIX_FMT_YUV411P:
			return "YUV411P";
		case V4L2_PIX_FMT_YUV410:
			return "YUV410";
		case V4L2_PIX_FMT_NV21:
			return "NV21";
		case V4L2_PIX_FMT_NV16:
			return "NV16";
		case V4L2_PIX_FMT_GREY:
			return "GREY";
		case V4L2_PIX_FMT_YVU410:
			return "YVU410";
		case V4L2_PIX_FMT_Y41P:
			return "Y41P";
		case V4L2_PIX_FMT_BGR24:
			return "BGR24";
		case V4L2_PIX_FMT_ABGR32:
			return "ABGR32";
		case V4L2_PIX_FMT_ARGB32:
			return "ARGB32";
		case V4L2_PIX_FMT_YVU420M:
			return "YVU420M";
		case V4L2_PIX_FMT_YVU422M:
			return "YVU422M";
		case V4L2_PIX_FMT_YVU444M:
			return "YVU444M";
		case V4L2_PIX_FMT_SBGGR8:
			return "SBGGR8";
		case V4L2_PIX_FMT_SGBRG8:
			return "SGBRG8";
		case V4L2_PIX_FMT_SGRBG8:
			return "SGRBG8";
		case V4L2_PIX_FMT_SRGGB8:
			return "SRGGB8";
		case V4L2_PIX_FMT_MJPEG:
			return "MJPEG";
		// Ajoutez d'autres formats selon vos besoins
		default:
			return "Unknown";
	}
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
auto unFourCC(const uint32_t iPixelFormat) -> std::string {
	constexpr uint32_t mmask = 0xff;
	char fcc[4];
	fcc[0] = static_cast<char>(iPixelFormat & mmask);
	fcc[1] = static_cast<char>((iPixelFormat >> 8u) & mmask);
	fcc[2] = static_cast<char>((iPixelFormat >> 16u) & mmask);
	fcc[3] = static_cast<char>((iPixelFormat >> 24u) & mmask);
	return fcc;
}
OWL_DIAG_POP

auto getDevicePixelFormat(const uint32_t iPixelFormat) -> Device::PixelFormat {
	if (iPixelFormat == V4L2_PIX_FMT_RGB24)
		return Device::PixelFormat::Rgb24;
	if (iPixelFormat == V4L2_PIX_FMT_NV12)
		return Device::PixelFormat::Nv12;
	if (iPixelFormat == V4L2_PIX_FMT_YUYV)
		return Device::PixelFormat::YuYv;
	if (iPixelFormat == V4L2_PIX_FMT_MJPEG)
		return Device::PixelFormat::MJpeg;
	return Device::PixelFormat::Unknwon;
}

auto getV4L2PixelFormat(const Device::PixelFormat& iPixelFormat) -> uint32_t {
	if (iPixelFormat == Device::PixelFormat::Rgb24)
		return V4L2_PIX_FMT_RGB24;
	if (iPixelFormat == Device::PixelFormat::Nv12)
		return V4L2_PIX_FMT_NV12;
	if (iPixelFormat == Device::PixelFormat::YuYv)
		return V4L2_PIX_FMT_YUYV;
	if (iPixelFormat == Device::PixelFormat::MJpeg)
		return V4L2_PIX_FMT_MJPEG;
	return 0;
}

}// namespace


void updateList(std::vector<shared<video::Device>>& ioList) {
	// check if all listed devices still exists
	if (std::remove_if(ioList.begin(), ioList.end(), [](const shared<video::Device>& iDev) {
			return !std::static_pointer_cast<Device>(iDev)->isValid();
		}) != ioList.end()) {
		OWL_CORE_WARN("Possible problems during video input listing.")
	}

	// search for new devices.
	size_t devCounter = 0;
	for (size_t iCam = 0; iCam < g_maxDevices; ++iCam) {
		auto testDev = mkShared<Device>(std::format("/dev/video{}", iCam).c_str());
		if (!testDev->isValid()) {
			continue;
		}
		// don't add a device that already exists
		if (std::find_if(ioList.begin(), ioList.end(), [&testDev](const shared<video::Device>& dev) {
				return testDev->getBusInfo() == static_pointer_cast<Device>(dev)->getBusInfo();
			}) != ioList.end())
			continue;
		OWL_CORE_TRACE("Found: {} ({}) [{}] ", devCounter, testDev->getName(), testDev->getBusInfo())
		ioList.push_back(testDev);
		++devCounter;
	}
}

Device::Device(std::string iFile) : video::Device{""}, m_file{std::move(iFile)} {
	if (!exists(std::filesystem::path(m_file)))
		return;

	const int fd = ::open(m_file.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd == 0) {
		OWL_CORE_WARN("Unable to open device file {}", m_file)
		return;
	}
	v4l2_capability capability{};
	const int vcap = ioctl(fd, VIDIOC_QUERYCAP, &capability);
	if (vcap < 0) {
		OWL_CORE_WARN("Unable to query the device capabilities.")
		::close(fd);
		return;
	}
	if (vcap == 0) {
		m_name = reinterpret_cast<const char*>(capability.card);
		m_busInfo = reinterpret_cast<const char*>(capability.bus_info);
	} else {
		// second method
		media_device_info mdi{};
		if (ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdi) > 0) {
			if (mdi.bus_info[0] != 0)
				m_busInfo = mdi.bus_info;
			else
				m_busInfo = std::format("platform: {}", mdi.driver);
			if (mdi.model[0] != 0)
				m_name = mdi.model;
			else
				m_name = mdi.driver;
		}
	}
	::close(fd);
}

Device::~Device() { close(); }

void Device::open() {
	OWL_CORE_INFO("Opening device ({}): {}", m_file, m_name)
	close();
	m_fileHandler = ::open(m_file.c_str(), O_RDWR | O_CLOEXEC);
	if (m_fileHandler <= 0) {
		OWL_CORE_WARN("({}) Unable to open the device.", m_file)
		return;
	}
	// manage format
	{
		v4l2_format fmt{};
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(m_fileHandler, VIDIOC_G_FMT, &fmt) < 0) {
			OWL_CORE_WARN("({}) Unable to get format of device.", m_file)
			close();
			return;
		}
		if (!isPixelFormatSupported(getDevicePixelFormat(fmt.fmt.pix.pixelformat))) {
			OWL_CORE_WARN("({}) Native pixel format unsupported!", m_file, getPixelFormatString(V4L2_PIX_FMT_NV12))
			OWL_CORE_WARN("({}) fourcc: {}", m_file, unFourCC(fmt.fmt.pix.pixelformat))
			fmt.fmt.pix.pixelformat = getV4L2PixelFormat(getFirstSupportedPixelFormat());
			if (ioctl(m_fileHandler, VIDIOC_S_FMT, &fmt) < 0) {
				OWL_CORE_WARN("({}) Unable to set format of device", m_file)
				close();
				return;
			}
			if (!isPixelFormatSupported(getDevicePixelFormat(fmt.fmt.pix.pixelformat))) {
				OWL_CORE_WARN("({}) Unable to set device to a suitable format", m_file)
				close();
				return;
			}
		}
		if (ioctl(m_fileHandler, VIDIOC_S_FMT, &fmt) < 0) {
			OWL_CORE_WARN("({}) Unable to set format of device", m_file)
			close();
			return;
		}
		OWL_CORE_INFO("({}) Using size is {} x {}", m_file, fmt.fmt.pix.width, fmt.fmt.pix.height)
		OWL_CORE_INFO("({}) Using pixel format: {}", m_file, getPixelFormatString(fmt.fmt.pix.pixelformat))
		m_pixFormat = getDevicePixelFormat(fmt.fmt.pix.pixelformat);
		m_size = {fmt.fmt.pix.width, fmt.fmt.pix.height};
	}
	// request buffer from device.
	{
		v4l2_requestbuffers requestBuffers{};
		requestBuffers.count = 1;// only one buffer requested.
		requestBuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		requestBuffers.memory = V4L2_MEMORY_MMAP;
		if (ioctl(m_fileHandler, VIDIOC_REQBUFS, &requestBuffers) < 0) {
			OWL_CORE_WARN("({}) Unable to request buffer.", m_file)
			close();
			return;
		}
	}
	// define the bufferInfo
	{
		OWL_DIAG_PUSH
		OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
		memset(&m_bufferInfo, 0, sizeof(m_bufferInfo));
		OWL_DIAG_POP
		m_bufferInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		m_bufferInfo.memory = V4L2_MEMORY_MMAP;
		m_bufferInfo.index = 0;
	}
	// query buffer from device
	{
		if (ioctl(m_fileHandler, VIDIOC_QUERYBUF, &m_bufferInfo) < 0) {
			OWL_CORE_WARN("({}) Unable to query buffer.", m_file)
			close();
			return;
		}
		OWL_CORE_INFO("({}) Buffer info, length {} , offset {}.", m_file, m_bufferInfo.length, m_bufferInfo.m.offset)
		mp_buffer = mmap(nullptr, m_bufferInfo.length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fileHandler,
						 m_bufferInfo.m.offset);
		if (mp_buffer == MAP_FAILED) {
			OWL_CORE_ERROR("({}) Unable to map the device buffer.", m_file)
			close();
			return;
		}
	}
	//Activate the streaming
	{
		uint32_t type = m_bufferInfo.type;
		if (ioctl(m_fileHandler, VIDIOC_STREAMON, &type) < 0) {
			OWL_CORE_WARN("({}) Unable to start the streaming.", m_file)
			close();
			return;
		}
		m_streaming = true;
	}
	// queue the buffer
	if (ioctl(m_fileHandler, VIDIOC_QBUF, &m_bufferInfo) < 0) {
		OWL_CORE_WARN("Device ({}) unable to queue the buffer.", m_file)
		close();
	}
}

void Device::close() {
	if (!isOpened())
		return;
	if (mp_buffer != nullptr)
		munmap(mp_buffer, m_bufferInfo.length);
	if (m_streaming) {
		if (ioctl(m_fileHandler, VIDIOC_STREAMOFF, &m_bufferInfo) < 0) {
			OWL_CORE_WARN("({}) Unable to close the streaming.", m_file)
		}
	}
	::close(m_fileHandler);
	m_fileHandler = 0;
	m_size = {1, 1};
	mp_buffer = nullptr;
	m_streaming = false;
}

auto Device::isOpened() const -> bool { return m_fileHandler != 0; }

void Device::fillFrame(shared<renderer::Texture>& ioFrame) {
	if (!m_streaming)
		return;// need to be open and ready!
	// recreate the frame if not the right size.
	if (!ioFrame || ioFrame->getSize() != m_size) {
		ioFrame = renderer::Texture2D::create({m_size, renderer::ImageFormat::Rgb8});
	}
	// dequeue the buffer
	if (ioctl(m_fileHandler, VIDIOC_DQBUF, &m_bufferInfo) < 0) {
		OWL_CORE_WARN("Device ({}) unable to dequeue the buffer.", m_file)
		return;
	}

	std::vector<uint8_t> convertedBuffer =
			getRgbBuffer(static_cast<const uint8_t*>(mp_buffer), static_cast<int32_t>(m_bufferInfo.bytesused));
	// frames are written after dequeue the buffer
	if (!convertedBuffer.empty()) {
		if (static_cast<size_t>(m_size.surface()) * 3ull != convertedBuffer.size()) {
			OWL_CORE_WARN("Device ({}) buffer size missmatch: {}, expecting {}.", m_file, m_bufferInfo.bytesused,
						  m_size.surface() * 3)
		} else {
			ioFrame->setData(convertedBuffer.data(), static_cast<uint32_t>(convertedBuffer.size()));
		}
	}
	// queue the buffer
	if (ioctl(m_fileHandler, VIDIOC_QBUF, &m_bufferInfo) < 0) {
		OWL_CORE_WARN("Device ({}) unable to queue the buffer.", m_file)
	}
}

auto Device::isValid() const -> bool {
	// fast out: open is assumed valid.
	if (isOpened())
		return true;

	// check if really initiated.
	if (m_file.empty())
		return false;
	if (m_name.empty())
		return false;
	if (m_busInfo.empty())
		return false;

	// check if still exists and still a video source.
	if (!exists(std::filesystem::path(m_file)))
		return false;
	const int fd = ::open(m_file.c_str(), O_RDWR | O_CLOEXEC);
	if (fd == 0)
		return false;
	// check have supported capabilities
	{
		v4l2_capability capability{};
		if (ioctl(fd, VIDIOC_QUERYCAP, &capability) < 0) {
			OWL_CORE_TRACE("Device {} ({}) unable to get Video capability.", m_name, m_file)
			::close(fd);
			return false;
		}
		if ((capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0u) {
			OWL_CORE_TRACE("Device {} ({}) does not have Video capability.", m_name, m_file)
			::close(fd);
			return false;// not a capture device.
		}
		if ((capability.capabilities & V4L2_CAP_STREAMING) == 0u) {
			OWL_CORE_TRACE("Device {} ({}) does not have streaming capability.", m_name, m_file)
			::close(fd);
			return false;// no read capability.
		}
	}
	// check have supported pixelformat.
	{
		if (getFirstSupportedPixelFormat(fd) == PixelFormat::Unknwon) {
			OWL_CORE_TRACE("Device {} ({}) Does not have a supported pixel format.", m_name, m_file)
			::close(fd);
			printSupportedFormat();
			return false;
		}
	}
	::close(fd);
	// everything seems ok.
	return true;
}

void Device::printSupportedFormat() const {
	if (!exists(std::filesystem::path(m_file)))
		return;
	const int fd = ::open(m_file.c_str(), O_RDWR | O_CLOEXEC);
	if (fd == 0)
		return;
	// Structure pour interroger les formats de pixel
	v4l2_fmtdesc formatDescription{};
	formatDescription.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	// Boucle pour interroger chaque format de pixel
	while (ioctl(fd, VIDIOC_ENUM_FMT, &formatDescription) == 0) {
		OWL_CORE_INFO("({}) index {} format {}", m_file, formatDescription.index,
					  getPixelFormatString(formatDescription.pixelformat))
		// Incrémentation pour passer au format de pixel suivant
		++formatDescription.index;
	}
	::close(fd);
}

auto Device::getFirstSupportedPixelFormat(int32_t iFd) const -> PixelFormat {
	if (iFd == -1)
		iFd = m_fileHandler;
	if (iFd == -1)
		return PixelFormat::Unknwon;
	v4l2_fmtdesc formatDescription{};
	formatDescription.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while (ioctl(iFd, VIDIOC_ENUM_FMT, &formatDescription) == 0) {
		if (const auto pix = getDevicePixelFormat(formatDescription.pixelformat); isPixelFormatSupported(pix))
			return pix;
		++formatDescription.index;
	}
	return PixelFormat::Unknwon;
}

}// namespace owl::input::video::linux64

#endif
