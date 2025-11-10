#include "server.h"
#include "wintcp.h"
#include "gamestate.h"
#include "packet.h"
#include "user.h"

Connection loopbackConnection;
reusable_inplace_vector<Connection, 5> clientSockets;
const void *serverLoopbackData;
size_t serverLoopbackSize;

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

	ServerboundPacketKind packet;
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

void serverSendPacket(ClientboundPacketKind packet, const void *data, size_t size, Connection &connection) {
	assert(isServer);
	void clientAcceptPacketLoopback(ClientboundPacketKind, const void *, size_t);
	if (&connection == &loopbackConnection)
		clientAcceptPacketLoopback(packet, data, size);
	else {
		send(connection.socket, reinterpret_cast<const char *>(&packet), sizeof(packet), 0);
		send(connection.socket, reinterpret_cast<const char *>(data), size, 0);
	}
}

void serverSendPacketAll(ClientboundPacketKind packet, const void *data, size_t size) {
	assert(isServer);
	void clientAcceptPacketLoopback(ClientboundPacketKind, const void *, size_t);
	clientAcceptPacketLoopback(packet, data, size);
	for (Connection &connection : clientSockets) {
		serverSendPacket(packet, data, size, connection);
	}
}

void serverRecieve(void *out, size_t size, Connection &connection) {
	if (&connection == &loopbackConnection) {
		assert(serverLoopbackSize == size);
		std::copy((char *)serverLoopbackData, (char *)serverLoopbackData + size, (char *)out);
	}
	else {
		recv(connection.socket, (char *)out, size, 0);
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
			ClientboundPacketClaim packet;
			packet.x = x;
			packet.y = y;
			packet.color = tb;
			serverSendPacketAll(ClientboundPacketKind::Claim, &packet, sizeof(packet));
		}
	}

	ClientboundPacketTick packet;
	serverSendPacketAll(ClientboundPacketKind::Tick, &packet, sizeof(packet));
}

void serverSendMap(Connection &connection) {
	FORMAPXY(x, y) {
		int tile = mapGetTile(x, y);
		if (tile == 0) continue;
		ClientboundPacketClaim packet;
		packet.x = x;
		packet.y = y;
		packet.color = tile;
		serverSendPacket(ClientboundPacketKind::Claim, &packet, sizeof(packet), connection);
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
	if (mapGetTileB(packet.x, packet.y) == 0) {
		mapSetTileB(packet.x, packet.y, connection.id + 1);
		ClientboundPacketClaim p;
		p.x = packet.x;
		p.y = packet.y;
		p.color = connection.id + 1;
		serverSendPacketAll(ClientboundPacketKind::Claim, &p, sizeof(p));
	}
}
PCK(Register) {
	PCKD(Register);
	auto id = users.Add(packet.name);
	if (id) {
		for (Users::User &user : users.users) {
			ClientboundPacketAddUser packet;
			packet.name.copy_from(user.name);
			if (user.idx == id) {
				serverSendPacketAll(ClientboundPacketKind::AddUser, &packet, sizeof(packet));
			}
			else {
				serverSendPacket(ClientboundPacketKind::AddUser, &packet, sizeof(packet), connection);
			}
		}

		ClientboundPacketId packet;
		packet.id = *id;
		connection.id = *id;
		serverSendPacket(ClientboundPacketKind::Id, &packet, sizeof(packet), connection);
		serverSendMap(connection);
	}
	else {
		ClientboundPacketFail packet;
		packet.failmsg.copy_from("No space!");
		serverSendPacket(ClientboundPacketKind::Fail, &packet, sizeof(packet), connection);
	}
}
PCK(Chat) {
	PCKD(Chat);
	ClientboundPacketChat p;
	p.chat.copy_from(packet.chat);
	p.chatauthor = connection.id;
	serverSendPacketAll(ClientboundPacketKind::Chat, &p, sizeof(p));
}
#undef PCK