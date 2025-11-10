#pragma once

#include "std.h"

void gameTextEntry(char *out, int count, const char *prefix, Color bg, Color fg);

void gamePickIsServer();
void gameInitSteps();
void gameLife();