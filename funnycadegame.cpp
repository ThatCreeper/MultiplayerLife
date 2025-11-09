#include "std.h"
#include "wintcp.h"
#include "packet.h"
#include "gamestate.h"
#include "user.h"
#include "server.h"

void clientAcceptPacket(ClientboundPacket &packet);

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

void clientSendPacket(ServerboundPacket &packet) {
	if (isServer)
		serverAcceptPacket(packet, loopbackConnection);
	else
		send(serverSocket, reinterpret_cast<const char *>(&packet), sizeof(packet), 0);
}

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