#include "game.h"

#include "gamestate.h"
#include "client.h"
#include "server.h"
#include "user.h"
#include "render.h"

template <size_t N>
void gameTextEntry(fixed_string<N> &out, const char *prefix, Color bg, Color fg);

void gameInitSteps() {
	gamePickIsServer();

	fixed_string<20>::cstr name;
	gameTextEntry(name, "Name", BLUE, WHITE);

	if (isServer) serverOpen();
	else clientOpen("127.0.0.1");

	clientRegister(name.fake_ref_short());
}

void gameLife() {
	mouseTileX = Clamp(GetMouseX() / 20, 0, 80);
	mouseTileY = Clamp(GetMouseY() / 20, 0, 25);

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		clientClaim(mouseTileX, mouseTileY);
	}

	gameUpdateChat();
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

template <size_t N>
void gameTextEntry(fixed_string<N> &out, const char *prefix, Color bg, Color fg) {
	while (true) {
		// Get char pressed (unicode character) on the queue
		int key = GetCharPressed();

		// Check if more characters have been pressed on the same frame
		while (key > 0)
		{
			// NOTE: Only allow keys in range [32..125]
			if ((key >= 32) && (key <= 125))
			{
				out.push_cback(key);
			}

			key = GetCharPressed();  // Check next character in the queue
		}

		if (IsKeyPressed(KEY_BACKSPACE))
		{
			out.pop_cback();
		}

		if (IsKeyPressed(KEY_ENTER)) break;

		BeginDrawing();
		ClearBackground(bg);

		DrawText(std::format("{}: \"{}\"", prefix, out.bytes()), 0, 0, 40, fg);

		EndDrawing();
	}
}

void gameUpdateChat() {
	// Get char pressed (unicode character) on the queue
	int key = GetCharPressed();
	bool updated = false;
	int len = globalChat.length();

	// Check if more characters have been pressed on the same frame
	while (key > 0)
	{
		// NOTE: Only allow keys in range [32..125]
		if ((key >= 32) && (key <= 125) && (!globalChat.full()))
		{
			globalChat.push_cback(key);
			updated = true;
		}

		key = GetCharPressed();  // Check next character in the queue
	}

	if (IsKeyPressed(KEY_BACKSPACE))
	{
		globalChat.pop_cback();
		updated = true;
	}

	if (updated) {
		SetWindowTitle(globalChat.bytes());
		clientUpdateChat();
	}
}

#define PCK(kind) void clientAcceptPacket##kind()
#define PCKD(kind) ClientboundPacket##kind packet; clientRecieve(&packet, sizeof(packet))
void clientRecieve(void *, size_t);
extern "C" int MessageBoxA(void *, const char *, const char *, int);
#define MB_OK 0x00000000L
PCK(AddUser) {
	PCKD(AddUser);
	users.Add(packet.name);
}
PCK(Claim) {
	PCKD(Claim);
	Particle p;
	p.size = 0;
	p.x = packet.x;
	p.y = packet.y;
	p.color = mapGetTile(p.x, p.y);
	particles.replace_push_replace(p);
	mapSetTile(packet.x, packet.y, packet.color);
}
PCK(Fail) {
	PCKD(Fail);
	auto str = packet.failmsg.elongate();
	MessageBoxA(nullptr, "Fail", str.c_str(), MB_OK);
	assert(0);
}
PCK(Id) {
	PCKD(Id);
	userId = packet.id;
}
PCK(Tick) {
	PCKD(Tick);
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
	PCKD(Chat);
	globalChat.copy_from(packet.chat);
	globalChatAuthor = packet.chatauthor;
	SetWindowTitle(globalChat.bytes());
}