#include "render.h"

#include "gamestate.h"
#include "user.h"

void renderBoard() {
	for (Particle &particle : particles) {
		particle.size += GetFrameTime() * 80;
	}
	particles.remove_if([](const Particle &particle) { return particle.size >= 20; });

	FORMAPXY(x, y) {
		int tile = mapGetTile(x, y);
		if (tile) DrawRectangle(x * 20, y * 20, 20, 20, colors[tile - 1]);

		DrawRectangleLines(x * 20 + 1, y * 20 + 1, 19, 19,
			(mouseTileX == x && mouseTileY == y) ? YELLOW : Color{40, 40, 40, 255});
	}
	for (const Particle &particle : particles) {
		int s = 20 - particle.size;
		DrawRectangle(particle.x * 20 + (20 - s) / 2, particle.y * 20 + (20 - s) / 2, s, s,
			particle.color == 0 ? BLACK : colors[particle.color - 1]);
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
}
