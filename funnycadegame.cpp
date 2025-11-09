#include "std.h"
#include "wintcp.h"
#include "packet.h"

int mouseTileX = 0;
int mouseTileY = 0;
int userId = -1;

constexpr float maxTickTime = 3;
float tickTime = 0;

int tiles[80 * 25];
int mapWrapX(int x) {
	while (x < 0) x += 80;
	while (x >= 80) x -= 80;
	return x;
}
int mapWrapY(int y) {
	while (y < 0) y += 25;
	while (y >= 25) y -= 25;
	return y;
}
int mapGetTile(int x, int y) {
	x = mapWrapX(x);
	y = mapWrapY(y);
	return tiles[y * 80 + x];
}
void mapSetTile(int x, int y, int id) {
	x = mapWrapX(x);
	y = mapWrapY(y);
	tiles[y * 80 + x] = id;
}
int tilesb[80 * 25];
int mapGetTileB(int x, int y) {
	return tilesb[y * 80 + x];
}
void mapSetTileB(int x, int y, int id) {
	tilesb[y * 80 + x] = id;
}
void mapClearB() {
	memset(tilesb, 0, sizeof(tilesb));
}

struct Users {
	struct User {
		char name[20];
		int idx;
	};
	inplace_vector<User, 6> users;

	std::optional<int> Add(char name[20]) {
		for (int i = 0; i < users.size(); i++) {
			if (strncmp(users.try_at(i)->name, name, 20) == 0) {
				return i;
			}
		}
		if (users.full()) return {};
		int idx = users.size();
		User &user = *users.try_emplace_back();
		memcpy(user.name, name, 20);
		user.idx = idx;
		return idx;
	}
	optional_ref<User> Get(int id) {
		return users.try_at(id);
	}
	size_t size() {
		return users.size();
	}
} users;

struct Connection {
	int id = 0;
	SOCKET socket;
};

void clientAcceptPacket(ClientboundPacket &packet);
void serverAcceptPacket(ServerboundPacket &packet, Connection &connection);

Connection loopbackConnection;
bool isServer = false;
SOCKET serverSocket;
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

void netRecievePackets() {
	if (isServer) {
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
	else {
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
}

void serverSendPacket(ClientboundPacket &packet, Connection &connection) {
	assert(isServer);
	if (&connection == &loopbackConnection)
		clientAcceptPacket(packet);
	else
		send(connection.socket, reinterpret_cast<const char *>(&packet), sizeof(packet), 0);
}

void serverSendPacketAll(ClientboundPacket &packet) {
	assert(isServer);
	clientAcceptPacket(packet);
	for (Connection &connection : clientSockets) {
		serverSendPacket(packet, connection);
	}
}

void clientSendPacket(ServerboundPacket &packet) {
	if (isServer)
		serverAcceptPacket(packet, loopbackConnection);
	else
		send(serverSocket, reinterpret_cast<const char *>(&packet), sizeof(packet), 0);
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
int playerScores[6] = { 0 };

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
	default:
		assert(0);
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
	MessageBoxA(nullptr, "Fail", TextFormat("%.*s", 20, packet.failmsg), MB_OK);
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
#undef PCK

template<size_t N>
int fuzzyMedian(int (&values)[N]) {
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
			int ns[] = { T(x-1,y-1), T(x  ,y-1), T(x+1,y-1),
			             T(x-1,y  ),             T(x+1,y  ),
			             T(x-1,y+1), T(x,  y+1), T(x+1,y+1)};
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

Color colors[] = {
	RED,
	GREEN,
	BLUE,
	PURPLE,
	ORANGE,
	GRAY
};

void textentry(char *out, int count, const char *prefix, Color bg, Color fg) {
	int letterCount = strnlen(out, count);
	while (true) {
		// Get char pressed (unicode character) on the queue
		int key = GetCharPressed();

		// Check if more characters have been pressed on the same frame
		while (key > 0)
		{
			// NOTE: Only allow keys in range [32..125]
			if ((key >= 32) && (key <= 125) && (letterCount < count))
			{
				out[letterCount] = (char)key;
				letterCount++;
			}

			key = GetCharPressed();  // Check next character in the queue
		}

		if (IsKeyPressed(KEY_BACKSPACE))
		{
			letterCount--;
			if (letterCount < 0) letterCount = 0;
			out[letterCount] = '\0';
		}

		if (IsKeyPressed(KEY_ENTER)) break;

		BeginDrawing();
		ClearBackground(bg);

		DrawText(TextFormat("%s: \"%.*s\"", prefix, letterCount, out), 0, 0, 40, fg);

		EndDrawing();
	}
}

void clientLife() {
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		ServerboundPacket packet;
		packet.kind = ServerboundPacket::Kind::Claim;
		packet.a = mouseTileX;
		packet.b = mouseTileY;
		clientSendPacket(packet);
	}
}

void renderBoard() {
	for (int y = 0; y < 25; y++) {
		for (int x = 0; x < 80; x++) {
			int tile = mapGetTile(x, y);
			if (tile) DrawRectangle(x * 20, y * 20, 20, 20, colors[tile - 1]);

			DrawRectangleLines(x * 20 + 1, y * 20 + 1, 19, 19,
				(mouseTileX == x && mouseTileY == y) ? YELLOW : DARKGRAY);
		}
	}
}

void renderUsers() {
	for (const Users::User &user : users.users) {
		int i = user.idx;
		int y = 20 * (i % 3);
		int x = (1600 / 4) * (i / 3);
		DrawText(TextFormat("%s%s - %d", user.name, i == userId ? " (you)" : "", playerScores[i]), x, 500 + y, 20, colors[i]);
	}
}

void renderTicker() {
	DrawCircle(1570, 530, 25, Fade(colors[userId], 1 - (tickTime / maxTickTime) * 2));
	DrawCircleSector({ 1570, 530 }, 25, -90, 360 * tickTime / maxTickTime - 90, 20, colors[userId]);
}

void renderHud() {
	renderUsers();
	renderTicker();
}

void clientRegister(char name[20]) {
	assert(userId == -1);
	ServerboundPacket packet;
	packet.kind = ServerboundPacket::Kind::Register;
	strncpy(packet.name, name, 20);
	clientSendPacket(packet);
}

void gamePickIsServer() {
	while (true) {
		if (IsKeyPressed(KEY_S)) {
			isServer = true;
			break;
		}
		if (IsKeyPressed(KEY_C)) {
			isServer = false;
			break;
		}

		BeginDrawing();
		ClearBackground(RED);
		DrawText("Press S to serve or C to connect", 0, 0, 40, WHITE);
		EndDrawing();
	}
}

void gameInitSteps() {
	gamePickIsServer();

	char name[20] = "\0";
	textentry(name, 20, "Name", BLUE, WHITE);

	if (isServer) serverOpen();
	else clientOpen("127.0.0.1");

	clientRegister(name);
}

void engineLife() {
	mouseTileX = Clamp(GetMouseX() / 20, 0, 80);
	mouseTileY = Clamp(GetMouseY() / 20, 0, 25);
}

int main() {
	InitWindow(1600, 560, "cade game");
	
	gameInitSteps();

	while (!WindowShouldClose()) {
		engineLife();

		clientLife();

		netRecievePackets();
		serverLife();

		BeginDrawing();
		ClearBackground(BLACK);

		renderBoard();
		renderHud();

		EndDrawing();
	}

	CloseWindow();
}