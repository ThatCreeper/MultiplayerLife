#pragma once

#include "packet.h"

void clientOpen(const char *host);
void clientRegister(char name[20]);
void clientSendPacket(ServerboundPacket &packet);
void clientAcceptPacket(ClientboundPacket &packet);
void clientRecievePackets();
void clientLife();