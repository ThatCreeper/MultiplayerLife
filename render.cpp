#include "render.h"

#include "gamestate.h"
#include "user.h"

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
		DrawText(TextFormat("%s%s - %d", user.name.bytes(), i == userId ? " (you)" : "", playerScores[i]), x, 500 + y, 20, colors[i]);
	}
}

void renderTicker() {
	DrawCircle(1570, 530, 25, Fade(colors[userId], 1 - (tickTime / maxTickTime) * 2));
	DrawCircleSector({ 1570, 530 }, 25, -90, 360 * tickTime / maxTickTime - 90, 20, colors[userId]);
}

void renderHud() {
	renderUsers();
	renderTicker();
	DrawText(TextFormat("%s", globalChat.bytes()), 50, 50, 40, colors[globalChatAuthor]);
}
