#pragma once

#define CloseWindow WinCloseWindowBreaker
#define ShowCursor WinShowCursorBreaker
#include <WinSock2.h>
#include <WS2tcpip.h>
#undef CloseWindow
#undef ShowCursor
#undef DrawText