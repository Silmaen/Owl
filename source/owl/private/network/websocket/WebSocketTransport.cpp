/**
 * @file WebSocketTransport.cpp
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "WebSocketTransport.h"

#ifdef OWL_PLATFORM_WEB
#include <emscripten/websocket.h>
#else
#include <httplib.h>
#endif

#include <atomic>
#include <format>
#include <utility>

namespace owl::network::websocket {

namespace {

auto connectedEvent(const PeerId iPeer) -> TransportEvent {
	return {.kind = TransportEvent::Kind::Connected,
			.peer = iPeer,
			.channel = 0,
			.reason = DisconnectReason::Graceful,
			.payload = {}};
}

}// namespace

#ifdef OWL_PLATFORM_WEB

struct WebSocketTransport::Link {
	// The transport the callbacks report to.
	WebSocketTransport* owner = nullptr;
	// Peer of the connection.
	PeerId peer = 0;
	// The browser socket.
	EMSCRIPTEN_WEBSOCKET_T socket = 0;
	// Whether the connection opened.
	bool open = false;
	// Bytes sent.
	uint64_t bytesSent = 0;
	// Bytes received.
	uint64_t bytesReceived = 0;

	Link() = default;
	Link(const Link&) = delete;
	Link(Link&&) = delete;
	auto operator=(const Link&) -> Link& = delete;
	auto operator=(Link&&) -> Link& = delete;
	~Link() {
		if (socket > 0)
			emscripten_websocket_delete(socket);
	}
};

struct WebSocketTransport::Listener {};

namespace {

auto onOpen([[maybe_unused]] int iType, [[maybe_unused]] const EmscriptenWebSocketOpenEvent* iEvent, void* ioLink)
		-> EM_BOOL {
	auto* link = static_cast<WebSocketTransport::Link*>(ioLink);
	link->open = true;
	link->owner->push(connectedEvent(link->peer));
	return EM_TRUE;
}

auto onMessage([[maybe_unused]] int iType, const EmscriptenWebSocketMessageEvent* iEvent, void* ioLink) -> EM_BOOL {
	auto* link = static_cast<WebSocketTransport::Link*>(ioLink);
	if (iEvent->isText != 0 || iEvent->numBytes == 0)
		return EM_TRUE;
	const std::span bytes{iEvent->data, iEvent->numBytes};
	link->owner->countReceived(link->peer, bytes.size());
	link->owner->push({.kind = TransportEvent::Kind::Received,
					   .peer = link->peer,
					   .channel = bytes.front(),
					   .reason = DisconnectReason::Graceful,
					   .payload = {bytes.begin() + 1, bytes.end()}});
	return EM_TRUE;
}

auto onClose([[maybe_unused]] int iType, [[maybe_unused]] const EmscriptenWebSocketCloseEvent* iEvent, void* ioLink)
		-> EM_BOOL {
	auto* link = static_cast<WebSocketTransport::Link*>(ioLink);
	// The link goes with the drop: nothing of it is read after.
	link->owner->drop(link->peer, link->open ? DisconnectReason::Graceful : DisconnectReason::Refused);
	return EM_TRUE;
}

}// namespace

#else

struct WebSocketTransport::Link {
	// Peer of the connection.
	PeerId peer = 0;
	// Guards server and client against the end of the connection.
	std::mutex mutex;
	// Server side: the socket of the handler, null once it returned.
	httplib::ws::WebSocket* server = nullptr;
	// Client side: the client.
	uniq<httplib::ws::WebSocketClient> client;
	// Client side: the thread reading the client.
	std::thread reader;
	// Bytes sent.
	uint64_t bytesSent = 0;
	// Bytes received.
	uint64_t bytesReceived = 0;

	// Sends one frame; false once the connection ended.
	auto send(const std::string& iFrame) -> bool {
		const std::scoped_lock<std::mutex> lock{mutex};
		if (server != nullptr)
			return server->send(iFrame.data(), iFrame.size());
		if (client != nullptr && client->is_open())
			return client->send(iFrame.data(), iFrame.size());
		return false;
	}

	// Closes the connection; its reader then reports the end.
	void close() {
		const std::scoped_lock<std::mutex> lock{mutex};
		if (server != nullptr)
			server->close();
		if (client != nullptr)
			client->close();
	}
};

struct WebSocketTransport::Listener {
	// The HTTP server, upgraded to WebSocket on the endpoint.
	httplib::Server server;
	// The thread of its accept loop.
	std::thread thread;
	// Peers accepted at the same time.
	uint32_t maxPeers = 0;
	// Peers connected now.
	std::atomic<uint32_t> peers{0};
};

namespace {

// Reads the frames of a connection until it ends, as `Received` events.
template<typename Socket>
void readFrames(WebSocketTransport& ioTransport, const PeerId iPeer, Socket& ioSocket) {
	std::string message;
	while (true) {
		const auto result = ioSocket.read(message);
		if (result == httplib::ws::ReadResult::Fail)
			break;
		if (result != httplib::ws::ReadResult::Binary || message.empty())
			continue;
		ioTransport.countReceived(iPeer, message.size());
		ioTransport.push({.kind = TransportEvent::Kind::Received,
						  .peer = iPeer,
						  .channel = static_cast<uint8_t>(message.front()),
						  .reason = DisconnectReason::Graceful,
						  .payload = {message.begin() + 1, message.end()}});
	}
}

}// namespace

#endif

WebSocketTransport::WebSocketTransport() : Transport{Type::WebSocket} {}

WebSocketTransport::~WebSocketTransport() { close(); }

auto WebSocketTransport::listen([[maybe_unused]] const uint16_t iPort, [[maybe_unused]] const uint32_t iMaxPeers)
		-> expected<void, NetworkError> {
#ifdef OWL_PLATFORM_WEB
	OWL_CORE_WARN("WebSocket: A page cannot listen, it connects to a server.")
	return unexpected{NetworkError::BackendUnavailable};
#else
	if (isOpen())
		return unexpected{NetworkError::AlreadyOpen};
	auto listener = mkUniq<Listener>();
	listener->maxPeers = iMaxPeers;
	auto* raw = listener.get();
	listener->server.WebSocket(std::string{g_path},
							   [this, raw](const httplib::Request&, httplib::ws::WebSocket& ioSocket) -> void {
								   if (raw->peers.fetch_add(1) >= raw->maxPeers) {
									   raw->peers.fetch_sub(1);
									   ioSocket.close(httplib::ws::CloseStatus::PolicyViolation, "full");
									   return;
								   }
								   auto link = mkShared<Link>();
								   link->server = &ioSocket;
								   const auto peer = track(link);
								   push(connectedEvent(peer));
								   readFrames(*this, peer, ioSocket);
								   {
									   const std::scoped_lock<std::mutex> lock{link->mutex};
									   link->server = nullptr;
								   }
								   drop(peer, DisconnectReason::Graceful);
								   raw->peers.fetch_sub(1);
							   });
	int port = iPort;
	if (iPort == 0)
		port = listener->server.bind_to_any_port("0.0.0.0");
	else if (!listener->server.bind_to_port("0.0.0.0", iPort))
		port = -1;
	if (port <= 0) {
		OWL_CORE_WARN("WebSocket: Cannot listen on port {}.", iPort)
		return unexpected{NetworkError::BindFailed};
	}
	listener->thread = std::thread([raw] -> void { raw->server.listen_after_bind(); });
	m_port = static_cast<uint16_t>(port);
	m_listener = std::move(listener);
	OWL_CORE_INFO("WebSocket: Listening on port {}{}.", m_port, g_path)
	return {};
#endif
}

auto WebSocketTransport::connect(const std::string& iHost, const uint16_t iPort) -> expected<PeerId, NetworkError> {
	if (m_listener != nullptr)
		return unexpected{NetworkError::AlreadyOpen};
	const auto url = std::format("ws://{}:{}{}", iHost, iPort, g_path);
	auto link = mkShared<Link>();
#ifdef OWL_PLATFORM_WEB
	EmscriptenWebSocketCreateAttributes attributes;
	emscripten_websocket_init_create_attributes(&attributes);
	attributes.url = url.c_str();
	const auto socket = emscripten_websocket_new(&attributes);
	if (socket <= 0) {
		OWL_CORE_WARN("WebSocket: Cannot connect to '{}'.", url)
		return unexpected{NetworkError::ConnectFailed};
	}
	link->owner = this;
	link->socket = socket;
	const auto peer = track(link);
	link->peer = peer;
	emscripten_websocket_set_onopen_callback(socket, link.get(), onOpen);
	emscripten_websocket_set_onmessage_callback(socket, link.get(), onMessage);
	emscripten_websocket_set_onclose_callback(socket, link.get(), onClose);
#else
	link->client = mkUniq<httplib::ws::WebSocketClient>(url);
	if (!link->client->is_valid()) {
		OWL_CORE_WARN("WebSocket: Bad server address '{}'.", url)
		return unexpected{NetworkError::ResolveFailed};
	}
	const auto peer = track(link);
	// The reader outlives the peer: the transport keeps its link until close() joins it.
	m_clients.push_back(link);
	auto* raw = link.get();
	link->reader = std::thread([this, raw, peer] -> void {
		if (!raw->client->connect()) {
			drop(peer, DisconnectReason::Refused);
			return;
		}
		push(connectedEvent(peer));
		readFrames(*this, peer, *raw->client);
		drop(peer, DisconnectReason::Graceful);
	});
#endif
	m_connecting = true;
	return peer;
}

void WebSocketTransport::disconnect(const PeerId iPeer) {
	shared<Link> link;
	{
		const std::scoped_lock<std::mutex> lock{m_mutex};
		if (const auto found = m_links.find(iPeer); found != m_links.end())
			link = found->second;
	}
	if (link == nullptr)
		return;
#ifdef OWL_PLATFORM_WEB
	emscripten_websocket_close(link->socket, 1000, "");
#else
	link->close();
#endif
}

void WebSocketTransport::close() {
	std::vector<shared<Link>> links;
	{
		const std::scoped_lock<std::mutex> lock{m_mutex};
		for (const auto& [peer, link]: m_links) links.push_back(link);
	}
#ifdef OWL_PLATFORM_WEB
	for (const auto& link: links) emscripten_websocket_close(link->socket, 1000, "");
#else
	for (const auto& link: links) link->close();
	for (const auto& link: m_clients)
		if (link->reader.joinable())
			link->reader.join();
	m_clients.clear();
	if (m_listener != nullptr) {
		m_listener->server.stop();
		if (m_listener->thread.joinable())
			m_listener->thread.join();
	}
#endif
	const std::scoped_lock<std::mutex> lock{m_mutex};
	m_listener.reset();
	m_links.clear();
	m_events.clear();
	m_port = 0;
	m_connecting = false;
}

auto WebSocketTransport::send(const PeerId iPeer, const uint8_t iChannel, [[maybe_unused]] const Delivery iDelivery,
							  const std::span<const uint8_t> iPayload) -> expected<void, NetworkError> {
	if (iChannel >= g_ChannelCount)
		return unexpected{NetworkError::InvalidChannel};
	shared<Link> link;
	{
		const std::scoped_lock<std::mutex> lock{m_mutex};
		const auto found = m_links.find(iPeer);
		if (found == m_links.end())
			return unexpected{NetworkError::UnknownPeer};
		link = found->second;
		link->bytesSent += iPayload.size() + 1;
	}
	std::string frame(iPayload.size() + 1, '\0');
	frame[0] = static_cast<char>(iChannel);
	std::ranges::copy(iPayload, frame.begin() + 1);
#ifdef OWL_PLATFORM_WEB
	if (!link->open ||
		emscripten_websocket_send_binary(link->socket, frame.data(), frame.size()) != EMSCRIPTEN_RESULT_SUCCESS)
		return unexpected{NetworkError::NotOpen};
#else
	if (!link->send(frame))
		return unexpected{NetworkError::NotOpen};
#endif
	return {};
}

void WebSocketTransport::poll(std::vector<TransportEvent>& oEvents) {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	while (!m_events.empty()) {
		oEvents.push_back(std::move(m_events.front()));
		m_events.pop_front();
	}
}

auto WebSocketTransport::isOpen() const -> bool {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	return m_listener != nullptr || m_connecting;
}

auto WebSocketTransport::getPort() const -> uint16_t { return m_port; }

auto WebSocketTransport::getPeers() const -> std::vector<PeerId> {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	std::vector<PeerId> peers;
	peers.reserve(m_links.size());
	for (const auto& [peer, link]: m_links) peers.push_back(peer);
	return peers;
}

auto WebSocketTransport::getStats(const PeerId iPeer) const -> expected<PeerStats, NetworkError> {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	const auto found = m_links.find(iPeer);
	if (found == m_links.end())
		return unexpected{NetworkError::UnknownPeer};
	return PeerStats{.roundTripMs = 0,
					 .packetLoss = 0.f,
					 .bytesSent = found->second->bytesSent,
					 .bytesReceived = found->second->bytesReceived};
}

void WebSocketTransport::push(TransportEvent iEvent) {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	m_events.push_back(std::move(iEvent));
}

void WebSocketTransport::countReceived(const PeerId iPeer, const uint64_t iBytes) {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	if (const auto found = m_links.find(iPeer); found != m_links.end())
		found->second->bytesReceived += iBytes;
}

void WebSocketTransport::drop(const PeerId iPeer, const DisconnectReason iReason) {
	shared<Link> link;
	{
		const std::scoped_lock<std::mutex> lock{m_mutex};
		const auto found = m_links.find(iPeer);
		if (found == m_links.end())
			return;
		link = std::move(found->second);
		m_links.erase(found);
		m_events.push_back({.kind = TransportEvent::Kind::Disconnected,
							.peer = iPeer,
							.channel = 0,
							.reason = iReason,
							.payload = {}});
	}
}

auto WebSocketTransport::track(shared<Link> iLink) -> PeerId {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	const PeerId peer = m_nextPeer++;
	iLink->peer = peer;
	m_links.emplace(peer, std::move(iLink));
	return peer;
}

}// namespace owl::network::websocket
