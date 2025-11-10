#pragma once

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
		char name[20];
		char chat[50];
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
		char name[20];
		char failmsg[20];
		struct {
			int chatAuthor;
			char chat[50];
		};
	};
};
