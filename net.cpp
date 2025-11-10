#include "net.h"

#include "server.h"
#include "client.h"

void netRecievePackets() {
	if (isServer) {
		serverRecievePackets();
	}
	else {
		clientRecievePackets();
	}
}
