#pragma once

#include "std.h"

struct ServerboundPacket {
	enum class Kind {
		Claim,
		Register,
		Chat
	} kind;
	union {
		struct {
			int a;
			int b;
		};
		fixed_string<20> name;
		fixed_string<50> chat;
	};
};

struct ClientboundPacket {
	enum class Kind {
		Claim,
		Id,
		AddUser,
		Fail,
		Tick,
		Chat
	} kind;
	union {
		struct {
			int a;
			int b;
			int c;
		};
		int id;
		fixed_string<20> name;
		fixed_string<20> failmsg;
		struct {
			int chatAuthor;
			fixed_string<50> chat;
		};
	};
};
