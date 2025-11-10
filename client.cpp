#include "client.h"
#include "std.h"
#include "gamestate.h"
#include "wintcp.h"
#include "user.h"

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
	ServerboundPacket packet;
	packet.kind = ServerboundPacket::Kind::Register;
	packet.name.copy_from(name);
	clientSendPacket(packet);
}

void clientSendPacket(ServerboundPacket &packet) {
	void serverAcceptPacketLoopback(ServerboundPacket &);
	if (isServer)
		serverAcceptPacketLoopback(packet);
	else
		send(serverSocket, reinterpret_cast<const char *>(&packet), sizeof(packet), 0);
}

#define PCK(kind) \
	case ClientboundPacket::Kind::kind: \
		void clientAcceptPacket##kind(ClientboundPacket &packet); \
		clientAcceptPacket##kind(packet); \
		break
void clientAcceptPacket(ClientboundPacket &packet) {
	switch (packet.kind) {
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
	ClientboundPacket packet;
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
#define PCK(kind) void clientAcceptPacket##kind(ClientboundPacket &packet)
PCK(AddUser) {
	users.Add(packet.name);
}
PCK(Claim) {
	mapSetTile(packet.a, packet.b, packet.c);
}
PCK(Fail) {
	MessageBoxA(nullptr, "Fail", TextFormat("%.*s", 20, packet.failmsg.bytes()), MB_OK);
	assert(0);
}
PCK(Id) {
	userId = packet.id;
}
PCK(Tick) {
	tickTime = 0;
	memset(playerScores, 0, sizeof(playerScores));
	for (int y = 0; y < 25; y++) {
		for (int x = 0; x < 80; x++) {
			int t = mapGetTile(x, y);
			if (t) playerScores[t - 1]++;
		}
	}
}
PCK(Chat) {
	globalChat.copy_from(packet.chat);
	globalChatAuthor = packet.chatAuthor;
}
#undef PCK

void clientClaim(int x, int y) {
	ServerboundPacket packet;
	packet.kind = ServerboundPacket::Kind::Claim;
	packet.a = x;
	packet.b = y;
	clientSendPacket(packet);
}

void clientUpdateChat()
{
	ServerboundPacket packet;
	packet.kind = ServerboundPacket::Kind::Chat;
	packet.chat.copy_from(globalChat);
	clientSendPacket(packet);
}
