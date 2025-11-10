#include "gamestate.h"

#include "std.h"

int mouseTileX = 0;
int mouseTileY = 0;
int userId = -1;

float tickTime = 0;

int tiles[80 * 25];
int tilesb[80 * 25]; // Only used on server

bool isServer = false;
unsigned __int64 serverSocket;
Color colors[] = {
	RED,
	GREEN,
	BLUE,
	PURPLE,
	ORANGE,
	GRAY
};
fixed_string<51> globalChat{ 0 };
int globalChatAuthor = 0;

int playerScores[6] = { 0 };

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

int mapGetTileB(int x, int y) {
	return tilesb[y * 80 + x];
}

void mapSetTileB(int x, int y, int id) {
	tilesb[y * 80 + x] = id;
}

void mapClearB() {
	memset(tilesb, 0, sizeof(tilesb));
}
