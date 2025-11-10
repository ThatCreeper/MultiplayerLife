#include "std.h"
#include "server.h"
#include "client.h"
#include "game.h"
#include "render.h"
#include "net.h"

int main() {
	InitWindow(1600, 560, "cade game");
	
	gameInitSteps();

	while (!WindowShouldClose()) {
		gameLife();

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