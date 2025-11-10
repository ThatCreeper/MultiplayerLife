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
			MessageBoxA(nullptr, TextFormat("%ull: %s Disconnect", clientSockets.index(connection), user ? user->name.bytes() : "(???)"), TextFormat("Conn error: %d", e), MB_OK);
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
	tickTime += GetFrameTime(); // This also affects the client... ugh
	if (!isServer) return;
	if (tickTime < maxTickTime) return;
	tickTime = 0;

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
	FORMAPXY(x, y) {
		int t = mapGetTileB(x, y);
		int tb = mapGetTileC(x, y);
		if (t != tb) {
			mapSetTileB(x, y, tb);
			ClientboundPacket packet;
			packet.kind = ClientboundPacket::Kind::Claim;
			packet.a = x;
			packet.b = y;
			packet.c = tb;
			serverSendPacketAll(packet);
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
		PCK(Chat);
	default:
		assert(0);
	}
}
#undef PCK
#define PCK(kind) void serverAcceptPacket##kind(ServerboundPacket &packet, Connection &connection)
PCK(Claim) {
	if (mapGetTileB(packet.a, packet.b) == 0) {
		mapSetTileB(packet.a, packet.b, connection.id + 1);
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
			packet.name.copy_from(user.name);
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
		packet.failmsg.copy_from("No space!");
		serverSendPacket(packet, connection);
	}
}
PCK(Chat) {
	ClientboundPacket p;
	p.kind = ClientboundPacket::Kind::Chat;
	p.chat.copy_from(packet.chat);
	p.chatAuthor = connection.id;
	serverSendPacketAll(p);
}
#undef PCK