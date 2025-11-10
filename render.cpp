#include "render.h"

#include "gamestate.h"
#include "user.h"

void DrawText(std::string_view text, int x, int y, int size, Color color) {
	DrawText(text.data(), x, y, size, color);
}

void renderBoard() {
	for (Particle &particle : particles) {
		particle.size += GetFrameTime() * 80;
	}
	particles.remove_if([](const Particle &particle) { return particle.size >= 20; });

	FORMAPXY(x, y) {
		int tile = mapGetTile(x, y);
		if (tile) DrawRectangle(x * 20, y * 20, 20, 20, colors[tile - 1]);
	}
	for (const Particle &particle : particles) {
		int s = 20 - particle.size;
		DrawRectangle(particle.x * 20 + (20 - s) / 2, particle.y * 20 + (20 - s) / 2, s, s,
			particle.color == 0 ? BLACK : colors[particle.color - 1]);
	}

	for (int y = 0; y <= 25; y++)
		DrawLine(0, y * 20, 1600, y * 20, Color{ 40, 40, 40, 255 });
	for (int x = 0; x <= 80; x++)
		DrawLine(x * 20, 0, x * 20, 500, Color{ 40, 40, 40, 255 });

	DrawRectangleLines(mouseTileX * 20 + 1, mouseTileY * 20 + 1, 19, 19, YELLOW);
}

void renderUsers() {
	for (const Users::User &user : users.users) {
		int i = user.idx;
		int y = 20 * (i % 3);
		int x = (1600 / 4) * (i / 3);
		DrawText(std::format("{}{} - {}", user.name.bytes(), i == userId ? " (you)" : "", playerScores[i]), x, 500 + y, 20, colors[i]);
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
