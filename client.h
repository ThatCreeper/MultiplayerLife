#pragma once

#include "packet.h"

void clientOpen(const char *host);
void clientRegister(const fixed_string<20> &name);
void clientSendPacket(ServerboundPacketKind packet, const void *data, size_t size);
void clientAcceptPacket(ClientboundPacketKind packet);
void clientAcceptPacketLoopback(ClientboundPacketKind packet, const void *data, size_t size);
void clientRecievePackets();
void clientClaim(int x, int y);
void clientUpdateChat();