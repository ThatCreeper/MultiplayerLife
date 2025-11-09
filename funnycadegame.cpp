#undef NDEBUG
#include <cassert>
#include <raylib.h>
#include <raymath.h>
#include <cstring>
#include <optional>
#define CloseWindow WinCloseWindowBreaker
#define ShowCursor WinShowCursorBreaker
#include <WinSock2.h>
#include <WS2tcpip.h>
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#define strncpy strncpy_s

int mouseTileX = 0;
int mouseTileY = 0;
int userId = -1;

constexpr float maxTickTime = 3;
float tickTime = 0;

struct ServerboundPacket {
	enum class Kind {
		Claim,
		Register
	} kind;
	union {
		struct {
			int a;
			int b;
		};
		char name[20];
	};
};
struct ClientboundPacket {
	enum class Kind {
		Claim,
		Id,
		AddUser,
		Fail,
		Tick
	} kind;
	union {
		struct {
			int a;
			int b;
			int c;
		};
		int id;
		char name[20];
		char failmsg[20];
	};
};

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
	};
	User users[6];
	int userc = 0;

	std::optional<int> Add(char name[20]) {
		for (int i = 0; i < userc; i++) {
			if (strncmp(users[i].name, name, 20) == 0) {
				return i;
			}
		}
		if (userc >= 6) return {};
		memcpy(users[userc].name, name, 20);
		return userc++;
	}
	User *Get(int id) {
		if (id < 0 || id >= userc) return nullptr;
		return &users[id];
	}
} users;

struct Connection {
	int id = 0;
	SOCKET socket;
	bool reusable = false;
};

void clientAcceptPacket(ClientboundPacket &packet);
void serverAcceptPacket(ServerboundPacket &packet, Connection &connection);

Connection loopbackConnection;
bool isServer = false;
SOCKET serverSocket;
Connection clientSockets[5];
int clientSocketc = 0;
int clientSocketr = 0;

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
		if (clientSocketc < 5 || clientSocketr > 0) {
			SOCKET socket = accept(serverSocket, nullptr, nullptr);
			if (socket != INVALID_SOCKET) {
				unsigned long nonblock = 1;
				ioctlsocket(socket, FIONBIO, &nonblock);
				Connection *connection = nullptr;
				if (clientSocketr > 0) {
					for (int i = 0; i < clientSocketc; i++) {
						if (clientSockets[i].reusable) {
							connection = &clientSockets[i];
							clientSocketr--;
							break;
						}
					}
				}
				else {
					connection = &clientSockets[clientSocketc++];
				}
				assert(connection != nullptr);
				connection->id = -1;
				connection->socket = socket;
				connection->reusable = false;
			}
		}

		ServerboundPacket packet;
		for (int i = 0; i < clientSocketc; i++) {
			Connection &connection = clientSockets[i];
			if (connection.reusable) continue;
			while (true) {
				if (recv(connection.socket, reinterpret_cast<char *>(&packet), sizeof(packet), 0) == SOCKET_ERROR) break;
				serverAcceptPacket(packet, connection);
			}
			if (int e = WSAGetLastError(); e != WSAEWOULDBLOCK) {
				connection.reusable = true;
				clientSocketr++;
				auto user = users.Get(connection.id);
				MessageBoxA(nullptr, TextFormat("%d: %.*s Disconnect", i, 20, user ? user->name : "(???)"), TextFormat("Conn error: %d", e), MB_OK);
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
	for (int i = 0; i < clientSocketc; i++) {
		serverSendPacket(packet, clientSockets[i]);
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
		for (int i = 0; i < users.userc; i++) {
			ClientboundPacket packet;
			packet.kind = ClientboundPacket::Kind::AddUser;
			memcpy(packet.name, users.users[i].name, 20);
			if (i == id) {
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

void serverLife() {
	if (!isServer) return;
	tickTime += GetFrameTime();
	if (tickTime < maxTickTime) return;
	tickTime = 0;

	mapClearB();
	for (int y = 0; y < 25; y++) {
		for (int x = 0; x < 80; x++) {
			int nA=mapGetTile(x-1,y-1), nB=mapGetTile(x  ,y-1), nC=mapGetTile(x+1,y-1),
				nD=mapGetTile(x-1,y  ), t =mapGetTile(x,  y  ), nF=mapGetTile(x+1,y  ),
				nG=mapGetTile(x-1,y+1), nH=mapGetTile(x,  y+1), nI=mapGetTile(x+1,y+1);
#define N(x) ((n##x)!=0)
			int n = N(A) + N(B) + N(C) + N(D) + N(F) + N(G) + N(H) + N(I);
#undef N
			int na = nA + nB + nC + nD + nF + nG + nH + nI;
			if (t && (n == 2 || n == 3)) mapSetTileB(x, y, t);
			if (!t && n == 3) mapSetTileB(x, y, na / n); // al gore rhythm
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
	for (int i = 0; i < users.userc; i++) {
		int y = 20 * (i % 3);
		int x = (1600 / 4) * (i / 3);
		DrawText(TextFormat("%s%s - %d", users.users[i].name, i == userId ? " (you)" : "", playerScores[i]), x, 500 + y, 20, colors[i]);
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

int main() {
	InitWindow(1600, 560, "cade game");
	
	gameInitSteps();

	while (!WindowShouldClose()) {
		mouseTileX = Clamp(GetMouseX() / 20, 0, 80);
		mouseTileY = Clamp(GetMouseY() / 20, 0, 25);

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