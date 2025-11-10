#include "net.h"

#include "server.h"
#include "client.h"
#include "gamestate.h"

void netRecievePackets() {
	if (isServer) {
		serverRecievePackets();
	}
	else {
		clientRecievePackets();
	}
}
