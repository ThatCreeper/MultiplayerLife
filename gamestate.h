#pragma once

extern int mouseTileX;
extern int mouseTileY;
extern int userId;

constexpr float maxTickTime = 3;
extern float tickTime;

extern int tiles[80 * 25];
extern int tilesb[80 * 25]; // Only used on server

int mapWrapX(int x);
int mapWrapY(int y);
int mapGetTile(int x, int y);
void mapSetTile(int x, int y, int id);

int mapGetTileB(int x, int y);
void mapSetTileB(int x, int y, int id);
void mapClearB();
