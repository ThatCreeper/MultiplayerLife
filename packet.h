#pragma once

#include "std.h"

enum class ServerboundPacketKind {
	Claim,
	Register,
	Chat
};
struct ServerboundPacketClaim {
	int x;
	int y;
};
struct ServerboundPacketRegister {
	fixed_string<20> name;
};
struct ServerboundPacketChat {
	fixed_string<50> chat;
};

enum class ClientboundPacketKind {
	Claim,
	Id,
	AddUser,
	Fail,
	Tick,
	Chat
};
struct ClientboundPacketClaim {
	int x;
	int y;
	int color;
};
struct ClientboundPacketId {
	size_t id;
};
struct ClientboundPacketAddUser {
	fixed_string<20> name;
};
struct ClientboundPacketFail {
	fixed_string<20> failmsg;
};
struct ClientboundPacketTick {
};
struct ClientboundPacketChat {
	size_t chatauthor = 0;
	fixed_string<50> chat;
};
