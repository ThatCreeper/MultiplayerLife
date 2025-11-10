#pragma once

#include "std.h"

extern int mouseTileX;
extern int mouseTileY;
extern int userId;

constexpr float maxTickTime = 2;
extern float tickTime;

extern int tiles[];
extern int tilesb[]; // Only used on server
#define FORMAPXY(x, y) for (int y = 0; y < 25; y++) for (int x = 0; x < 80; x++)

extern bool isServer;
extern unsigned __int64 serverSocket;
extern Color colors[];
extern fixed_string<51> globalChat;
extern int globalChatAuthor;

extern int playerScores[6];

int mapWrapX(int x);
int mapWrapY(int y);
int mapGetTile(int x, int y);
void mapSetTile(int x, int y, int id);

int mapGetTileB(int x, int y);
void mapSetTileB(int x, int y, int id);
void mapClearB();
