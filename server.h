#pragma once

#include "std.h"
#include "packet.h"

struct Connection {
	int id = 0;
	unsigned __int64 socket;
};

void serverAcceptPacket(ServerboundPacket &packet, Connection &connection);
void netRecievePacketsServer();
void serverSendPacket(ClientboundPacket &packet, Connection &connection);
void serverSendPacketAll(ClientboundPacket &packet);
void serverLife();
