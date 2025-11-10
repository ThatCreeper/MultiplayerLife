#include "client.h"
#include "std.h"
#include "gamestate.h"
#include "wintcp.h"

const void *clientLoopbackData;
size_t clientLoopbackSize;

#define SEND_A_PACKET(kind, pck) clientSendPacket(ServerboundPacketKind::kind, &pck, sizeof(pck))
#define SEND_PACKET(kind, ...) { ServerboundPacket##kind pck_s __VA_ARGS__; SEND_A_PACKET(kind, pck_s); }

void clientOpen(const char *host) {
	WSADATA wsaData;
	assert(!WSAStartup(MAKEWORD(2, 2), &wsaData));
	addrinfo *result = nullptr;
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	assert(!getaddrinfo(host, "9142", &hints, &result));
	serverSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	assert(serverSocket != INVALID_SOCKET);
	assert(connect(serverSocket, result->ai_addr, result->ai_addrlen) != SOCKET_ERROR);
	freeaddrinfo(result);
	// non-blockify
	unsigned long nonblock = 1;
	ioctlsocket(serverSocket, FIONBIO, &nonblock);
}

void clientRegister(const fixed_string<20> &name) {
	assert(userId == -1);
	ServerboundPacketRegister packet{};
	packet.name.copy_from(name);
	SEND_A_PACKET(Register, packet);
}

void clientSendPacket(ServerboundPacketKind packet, const void *data, size_t size) {
	void serverAcceptPacketLoopback(ServerboundPacketKind, const void *, size_t);
	if (isServer)
		serverAcceptPacketLoopback(packet, data, size);
	else {
		send(serverSocket, reinterpret_cast<const char *>(&packet), sizeof(packet), 0);
		send(serverSocket, reinterpret_cast<const char *>(data), size, 0);
	}
}

void clientAcceptPacketLoopback(ClientboundPacketKind packet, const void *data, size_t size) {
	clientLoopbackData = data;
	clientLoopbackSize = size;
	clientAcceptPacket(packet);
}

void clientRecieve(void *out, size_t size) {
	//std::println("<- {}", size);
	if (isServer) {
		assert(size == clientLoopbackSize);
		std::copy((char *)clientLoopbackData, (char *)clientLoopbackData + size, (char *)out);
	}
	else {
		recv(serverSocket, (char *)out, size, 0);
	}
}

#define PCK(kind) \
	case ClientboundPacketKind::kind: \
		void clientAcceptPacket##kind(); \
		clientAcceptPacket##kind(); \
		break
void clientAcceptPacket(ClientboundPacketKind packet) {
	//std::println("{}", (int)packet);
	switch (packet) {
		PCK(AddUser);
		PCK(Claim);
		PCK(Fail);
		PCK(Id);
		PCK(Tick);
		PCK(Chat);
	default:
		assert(0);
	}
}
void clientRecievePackets()
{
	ClientboundPacketKind packet;
	while (true) {
		if (recv(serverSocket, reinterpret_cast<char *>(&packet), sizeof(packet), 0) == SOCKET_ERROR) break;
		clientAcceptPacket(packet);
	}
	if (int e = WSAGetLastError(); e != WSAEWOULDBLOCK) {
		MessageBoxA(nullptr, "srv conn: Error", TextFormat("Conn error: %d", e), MB_OK);
		exit(1);
	}
}
#undef PCK

void clientClaim(int x, int y) {
	SEND_PACKET(Claim, { .x = x, .y = y });
}

void clientUpdateChat()
{
	ServerboundPacketChat packet{};
	packet.chat.copy_from(globalChat);
	SEND_A_PACKET(Chat, packet);
}
