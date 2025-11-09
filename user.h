#pragma once

#include "std.h"

struct Users {
	struct User {
		char name[20];
		int idx;
	};
	inplace_vector<User, 6> users;

	std::optional<int> Add(char name[20]);
	optional_ref<User> Get(int id);
	size_t size() const;
};

extern Users users;
