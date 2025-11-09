#pragma once

struct ServerboundPacket {
	enum class Kind {
		Claim,
		Register
	} kind;
	union {
		struct {
			int a;
			int b;
		};
		char name[20];
	};
};

struct ClientboundPacket {
	enum class Kind {
		Claim,
		Id,
		AddUser,
		Fail,
		Tick
	} kind;
	union {
		struct {
			int a;
			int b;
			int c;
		};
		int id;
		char name[20];
		char failmsg[20];
	};
};
