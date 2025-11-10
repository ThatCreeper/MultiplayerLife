#include "game.h"

#include "gamestate.h"
#include "client.h"
#include "server.h"

void gameTextEntry(char *out, int count, const char *prefix, Color bg, Color fg) {
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
	gameTextEntry(name, 20, "Name", BLUE, WHITE);

	if (isServer) serverOpen();
	else clientOpen("127.0.0.1");

	clientRegister(name);
}

void gameLife() {
	mouseTileX = Clamp(GetMouseX() / 20, 0, 80);
	mouseTileY = Clamp(GetMouseY() / 20, 0, 25);
}
