#include "gamestate.h"

#include "std.h"

int mouseTileX = 0;
int mouseTileY = 0;
size_t userId = (size_t) - 1;

float tickTime = 0;

int tiles[80 * 25];
int tilesb[80 * 25]; // Only used on server
int tilesc[80 * 25]; // Only used on server

reusable_inplace_vector<Particle, 200> particles;

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
fixed_string<50>::cstr globalChat;
size_t globalChatAuthor = 0;

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
	x = mapWrapX(x);
	y = mapWrapY(y);
	return tilesb[y * 80 + x];
}

void mapSetTileB(int x, int y, int id) {
	x = mapWrapX(x);
	y = mapWrapY(y);
	tilesb[y * 80 + x] = id;
}

int mapGetTileC(int x, int y) {
	x = mapWrapX(x);
	y = mapWrapY(y);
	return tilesc[y * 80 + x];
}

void mapSetTileC(int x, int y, int id) {
	x = mapWrapX(x);
	y = mapWrapY(y);
	tilesc[y * 80 + x] = id;
}

void mapClearC() {
	memset(tilesc, 0, sizeof(tilesc));
}
