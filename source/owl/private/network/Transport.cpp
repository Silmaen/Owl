/**
 * @file Transport.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "network/LoopbackTransport.h"
#include "network/MixedTransport.h"
#include "network/Transport.h"
#if OWL_WITH_NETWORK
#ifndef OWL_PLATFORM_WEB
#include "enet/EnetTransport.h"
#endif
#include "websocket/WebSocketTransport.h"
#endif

#include <utility>
#include <vector>

namespace owl::network {

namespace {

auto createEnet() -> expected<uniq<Transport>, NetworkError> {
#if OWL_WITH_NETWORK && defined(OWL_PLATFORM_WEB)
	OWL_CORE_WARN("Transport: A page has no ENet (no UDP), use WebSocket.")
	return unexpected{NetworkError::BackendUnavailable};
#elif OWL_WITH_NETWORK
	auto transport = mkUniq<enet::EnetTransport>();
	if (!transport->isAvailable()) {
		OWL_CORE_WARN("Transport: ENet is unavailable.")
		return unexpected{NetworkError::BackendUnavailable};
	}
	return transport;
#else
	OWL_CORE_WARN("Transport: ENet not built in (OWL_MODULE_NETWORK=OFF).")
	return unexpected{NetworkError::BackendUnavailable};
#endif
}

}// namespace

Transport::~Transport() = default;

auto Transport::create(const Type iType) -> expected<uniq<Transport>, NetworkError> {
	switch (iType) {
		case Type::Loopback:
			return mkUniq<LoopbackTransport>();
		case Type::Enet:
			return createEnet();
		case Type::WebSocket:
#if OWL_WITH_NETWORK
			return mkUniq<websocket::WebSocketTransport>();
#else
			OWL_CORE_WARN("Transport: WebSocket not built in (OWL_MODULE_NETWORK=OFF).")
			return unexpected{NetworkError::BackendUnavailable};
#endif
		case Type::Mixed:
			{
#if OWL_WITH_NETWORK && !defined(OWL_PLATFORM_WEB)
				auto enetTransport = createEnet();
				if (!enetTransport)
					return unexpected{enetTransport.error()};
				std::vector<uniq<Transport>> transports;
				transports.push_back(std::move(*enetTransport));
				transports.push_back(mkUniq<websocket::WebSocketTransport>());
				return mkUniq<MixedTransport>(std::move(transports));
#else
				OWL_CORE_WARN("Transport: ENet and WebSocket at once need a desktop build with OWL_MODULE_NETWORK.")
				return unexpected{NetworkError::BackendUnavailable};
#endif
			}
	}
	OWL_CORE_ERROR("Transport: unknown transport type.")
	return unexpected{NetworkError::BackendUnavailable};
}

}// namespace owl::network
