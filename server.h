#pragma once

#include "std.h"
#include "packet.h"

struct Connection {
	size_t id = 0;
	unsigned __int64 socket = 0;
};

void serverAcceptPacket(ServerboundPacketKind packet, Connection &connection);
void serverAcceptPacketLoopback(ServerboundPacketKind packet, const void *data, size_t size);
void serverRecievePackets();
void serverSendPacket(ClientboundPacketKind packet, const void *data, size_t size, Connection &connection);
void serverSendPacketAll(ClientboundPacketKind packet, const void *data, size_t size);
void serverLife();
void serverOpen();
