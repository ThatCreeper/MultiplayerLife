#include "server.h"
#include "wintcp.h"
#include "gamestate.h"
#include "packet.h"
#include "user.h"

Connection loopbackConnection;
reusable_inplace_vector<Connection, 5> clientSockets;

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
	assert(bind(serverSocket, result->ai_addr, result->ai_addrlen) != SOCKET_ERROR);
	freeaddrinfo(result);
	assert(listen(serverSocket, 5) != SOCKET_ERROR);
	// non-blockify
	unsigned long nonblock = 1;
	ioctlsocket(serverSocket, FIONBIO, &nonblock);
}

void serverRecievePackets() {
	assert(isServer);

	if (!clientSockets.full()) {
		SOCKET socket = accept(serverSocket, nullptr, nullptr);
		if (socket != INVALID_SOCKET) {
			unsigned long nonblock = 1;
			ioctlsocket(socket, FIONBIO, &nonblock);
			Connection &connection = *clientSockets.try_emplace_replace();
			connection.id = -1;
			connection.socket = socket;
		}
	}

	ServerboundPacket packet;
	for (Connection &connection : clientSockets) {
		while (true) {
			if (recv(connection.socket, reinterpret_cast<char *>(&packet), sizeof(packet), 0) == SOCKET_ERROR) break;
			serverAcceptPacket(packet, connection);
		}
		if (int e = WSAGetLastError(); e != WSAEWOULDBLOCK) {
			auto user = users.Get(connection.id);
			MessageBoxA(nullptr, TextFormat("%ll: %.*s Disconnect", clientSockets.index(connection), 20, user ? user->name : "(???)"), TextFormat("Conn error: %d", e), MB_OK);
			clientSockets.remove_safe_iter(connection); // This is fine(-ish) because it doesn't rearrange anything
		}
	}
}

void serverSendPacket(ClientboundPacket &packet, Connection &connection) {
	assert(isServer);
	void clientAcceptPacket(ClientboundPacket &);
	if (&connection == &loopbackConnection)
		clientAcceptPacket(packet);
	else
		send(connection.socket, reinterpret_cast<const char *>(&packet), sizeof(packet), 0);
}

void serverSendPacketAll(ClientboundPacket &packet) {
	assert(isServer);
	void clientAcceptPacket(ClientboundPacket &);
	clientAcceptPacket(packet);
	for (Connection &connection : clientSockets) {
		serverSendPacket(packet, connection);
	}
}

template<size_t N>
int fuzzyMedian(int(&values)[N]) {
	int middle = std::clamp(N / 2 + GetRandomValue(-1, 1), 0ull, N);
	int sorted[N];
	std::copy(values, values + N, sorted);
	std::nth_element(sorted, sorted + middle, sorted + N);
	return sorted[middle];
}

void serverLife() {
	if (!isServer) return;
	tickTime += GetFrameTime();
	if (tickTime < maxTickTime) return;
	tickTime = 0;

	mapClearB();
	for (int y = 0; y < 25; y++) {
		for (int x = 0; x < 80; x++) {
#define T mapGetTile
			int ns[] = { T(x - 1,y - 1), T(x  ,y - 1), T(x + 1,y - 1),
				T(x - 1,y),             T(x + 1,y),
				T(x - 1,y + 1), T(x,  y + 1), T(x + 1,y + 1) };
			int t = T(x, y);
#undef T
			int n = 0;
			for (int N : ns) n += N != 0;
			if (t && (n == 2 || n == 3)) mapSetTileB(x, y, t);
			if (!t && n == 3) mapSetTileB(x, y, fuzzyMedian(ns));
		}
	}

	for (int y = 0; y < 25; y++) {
		for (int x = 0; x < 80; x++) {
			int t = mapGetTile(x, y);
			int tb = mapGetTileB(x, y);
			if (t != tb) {
				mapSetTile(x, y, tb);
				ClientboundPacket packet;
				packet.kind = ClientboundPacket::Kind::Claim;
				packet.a = x;
				packet.b = y;
				packet.c = tb;
				serverSendPacketAll(packet);
			}
		}
	}

	ClientboundPacket packet;
	packet.kind = ClientboundPacket::Kind::Tick;
	serverSendPacketAll(packet);
}


void serverAcceptPacketLoopback(ServerboundPacket &packet)
{
	serverAcceptPacket(packet, loopbackConnection);
}

#define PCK(kind) \
	case ServerboundPacket::Kind::kind: \
		void serverAcceptPacket##kind(ServerboundPacket &packet, Connection &connection); \
		serverAcceptPacket##kind(packet, connection); \
		break
void serverAcceptPacket(ServerboundPacket &packet, Connection &connection) {
	assert(isServer);
	switch (packet.kind) {
		PCK(Claim);
		PCK(Register);
	default:
		assert(0);
	}
}
#undef PCK
#define PCK(kind) void serverAcceptPacket##kind(ServerboundPacket &packet, Connection &connection)
PCK(Claim) {
	if (mapGetTile(packet.a, packet.b) == 0) {
		mapSetTile(packet.a, packet.b, connection.id + 1);
		ClientboundPacket p;
		p.kind = ClientboundPacket::Kind::Claim;
		p.a = packet.a;
		p.b = packet.b;
		p.c = connection.id + 1;
		serverSendPacketAll(p);
	}
}
PCK(Register) {
	auto id = users.Add(packet.name);
	if (id) {
		for (Users::User &user : users.users) {
			ClientboundPacket packet;
			packet.kind = ClientboundPacket::Kind::AddUser;
			memcpy(packet.name, user.name, 20);
			if (user.idx == id) {
				serverSendPacketAll(packet);
			}
			else {
				serverSendPacket(packet, connection);
			}
		}

		ClientboundPacket packet;
		packet.kind = ClientboundPacket::Kind::Id;
		packet.id = *id;
		connection.id = *id;
		serverSendPacket(packet, connection);
		for (int y = 0; y < 25; y++) {
			for (int x = 0; x < 80; x++) {
				int tile = mapGetTile(x, y);
				if (tile == 0) continue;
				ClientboundPacket packet;
				packet.kind = ClientboundPacket::Kind::Claim;
				packet.a = x;
				packet.b = y;
				packet.c = tile;
				serverSendPacket(packet, connection);
			}
		}
	}
	else {
		ClientboundPacket packet;
		packet.kind = ClientboundPacket::Kind::Fail;
		strncpy(packet.failmsg, "No space!", 20);
		serverSendPacket(packet, connection);
	}
}
#undef PCK