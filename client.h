#pragma once

#include "packet.h"

void clientOpen(const char *host);
void clientRegister(const fixed_string<20> &name);
void clientSendPacket(ServerboundPacket &packet);
void clientAcceptPacket(ClientboundPacket &packet);
void clientRecievePackets();
void clientClaim(int x, int y);
void clientUpdateChat();