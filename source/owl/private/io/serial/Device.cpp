/**
 * @file Device.cpp
 * @author Silmaen
 * @date 05/06/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "io/serial/Device.h"

#include <utility>

namespace owl::io::serial {

Device::Device() = default;

Device::Device(std::string iPort, std::string iName, std::string iBusInfo)
	: m_port(std::move(iPort)), m_name(std::move(iName)), m_busInfo(std::move(iBusInfo)) {}

Device::~Device() = default;

}// namespace owl::io::serial
