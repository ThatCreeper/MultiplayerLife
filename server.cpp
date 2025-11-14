#include "server.h"
#include "wintcp.h"
#include "gamestate.h"
#include "packet.h"
#include "user.h"

Connection loopbackConnection;
reusable_inplace_vector<Connection, 5> clientSockets;
const void *serverLoopbackData;
size_t serverLoopbackSize;

#define SEND_A_PACKET_ALL(kind, pck) serverSendPacketAll(ClientboundPacketKind::kind, &pck, sizeof(pck))
#define SEND_A_PACKET(kind, pck, conn) serverSendPacket(ClientboundPacketKind::kind, &pck, sizeof(pck), conn)
#define SEND_PACKET(kind, conn, ...) { ClientboundPacket##kind pkt_p __VA_ARGS__; SEND_A_PACKET(kind, pkt_p, conn); }
#define SEND_PACKET_ALL(kind, ...) { ClientboundPacket##kind pkt_p __VA_ARGS__; SEND_A_PACKET_ALL(kind, pkt_p); }

void serverOpen() {
	WSADATA wsaData;
	assert(!WSAStartup(MAKEWORD(2, 2), &wsaData));
	addrinfo *result = nullptr;
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	assert(!getaddrinfo(nullptr, "9142", &hints, &result));
	serverSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	assert(serverSocket != INVALID_SOCKET);
	assert(bind(serverSocket, result->ai_addr, static_cast<int>(result->ai_addrlen)) != SOCKET_ERROR);
	freeaddrinfo(result);
	assert(listen(serverSocket, 5) != SOCKET_ERROR);
	// non-blockify
	unsigned long nonblock = 1;
	ioctlsocket(serverSocket, FIONBIO, &nonblock);
}

void serverAcceptConnections() {
	if (clientSockets.full()) return;
	SOCKET socket = accept(serverSocket, nullptr, nullptr);
	if (socket != INVALID_SOCKET) {
		unsigned long nonblock = 1;
		ioctlsocket(socket, FIONBIO, &nonblock);
		Connection &connection = *clientSockets.try_emplace_replace();
		connection.id = -1;
		connection.socket = socket;
	}
}

void serverRecievePackets() {
	assert(isServer);

	serverAcceptConnections();

	ServerboundPacketKind packet;
	for (Connection &connection : clientSockets) {
		while (true) {
			if (recv(connection.socket, reinterpret_cast<char *>(&packet), sizeof(packet), 0) == SOCKET_ERROR) break;
			serverAcceptPacket(packet, connection);
		}
		if (int e = WSAGetLastError(); e != WSAEWOULDBLOCK) {
			auto user = users.Get(connection.id);
			auto err = std::format("{}: {} Disconnect", *clientSockets.index(connection), user ? user->name.bytes() : "(???)");
			auto desc = std::format("Conn error: {}", e);
			MessageBoxA(nullptr, err.c_str(), desc.c_str(), MB_OK);
			clientSockets.remove_safe_iter(connection); // This is fine(-ish) because it doesn't rearrange anything
		}
	}
}

void serverSendPacket(ClientboundPacketKind packet, const void *data, size_t size, Connection &connection) {
	assert(isServer);
	std::println("S-> {} (->{})\n  \\_{}", (int)packet, connection.id, size);
	void clientAcceptPacketLoopback(ClientboundPacketKind, const void *, size_t);
	if (&connection == &loopbackConnection) {
		clientAcceptPacketLoopback(packet, data, size);
	}
	else {
		send(connection.socket, reinterpret_cast<const char *>(&packet), sizeof(packet), 0);
		send(connection.socket, reinterpret_cast<const char *>(data), static_cast<int>(size), 0);
	}
}

void serverSendPacketAll(ClientboundPacketKind packet, const void *data, size_t size) {
	assert(isServer);
	serverSendPacket(packet, data, size, loopbackConnection);
	for (Connection &connection : clientSockets) {
		serverSendPacket(packet, data, size, connection);
	}
}

void serverRecieve(void *out, size_t size, Connection &connection) {
	std::println("  \\_{}", size);
	if (&connection == &loopbackConnection) {
		assert(serverLoopbackSize == size);
		std::copy((char *)serverLoopbackData, (char *)serverLoopbackData + size, (char *)out);
	}
	else {
		recv(connection.socket, (char *)out, static_cast<int>(size), 0);
	}
}

void serverUpdateTilesC() {
	mapClearC();

	FORMAPXY(x, y) {
#define T(a,b) mapGetTileB(x+a,y+b)
		int ns[] = { T(-1,-1), T(0,-1), T(1,-1),
					 T(-1,0),  T(1,0),
					 T(-1,1),  T(0,1),  T(1,1) };
		int t = T(0, 0);
#undef T
		int n = 0;
		int nT = 0;
		for (int N : ns) n += (N != 0);
		for (int N : ns) nT += N;
		if (t && (n == 2 || n == 3)) mapSetTileC(x, y, t);
		if (!t && n == 3) mapSetTileC(x, y, nT / n);
	}
}

void serverPropagateChangesTilesC() {
	FORMAPXY(x, y) {
		int t = mapGetTileB(x, y);
		int tb = mapGetTileC(x, y);
		if (t == tb) continue;
		mapSetTileB(x, y, tb);
		SEND_PACKET_ALL(Claim, { .x = x, .y = y, .color = tb });
	}
}

void serverLife() {
	tickTime += GetFrameTime(); // This also affects the client... ugh
	if (!isServer) return;
	if (tickTime < maxTickTime) return;
	tickTime = 0;

	serverUpdateTilesC();
	serverPropagateChangesTilesC();

	SEND_PACKET_ALL(Tick, {});
}

void serverSendMapInitialConfiguration(Connection &connection) {
	FORMAPXY(x, y) {
		int tile = mapGetTileB(x, y);
		if (tile == 0) continue;
		ClientboundPacketClaim packet;
		packet.x = x;
		packet.y = y;
		packet.color = tile;
		SEND_A_PACKET(Claim, packet, connection);
	}
}

void serverAcceptPacketLoopback(ServerboundPacketKind packet, const void *data, size_t size)
{
	serverLoopbackData = data;
	serverLoopbackSize = size;
	serverAcceptPacket(packet, loopbackConnection);
}

#define PCK(kind) \
	case ServerboundPacketKind::kind: \
		void serverAcceptPacket##kind(Connection &connection); \
		serverAcceptPacket##kind(connection); \
		break
void serverAcceptPacket(ServerboundPacketKind packet, Connection &connection) {
	assert(isServer);
	std::println("S<- {}", (int)packet);
	switch (packet) {
		PCK(Claim);
		PCK(Register);
		PCK(Chat);
	default:
		assert(0);
	}
}
#undef PCK
#define PCK(kind) void serverAcceptPacket##kind(Connection &connection)
#define PCKD(kind) ServerboundPacket##kind packet; serverRecieve(&packet, sizeof(packet), connection)
PCK(Claim) {
	PCKD(Claim);
	if (mapGetTileB(packet.x, packet.y) != 0) return; // Prevent reclaiming others' tiles
	int color = static_cast<int>(connection.id) + 1;
	mapSetTileB(packet.x, packet.y, color);
	SEND_PACKET_ALL(Claim, { .x = packet.x, .y = packet.y, .color = color });
}
PCK(Register) {
	PCKD(Register);
	auto id = users.Add(packet.name);
	if (!id) {
		SEND_PACKET(Fail, connection, { .failmsg{"No space!"} });
		return;
	}
	for (Users::User &user : users.users) {
		ClientboundPacketAddUser packet{};
		packet.name.copy_from(user.name);
		if (user.idx == id) {
			SEND_A_PACKET_ALL(AddUser, packet);
		}
		else {
			SEND_A_PACKET(AddUser, packet, connection);
		}
	}

	connection.id = *id;
	SEND_PACKET(Id, connection, { .id = connection.id });
	serverSendMapInitialConfiguration(connection);
}
PCK(Chat) {
	PCKD(Chat);
	ClientboundPacketChat p;
	p.chat.copy_from(packet.chat);
	p.chatauthor = connection.id;
	SEND_A_PACKET_ALL(Chat, p);
}
#undef PCK