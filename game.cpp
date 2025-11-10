#include "game.h"

#include "gamestate.h"
#include "client.h"
#include "server.h"

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

		DrawText(TextFormat("%s: \"%s\"", prefix, out.bytes()), 0, 0, 40, fg);

		EndDrawing();
	}
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

	fixed_string<21> name{ 0 };
	gameTextEntry(name, "Name", BLUE, WHITE);

	if (isServer) serverOpen();
	else clientOpen("127.0.0.1");

	clientRegister(name.fake_ref_short());
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

	if (updated) clientUpdateChat();
}

void gameLife() {
	mouseTileX = Clamp(GetMouseX() / 20, 0, 80);
	mouseTileY = Clamp(GetMouseY() / 20, 0, 25);

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		clientClaim(mouseTileX, mouseTileY);
	}

	gameUpdateChat();
}
